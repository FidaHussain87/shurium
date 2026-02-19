/**
 * SHURIUM Mobile Wallet - Send Screen
 * Send SHR to another address
 */

import React, { useState, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TextInput,
  TouchableOpacity,
  Alert,
  ActivityIndicator,
  KeyboardAvoidingView,
  Platform,
  ScrollView,
} from 'react-native';
import { useNavigation } from '@react-navigation/native';
import { useWalletStore } from '../store/wallet';
import { validateAddress, parseSHR, formatSHR, parseShuriumURI } from '../utils/crypto';

export const SendScreen: React.FC = () => {
  const navigation = useNavigation();
  const { balance, network, sendTransaction } = useWalletStore();
  
  const [address, setAddress] = useState('');
  const [amount, setAmount] = useState('');
  const [memo, setMemo] = useState('');
  const [isSending, setIsSending] = useState(false);
  const [addressError, setAddressError] = useState<string | null>(null);
  const [amountError, setAmountError] = useState<string | null>(null);

  const availableBalance = balance ? parseFloat(balance.total) : 0;

  const validateAddressInput = useCallback((value: string) => {
    setAddress(value);
    setAddressError(null);
    
    if (value.length === 0) return;
    
    // Check for SHURIUM URI format
    if (value.toLowerCase().startsWith('shurium:')) {
      const parsed = parseShuriumURI(value);
      if (parsed) {
        setAddress(parsed.address);
        if (parsed.amount) {
          setAmount(String(parsed.amount));
        }
        if (parsed.message) {
          setMemo(parsed.message);
        }
        return;
      }
    }
    
    if (!validateAddress(value, network)) {
      setAddressError('Invalid SHURIUM address');
    }
  }, [network]);

  const validateAmountInput = useCallback((value: string) => {
    setAmount(value);
    setAmountError(null);
    
    if (value.length === 0) return;
    
    const numValue = parseFloat(value);
    
    if (isNaN(numValue) || numValue <= 0) {
      setAmountError('Enter a valid amount');
      return;
    }
    
    if (numValue > availableBalance) {
      setAmountError('Insufficient balance');
      return;
    }
    
    // Check minimum amount (dust threshold)
    if (numValue < 0.00001) {
      setAmountError('Amount too small (min: 0.00001 SHR)');
      return;
    }
  }, [availableBalance]);

  const setMaxAmount = useCallback(() => {
    // Leave some for fee (estimate 0.0001 SHR)
    const maxAmount = Math.max(0, availableBalance - 0.0001);
    setAmount(maxAmount.toFixed(8));
    validateAmountInput(maxAmount.toFixed(8));
  }, [availableBalance, validateAmountInput]);

  const handleSend = async () => {
    // Final validation
    if (!address || addressError) {
      Alert.alert('Error', 'Please enter a valid address');
      return;
    }
    
    if (!amount || amountError) {
      Alert.alert('Error', 'Please enter a valid amount');
      return;
    }
    
    const numAmount = parseFloat(amount);
    
    // Confirmation dialog
    Alert.alert(
      'Confirm Transaction',
      `Send ${amount} SHR to:\n${address.substring(0, 20)}...${address.substring(address.length - 8)}?`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Send',
          style: 'destructive',
          onPress: async () => {
            setIsSending(true);
            try {
              const txid = await sendTransaction(address, numAmount, memo || undefined);
              Alert.alert(
                'Transaction Sent',
                `Your transaction has been broadcast.\n\nTXID: ${txid.substring(0, 16)}...`,
                [{ text: 'OK', onPress: () => navigation.goBack() }]
              );
            } catch (error) {
              Alert.alert('Transaction Failed', (error as Error).message);
            } finally {
              setIsSending(false);
            }
          },
        },
      ]
    );
  };

  const handleScanQR = () => {
    // Navigate to QR scanner screen
    // navigation.navigate('QRScanner', { onScan: validateAddressInput });
    Alert.alert('QR Scanner', 'QR scanning will be available in the next update');
  };

  return (
    <KeyboardAvoidingView
      style={styles.container}
      behavior={Platform.OS === 'ios' ? 'padding' : 'height'}
    >
      <ScrollView style={styles.scrollView} keyboardShouldPersistTaps="handled">
        {/* Balance Display */}
        <View style={styles.balanceContainer}>
          <Text style={styles.balanceLabel}>Available Balance</Text>
          <Text style={styles.balanceAmount}>
            {availableBalance.toFixed(8)} SHR
          </Text>
        </View>

        {/* Address Input */}
        <View style={styles.inputContainer}>
          <Text style={styles.inputLabel}>Recipient Address</Text>
          <View style={styles.addressInputRow}>
            <TextInput
              style={[styles.input, styles.addressInput, addressError && styles.inputError]}
              placeholder="SHR address or SHURIUM URI"
              placeholderTextColor="#666"
              value={address}
              onChangeText={validateAddressInput}
              autoCapitalize="none"
              autoCorrect={false}
            />
            <TouchableOpacity style={styles.scanButton} onPress={handleScanQR}>
              <Text style={styles.scanButtonText}>QR</Text>
            </TouchableOpacity>
          </View>
          {addressError && <Text style={styles.errorText}>{addressError}</Text>}
        </View>

        {/* Amount Input */}
        <View style={styles.inputContainer}>
          <View style={styles.amountLabelRow}>
            <Text style={styles.inputLabel}>Amount</Text>
            <TouchableOpacity onPress={setMaxAmount}>
              <Text style={styles.maxButton}>MAX</Text>
            </TouchableOpacity>
          </View>
          <View style={styles.amountInputRow}>
            <TextInput
              style={[styles.input, styles.amountInput, amountError && styles.inputError]}
              placeholder="0.00000000"
              placeholderTextColor="#666"
              value={amount}
              onChangeText={validateAmountInput}
              keyboardType="decimal-pad"
            />
            <Text style={styles.currencyLabel}>SHR</Text>
          </View>
          {amountError && <Text style={styles.errorText}>{amountError}</Text>}
        </View>

        {/* Memo Input (Optional) */}
        <View style={styles.inputContainer}>
          <Text style={styles.inputLabel}>Memo (Optional)</Text>
          <TextInput
            style={styles.input}
            placeholder="Add a note"
            placeholderTextColor="#666"
            value={memo}
            onChangeText={setMemo}
            maxLength={256}
          />
        </View>

        {/* Fee Estimate */}
        <View style={styles.feeContainer}>
          <Text style={styles.feeLabel}>Estimated Fee</Text>
          <Text style={styles.feeAmount}>~0.0001 SHR</Text>
        </View>

        {/* Send Button */}
        <TouchableOpacity
          style={[
            styles.sendButton,
            (!address || !amount || addressError || amountError || isSending) && styles.sendButtonDisabled,
          ]}
          onPress={handleSend}
          disabled={!address || !amount || !!addressError || !!amountError || isSending}
        >
          {isSending ? (
            <ActivityIndicator color="#fff" />
          ) : (
            <Text style={styles.sendButtonText}>Send SHR</Text>
          )}
        </TouchableOpacity>

        {/* Network Warning */}
        {network !== 'mainnet' && (
          <View style={styles.warningContainer}>
            <Text style={styles.warningText}>
              You are on {network.toUpperCase()}. These are not real coins.
            </Text>
          </View>
        )}
      </ScrollView>
    </KeyboardAvoidingView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#121212',
  },
  scrollView: {
    flex: 1,
    padding: 16,
  },
  balanceContainer: {
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
    marginBottom: 24,
    alignItems: 'center',
  },
  balanceLabel: {
    color: '#888',
    fontSize: 14,
  },
  balanceAmount: {
    color: '#fff',
    fontSize: 24,
    fontWeight: 'bold',
    marginTop: 4,
  },
  inputContainer: {
    marginBottom: 20,
  },
  inputLabel: {
    color: '#888',
    fontSize: 14,
    marginBottom: 8,
  },
  input: {
    backgroundColor: '#1E1E1E',
    borderRadius: 12,
    padding: 16,
    color: '#fff',
    fontSize: 16,
  },
  inputError: {
    borderWidth: 1,
    borderColor: '#F44336',
  },
  addressInputRow: {
    flexDirection: 'row',
  },
  addressInput: {
    flex: 1,
    borderTopRightRadius: 0,
    borderBottomRightRadius: 0,
  },
  scanButton: {
    backgroundColor: '#2196F3',
    paddingHorizontal: 16,
    justifyContent: 'center',
    alignItems: 'center',
    borderTopRightRadius: 12,
    borderBottomRightRadius: 12,
  },
  scanButtonText: {
    color: '#fff',
    fontWeight: 'bold',
  },
  amountLabelRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  maxButton: {
    color: '#2196F3',
    fontSize: 14,
    fontWeight: 'bold',
  },
  amountInputRow: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1E1E1E',
    borderRadius: 12,
  },
  amountInput: {
    flex: 1,
    borderRadius: 12,
  },
  currencyLabel: {
    color: '#888',
    fontSize: 16,
    paddingRight: 16,
  },
  errorText: {
    color: '#F44336',
    fontSize: 12,
    marginTop: 4,
  },
  feeContainer: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 12,
    borderTopWidth: 1,
    borderTopColor: '#333',
    marginTop: 8,
    marginBottom: 24,
  },
  feeLabel: {
    color: '#888',
    fontSize: 14,
  },
  feeAmount: {
    color: '#fff',
    fontSize: 14,
  },
  sendButton: {
    backgroundColor: '#F44336',
    padding: 18,
    borderRadius: 12,
    alignItems: 'center',
  },
  sendButtonDisabled: {
    opacity: 0.5,
  },
  sendButtonText: {
    color: '#fff',
    fontSize: 18,
    fontWeight: 'bold',
  },
  warningContainer: {
    marginTop: 16,
    padding: 12,
    backgroundColor: 'rgba(255,152,0,0.1)',
    borderRadius: 8,
  },
  warningText: {
    color: '#FF9800',
    fontSize: 12,
    textAlign: 'center',
  },
});

export default SendScreen;
