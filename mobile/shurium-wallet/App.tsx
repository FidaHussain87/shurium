/**
 * SHURIUM Mobile Wallet
 * Main App Entry Point
 */

import React, { useEffect } from 'react';
import { StatusBar, LogBox } from 'react-native';
import { GestureHandlerRootView } from 'react-native-gesture-handler';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { AppNavigation } from './src/navigation/AppNavigation';
import { useWalletStore } from './src/store/wallet';
import { getRPCService } from './src/services/rpc';

// Ignore specific warnings in development
LogBox.ignoreLogs([
  'Non-serializable values were found in the navigation state',
]);

const App: React.FC = () => {
  const { network, isInitialized, checkConnection, initializeWallet } = useWalletStore();

  useEffect(() => {
    // Initialize RPC service with default network config
    const networkConfigs = {
      mainnet: { rpcHost: 'localhost', rpcPort: 8332 },
      testnet: { rpcHost: 'localhost', rpcPort: 18332 },
      regtest: { rpcHost: 'localhost', rpcPort: 18443 },
    };

    const config = networkConfigs[network];
    getRPCService({
      network,
      ...config,
    });

    // Check connection on startup
    checkConnection();

    // Auto-initialize wallet if not already done
    // In production, this would prompt for wallet creation/import
    if (!isInitialized) {
      initializeWallet();
    }
  }, []);

  return (
    <GestureHandlerRootView style={{ flex: 1 }}>
      <SafeAreaProvider>
        <StatusBar barStyle="light-content" backgroundColor="#121212" />
        <AppNavigation />
      </SafeAreaProvider>
    </GestureHandlerRootView>
  );
};

export default App;
