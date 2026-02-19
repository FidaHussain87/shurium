/**
 * SHURIUM Mobile Wallet - Staking Screen
 * Manage staking, delegations, and rewards
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  TextInput,
  Alert,
  ActivityIndicator,
  RefreshControl,
  Modal,
} from 'react-native';
import { useWalletStore } from '../store/wallet';
import type { ValidatorInfo, DelegationInfo } from '../types';

type TabType = 'overview' | 'validators' | 'delegations';

export const StakingScreen: React.FC = () => {
  const {
    stakingInfo,
    balance,
    refreshStakingInfo,
    stake,
    unstake,
    claimStakingRewards,
    lastError,
    clearError,
  } = useWalletStore();

  const [activeTab, setActiveTab] = useState<TabType>('overview');
  const [refreshing, setRefreshing] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  
  // Delegation modal state
  const [showDelegateModal, setShowDelegateModal] = useState(false);
  const [selectedValidator, setSelectedValidator] = useState<ValidatorInfo | null>(null);
  const [delegateAmount, setDelegateAmount] = useState('');
  const [isUnstaking, setIsUnstaking] = useState(false);

  useEffect(() => {
    refreshStakingInfo();
  }, []);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await refreshStakingInfo();
    setRefreshing(false);
  }, [refreshStakingInfo]);

  const handleDelegate = async () => {
    if (!selectedValidator || !delegateAmount) return;
    
    const amount = parseFloat(delegateAmount);
    if (isNaN(amount) || amount <= 0) {
      Alert.alert('Error', 'Please enter a valid amount');
      return;
    }

    const availableBalance = balance ? parseFloat(balance.total) : 0;
    if (amount > availableBalance) {
      Alert.alert('Error', 'Insufficient balance');
      return;
    }

    Alert.alert(
      'Confirm Delegation',
      `Delegate ${delegateAmount} SHR to ${selectedValidator.name}?`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Delegate',
          onPress: async () => {
            setIsLoading(true);
            try {
              await stake(selectedValidator.id, amount);
              Alert.alert('Success', 'Delegation successful!');
              setShowDelegateModal(false);
              setDelegateAmount('');
              setSelectedValidator(null);
            } catch (error) {
              Alert.alert('Error', (error as Error).message);
            } finally {
              setIsLoading(false);
            }
          },
        },
      ]
    );
  };

  const handleUndelegate = async () => {
    if (!selectedValidator || !delegateAmount) return;
    
    const amount = parseFloat(delegateAmount);
    if (isNaN(amount) || amount <= 0) {
      Alert.alert('Error', 'Please enter a valid amount');
      return;
    }

    Alert.alert(
      'Confirm Undelegation',
      `Undelegate ${delegateAmount} SHR from ${selectedValidator.name}?\n\nNote: There may be an unbonding period.`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Undelegate',
          style: 'destructive',
          onPress: async () => {
            setIsLoading(true);
            try {
              await unstake(selectedValidator.id, amount);
              Alert.alert('Success', 'Undelegation initiated!');
              setShowDelegateModal(false);
              setDelegateAmount('');
              setSelectedValidator(null);
            } catch (error) {
              Alert.alert('Error', (error as Error).message);
            } finally {
              setIsLoading(false);
            }
          },
        },
      ]
    );
  };

  const handleClaimRewards = async () => {
    const rewards = parseFloat(stakingInfo?.rewards || '0');
    if (rewards <= 0) {
      Alert.alert('No Rewards', 'You have no rewards to claim');
      return;
    }

    Alert.alert(
      'Claim Rewards',
      `Claim ${stakingInfo?.rewards} SHR in staking rewards?`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Claim',
          onPress: async () => {
            setIsLoading(true);
            try {
              await claimStakingRewards();
              Alert.alert('Success', 'Rewards claimed successfully!');
            } catch (error) {
              Alert.alert('Error', (error as Error).message);
            } finally {
              setIsLoading(false);
            }
          },
        },
      ]
    );
  };

  const openDelegateModal = (validator: ValidatorInfo, unstaking: boolean = false) => {
    setSelectedValidator(validator);
    setIsUnstaking(unstaking);
    setDelegateAmount('');
    setShowDelegateModal(true);
  };

  const renderOverview = () => (
    <View style={styles.overviewContainer}>
      {/* Staking Status */}
      <View style={styles.statusCard}>
        <View style={styles.statusRow}>
          <View style={[styles.statusDot, { 
            backgroundColor: stakingInfo?.isStaking ? '#4CAF50' : '#666' 
          }]} />
          <Text style={styles.statusText}>
            {stakingInfo?.isStaking ? 'Staking Active' : 'Not Staking'}
          </Text>
        </View>
      </View>

      {/* Staking Stats */}
      <View style={styles.statsContainer}>
        <View style={styles.statCard}>
          <Text style={styles.statLabel}>Total Staked</Text>
          <Text style={styles.statValue}>
            {stakingInfo?.totalStaked || '0'} SHR
          </Text>
        </View>
        
        <View style={styles.statCard}>
          <Text style={styles.statLabel}>Available Rewards</Text>
          <Text style={[styles.statValue, { color: '#4CAF50' }]}>
            {stakingInfo?.rewards || '0'} SHR
          </Text>
        </View>
      </View>

      {/* Claim Rewards Button */}
      {parseFloat(stakingInfo?.rewards || '0') > 0 && (
        <TouchableOpacity 
          style={styles.claimButton}
          onPress={handleClaimRewards}
          disabled={isLoading}
        >
          {isLoading ? (
            <ActivityIndicator color="#fff" />
          ) : (
            <Text style={styles.claimButtonText}>
              Claim {stakingInfo?.rewards} SHR
            </Text>
          )}
        </TouchableOpacity>
      )}

      {/* Quick Delegate */}
      <View style={styles.quickActions}>
        <Text style={styles.sectionTitle}>Quick Actions</Text>
        <TouchableOpacity 
          style={styles.quickActionButton}
          onPress={() => setActiveTab('validators')}
        >
          <Text style={styles.quickActionText}>Find Validators to Stake</Text>
        </TouchableOpacity>
      </View>

      {/* My Delegations Summary */}
      {stakingInfo?.delegations && stakingInfo.delegations.length > 0 && (
        <View style={styles.delegationsSummary}>
          <Text style={styles.sectionTitle}>Active Delegations</Text>
          {stakingInfo.delegations.slice(0, 3).map((del, index) => (
            <View key={index} style={styles.delegationItem}>
              <Text style={styles.delegationValidator}>{del.validatorName}</Text>
              <Text style={styles.delegationAmount}>{del.amount} SHR</Text>
            </View>
          ))}
          {stakingInfo.delegations.length > 3 && (
            <TouchableOpacity onPress={() => setActiveTab('delegations')}>
              <Text style={styles.seeMoreText}>
                +{stakingInfo.delegations.length - 3} more
              </Text>
            </TouchableOpacity>
          )}
        </View>
      )}
    </View>
  );

  const renderValidators = () => (
    <View style={styles.validatorsContainer}>
      {!stakingInfo?.validators || stakingInfo.validators.length === 0 ? (
        <View style={styles.emptyState}>
          <Text style={styles.emptyText}>No validators found</Text>
          <Text style={styles.emptySubtext}>
            Pull to refresh or check your connection
          </Text>
        </View>
      ) : (
        stakingInfo.validators.map((validator) => (
          <TouchableOpacity
            key={validator.id}
            style={styles.validatorCard}
            onPress={() => openDelegateModal(validator, false)}
          >
            <View style={styles.validatorHeader}>
              <View style={styles.validatorInfo}>
                <Text style={styles.validatorName}>{validator.name}</Text>
                <Text style={styles.validatorAddress} numberOfLines={1}>
                  {validator.address}
                </Text>
              </View>
              <View style={[styles.validatorStatus, { 
                backgroundColor: validator.isActive ? '#4CAF50' : '#F44336' 
              }]}>
                <Text style={styles.validatorStatusText}>
                  {validator.isActive ? 'Active' : 'Inactive'}
                </Text>
              </View>
            </View>
            
            <View style={styles.validatorStats}>
              <View style={styles.validatorStat}>
                <Text style={styles.validatorStatLabel}>Commission</Text>
                <Text style={styles.validatorStatValue}>{validator.commission}%</Text>
              </View>
              <View style={styles.validatorStat}>
                <Text style={styles.validatorStatLabel}>Total Staked</Text>
                <Text style={styles.validatorStatValue}>{validator.totalStaked}</Text>
              </View>
              <View style={styles.validatorStat}>
                <Text style={styles.validatorStatLabel}>Delegators</Text>
                <Text style={styles.validatorStatValue}>{validator.delegatorCount}</Text>
              </View>
              <View style={styles.validatorStat}>
                <Text style={styles.validatorStatLabel}>Uptime</Text>
                <Text style={styles.validatorStatValue}>{validator.uptime}%</Text>
              </View>
            </View>

            <TouchableOpacity 
              style={styles.delegateButton}
              onPress={() => openDelegateModal(validator, false)}
            >
              <Text style={styles.delegateButtonText}>Delegate</Text>
            </TouchableOpacity>
          </TouchableOpacity>
        ))
      )}
    </View>
  );

  const renderDelegations = () => (
    <View style={styles.delegationsContainer}>
      {!stakingInfo?.delegations || stakingInfo.delegations.length === 0 ? (
        <View style={styles.emptyState}>
          <Text style={styles.emptyText}>No active delegations</Text>
          <Text style={styles.emptySubtext}>
            Stake SHR with validators to earn rewards
          </Text>
          <TouchableOpacity 
            style={styles.startStakingButton}
            onPress={() => setActiveTab('validators')}
          >
            <Text style={styles.startStakingText}>Start Staking</Text>
          </TouchableOpacity>
        </View>
      ) : (
        stakingInfo.delegations.map((delegation, index) => (
          <View key={index} style={styles.delegationCard}>
            <View style={styles.delegationHeader}>
              <Text style={styles.delegationValidatorName}>
                {delegation.validatorName}
              </Text>
              <Text style={styles.delegationRewards}>
                +{delegation.rewards} SHR
              </Text>
            </View>
            
            <View style={styles.delegationDetails}>
              <View style={styles.delegationDetail}>
                <Text style={styles.delegationDetailLabel}>Staked Amount</Text>
                <Text style={styles.delegationDetailValue}>{delegation.amount} SHR</Text>
              </View>
              <View style={styles.delegationDetail}>
                <Text style={styles.delegationDetailLabel}>Started</Text>
                <Text style={styles.delegationDetailValue}>
                  {new Date(delegation.startTime * 1000).toLocaleDateString()}
                </Text>
              </View>
            </View>

            <View style={styles.delegationActions}>
              <TouchableOpacity 
                style={[styles.delegationAction, styles.delegationActionAdd]}
                onPress={() => {
                  const validator = stakingInfo?.validators?.find(
                    v => v.id === delegation.validatorId
                  );
                  if (validator) openDelegateModal(validator, false);
                }}
              >
                <Text style={styles.delegationActionText}>Add More</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={[styles.delegationAction, styles.delegationActionRemove]}
                onPress={() => {
                  const validator = stakingInfo?.validators?.find(
                    v => v.id === delegation.validatorId
                  );
                  if (validator) openDelegateModal(validator, true);
                }}
              >
                <Text style={styles.delegationActionText}>Undelegate</Text>
              </TouchableOpacity>
            </View>
          </View>
        ))
      )}
    </View>
  );

  return (
    <View style={styles.container}>
      {/* Error Banner */}
      {lastError && (
        <TouchableOpacity style={styles.errorBanner} onPress={clearError}>
          <Text style={styles.errorText}>{lastError}</Text>
        </TouchableOpacity>
      )}

      {/* Tab Navigation */}
      <View style={styles.tabBar}>
        <TouchableOpacity 
          style={[styles.tab, activeTab === 'overview' && styles.activeTab]}
          onPress={() => setActiveTab('overview')}
        >
          <Text style={[styles.tabText, activeTab === 'overview' && styles.activeTabText]}>
            Overview
          </Text>
        </TouchableOpacity>
        <TouchableOpacity 
          style={[styles.tab, activeTab === 'validators' && styles.activeTab]}
          onPress={() => setActiveTab('validators')}
        >
          <Text style={[styles.tabText, activeTab === 'validators' && styles.activeTabText]}>
            Validators
          </Text>
        </TouchableOpacity>
        <TouchableOpacity 
          style={[styles.tab, activeTab === 'delegations' && styles.activeTab]}
          onPress={() => setActiveTab('delegations')}
        >
          <Text style={[styles.tabText, activeTab === 'delegations' && styles.activeTabText]}>
            My Stakes
          </Text>
        </TouchableOpacity>
      </View>

      {/* Content */}
      <ScrollView
        style={styles.content}
        refreshControl={
          <RefreshControl refreshing={refreshing} onRefresh={onRefresh} />
        }
      >
        {activeTab === 'overview' && renderOverview()}
        {activeTab === 'validators' && renderValidators()}
        {activeTab === 'delegations' && renderDelegations()}
      </ScrollView>

      {/* Delegate/Undelegate Modal */}
      <Modal
        visible={showDelegateModal}
        transparent
        animationType="slide"
        onRequestClose={() => setShowDelegateModal(false)}
      >
        <View style={styles.modalOverlay}>
          <View style={styles.modalContent}>
            <Text style={styles.modalTitle}>
              {isUnstaking ? 'Undelegate from' : 'Delegate to'} {selectedValidator?.name}
            </Text>
            
            <View style={styles.modalInput}>
              <Text style={styles.modalInputLabel}>Amount (SHR)</Text>
              <TextInput
                style={styles.modalTextInput}
                placeholder="0.00000000"
                placeholderTextColor="#666"
                value={delegateAmount}
                onChangeText={setDelegateAmount}
                keyboardType="decimal-pad"
              />
              {!isUnstaking && balance && (
                <TouchableOpacity 
                  onPress={() => setDelegateAmount(balance.total)}
                  style={styles.maxButton}
                >
                  <Text style={styles.maxButtonText}>MAX</Text>
                </TouchableOpacity>
              )}
            </View>

            {!isUnstaking && (
              <Text style={styles.modalHint}>
                Available: {balance?.total || '0'} SHR
              </Text>
            )}

            <View style={styles.modalActions}>
              <TouchableOpacity 
                style={styles.modalCancelButton}
                onPress={() => setShowDelegateModal(false)}
              >
                <Text style={styles.modalCancelText}>Cancel</Text>
              </TouchableOpacity>
              <TouchableOpacity 
                style={[
                  styles.modalConfirmButton,
                  isUnstaking && styles.modalConfirmButtonDanger
                ]}
                onPress={isUnstaking ? handleUndelegate : handleDelegate}
                disabled={isLoading}
              >
                {isLoading ? (
                  <ActivityIndicator color="#fff" size="small" />
                ) : (
                  <Text style={styles.modalConfirmText}>
                    {isUnstaking ? 'Undelegate' : 'Delegate'}
                  </Text>
                )}
              </TouchableOpacity>
            </View>
          </View>
        </View>
      </Modal>
    </View>
  );
};

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#121212',
  },
  errorBanner: {
    backgroundColor: '#F44336',
    padding: 12,
    alignItems: 'center',
  },
  errorText: {
    color: '#fff',
    fontSize: 14,
  },
  tabBar: {
    flexDirection: 'row',
    backgroundColor: '#1E1E1E',
    borderBottomWidth: 1,
    borderBottomColor: '#333',
  },
  tab: {
    flex: 1,
    paddingVertical: 16,
    alignItems: 'center',
  },
  activeTab: {
    borderBottomWidth: 2,
    borderBottomColor: '#2196F3',
  },
  tabText: {
    color: '#888',
    fontSize: 14,
  },
  activeTabText: {
    color: '#2196F3',
    fontWeight: 'bold',
  },
  content: {
    flex: 1,
  },
  overviewContainer: {
    padding: 16,
  },
  statusCard: {
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
    marginBottom: 16,
  },
  statusRow: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  statusDot: {
    width: 12,
    height: 12,
    borderRadius: 6,
    marginRight: 8,
  },
  statusText: {
    color: '#fff',
    fontSize: 16,
  },
  statsContainer: {
    flexDirection: 'row',
    gap: 12,
    marginBottom: 16,
  },
  statCard: {
    flex: 1,
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
    alignItems: 'center',
  },
  statLabel: {
    color: '#888',
    fontSize: 12,
    marginBottom: 4,
  },
  statValue: {
    color: '#fff',
    fontSize: 18,
    fontWeight: 'bold',
  },
  claimButton: {
    backgroundColor: '#4CAF50',
    padding: 16,
    borderRadius: 12,
    alignItems: 'center',
    marginBottom: 24,
  },
  claimButtonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
  },
  quickActions: {
    marginBottom: 24,
  },
  sectionTitle: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 12,
  },
  quickActionButton: {
    backgroundColor: '#2196F3',
    padding: 16,
    borderRadius: 12,
    alignItems: 'center',
  },
  quickActionText: {
    color: '#fff',
    fontSize: 14,
    fontWeight: '500',
  },
  delegationsSummary: {
    backgroundColor: '#1E1E1E',
    padding: 16,
    borderRadius: 12,
  },
  delegationItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 8,
    borderBottomWidth: 1,
    borderBottomColor: '#333',
  },
  delegationValidator: {
    color: '#fff',
    fontSize: 14,
  },
  delegationAmount: {
    color: '#4CAF50',
    fontSize: 14,
    fontWeight: '500',
  },
  seeMoreText: {
    color: '#2196F3',
    fontSize: 14,
    marginTop: 8,
    textAlign: 'center',
  },
  validatorsContainer: {
    padding: 16,
  },
  validatorCard: {
    backgroundColor: '#1E1E1E',
    borderRadius: 12,
    padding: 16,
    marginBottom: 12,
  },
  validatorHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'flex-start',
    marginBottom: 12,
  },
  validatorInfo: {
    flex: 1,
  },
  validatorName: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
  },
  validatorAddress: {
    color: '#666',
    fontSize: 12,
    marginTop: 2,
  },
  validatorStatus: {
    paddingHorizontal: 8,
    paddingVertical: 4,
    borderRadius: 4,
  },
  validatorStatusText: {
    color: '#fff',
    fontSize: 10,
    fontWeight: 'bold',
  },
  validatorStats: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    marginBottom: 12,
  },
  validatorStat: {
    width: '50%',
    paddingVertical: 4,
  },
  validatorStatLabel: {
    color: '#888',
    fontSize: 12,
  },
  validatorStatValue: {
    color: '#fff',
    fontSize: 14,
  },
  delegateButton: {
    backgroundColor: '#2196F3',
    padding: 12,
    borderRadius: 8,
    alignItems: 'center',
  },
  delegateButtonText: {
    color: '#fff',
    fontWeight: 'bold',
  },
  delegationsContainer: {
    padding: 16,
  },
  delegationCard: {
    backgroundColor: '#1E1E1E',
    borderRadius: 12,
    padding: 16,
    marginBottom: 12,
  },
  delegationHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 12,
  },
  delegationValidatorName: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
  },
  delegationRewards: {
    color: '#4CAF50',
    fontSize: 14,
    fontWeight: '500',
  },
  delegationDetails: {
    flexDirection: 'row',
    marginBottom: 12,
  },
  delegationDetail: {
    flex: 1,
  },
  delegationDetailLabel: {
    color: '#888',
    fontSize: 12,
  },
  delegationDetailValue: {
    color: '#fff',
    fontSize: 14,
  },
  delegationActions: {
    flexDirection: 'row',
    gap: 8,
  },
  delegationAction: {
    flex: 1,
    padding: 10,
    borderRadius: 8,
    alignItems: 'center',
  },
  delegationActionAdd: {
    backgroundColor: '#2196F3',
  },
  delegationActionRemove: {
    backgroundColor: '#F44336',
  },
  delegationActionText: {
    color: '#fff',
    fontSize: 14,
    fontWeight: '500',
  },
  emptyState: {
    alignItems: 'center',
    paddingVertical: 48,
  },
  emptyText: {
    color: '#fff',
    fontSize: 18,
  },
  emptySubtext: {
    color: '#666',
    fontSize: 14,
    marginTop: 8,
    textAlign: 'center',
  },
  startStakingButton: {
    backgroundColor: '#2196F3',
    paddingHorizontal: 24,
    paddingVertical: 12,
    borderRadius: 8,
    marginTop: 16,
  },
  startStakingText: {
    color: '#fff',
    fontWeight: 'bold',
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
  modalInput: {
    marginBottom: 8,
  },
  modalInputLabel: {
    color: '#888',
    fontSize: 14,
    marginBottom: 8,
  },
  modalTextInput: {
    backgroundColor: '#2A2A2A',
    borderRadius: 12,
    padding: 16,
    color: '#fff',
    fontSize: 18,
  },
  maxButton: {
    position: 'absolute',
    right: 12,
    top: 38,
    backgroundColor: '#2196F3',
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 4,
  },
  maxButtonText: {
    color: '#fff',
    fontSize: 12,
    fontWeight: 'bold',
  },
  modalHint: {
    color: '#666',
    fontSize: 12,
    marginBottom: 24,
  },
  modalActions: {
    flexDirection: 'row',
    gap: 12,
  },
  modalCancelButton: {
    flex: 1,
    padding: 16,
    borderRadius: 12,
    backgroundColor: '#333',
    alignItems: 'center',
  },
  modalCancelText: {
    color: '#fff',
    fontSize: 16,
  },
  modalConfirmButton: {
    flex: 1,
    padding: 16,
    borderRadius: 12,
    backgroundColor: '#2196F3',
    alignItems: 'center',
  },
  modalConfirmButtonDanger: {
    backgroundColor: '#F44336',
  },
  modalConfirmText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
  },
});

export default StakingScreen;
