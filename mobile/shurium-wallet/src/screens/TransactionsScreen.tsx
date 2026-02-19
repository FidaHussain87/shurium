/**
 * SHURIUM Mobile Wallet - Transactions Screen
 * Full transaction history with filtering
 */

import React, { useState, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  FlatList,
  TouchableOpacity,
  RefreshControl,
  ActivityIndicator,
} from 'react-native';
import { useNavigation } from '@react-navigation/native';
import { useWalletStore } from '../store/wallet';
import type { Transaction, TransactionType } from '../types';
import type { NativeStackNavigationProp } from '@react-navigation/native-stack';

type FilterType = 'all' | TransactionType;

const FILTERS: { type: FilterType; label: string }[] = [
  { type: 'all', label: 'All' },
  { type: 'send', label: 'Sent' },
  { type: 'receive', label: 'Received' },
  { type: 'stake', label: 'Staking' },
  { type: 'ubi_claim', label: 'UBI' },
];

export const TransactionsScreen: React.FC = () => {
  const navigation = useNavigation<NativeStackNavigationProp<any>>();
  const { transactions, isLoadingTransactions, refreshTransactions } = useWalletStore();
  
  const [activeFilter, setActiveFilter] = useState<FilterType>('all');
  const [refreshing, setRefreshing] = useState(false);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await refreshTransactions(100);
    setRefreshing(false);
  }, [refreshTransactions]);

  const filteredTransactions = transactions.filter(tx => 
    activeFilter === 'all' || tx.type === activeFilter
  );

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

  const getTransactionIcon = (type: string) => {
    switch (type) {
      case 'send': return '↑';
      case 'receive': return '↓';
      case 'stake': return 'S';
      case 'unstake': return 'U';
      case 'ubi_claim': return '$';
      default: return '?';
    }
  };

  const formatDate = (timestamp: number) => {
    const date = new Date(timestamp * 1000);
    const now = new Date();
    const diff = now.getTime() - date.getTime();
    
    // Less than 24 hours
    if (diff < 86400000) {
      return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    }
    // Less than 7 days
    if (diff < 604800000) {
      return date.toLocaleDateString([], { weekday: 'short', hour: '2-digit', minute: '2-digit' });
    }
    // Otherwise
    return date.toLocaleDateString([], { month: 'short', day: 'numeric', year: 'numeric' });
  };

  const renderTransaction = ({ item: tx }: { item: Transaction }) => (
    <TouchableOpacity
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
        <Text style={styles.txTime}>{formatDate(tx.timestamp)}</Text>
        {tx.memo && (
          <Text style={styles.txMemo} numberOfLines={1}>{tx.memo}</Text>
        )}
      </View>
      
      <View style={styles.txAmountContainer}>
        <Text style={[styles.txAmount, { color: getTransactionColor(tx.type) }]}>
          {tx.type === 'send' ? '-' : '+'}{parseFloat(tx.amount).toFixed(8)} SHR
        </Text>
        <Text style={[
          styles.txStatus,
          { color: tx.status === 'confirmed' ? '#4CAF50' : 
                   tx.status === 'pending' ? '#FF9800' : '#F44336' }
        ]}>
          {tx.status === 'confirmed' 
            ? `${tx.confirmations} conf`
            : tx.status.charAt(0).toUpperCase() + tx.status.slice(1)}
        </Text>
      </View>
    </TouchableOpacity>
  );

  const renderEmptyState = () => (
    <View style={styles.emptyState}>
      <Text style={styles.emptyIcon}>📋</Text>
      <Text style={styles.emptyText}>
        {activeFilter === 'all' 
          ? 'No transactions yet'
          : `No ${activeFilter.replace('_', ' ')} transactions`}
      </Text>
      <Text style={styles.emptySubtext}>
        Your transaction history will appear here
      </Text>
    </View>
  );

  return (
    <View style={styles.container}>
      {/* Filter Tabs */}
      <View style={styles.filterContainer}>
        <FlatList
          horizontal
          data={FILTERS}
          keyExtractor={(item) => item.type}
          showsHorizontalScrollIndicator={false}
          renderItem={({ item }) => (
            <TouchableOpacity
              style={[
                styles.filterButton,
                activeFilter === item.type && styles.filterButtonActive
              ]}
              onPress={() => setActiveFilter(item.type)}
            >
              <Text style={[
                styles.filterText,
                activeFilter === item.type && styles.filterTextActive
              ]}>
                {item.label}
              </Text>
            </TouchableOpacity>
          )}
        />
      </View>

      {/* Transaction Count */}
      <View style={styles.countContainer}>
        <Text style={styles.countText}>
          {filteredTransactions.length} transaction{filteredTransactions.length !== 1 ? 's' : ''}
        </Text>
      </View>

      {/* Transaction List */}
      {isLoadingTransactions && transactions.length === 0 ? (
        <View style={styles.loadingContainer}>
          <ActivityIndicator size="large" color="#2196F3" />
          <Text style={styles.loadingText}>Loading transactions...</Text>
        </View>
      ) : (
        <FlatList
          data={filteredTransactions}
          keyExtractor={(item) => item.txid}
          renderItem={renderTransaction}
          ListEmptyComponent={renderEmptyState}
          refreshControl={
            <RefreshControl refreshing={refreshing} onRefresh={onRefresh} />
          }
          contentContainerStyle={filteredTransactions.length === 0 ? styles.emptyContainer : undefined}
        />
      )}
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#121212',
  },
  filterContainer: {
    paddingVertical: 12,
    paddingHorizontal: 8,
    borderBottomWidth: 1,
    borderBottomColor: '#333',
  },
  filterButton: {
    paddingHorizontal: 16,
    paddingVertical: 8,
    marginHorizontal: 4,
    borderRadius: 20,
    backgroundColor: '#1E1E1E',
  },
  filterButtonActive: {
    backgroundColor: '#2196F3',
  },
  filterText: {
    color: '#888',
    fontSize: 14,
  },
  filterTextActive: {
    color: '#fff',
    fontWeight: 'bold',
  },
  countContainer: {
    paddingHorizontal: 16,
    paddingVertical: 8,
  },
  countText: {
    color: '#666',
    fontSize: 12,
  },
  transactionItem: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1E1E1E',
    padding: 16,
    marginHorizontal: 16,
    marginVertical: 4,
    borderRadius: 12,
  },
  txIcon: {
    width: 44,
    height: 44,
    borderRadius: 22,
    justifyContent: 'center',
    alignItems: 'center',
    marginRight: 12,
  },
  txIconText: {
    color: '#fff',
    fontSize: 20,
    fontWeight: 'bold',
  },
  txDetails: {
    flex: 1,
  },
  txType: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '500',
  },
  txTime: {
    color: '#666',
    fontSize: 12,
    marginTop: 2,
  },
  txMemo: {
    color: '#888',
    fontSize: 12,
    marginTop: 2,
    fontStyle: 'italic',
  },
  txAmountContainer: {
    alignItems: 'flex-end',
  },
  txAmount: {
    fontSize: 16,
    fontWeight: 'bold',
  },
  txStatus: {
    fontSize: 10,
    marginTop: 2,
  },
  emptyContainer: {
    flex: 1,
  },
  emptyState: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingVertical: 48,
  },
  emptyIcon: {
    fontSize: 48,
    marginBottom: 16,
  },
  emptyText: {
    color: '#fff',
    fontSize: 18,
    marginBottom: 8,
  },
  emptySubtext: {
    color: '#666',
    fontSize: 14,
  },
  loadingContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
  },
  loadingText: {
    color: '#888',
    marginTop: 12,
  },
});

export default TransactionsScreen;
