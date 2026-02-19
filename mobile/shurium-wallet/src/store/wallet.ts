/**
 * SHURIUM Mobile Wallet - Wallet Store
 * Global state management using Zustand
 */

import { create } from 'zustand';
import { persist, createJSONStorage } from 'zustand/middleware';
import AsyncStorage from '@react-native-async-storage/async-storage';
import {
  WalletState,
  WalletAccount,
  NetworkType,
  Balance,
  Transaction,
  StakingInfo,
  UBIInfo,
  WalletConfig,
} from '../types';
import { getRPCService } from '../services/rpc';
import { generateMnemonic, validateMnemonic, getDerivationPath } from '../utils/crypto';

// Network configurations
const NETWORK_CONFIGS: Record<NetworkType, { rpcPort: number; rpcHost: string }> = {
  mainnet: { rpcHost: 'localhost', rpcPort: 8332 },
  testnet: { rpcHost: 'localhost', rpcPort: 18332 },
  regtest: { rpcHost: 'localhost', rpcPort: 18443 },
};

interface WalletStore extends WalletState {
  // Balances
  balance: Balance | null;
  
  // Transactions
  transactions: Transaction[];
  isLoadingTransactions: boolean;
  
  // Staking
  stakingInfo: StakingInfo | null;
  
  // UBI
  ubiInfo: UBIInfo | null;
  
  // Connection status
  isConnected: boolean;
  lastError: string | null;
  
  // Actions - Wallet Management
  initializeWallet: (mnemonic?: string, password?: string) => Promise<boolean>;
  lockWallet: () => void;
  unlockWallet: (password: string) => Promise<boolean>;
  setBackupComplete: () => void;
  
  // Actions - Account Management
  addAccount: (name: string) => Promise<WalletAccount | null>;
  setActiveAccount: (accountId: string) => void;
  renameAccount: (accountId: string, name: string) => void;
  
  // Actions - Network
  setNetwork: (network: NetworkType) => void;
  updateRPCConfig: (config: Partial<WalletConfig>) => void;
  checkConnection: () => Promise<boolean>;
  
  // Actions - Data Fetching
  refreshBalance: () => Promise<void>;
  refreshTransactions: (count?: number) => Promise<void>;
  refreshStakingInfo: () => Promise<void>;
  refreshUBIInfo: () => Promise<void>;
  refreshAll: () => Promise<void>;
  
  // Actions - Transactions
  sendTransaction: (toAddress: string, amount: number, memo?: string) => Promise<string>;
  
  // Actions - Staking
  stake: (validatorId: string, amount: number) => Promise<string>;
  unstake: (validatorId: string, amount: number) => Promise<string>;
  claimStakingRewards: () => Promise<string>;
  
  // Actions - UBI
  claimUBI: () => Promise<string>;
  
  // Actions - Testnet
  requestFaucet: (amount?: number) => Promise<string>;
  
  // Utility
  clearError: () => void;
  reset: () => void;
}

const initialState: Omit<WalletStore, 
  'initializeWallet' | 'lockWallet' | 'unlockWallet' | 'setBackupComplete' |
  'addAccount' | 'setActiveAccount' | 'renameAccount' |
  'setNetwork' | 'updateRPCConfig' | 'checkConnection' |
  'refreshBalance' | 'refreshTransactions' | 'refreshStakingInfo' | 'refreshUBIInfo' | 'refreshAll' |
  'sendTransaction' | 'stake' | 'unstake' | 'claimStakingRewards' | 'claimUBI' | 'requestFaucet' |
  'clearError' | 'reset'
> = {
  isInitialized: false,
  isLocked: true,
  hasBackup: false,
  accounts: [],
  activeAccountId: null,
  network: 'mainnet',
  balance: null,
  transactions: [],
  isLoadingTransactions: false,
  stakingInfo: null,
  ubiInfo: null,
  isConnected: false,
  lastError: null,
};

