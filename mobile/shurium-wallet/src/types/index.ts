/**
 * SHURIUM Mobile Wallet Types
 * Core type definitions for the wallet application
 */

// Network types
export type NetworkType = 'mainnet' | 'testnet' | 'regtest';

// Wallet types
export interface WalletConfig {
  network: NetworkType;
  rpcHost: string;
  rpcPort: number;
  rpcUser?: string;
  rpcPassword?: string;
}

export interface WalletAccount {
  id: string;
  name: string;
  address: string;
  publicKey: string;
  derivationPath: string;
  createdAt: number;
  isDefault: boolean;
}

export interface WalletState {
  isInitialized: boolean;
  isLocked: boolean;
  hasBackup: boolean;
  accounts: WalletAccount[];
  activeAccountId: string | null;
  network: NetworkType;
}

// Balance types
export interface Balance {
  confirmed: string;
  unconfirmed: string;
  total: string;
  confirmedSatoshis: number;
  unconfirmedSatoshis: number;
}

// Transaction types
export type TransactionType = 'send' | 'receive' | 'stake' | 'unstake' | 'ubi_claim' | 'unknown';
export type TransactionStatus = 'pending' | 'confirmed' | 'failed';

export interface Transaction {
  txid: string;
  type: TransactionType;
  status: TransactionStatus;
  amount: string;
  amountSatoshis: number;
  fee: string;
  feeSatoshis: number;
  fromAddress: string;
  toAddress: string;
  confirmations: number;
  blockHeight: number | null;
  blockHash: string | null;
  timestamp: number;
  memo?: string;
}

export interface TransactionInput {
  txid: string;
  vout: number;
  address: string;
  amount: number;
}

export interface TransactionOutput {
  address: string;
  amount: number;
}

// UTXO types
export interface UTXO {
  txid: string;
  vout: number;
  address: string;
  amount: number;
  confirmations: number;
  spendable: boolean;
}

// Staking types
export interface StakingInfo {
  isStaking: boolean;
  totalStaked: string;
  rewards: string;
  validators: ValidatorInfo[];
  delegations: DelegationInfo[];
}

export interface ValidatorInfo {
  id: string;
  address: string;
  name: string;
  commission: number;
  totalStaked: string;
  delegatorCount: number;
  isActive: boolean;
  uptime: number;
}

export interface DelegationInfo {
  validatorId: string;
  validatorName: string;
  amount: string;
  rewards: string;
  startTime: number;
  unlockTime: number | null;
}

// UBI types
export interface UBIInfo {
  identityId: string | null;
  isRegistered: boolean;
  isEligible: boolean;
  verificationLevel: string;
  lastClaimTime: number | null;
  nextClaimTime: number | null;
  claimableAmount: string;
  totalClaimed: string;
  claimHistory: UBIClaim[];
}

export interface UBIClaim {
  txid: string;
  amount: string;
  timestamp: number;
  blockHeight: number;
}

// RPC types
export interface RPCRequest {
  jsonrpc: '2.0';
  id: number;
  method: string;
  params: unknown[];
}

export interface RPCResponse<T = unknown> {
  jsonrpc: '2.0';
  id: number;
  result?: T;
  error?: RPCError;
}

export interface RPCError {
  code: number;
  message: string;
  data?: unknown;
}

// Blockchain info
export interface BlockchainInfo {
  chain: string;
  blocks: number;
  headers: number;
  bestBlockHash: string;
  difficulty: number;
  medianTime: number;
  verificationProgress: number;
  pruned: boolean;
  initialBlockDownload: boolean;
}

// Network info
export interface NetworkInfo {
  version: number;
  subversion: string;
  protocolVersion: number;
  connections: number;
  networkActive: boolean;
  reachable: boolean;
}

// Address validation
export interface AddressValidation {
  isValid: boolean;
  address: string;
  isScript: boolean;
  network: NetworkType;
}

// Fee estimation
export interface FeeEstimate {
  feerate: number;
  blocks: number;
}

// Send transaction
export interface SendTransactionParams {
  toAddress: string;
  amount: number;
  fee?: number;
  memo?: string;
  subtractFee?: boolean;
}

export interface SendTransactionResult {
  txid: string;
  hex: string;
  fee: number;
}

// QR Code data
export interface QRPaymentRequest {
  address: string;
  amount?: number;
  label?: string;
  message?: string;
}
