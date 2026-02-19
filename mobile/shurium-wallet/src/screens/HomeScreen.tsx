/**
 * SHURIUM Mobile Wallet - Home Screen
 * Main wallet dashboard showing balance, quick actions, and recent transactions
 */

import React, { useEffect, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  RefreshControl,
  TouchableOpacity,
  ActivityIndicator,
} from 'react-native';
import { useNavigation } from '@react-navigation/native';
import { useWalletStore } from '../store/wallet';
import { formatSHR } from '../utils/crypto';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';
import type { Transaction } from '../types';

type NavigationProp = NativeStackNavigationProp<any>;

export const HomeScreen: React.FC = () => {
  const navigation = useNavigation<NavigationProp>();
  const {
    isConnected,
    network,
    balance,
    transactions,
    isLoadingTransactions,
    accounts,
    activeAccountId,
    ubiInfo,
    lastError,
    refreshAll,
    checkConnection,
    clearError,
  } = useWalletStore();

  const [refreshing, setRefreshing] = React.useState(false);

  const activeAccount = accounts.find(a => a.id === activeAccountId);

  useEffect(() => {
    checkConnection();
  }, []);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await refreshAll();
    setRefreshing(false);
  }, [refreshAll]);

  const getNetworkColor = () => {
    switch (network) {
      case 'mainnet': return '#4CAF50';
      case 'testnet': return '#FF9800';
      case 'regtest': return '#9C27B0';
    }
  };

  const formatAmount = (amount: string | number) => {
    const num = typeof amount === 'string' ? parseFloat(amount) : amount;
    return num.toFixed(8);
  };

  const getTransactionIcon = (type: string) => {
    switch (type) {
      case 'send': return '-';
      case 'receive': return '+';
      case 'stake': return 'S';
      case 'unstake': return 'U';
      case 'ubi_claim': return 'UBI';
      default: return '?';
    }
  };

  const getTransactionColor = (type: string) => {
    switch (type) {
      case 'send': return '#F44336';
      case 'receive': return '#4CAF50';
      case 'stake': return '#2196F3';
      case 'unstake': return '#FF9800';
      case 'ubi_claim': return '#9C27B0';
      default: return '#757575';
    }
  };

  return (
    <ScrollView
      style={styles.container}
      refreshControl={
        <RefreshControl refreshing={refreshing} onRefresh={onRefresh} />
      }
    >
      {/* Connection Status */}
      <View style={styles.statusBar}>
        <View style={[styles.statusDot, { backgroundColor: isConnected ? '#4CAF50' : '#F44336' }]} />
        <Text style={styles.statusText}>
          {isConnected ? 'Connected' : 'Disconnected'}
        </Text>
        <View style={[styles.networkBadge, { backgroundColor: getNetworkColor() }]}>
          <Text style={styles.networkText}>{network.toUpperCase()}</Text>
        </View>
      </View>

      {/* Error Banner */}
      {lastError && (
        <TouchableOpacity style={styles.errorBanner} onPress={clearError}>
          <Text style={styles.errorText}>{lastError}</Text>
          <Text style={styles.errorDismiss}>Tap to dismiss</Text>
        </TouchableOpacity>
      )}

      {/* Balance Card */}
      <View style={styles.balanceCard}>
        <Text style={styles.balanceLabel}>Total Balance</Text>
        <Text style={styles.balanceAmount}>
          {balance ? formatAmount(balance.total) : '0.00000000'} SHR
        </Text>
        {balance && balance.unconfirmedSatoshis > 0 && (
          <Text style={styles.pendingBalance}>
            +{formatAmount(balance.unconfirmed)} pending
          </Text>
        )}
        {activeAccount && (
          <Text style={styles.addressText} numberOfLines={1} ellipsizeMode="middle">
            {activeAccount.address}
          </Text>
        )}
      </View>

      {/* Quick Actions */}
      <View style={styles.actionsContainer}>
        <TouchableOpacity
          style={styles.actionButton}
          onPress={() => navigation.navigate('Send')}
        >
          <View style={[styles.actionIcon, { backgroundColor: '#F44336' }]}>
            <Text style={styles.actionIconText}>^</Text>
          </View>
          <Text style={styles.actionText}>Send</Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={styles.actionButton}
          onPress={() => navigation.navigate('Receive')}
        >
          <View style={[styles.actionIcon, { backgroundColor: '#4CAF50' }]}>
            <Text style={styles.actionIconText}>v</Text>
          </View>
          <Text style={styles.actionText}>Receive</Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={styles.actionButton}
          onPress={() => navigation.navigate('Staking')}
        >
          <View style={[styles.actionIcon, { backgroundColor: '#2196F3' }]}>
            <Text style={styles.actionIconText}>%</Text>
          </View>
          <Text style={styles.actionText}>Stake</Text>
        </TouchableOpacity>

        <TouchableOpacity
          style={styles.actionButton}
          onPress={() => navigation.navigate('UBI')}
        >
          <View style={[styles.actionIcon, { backgroundColor: '#9C27B0' }]}>
            <Text style={styles.actionIconText}>$</Text>
          </View>
          <Text style={styles.actionText}>UBI</Text>
        </TouchableOpacity>
      </View>

      {/* UBI Card (if eligible) */}
      {ubiInfo && ubiInfo.isEligible && parseFloat(ubiInfo.claimableAmount) > 0 && (
        <TouchableOpacity
          style={styles.ubiCard}
          onPress={() => navigation.navigate('UBI')}
        >
          <Text style={styles.ubiTitle}>UBI Available</Text>
          <Text style={styles.ubiAmount}>
            {formatAmount(ubiInfo.claimableAmount)} SHR
          </Text>
          <Text style={styles.ubiAction}>Tap to claim</Text>
        </TouchableOpacity>
      )}

      {/* Recent Transactions */}
      <View style={styles.transactionsSection}>
        <View style={styles.sectionHeader}>
          <Text style={styles.sectionTitle}>Recent Transactions</Text>
          <TouchableOpacity onPress={() => navigation.navigate('Transactions')}>
            <Text style={styles.seeAllText}>See All</Text>
          </TouchableOpacity>
        </View>

        {isLoadingTransactions ? (
          <ActivityIndicator style={styles.loader} />
        ) : transactions.length === 0 ? (
          <View style={styles.emptyState}>
            <Text style={styles.emptyText}>No transactions yet</Text>
            {(network === 'testnet' || network === 'regtest') && (
              <TouchableOpacity
                style={styles.faucetButton}
                onPress={() => navigation.navigate('Faucet')}
              >
                <Text style={styles.faucetButtonText}>Get Test SHR</Text>
              </TouchableOpacity>
            )}
          </View>
        ) : (
          transactions.slice(0, 5).map((tx: Transaction) => (
            <TouchableOpacity
              key={tx.txid}
              style={styles.transactionItem}
              onPress={() => navigation.navigate('TransactionDetail', { txid: tx.txid })}
            >
              <View style={[styles.txIcon, { backgroundColor: getTransactionColor(tx.type) }]}>
                <Text style={styles.txIconText}>{getTransactionIcon(tx.type)}</Text>
              </View>
              <View style={styles.txDetails}>
                <Text style={styles.txType}>
                  {tx.type.charAt(0).toUpperCase() + tx.type.slice(1).replace('_', ' ')}
                </Text>
                <Text style={styles.txTime}>
                  {new Date(tx.timestamp * 1000).toLocaleDateString()}
                </Text>
              </View>
              <View style={styles.txAmountContainer}>
                <Text style={[styles.txAmount, { color: getTransactionColor(tx.type) }]}>
                  {tx.type === 'send' ? '-' : '+'}{formatAmount(tx.amount)} SHR
                </Text>
                <Text style={styles.txConfirmations}>
                  {tx.confirmations === 0 ? 'Pending' : `${tx.confirmations} conf`}
                </Text>
              </View>
            </TouchableOpacity>
          ))
        )}
      </View>
    </ScrollView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#121212',
  },
  statusBar: {
    flexDirection: 'row',
    alignItems: 'center',
    padding: 16,
    paddingTop: 8,
  },
  statusDot: {
    width: 8,
    height: 8,
    borderRadius: 4,
    marginRight: 8,
  },
  statusText: {
    color: '#888',
    fontSize: 12,
    flex: 1,
  },
  networkBadge: {
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 4,
  },
  networkText: {
    color: '#fff',
    fontSize: 10,
    fontWeight: 'bold',
  },
  errorBanner: {
    backgroundColor: '#F44336',
    padding: 12,
    marginHorizontal: 16,
    borderRadius: 8,
    marginBottom: 16,
  },
  errorText: {
    color: '#fff',
    fontSize: 14,
  },
  errorDismiss: {
    color: 'rgba(255,255,255,0.7)',
    fontSize: 12,
    marginTop: 4,
  },
  balanceCard: {
    backgroundColor: '#1E1E1E',
    margin: 16,
    padding: 24,
    borderRadius: 16,
    alignItems: 'center',
  },
  balanceLabel: {
    color: '#888',
    fontSize: 14,
    marginBottom: 8,
  },
  balanceAmount: {
    color: '#fff',
    fontSize: 36,
    fontWeight: 'bold',
  },
  pendingBalance: {
    color: '#FF9800',
    fontSize: 14,
    marginTop: 4,
  },
  addressText: {
    color: '#666',
    fontSize: 12,
    marginTop: 16,
    maxWidth: '80%',
  },
  actionsContainer: {
    flexDirection: 'row',
    justifyContent: 'space-around',
    paddingHorizontal: 16,
    marginBottom: 24,
  },
  actionButton: {
    alignItems: 'center',
  },
  actionIcon: {
    width: 56,
    height: 56,
    borderRadius: 28,
    justifyContent: 'center',
    alignItems: 'center',
    marginBottom: 8,
  },
  actionIconText: {
    color: '#fff',
    fontSize: 24,
    fontWeight: 'bold',
  },
  actionText: {
    color: '#fff',
    fontSize: 12,
  },
  ubiCard: {
    backgroundColor: '#9C27B0',
    margin: 16,
    padding: 16,
    borderRadius: 12,
    alignItems: 'center',
  },
  ubiTitle: {
    color: 'rgba(255,255,255,0.8)',
    fontSize: 14,
  },
  ubiAmount: {
    color: '#fff',
    fontSize: 24,
    fontWeight: 'bold',
    marginVertical: 4,
  },
  ubiAction: {
    color: 'rgba(255,255,255,0.6)',
    fontSize: 12,
  },
  transactionsSection: {
    padding: 16,
  },
  sectionHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 16,
  },
  sectionTitle: {
    color: '#fff',
    fontSize: 18,
    fontWeight: 'bold',
  },
  seeAllText: {
    color: '#2196F3',
    fontSize: 14,
  },
  loader: {
    marginVertical: 24,
  },
  emptyState: {
    alignItems: 'center',
    paddingVertical: 32,
  },
  emptyText: {
    color: '#666',
    fontSize: 16,
  },
  faucetButton: {
    marginTop: 16,
    backgroundColor: '#2196F3',
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: 8,
  },
  faucetButtonText: {
    color: '#fff',
    fontWeight: 'bold',
  },
  transactionItem: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
    marginBottom: 8,
  },
  txIcon: {
    width: 40,
    height: 40,
    borderRadius: 20,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
  },
  txIconText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
  },
  txDetails: {
    flex: 1,
  },
  txType: {
    color: '#fff',
    fontSize: 14,
    fontWeight: '500',
  },
  txTime: {
    color: '#666',
    fontSize: 12,
    marginTop: 2,
  },
  txAmountContainer: {
    alignItems: 'flex-end',
  },
  txAmount: {
    fontSize: 14,
    fontWeight: 'bold',
  },
  txConfirmations: {
    color: '#666',
    fontSize: 10,
    marginTop: 2,
  },
});

export default HomeScreen;