export const useWalletStore = create<WalletStore>()(
  persist(
    (set, get) => ({
      ...initialState,

      // =========================================================================
      // Wallet Management
      // =========================================================================

      initializeWallet: async (mnemonic?: string, password?: string) => {
        try {
          // Generate or validate mnemonic
          const finalMnemonic = mnemonic || generateMnemonic();
          
          if (mnemonic && !validateMnemonic(mnemonic)) {
            set({ lastError: 'Invalid mnemonic phrase' });
            return false;
          }

          // In a real implementation, we would:
          // 1. Derive master key from mnemonic
          // 2. Store encrypted mnemonic in secure storage
          // 3. Generate first account
          
          // For now, we'll use the node's wallet management
          const rpc = getRPCService();
          const address = await rpc.getNewAddress('Default');
          
          const account: WalletAccount = {
            id: '0',
            name: 'Default',
            address,
            publicKey: '', // Would be derived from mnemonic
            derivationPath: getDerivationPath(0),
            createdAt: Date.now(),
            isDefault: true,
          };

          set({
            isInitialized: true,
            isLocked: false,
            accounts: [account],
            activeAccountId: account.id,
            lastError: null,
          });

          // Refresh data
          await get().refreshAll();

          return true;
        } catch (error) {
          set({ lastError: `Failed to initialize wallet: ${(error as Error).message}` });
          return false;
        }
      },

      lockWallet: () => {
        set({ isLocked: true });
      },

      unlockWallet: async (password: string) => {
        try {
          // In a real implementation, verify password against stored hash
          // and decrypt the mnemonic
          
          set({ isLocked: false, lastError: null });
          await get().refreshAll();
          return true;
        } catch (error) {
          set({ lastError: `Failed to unlock wallet: ${(error as Error).message}` });
          return false;
        }
      },

      setBackupComplete: () => {
        set({ hasBackup: true });
      },

      // =========================================================================
      // Account Management
      // =========================================================================

      addAccount: async (name: string) => {
        try {
          const rpc = getRPCService();
          const address = await rpc.getNewAddress(name);
          
          const accounts = get().accounts;
          const newId = String(accounts.length);
          
          const account: WalletAccount = {
            id: newId,
            name,
            address,
            publicKey: '',
            derivationPath: getDerivationPath(parseInt(newId)),
            createdAt: Date.now(),
            isDefault: false,
          };

          set({ accounts: [...accounts, account] });
          return account;
        } catch (error) {
          set({ lastError: `Failed to add account: ${(error as Error).message}` });
          return null;
        }
      },

      setActiveAccount: (accountId: string) => {
        const account = get().accounts.find(a => a.id === accountId);
        if (account) {
          set({ activeAccountId: accountId });
        }
      },

      renameAccount: (accountId: string, name: string) => {
        const accounts = get().accounts.map(a => 
          a.id === accountId ? { ...a, name } : a
        );
        set({ accounts });
      },

      // =========================================================================
      // Network
      // =========================================================================

      setNetwork: (network: NetworkType) => {
        const config = NETWORK_CONFIGS[network];
        const rpc = getRPCService();
        rpc.updateConfig({ network, ...config });
        
        set({ network, isConnected: false });
        get().checkConnection();
      },

      updateRPCConfig: (config: Partial<WalletConfig>) => {
        const rpc = getRPCService();
        rpc.updateConfig(config);
        get().checkConnection();
      },

      checkConnection: async () => {
        try {
          const rpc = getRPCService();
          await rpc.getBlockCount();
          set({ isConnected: true, lastError: null });
          return true;
        } catch (error) {
          set({ isConnected: false, lastError: `Connection failed: ${(error as Error).message}` });
          return false;
        }
      },

      // =========================================================================
      // Data Fetching
      // =========================================================================

      refreshBalance: async () => {
        try {
          const rpc = getRPCService();
          const balance = await rpc.getBalance();
          set({ balance, lastError: null });
        } catch (error) {
          set({ lastError: `Failed to fetch balance: ${(error as Error).message}` });
        }
      },

      refreshTransactions: async (count: number = 20) => {
        try {
          set({ isLoadingTransactions: true });
          const rpc = getRPCService();
          const transactions = await rpc.listTransactions(count);
          set({ transactions, isLoadingTransactions: false, lastError: null });
        } catch (error) {
          set({ 
            isLoadingTransactions: false,
            lastError: `Failed to fetch transactions: ${(error as Error).message}` 
          });
        }
      },

      refreshStakingInfo: async () => {
        try {
          const rpc = getRPCService();
          const stakingInfo = await rpc.getStakingInfo();
          
          // Also fetch delegations
          const delegations = await rpc.listDelegations();
          stakingInfo.delegations = delegations;
          
          // Fetch validators
          const validators = await rpc.listValidators();
          stakingInfo.validators = validators;
          
          set({ stakingInfo, lastError: null });
        } catch (error) {
          // Staking may not be available on all networks
          set({ stakingInfo: null });
        }
      },

      refreshUBIInfo: async () => {
        try {
          const activeAccount = get().accounts.find(a => a.id === get().activeAccountId);
          if (!activeAccount) return;
          
          const rpc = getRPCService();
          const ubiInfo = await rpc.getIdentityInfo(activeAccount.address);
          set({ ubiInfo, lastError: null });
        } catch (error) {
          // UBI info may not be available
          set({ ubiInfo: null });
        }
      },

      refreshAll: async () => {
        await Promise.all([
          get().refreshBalance(),
          get().refreshTransactions(),
          get().refreshStakingInfo(),
          get().refreshUBIInfo(),
        ]);
      },

      // =========================================================================
      // Transactions
      // =========================================================================

      sendTransaction: async (toAddress: string, amount: number, memo?: string) => {
        try {
          const rpc = getRPCService();
          const txid = await rpc.sendToAddress(toAddress, amount, memo);
          
          // Refresh balance and transactions after sending
          await get().refreshBalance();
          await get().refreshTransactions();
          
          return txid;
        } catch (error) {
          const errorMessage = `Failed to send transaction: ${(error as Error).message}`;
          set({ lastError: errorMessage });
          throw new Error(errorMessage);
        }
      },

      // =========================================================================
      // Staking
      // =========================================================================

      stake: async (validatorId: string, amount: number) => {
        try {
          const rpc = getRPCService();
          const txid = await rpc.delegate(validatorId, amount);
          
          await get().refreshBalance();
          await get().refreshStakingInfo();
          
          return txid;
        } catch (error) {
          const errorMessage = `Failed to stake: ${(error as Error).message}`;
          set({ lastError: errorMessage });
          throw new Error(errorMessage);
        }
      },

      unstake: async (validatorId: string, amount: number) => {
        try {
          const rpc = getRPCService();
          const txid = await rpc.undelegate(validatorId, amount);
          
          await get().refreshBalance();
          await get().refreshStakingInfo();
          
          return txid;
        } catch (error) {
          const errorMessage = `Failed to unstake: ${(error as Error).message}`;
          set({ lastError: errorMessage });
          throw new Error(errorMessage);
        }
      },

      claimStakingRewards: async () => {
        try {
          const rpc = getRPCService();
          const txid = await rpc.claimStakingRewards();
          
          await get().refreshBalance();
          await get().refreshStakingInfo();
          
          return txid;
        } catch (error) {
          const errorMessage = `Failed to claim rewards: ${(error as Error).message}`;
          set({ lastError: errorMessage });
          throw new Error(errorMessage);
        }
      },

      // =========================================================================
      // UBI
      // =========================================================================

      claimUBI: async () => {
        try {
          const ubiInfo = get().ubiInfo;
          if (!ubiInfo?.identityId) {
            throw new Error('No identity registered');
          }
          
          const rpc = getRPCService();
          const txid = await rpc.claimUBI(ubiInfo.identityId);
          
          await get().refreshBalance();
          await get().refreshUBIInfo();
          
          return txid;
        } catch (error) {
          const errorMessage = `Failed to claim UBI: ${(error as Error).message}`;
          set({ lastError: errorMessage });
          throw new Error(errorMessage);
        }
      },

      // =========================================================================
      // Testnet
      // =========================================================================

      requestFaucet: async (amount?: number) => {
        try {
          const network = get().network;
          if (network === 'mainnet') {
            throw new Error('Faucet is only available on testnet and regtest');
          }
          
          const activeAccount = get().accounts.find(a => a.id === get().activeAccountId);
          if (!activeAccount) {
            throw new Error('No active account');
          }
          
          const rpc = getRPCService();
          const txid = await rpc.getFaucet(activeAccount.address, amount);
          
          await get().refreshBalance();
          await get().refreshTransactions();
          
          return txid;
        } catch (error) {
          const errorMessage = `Failed to request faucet: ${(error as Error).message}`;
          set({ lastError: errorMessage });
          throw new Error(errorMessage);
        }
      },

      // =========================================================================
      // Utility
      // =========================================================================

      clearError: () => {
        set({ lastError: null });
      },

      reset: () => {
        set(initialState);
      },
    }),
    {
      name: 'shurium-wallet-storage',
      storage: createJSONStorage(() => AsyncStorage),
      partialize: (state) => ({
        isInitialized: state.isInitialized,
        hasBackup: state.hasBackup,
        accounts: state.accounts,
        activeAccountId: state.activeAccountId,
        network: state.network,
      }),
    }
  )
);

export default useWalletStore;
