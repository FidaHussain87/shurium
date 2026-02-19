/**
 * SHURIUM Mobile Wallet - Settings Screen
 * Network configuration, security, and app settings
 */

import React, { useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Switch,
  Alert,
  TextInput,
  Modal,
} from 'react-native';
import { useWalletStore } from '../store/wallet';
import type { NetworkType } from '../types';

export const SettingsScreen: React.FC = () => {
  const {
    network,
    setNetwork,
    accounts,
    activeAccountId,
    setActiveAccount,
    hasBackup,
    setBackupComplete,
    lockWallet,
    reset,
  } = useWalletStore();

  const [showNetworkModal, setShowNetworkModal] = useState(false);
  const [showAccountModal, setShowAccountModal] = useState(false);
  const [biometricEnabled, setBiometricEnabled] = useState(false);
  const [notificationsEnabled, setNotificationsEnabled] = useState(true);

  const networks: { type: NetworkType; name: string; description: string }[] = [
    { type: 'mainnet', name: 'Mainnet', description: 'Production network with real SHR' },
    { type: 'testnet', name: 'Testnet', description: 'Test network for development' },
    { type: 'regtest', name: 'Regtest', description: 'Local regression testing' },
  ];

  const handleNetworkChange = (newNetwork: NetworkType) => {
    if (newNetwork === network) {
      setShowNetworkModal(false);
      return;
    }

    Alert.alert(
      'Change Network',
      `Switch to ${newNetwork}? Your wallet will reconnect to the new network.`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Switch',
          onPress: () => {
            setNetwork(newNetwork);
            setShowNetworkModal(false);
          },
        },
      ]
    );
  };

  const handleBackupWallet = () => {
    Alert.alert(
      'Backup Wallet',
      'Your recovery phrase will be displayed. Make sure no one else can see your screen.',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Show Phrase',
          onPress: () => {
            // In production, this would show the actual mnemonic from secure storage
            Alert.alert(
              'Recovery Phrase',
              'This feature will display your 24-word recovery phrase.\n\nNever share this with anyone!',
              [
                { 
                  text: 'I Understand', 
                  onPress: () => setBackupComplete() 
                }
              ]
            );
          },
        },
      ]
    );
  };

  const handleLockWallet = () => {
    Alert.alert(
      'Lock Wallet',
      'Lock your wallet now?',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Lock',
          onPress: () => lockWallet(),
        },
      ]
    );
  };

  const handleResetWallet = () => {
    Alert.alert(
      'Reset Wallet',
      'This will delete all wallet data from this device. Make sure you have your recovery phrase backed up!',
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Reset',
          style: 'destructive',
          onPress: () => {
            Alert.alert(
              'Confirm Reset',
              'Are you absolutely sure? This action cannot be undone.',
              [
                { text: 'Cancel', style: 'cancel' },
                {
                  text: 'Yes, Reset',
                  style: 'destructive',
                  onPress: () => reset(),
                },
              ]
            );
          },
        },
      ]
    );
  };

  const activeAccount = accounts.find(a => a.id === activeAccountId);

  return (
    <ScrollView style={styles.container}>
      {/* Network Section */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Network</Text>
        <TouchableOpacity 
          style={styles.settingItem}
          onPress={() => setShowNetworkModal(true)}
        >
          <View style={styles.settingInfo}>
            <Text style={styles.settingLabel}>Current Network</Text>
            <Text style={styles.settingValue}>{network.charAt(0).toUpperCase() + network.slice(1)}</Text>
          </View>
          <Text style={styles.chevron}>›</Text>
        </TouchableOpacity>
      </View>

      {/* Account Section */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Account</Text>
        <TouchableOpacity 
          style={styles.settingItem}
          onPress={() => setShowAccountModal(true)}
        >
          <View style={styles.settingInfo}>
            <Text style={styles.settingLabel}>Active Account</Text>
            <Text style={styles.settingValue}>{activeAccount?.name || 'Default'}</Text>
          </View>
          <Text style={styles.chevron}>›</Text>
        </TouchableOpacity>
        <View style={styles.settingItem}>
          <View style={styles.settingInfo}>
            <Text style={styles.settingLabel}>Address</Text>
            <Text style={styles.settingValueSmall} numberOfLines={1}>
              {activeAccount?.address || 'No address'}
            </Text>
          </View>
        </View>
      </View>

      {/* Security Section */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Security</Text>
        
        <View style={styles.settingItem}>
          <View style={styles.settingInfo}>
            <Text style={styles.settingLabel}>Biometric Unlock</Text>
            <Text style={styles.settingDescription}>Use Face ID / Touch ID</Text>
          </View>
          <Switch
            value={biometricEnabled}
            onValueChange={setBiometricEnabled}
            trackColor={{ false: '#333', true: '#2196F3' }}
            thumbColor="#fff"
          />
        </View>

        <TouchableOpacity style={styles.settingItem} onPress={handleBackupWallet}>
          <View style={styles.settingInfo}>
            <Text style={styles.settingLabel}>Backup Wallet</Text>
            <Text style={[
              styles.settingDescription,
              { color: hasBackup ? '#4CAF50' : '#FF9800' }
            ]}>
              {hasBackup ? 'Backup completed' : 'Not backed up - tap to backup'}
            </Text>
          </View>
          <Text style={styles.chevron}>›</Text>
        </TouchableOpacity>

        <TouchableOpacity style={styles.settingItem} onPress={handleLockWallet}>
          <View style={styles.settingInfo}>
            <Text style={styles.settingLabel}>Lock Wallet</Text>
            <Text style={styles.settingDescription}>Lock immediately</Text>
          </View>
          <Text style={styles.chevron}>›</Text>
        </TouchableOpacity>
      </View>

      {/* Notifications Section */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>Notifications</Text>
        
        <View style={styles.settingItem}>
          <View style={styles.settingInfo}>
            <Text style={styles.settingLabel}>Push Notifications</Text>
            <Text style={styles.settingDescription}>Transaction alerts</Text>
          </View>
          <Switch
            value={notificationsEnabled}
            onValueChange={setNotificationsEnabled}
            trackColor={{ false: '#333', true: '#2196F3' }}
            thumbColor="#fff"
          />
        </View>
      </View>

      {/* About Section */}
      <View style={styles.section}>
        <Text style={styles.sectionTitle}>About</Text>
        
        <View style={styles.settingItem}>
          <View style={styles.settingInfo}>
            <Text style={styles.settingLabel}>Version</Text>
            <Text style={styles.settingValue}>0.1.0</Text>
          </View>
        </View>

        <View style={styles.settingItem}>
          <View style={styles.settingInfo}>
            <Text style={styles.settingLabel}>SHURIUM Network</Text>
            <Text style={styles.settingDescription}>
              Cryptocurrency with Universal Basic Income
            </Text>
          </View>
        </View>
      </View>

      {/* Danger Zone */}
      <View style={[styles.section, styles.dangerSection]}>
        <Text style={[styles.sectionTitle, { color: '#F44336' }]}>Danger Zone</Text>
        
        <TouchableOpacity 
          style={[styles.settingItem, styles.dangerItem]}
          onPress={handleResetWallet}
        >
          <View style={styles.settingInfo}>
            <Text style={[styles.settingLabel, { color: '#F44336' }]}>Reset Wallet</Text>
            <Text style={styles.settingDescription}>Delete all wallet data</Text>
          </View>
          <Text style={[styles.chevron, { color: '#F44336' }]}>›</Text>
        </TouchableOpacity>
      </View>

      {/* Network Selection Modal */}
      <Modal
        visible={showNetworkModal}
        transparent
        animationType="slide"
        onRequestClose={() => setShowNetworkModal(false)}
      >
        <View style={styles.modalOverlay}>
          <View style={styles.modalContent}>
            <Text style={styles.modalTitle}>Select Network</Text>
            
            {networks.map((net) => (
              <TouchableOpacity
                key={net.type}
                style={[
                  styles.networkOption,
                  network === net.type && styles.networkOptionActive
                ]}
                onPress={() => handleNetworkChange(net.type)}
              >
                <View style={styles.networkInfo}>
                  <Text style={[
                    styles.networkName,
                    network === net.type && styles.networkNameActive
                  ]}>
                    {net.name}
                  </Text>
                  <Text style={styles.networkDescription}>{net.description}</Text>
                </View>
                {network === net.type && (
                  <Text style={styles.checkmark}>✓</Text>
                )}
              </TouchableOpacity>
            ))}

            <TouchableOpacity 
              style={styles.modalCloseButton}
              onPress={() => setShowNetworkModal(false)}
            >
              <Text style={styles.modalCloseText}>Cancel</Text>
            </TouchableOpacity>
          </View>
        </View>
      </Modal>

      {/* Account Selection Modal */}
      <Modal
        visible={showAccountModal}
        transparent
        animationType="slide"
        onRequestClose={() => setShowAccountModal(false)}
      >
        <View style={styles.modalOverlay}>
          <View style={styles.modalContent}>
            <Text style={styles.modalTitle}>Select Account</Text>
            
            {accounts.map((account) => (
              <TouchableOpacity
                key={account.id}
                style={[
                  styles.networkOption,
                  activeAccountId === account.id && styles.networkOptionActive
                ]}
                onPress={() => {
                  setActiveAccount(account.id);
                  setShowAccountModal(false);
                }}
              >
                <View style={styles.networkInfo}>
                  <Text style={[
                    styles.networkName,
                    activeAccountId === account.id && styles.networkNameActive
                  ]}>
                    {account.name}
                  </Text>
                  <Text style={styles.networkDescription} numberOfLines={1}>
                    {account.address}
                  </Text>
                </View>
                {activeAccountId === account.id && (
                  <Text style={styles.checkmark}>✓</Text>
                )}
              </TouchableOpacity>
            ))}

            <TouchableOpacity 
              style={styles.modalCloseButton}
              onPress={() => setShowAccountModal(false)}
            >
              <Text style={styles.modalCloseText}>Cancel</Text>
            </TouchableOpacity>
          </View>
        </View>
      </Modal>
    </ScrollView>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#121212',
  },
  section: {
    marginTop: 24,
    paddingHorizontal: 16,
  },
  sectionTitle: {
    color: '#888',
    fontSize: 12,
    fontWeight: 'bold',
    textTransform: 'uppercase',
    marginBottom: 8,
    marginLeft: 4,
  },
  settingItem: {
    backgroundColor: '#1E1E1E',
    padding: 16,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    borderBottomWidth: 1,
    borderBottomColor: '#333',
  },
  settingInfo: {
    flex: 1,
  },
  settingLabel: {
    color: '#fff',
    fontSize: 16,
  },
  settingValue: {
    color: '#888',
    fontSize: 14,
    marginTop: 2,
  },
  settingValueSmall: {
    color: '#666',
    fontSize: 12,
    marginTop: 2,
    fontFamily: 'monospace',
  },
  settingDescription: {
    color: '#666',
    fontSize: 12,
    marginTop: 2,
  },
  chevron: {
    color: '#666',
    fontSize: 24,
    marginLeft: 8,
  },
  dangerSection: {
    marginBottom: 48,
  },
  dangerItem: {
    backgroundColor: 'rgba(244, 67, 54, 0.1)',
  },
  modalOverlay: {
    flex: 1,
    backgroundColor: 'rgba(0,0,0,0.7)',
    justifyContent: 'flex-end',
  },
  modalContent: {
    backgroundColor: '#1E1E1E',
    borderTopLeftRadius: 20,
    borderTopRightRadius: 20,
    padding: 24,
  },
  modalTitle: {
    color: '#fff',
    fontSize: 20,
    fontWeight: 'bold',
    marginBottom: 24,
    textAlign: 'center',
  },
  networkOption: {
    flexDirection: 'row',
    alignItems: 'center',
    padding: 16,
    backgroundColor: '#2A2A2A',
    borderRadius: 12,
    marginBottom: 8,
  },
  networkOptionActive: {
    backgroundColor: 'rgba(33, 150, 243, 0.2)',
    borderWidth: 1,
    borderColor: '#2196F3',
  },
  networkInfo: {
    flex: 1,
  },
  networkName: {
    color: '#fff',
    fontSize: 16,
    fontWeight: '500',
  },
  networkNameActive: {
    color: '#2196F3',
  },
  networkDescription: {
    color: '#888',
    fontSize: 12,
    marginTop: 2,
  },
  checkmark: {
    color: '#2196F3',
    fontSize: 20,
    fontWeight: 'bold',
  },
  modalCloseButton: {
    padding: 16,
    alignItems: 'center',
    marginTop: 8,
  },
  modalCloseText: {
    color: '#888',
    fontSize: 16,
  },
});

export default SettingsScreen;
