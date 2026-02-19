/**
 * SHURIUM Mobile Wallet - RPC Service
 * Handles communication with SHURIUM node via JSON-RPC
 */

import {
  RPCRequest,
  RPCResponse,
  RPCError,
  WalletConfig,
  Balance,
  Transaction,
  TransactionType,
  TransactionStatus,
  UTXO,
  BlockchainInfo,
  NetworkInfo,
  FeeEstimate,
  StakingInfo,
  ValidatorInfo,
  DelegationInfo,
  UBIInfo,
  AddressValidation,
  SendTransactionResult,
} from '../types';

class RPCService {
  private config: WalletConfig;
  private requestId: number = 0;

  constructor(config: WalletConfig) {
    this.config = config;
  }

  /**
   * Update RPC configuration
   */
  updateConfig(config: Partial<WalletConfig>): void {
    this.config = { ...this.config, ...config };
  }

  /**
   * Get current configuration
   */
  getConfig(): WalletConfig {
    return { ...this.config };
  }

  /**
   * Make an RPC call to the SHURIUM node
   */
  private async call<T>(method: string, params: unknown[] = []): Promise<T> {
    const request: RPCRequest = {
      jsonrpc: '2.0',
      id: ++this.requestId,
      method,
      params,
    };

    const url = `http://${this.config.rpcHost}:${this.config.rpcPort}`;
    
    const headers: Record<string, string> = {
      'Content-Type': 'application/json',
    };

    // Add basic auth if credentials provided
    if (this.config.rpcUser && this.config.rpcPassword) {
      const auth = Buffer.from(`${this.config.rpcUser}:${this.config.rpcPassword}`).toString('base64');
      headers['Authorization'] = `Basic ${auth}`;
    }

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers,
        body: JSON.stringify(request),
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json() as RPCResponse<T>;

      if (data.error) {
        throw new RPCServiceError(data.error.code, data.error.message, data.error.data);
      }

      return data.result as T;
    } catch (error) {
      if (error instanceof RPCServiceError) {
        throw error;
      }
      throw new RPCServiceError(-1, `Network error: ${(error as Error).message}`);
    }
  }

  // ===========================================================================
  // Blockchain Info
  // ===========================================================================

  async getBlockchainInfo(): Promise<BlockchainInfo> {
    const result = await this.call<any>('getblockchaininfo');
    return {
      chain: result.chain,
      blocks: result.blocks,
      headers: result.headers,
      bestBlockHash: result.bestblockhash,
      difficulty: result.difficulty,
      medianTime: result.mediantime,
      verificationProgress: result.verificationprogress,
      pruned: result.pruned,
      initialBlockDownload: result.initialblockdownload,
    };
  }

  async getNetworkInfo(): Promise<NetworkInfo> {
    const result = await this.call<any>('getnetworkinfo');
    return {
      version: result.version,
      subversion: result.subversion,
      protocolVersion: result.protocolversion,
      connections: result.connections,
      networkActive: result.networkactive,
      reachable: result.reachable ?? true,
    };
  }

  async getBlockCount(): Promise<number> {
    return this.call<number>('getblockcount');
  }

  // ===========================================================================
  // Wallet Operations
  // ===========================================================================

  async getBalance(address?: string): Promise<Balance> {
    // Get wallet balance or balance for specific address
    const result = await this.call<any>('getbalance', address ? [address] : []);
    
    // Handle both object and number responses
    if (typeof result === 'number' || typeof result === 'string') {
      const total = parseFloat(String(result));
      const satoshis = Math.round(total * 100000000);
      return {
        confirmed: String(total),
        unconfirmed: '0',
        total: String(total),
        confirmedSatoshis: satoshis,
        unconfirmedSatoshis: 0,
      };
    }

    return {
      confirmed: result.confirmed ?? result.balance ?? '0',
      unconfirmed: result.unconfirmed ?? '0',
      total: result.total ?? result.balance ?? '0',
      confirmedSatoshis: Math.round((parseFloat(result.confirmed ?? result.balance ?? '0')) * 100000000),
      unconfirmedSatoshis: Math.round((parseFloat(result.unconfirmed ?? '0')) * 100000000),
    };
  }

  async getNewAddress(label?: string): Promise<string> {
    return this.call<string>('getnewaddress', label ? [label] : []);
  }

  async listAddresses(): Promise<string[]> {
    const result = await this.call<any>('listaddresses');
    return Array.isArray(result) ? result : [];
  }

  async validateAddress(address: string): Promise<AddressValidation> {
    const result = await this.call<any>('validateaddress', [address]);
    return {
      isValid: result.isvalid,
      address: result.address ?? address,
      isScript: result.isscript ?? false,
      network: this.config.network,
    };
  }

  // ===========================================================================
  // Transactions
  // ===========================================================================

  async listTransactions(count: number = 10, skip: number = 0): Promise<Transaction[]> {
    const result = await this.call<any[]>('listtransactions', ['*', count, skip]);
    
    return result.map(tx => this.mapTransaction(tx));
  }

  async getTransaction(txid: string): Promise<Transaction> {
    const result = await this.call<any>('gettransaction', [txid]);
    return this.mapTransaction(result);
  }

  async sendToAddress(
    address: string,
    amount: number,
    comment?: string,
    subtractFee: boolean = false
  ): Promise<string> {
    const params: any[] = [address, amount];
    if (comment) params.push(comment);
    if (subtractFee) {
      params.push(''); // comment_to
      params.push(subtractFee);
    }
    
    return this.call<string>('sendtoaddress', params);
  }

  async createRawTransaction(
    inputs: Array<{ txid: string; vout: number }>,
    outputs: Record<string, number>
  ): Promise<string> {
    return this.call<string>('createrawtransaction', [inputs, outputs]);
  }

  async signRawTransaction(hexString: string): Promise<{ hex: string; complete: boolean }> {
    return this.call<{ hex: string; complete: boolean }>('signrawtransaction', [hexString]);
  }

  async sendRawTransaction(hexString: string): Promise<string> {
    return this.call<string>('sendrawtransaction', [hexString]);
  }

  async estimateFee(blocks: number = 6): Promise<FeeEstimate> {
    const result = await this.call<any>('estimatefee', [blocks]);
    return {
      feerate: typeof result === 'number' ? result : result.feerate ?? 0.0001,
      blocks,
    };
  }

  // ===========================================================================
  // UTXOs
  // ===========================================================================

  async listUnspent(minConfirmations: number = 1, maxConfirmations: number = 9999999): Promise<UTXO[]> {
    const result = await this.call<any[]>('listunspent', [minConfirmations, maxConfirmations]);
    
    return result.map(utxo => ({
      txid: utxo.txid,
      vout: utxo.vout,
      address: utxo.address,
      amount: utxo.amount,
      confirmations: utxo.confirmations,
      spendable: utxo.spendable ?? true,
    }));
  }

  // ===========================================================================
  // Staking
  // ===========================================================================

  async getStakingInfo(): Promise<StakingInfo> {
    const result = await this.call<any>('getstakinginfo');
    
    return {
      isStaking: result.staking ?? result.enabled ?? false,
      totalStaked: result.totalstaked ?? result.balance ?? '0',
      rewards: result.rewards ?? '0',
      validators: [],
      delegations: [],
    };
  }

  async listValidators(): Promise<ValidatorInfo[]> {
    const result = await this.call<any[]>('listvalidators');
    
    return result.map(v => ({
      id: v.id ?? v.validator_id,
      address: v.address,
      name: v.name ?? 'Validator',
      commission: v.commission ?? 0,
      totalStaked: v.total_staked ?? v.stake ?? '0',
      delegatorCount: v.delegator_count ?? v.delegators ?? 0,
      isActive: v.active ?? true,
      uptime: v.uptime ?? 100,
    }));
  }

  async getValidatorInfo(validatorId: string): Promise<ValidatorInfo> {
    const result = await this.call<any>('getvalidatorinfo', [validatorId]);
    
    return {
      id: result.id ?? result.validator_id ?? validatorId,
      address: result.address,
      name: result.name ?? 'Validator',
      commission: result.commission ?? 0,
      totalStaked: result.total_staked ?? result.stake ?? '0',
      delegatorCount: result.delegator_count ?? result.delegators ?? 0,
      isActive: result.active ?? true,
      uptime: result.uptime ?? 100,
    };
  }

  async delegate(validatorId: string, amount: number): Promise<string> {
    return this.call<string>('delegate', [validatorId, amount]);
  }

  async undelegate(validatorId: string, amount: number): Promise<string> {
    return this.call<string>('undelegate', [validatorId, amount]);
  }

  async claimStakingRewards(): Promise<string> {
    return this.call<string>('claimrewards');
  }

  async listDelegations(): Promise<DelegationInfo[]> {
    const result = await this.call<any[]>('listdelegations');
    
    return result.map(d => ({
      validatorId: d.validator_id ?? d.validator,
      validatorName: d.validator_name ?? 'Validator',
      amount: d.amount ?? d.stake ?? '0',
      rewards: d.rewards ?? '0',
      startTime: d.start_time ?? d.created ?? 0,
      unlockTime: d.unlock_time ?? null,
    }));
  }

  // ===========================================================================
  // UBI (Universal Basic Income)
  // ===========================================================================

  async getIdentityInfo(address?: string): Promise<UBIInfo> {
    try {
      const result = await this.call<any>('getidentityinfo', address ? [address] : []);
      
      return {
        identityId: result.identity_id ?? result.id ?? null,
        isRegistered: result.registered ?? result.has_identity ?? false,
        isEligible: result.eligible ?? result.ubi_eligible ?? false,
        verificationLevel: result.verification_level ?? 'none',
        lastClaimTime: result.last_claim ?? result.last_claim_time ?? null,
        nextClaimTime: result.next_claim ?? result.next_claim_time ?? null,
        claimableAmount: result.claimable ?? result.claimable_amount ?? '0',
        totalClaimed: result.total_claimed ?? '0',
        claimHistory: [],
      };
    } catch (error) {
      // Return default UBI info if identity not found
      return {
        identityId: null,
        isRegistered: false,
        isEligible: false,
        verificationLevel: 'none',
        lastClaimTime: null,
        nextClaimTime: null,
        claimableAmount: '0',
        totalClaimed: '0',
        claimHistory: [],
      };
    }
  }

  async claimUBI(identityId: string): Promise<string> {
    return this.call<string>('claimubi', [identityId]);
  }

  async getUBIInfo(identityId: string): Promise<UBIInfo> {
    const result = await this.call<any>('getubiinfo', [identityId]);
    
    return {
      identityId,
      isRegistered: true,
      isEligible: result.eligible ?? true,
      verificationLevel: result.verification_level ?? 'basic',
      lastClaimTime: result.last_claim ?? null,
      nextClaimTime: result.next_claim ?? null,
      claimableAmount: result.claimable ?? '0',
      totalClaimed: result.total_claimed ?? '0',
      claimHistory: (result.claims ?? []).map((c: any) => ({
        txid: c.txid,
        amount: c.amount,
        timestamp: c.timestamp ?? c.time,
        blockHeight: c.block_height ?? c.height ?? 0,
      })),
    };
  }

  // ===========================================================================
  // Faucet (Testnet only)
  // ===========================================================================

  async getFaucet(address?: string, amount?: number): Promise<string> {
    const params: any[] = [];
    if (address) params.push(address);
    if (amount) params.push(amount);
    
    return this.call<string>('getfaucet', params);
  }

  // ===========================================================================
  // Private Helpers
  // ===========================================================================

  private mapTransaction(tx: any): Transaction {
    return {
      txid: tx.txid,
      type: this.determineTransactionType(tx),
      status: this.determineTransactionStatus(tx),
      amount: String(Math.abs(tx.amount ?? 0)),
      amountSatoshis: Math.round(Math.abs(tx.amount ?? 0) * 100000000),
      fee: String(tx.fee ?? 0),
      feeSatoshis: Math.round((tx.fee ?? 0) * 100000000),
      fromAddress: tx.address ?? '',
      toAddress: tx.address ?? '',
      confirmations: tx.confirmations ?? 0,
      blockHeight: tx.blockheight ?? tx.block_height ?? null,
      blockHash: tx.blockhash ?? tx.block_hash ?? null,
      timestamp: tx.time ?? tx.timestamp ?? tx.blocktime ?? Date.now() / 1000,
      memo: tx.comment ?? tx.label ?? undefined,
    };
  }

  private determineTransactionType(tx: any): TransactionType {
    const category = tx.category?.toLowerCase();
    
    if (category === 'send') return 'send';
    if (category === 'receive') return 'receive';
    if (category === 'stake' || tx.is_stake) return 'stake';
    if (category === 'unstake' || tx.is_unstake) return 'unstake';
    if (category === 'ubi' || tx.is_ubi) return 'ubi_claim';
    
    // Determine by amount sign
    if (tx.amount < 0) return 'send';
    if (tx.amount > 0) return 'receive';
    
    return 'unknown';
  }

  private determineTransactionStatus(tx: any): TransactionStatus {
    if (tx.confirmations === undefined || tx.confirmations === null) {
      return 'pending';
    }
    if (tx.confirmations < 0) {
      return 'failed';
    }
    if (tx.confirmations === 0) {
      return 'pending';
    }
    return 'confirmed';
  }
}

/**
 * Custom error class for RPC errors
 */
export class RPCServiceError extends Error {
  code: number;
  data?: unknown;

  constructor(code: number, message: string, data?: unknown) {
    super(message);
    this.name = 'RPCServiceError';
    this.code = code;
    this.data = data;
  }
}

// Default configuration
const DEFAULT_CONFIG: WalletConfig = {
  network: 'mainnet',
  rpcHost: 'localhost',
  rpcPort: 8332,
};

// Singleton instance
let rpcServiceInstance: RPCService | null = null;

/**
 * Get or create RPC service instance
 */
export function getRPCService(config?: WalletConfig): RPCService {
  if (!rpcServiceInstance) {
    rpcServiceInstance = new RPCService(config ?? DEFAULT_CONFIG);
  } else if (config) {
    rpcServiceInstance.updateConfig(config);
  }
  return rpcServiceInstance;
}

/**
 * Reset RPC service instance (for testing)
 */
export function resetRPCService(): void {
  rpcServiceInstance = null;
}

export { RPCService };
export default RPCService;
