/**
 * SHURIUM Mobile Wallet - Receive Screen
 * Display address and QR code for receiving SHR
 */

import React, { useState, useMemo } from 'react';
import {
  View,
  Text,
  StyleSheet,
  TouchableOpacity,
  TextInput,
  Share,
  Clipboard,
  Alert,
  ScrollView,
} from 'react-native';
import QRCode from 'react-native-qrcode-svg';
import { useWalletStore } from '../store/wallet';
import { createShuriumURI } from '../utils/crypto';

export const ReceiveScreen: React.FC = () => {
  const { accounts, activeAccountId, network } = useWalletStore();
  const [requestAmount, setRequestAmount] = useState('');
  const [label, setLabel] = useState('');
  const [showOptions, setShowOptions] = useState(false);

  const activeAccount = accounts.find(a => a.id === activeAccountId);
  const address = activeAccount?.address || '';

  // Generate SHURIUM URI with optional amount and label
  const shuriumURI = useMemo(() => {
    const amount = requestAmount ? parseFloat(requestAmount) : undefined;
    return createShuriumURI(address, amount, label || undefined);
  }, [address, requestAmount, label]);

  const copyAddress = () => {
    Clipboard.setString(address);
    Alert.alert('Copied', 'Address copied to clipboard');
  };

  const copyURI = () => {
    Clipboard.setString(shuriumURI);
    Alert.alert('Copied', 'Payment request copied to clipboard');
  };

  const shareAddress = async () => {
    try {
      await Share.share({
        message: showOptions && (requestAmount || label) 
          ? `Please send SHR to: ${shuriumURI}`
          : `My SHURIUM address: ${address}`,
        title: 'SHURIUM Address',
      });
    } catch (error) {
      console.error('Share error:', error);
    }
  };

  const getNetworkLabel = () => {
    switch (network) {
      case 'mainnet': return null;
      case 'testnet': return 'TESTNET';
      case 'regtest': return 'REGTEST';
    }
  };

  return (
    <ScrollView style={styles.container} contentContainerStyle={styles.content}>
      {/* Network Badge */}
      {getNetworkLabel() && (
        <View style={styles.networkBadge}>
          <Text style={styles.networkText}>{getNetworkLabel()}</Text>
        </View>
      )}

      {/* QR Code */}
      <View style={styles.qrContainer}>
        <View style={styles.qrWrapper}>
          <QRCode
            value={showOptions && (requestAmount || label) ? shuriumURI : address}
            size={200}
            backgroundColor="#FFFFFF"
            color="#000000"
          />
        </View>
        <Text style={styles.qrHint}>
          Scan this QR code to send SHR
        </Text>
      </View>

      {/* Address Display */}
      <View style={styles.addressContainer}>
        <Text style={styles.addressLabel}>Your Address</Text>
        <TouchableOpacity style={styles.addressBox} onPress={copyAddress}>
          <Text style={styles.addressText} selectable>
            {address}
          </Text>
        </TouchableOpacity>
        <Text style={styles.tapHint}>Tap to copy</Text>
      </View>

      {/* Request Amount Toggle */}
      <TouchableOpacity 
        style={styles.optionsToggle}
        onPress={() => setShowOptions(!showOptions)}
      >
        <Text style={styles.optionsToggleText}>
          {showOptions ? '- Hide request options' : '+ Add amount request'}
        </Text>
      </TouchableOpacity>

      {/* Request Options */}
      {showOptions && (
        <View style={styles.optionsContainer}>
          <View style={styles.inputGroup}>
            <Text style={styles.inputLabel}>Request Amount (optional)</Text>
            <View style={styles.amountInputRow}>
              <TextInput
                style={styles.input}
                placeholder="0.00000000"
                placeholderTextColor="#666"
                value={requestAmount}
                onChangeText={setRequestAmount}
                keyboardType="decimal-pad"
              />
              <Text style={styles.currencyLabel}>SHR</Text>
            </View>
          </View>

          <View style={styles.inputGroup}>
            <Text style={styles.inputLabel}>Label (optional)</Text>
            <TextInput
              style={styles.input}
              placeholder="Payment for..."
              placeholderTextColor="#666"
              value={label}
              onChangeText={setLabel}
            />
          </View>

          {(requestAmount || label) && (
            <View style={styles.uriContainer}>
              <Text style={styles.uriLabel}>Payment Request URI</Text>
              <TouchableOpacity style={styles.uriBox} onPress={copyURI}>
                <Text style={styles.uriText} numberOfLines={2}>
                  {shuriumURI}
                </Text>
              </TouchableOpacity>
            </View>
          )}
        </View>
      )}

      {/* Action Buttons */}
      <View style={styles.actions}>
        <TouchableOpacity style={styles.actionButton} onPress={copyAddress}>
          <Text style={styles.actionIcon}>C</Text>
          <Text style={styles.actionText}>Copy</Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.actionButton} onPress={shareAddress}>
          <Text style={styles.actionIcon}>S</Text>
          <Text style={styles.actionText}>Share</Text>
        </TouchableOpacity>
      </View>

      {/* Account Selector (if multiple accounts) */}
      {accounts.length > 1 && (
        <View style={styles.accountSelector}>
          <Text style={styles.accountLabel}>Receiving to:</Text>
          <Text style={styles.accountName}>{activeAccount?.name || 'Default'}</Text>
        </View>
      )}
    </ScrollView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#121212',
  },
  content: {
    padding: 16,
    alignItems: 'center',
  },
  networkBadge: {
    backgroundColor: '#FF9800',
    paddingHorizontal: 12,
    paddingVertical: 4,
    borderRadius: 4,
    marginBottom: 16,
  },
  networkText: {
    color: '#fff',
    fontSize: 12,
    fontWeight: 'bold',
  },
  qrContainer: {
    alignItems: 'center',
    marginBottom: 24,
  },
  qrWrapper: {
    padding: 16,
    backgroundColor: '#FFFFFF',
    borderRadius: 16,
  },
  qrHint: {
    color: '#888',
    fontSize: 14,
    marginTop: 12,
  },
  addressContainer: {
    width: '100%',
    marginBottom: 16,
  },
  addressLabel: {
    color: '#888',
    fontSize: 14,
    marginBottom: 8,
  },
  addressBox: {
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
  },
  addressText: {
    color: '#fff',
    fontSize: 14,
    fontFamily: 'monospace',
    textAlign: 'center',
  },
  tapHint: {
    color: '#666',
    fontSize: 12,
    textAlign: 'center',
    marginTop: 4,
  },
  optionsToggle: {
    marginVertical: 16,
  },
  optionsToggleText: {
    color: '#2196F3',
    fontSize: 14,
  },
  optionsContainer: {
    width: '100%',
    backgroundColor: '#1E1E1E',
    borderRadius: 12,
    padding: 16,
    marginBottom: 16,
  },
  inputGroup: {
    marginBottom: 16,
  },
  inputLabel: {
    color: '#888',
    fontSize: 14,
    marginBottom: 8,
  },
  input: {
    backgroundColor: '#2A2A2A',
    borderRadius: 8,
    padding: 12,
    color: '#fff',
    fontSize: 16,
    flex: 1,
  },
  amountInputRow: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#2A2A2A',
    borderRadius: 8,
  },
  currencyLabel: {
    color: '#888',
    fontSize: 16,
    paddingRight: 12,
  },
  uriContainer: {
    marginTop: 8,
  },
  uriLabel: {
    color: '#888',
    fontSize: 12,
    marginBottom: 4,
  },
  uriBox: {
    backgroundColor: '#2A2A2A',
    padding: 12,
    borderRadius: 8,
  },
  uriText: {
    color: '#4CAF50',
    fontSize: 12,
    fontFamily: 'monospace',
  },
  actions: {
    flexDirection: 'row',
    justifyContent: 'center',
    gap: 24,
    marginVertical: 24,
  },
  actionButton: {
    alignItems: 'center',
    padding: 16,
  },
  actionIcon: {
    width: 48,
    height: 48,
    borderRadius: 24,
    backgroundColor: '#2196F3',
    color: '#fff',
    fontSize: 20,
    fontWeight: 'bold',
    textAlign: 'center',
    lineHeight: 48,
    marginBottom: 8,
    overflow: 'hidden',
  },
  actionText: {
    color: '#fff',
    fontSize: 14,
  },
  accountSelector: {
    flexDirection: 'row',
    alignItems: 'center',
    backgroundColor: '#1E1E1E',
    padding: 12,
    borderRadius: 8,
    width: '100%',
  },
  accountLabel: {
    color: '#888',
    fontSize: 14,
    marginRight: 8,
  },
  accountName: {
    color: '#fff',
    fontSize: 14,
    fontWeight: '500',
  },
});

export default ReceiveScreen;
