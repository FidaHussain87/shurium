/**
 * SHURIUM Mobile Wallet - Faucet Screen
 * Request test coins on testnet/regtest
 */

import React, { useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TouchableOpacity,
  TextInput,
  Alert,
  ActivityIndicator,
} from 'react-native';
import { useNavigation } from '@react-navigation/native';
import { useWalletStore } from '../store/wallet';

export const FaucetScreen: React.FC = () => {
  const navigation = useNavigation();
  const { network, accounts, activeAccountId, requestFaucet, balance } = useWalletStore();
  
  const [amount, setAmount] = useState('100');
  const [isRequesting, setIsRequesting] = useState(false);

  const activeAccount = accounts.find(a => a.id === activeAccountId);

  const handleRequest = async () => {
    if (network === 'mainnet') {
      Alert.alert('Not Available', 'Faucet is only available on testnet and regtest');
      return;
    }

    const requestAmount = parseFloat(amount);
    if (isNaN(requestAmount) || requestAmount <= 0) {
      Alert.alert('Invalid Amount', 'Please enter a valid amount');
      return;
    }

    if (requestAmount > 1000) {
      Alert.alert('Amount Too High', 'Maximum faucet request is 1000 SHR');
      return;
    }

    setIsRequesting(true);
    try {
      const txid = await requestFaucet(requestAmount);
      Alert.alert(
        'Coins Received!',
        `${requestAmount} test SHR has been sent to your wallet.\n\nTransaction: ${txid.substring(0, 16)}...`,
        [{ text: 'OK', onPress: () => navigation.goBack() }]
      );
    } catch (error) {
      Alert.alert('Request Failed', (error as Error).message);
    } finally {
      setIsRequesting(false);
    }
  };

  if (network === 'mainnet') {
    return (
      <View style={styles.container}>
        <View style={styles.notAvailable}>
          <Text style={styles.notAvailableIcon}>🚫</Text>
          <Text style={styles.notAvailableTitle}>Not Available</Text>
          <Text style={styles.notAvailableText}>
            The faucet is only available on testnet and regtest networks.
          </Text>
          <TouchableOpacity style={styles.switchButton}>
            <Text style={styles.switchButtonText}>Switch to Testnet</Text>
          </TouchableOpacity>
        </View>
      </View>
    );
  }

  return (
    <View style={styles.container}>
      {/* Network Badge */}
      <View style={[
        styles.networkBadge,
        { backgroundColor: network === 'testnet' ? '#FF9800' : '#9C27B0' }
      ]}>
        <Text style={styles.networkText}>{network.toUpperCase()}</Text>
      </View>

      {/* Info Card */}
      <View style={styles.infoCard}>
        <Text style={styles.infoTitle}>Test Coin Faucet</Text>
        <Text style={styles.infoText}>
          Request free test SHR to experiment with the wallet. 
          These coins have no real value and can only be used on the {network} network.
        </Text>
      </View>

      {/* Current Balance */}
      <View style={styles.balanceCard}>
        <Text style={styles.balanceLabel}>Current Balance</Text>
        <Text style={styles.balanceAmount}>
          {balance ? parseFloat(balance.total).toFixed(8) : '0.00000000'} SHR
        </Text>
      </View>

      {/* Request Form */}
      <View style={styles.formCard}>
        <Text style={styles.formLabel}>Request Amount</Text>
        
        <View style={styles.amountContainer}>
          <TextInput
            style={styles.amountInput}
            value={amount}
            onChangeText={setAmount}
            keyboardType="decimal-pad"
            placeholder="100"
            placeholderTextColor="#666"
          />
          <Text style={styles.amountCurrency}>SHR</Text>
        </View>

        {/* Quick Amount Buttons */}
        <View style={styles.quickAmounts}>
          {['10', '50', '100', '500', '1000'].map((value) => (
            <TouchableOpacity
              key={value}
              style={[
                styles.quickAmountButton,
                amount === value && styles.quickAmountButtonActive
              ]}
              onPress={() => setAmount(value)}
            >
              <Text style={[
                styles.quickAmountText,
                amount === value && styles.quickAmountTextActive
              ]}>
                {value}
              </Text>
            </TouchableOpacity>
          ))}
        </View>

        {/* Receiving Address */}
        <View style={styles.addressContainer}>
          <Text style={styles.addressLabel}>Receiving Address</Text>
          <Text style={styles.addressText} numberOfLines={1}>
            {activeAccount?.address || 'No address'}
          </Text>
        </View>

        {/* Request Button */}
        <TouchableOpacity
          style={[styles.requestButton, isRequesting && styles.requestButtonDisabled]}
          onPress={handleRequest}
          disabled={isRequesting}
        >
          {isRequesting ? (
            <ActivityIndicator color="#fff" />
          ) : (
            <Text style={styles.requestButtonText}>
              Request {amount} SHR
            </Text>
          )}
        </TouchableOpacity>
      </View>

      {/* Limits Info */}
      <View style={styles.limitsCard}>
        <Text style={styles.limitsTitle}>Faucet Limits</Text>
        <View style={styles.limitRow}>
          <Text style={styles.limitLabel}>Minimum request</Text>
          <Text style={styles.limitValue}>1 SHR</Text>
        </View>
        <View style={styles.limitRow}>
          <Text style={styles.limitLabel}>Maximum request</Text>
          <Text style={styles.limitValue}>1,000 SHR</Text>
        </View>
        <View style={styles.limitRow}>
          <Text style={styles.limitLabel}>Default amount</Text>
          <Text style={styles.limitValue}>100 SHR</Text>
        </View>
      </View>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#121212',
    padding: 16,
  },
  networkBadge: {
    alignSelf: 'center',
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 20,
    marginBottom: 16,
  },
  networkText: {
    color: '#fff',
    fontSize: 12,
    fontWeight: 'bold',
  },
  infoCard: {
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
    marginBottom: 16,
  },
  infoTitle: {
    color: '#fff',
    fontSize: 18,
    fontWeight: 'bold',
    marginBottom: 8,
  },
  infoText: {
    color: '#888',
    fontSize: 14,
    lineHeight: 20,
  },
  balanceCard: {
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
    marginBottom: 16,
    alignItems: 'center',
  },
  balanceLabel: {
    color: '#888',
    fontSize: 12,
  },
  balanceAmount: {
    color: '#fff',
    fontSize: 24,
    fontWeight: 'bold',
    marginTop: 4,
  },
  formCard: {
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
    marginBottom: 16,
  },
  formLabel: {
    color: '#888',
    fontSize: 14,
    marginBottom: 8,
  },
  amountContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#2A2A2A',
    borderRadius: 12,
    marginBottom: 12,
  },
  amountInput: {
    flex: 1,
    padding: 16,
    color: '#fff',
    fontSize: 24,
    fontWeight: 'bold',
  },
  amountCurrency: {
    color: '#888',
    fontSize: 16,
    paddingRight: 16,
  },
  quickAmounts: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    marginBottom: 16,
  },
  quickAmountButton: {
    paddingHorizontal: 12,
    paddingVertical: 8,
    borderRadius: 8,
    backgroundColor: '#2A2A2A',
  },
  quickAmountButtonActive: {
    backgroundColor: '#2196F3',
  },
  quickAmountText: {
    color: '#888',
    fontSize: 14,
  },
  quickAmountTextActive: {
    color: '#fff',
    fontWeight: 'bold',
  },
  addressContainer: {
    marginBottom: 16,
    paddingTop: 16,
    borderTopWidth: 1,
    borderTopColor: '#333',
  },
  addressLabel: {
    color: '#888',
    fontSize: 12,
    marginBottom: 4,
  },
  addressText: {
    color: '#666',
    fontSize: 12,
    fontFamily: 'monospace',
  },
  requestButton: {
    backgroundColor: '#4CAF50',
    padding: 18,
    borderRadius: 12,
    alignItems: 'center',
  },
  requestButtonDisabled: {
    opacity: 0.7,
  },
  requestButtonText: {
    color: '#fff',
    fontSize: 18,
    fontWeight: 'bold',
  },
  limitsCard: {
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
  },
  limitsTitle: {
    color: '#fff',
    fontSize: 14,
    fontWeight: 'bold',
    marginBottom: 12,
  },
  limitRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 8,
    borderBottomWidth: 1,
    borderBottomColor: '#333',
  },
  limitLabel: {
    color: '#888',
    fontSize: 14,
  },
  limitValue: {
    color: '#fff',
    fontSize: 14,
  },
  notAvailable: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingHorizontal: 32,
  },
  notAvailableIcon: {
    fontSize: 64,
    marginBottom: 16,
  },
  notAvailableTitle: {
    color: '#fff',
    fontSize: 24,
    fontWeight: 'bold',
    marginBottom: 8,
  },
  notAvailableText: {
    color: '#888',
    fontSize: 16,
    textAlign: 'center',
    marginBottom: 24,
  },
  switchButton: {
    backgroundColor: '#FF9800',
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: 8,
  },
  switchButtonText: {
    color: '#fff',
    fontWeight: 'bold',
  },
});

export default FaucetScreen;
