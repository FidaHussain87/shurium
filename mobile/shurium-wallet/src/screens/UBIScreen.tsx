/**
 * SHURIUM Mobile Wallet - UBI Screen
 * Universal Basic Income - Identity status, claims, and history
 * This is SHURIUM's flagship feature for financial inclusion
 */

import React, { useState, useEffect, useCallback } from 'react';
import {
  View,
  Text,
  StyleSheet,
  ScrollView,
  TouchableOpacity,
  Alert,
  ActivityIndicator,
  RefreshControl,
} from 'react-native';
import { useWalletStore } from '../store/wallet';

// Verification level colors and descriptions
const VERIFICATION_LEVELS = {
  none: { color: '#666', label: 'Not Verified', description: 'Register your identity to claim UBI' },
  basic: { color: '#FF9800', label: 'Basic', description: 'Limited UBI eligibility' },
  standard: { color: '#2196F3', label: 'Standard', description: 'Standard UBI rate' },
  enhanced: { color: '#4CAF50', label: 'Enhanced', description: 'Enhanced UBI rate' },
  maximum: { color: '#9C27B0', label: 'Maximum', description: 'Maximum UBI rate' },
};

export const UBIScreen: React.FC = () => {
  const {
    ubiInfo,
    accounts,
    activeAccountId,
    refreshUBIInfo,
    claimUBI,
    lastError,
    clearError,
  } = useWalletStore();

  const [refreshing, setRefreshing] = useState(false);
  const [isClaiming, setIsClaiming] = useState(false);

  const activeAccount = accounts.find(a => a.id === activeAccountId);

  useEffect(() => {
    refreshUBIInfo();
  }, []);

  const onRefresh = useCallback(async () => {
    setRefreshing(true);
    await refreshUBIInfo();
    setRefreshing(false);
  }, [refreshUBIInfo]);

  const handleClaimUBI = async () => {
    if (!ubiInfo?.isEligible) {
      Alert.alert('Not Eligible', 'You are not currently eligible for UBI claims');
      return;
    }

    const claimable = parseFloat(ubiInfo.claimableAmount);
    if (claimable <= 0) {
      Alert.alert('No UBI Available', 'You have no UBI to claim at this time');
      return;
    }

    Alert.alert(
      'Claim UBI',
      `Claim ${ubiInfo.claimableAmount} SHR?\n\nThis is your Universal Basic Income allocation.`,
      [
        { text: 'Cancel', style: 'cancel' },
        {
          text: 'Claim',
          onPress: async () => {
            setIsClaiming(true);
            try {
              const txid = await claimUBI();
              Alert.alert(
                'UBI Claimed!',
                `Successfully claimed ${ubiInfo.claimableAmount} SHR\n\nTransaction: ${txid.substring(0, 16)}...`,
                [{ text: 'OK' }]
              );
            } catch (error) {
              Alert.alert('Claim Failed', (error as Error).message);
            } finally {
              setIsClaiming(false);
            }
          },
        },
      ]
    );
  };

  const getVerificationLevel = () => {
    const level = ubiInfo?.verificationLevel?.toLowerCase() || 'none';
    return VERIFICATION_LEVELS[level as keyof typeof VERIFICATION_LEVELS] || VERIFICATION_LEVELS.none;
  };

  const formatDate = (timestamp: number | null) => {
    if (!timestamp) return 'Never';
    return new Date(timestamp * 1000).toLocaleString();
  };

  const getTimeUntilNextClaim = () => {
    if (!ubiInfo?.nextClaimTime) return null;
    const now = Date.now() / 1000;
    const diff = ubiInfo.nextClaimTime - now;
    if (diff <= 0) return 'Available now';
    
    const hours = Math.floor(diff / 3600);
    const minutes = Math.floor((diff % 3600) / 60);
    
    if (hours > 24) {
      const days = Math.floor(hours / 24);
      return `${days} day${days > 1 ? 's' : ''} ${hours % 24}h`;
    }
    return `${hours}h ${minutes}m`;
  };

  const verificationLevel = getVerificationLevel();

  return (
    <ScrollView
      style={styles.container}
      refreshControl={
        <RefreshControl refreshing={refreshing} onRefresh={onRefresh} />
      }
    >
      {/* Error Banner */}
      {lastError && (
        <TouchableOpacity style={styles.errorBanner} onPress={clearError}>
          <Text style={styles.errorText}>{lastError}</Text>
        </TouchableOpacity>
      )}

      {/* Header Card - What is UBI */}
      <View style={styles.headerCard}>
        <Text style={styles.headerTitle}>Universal Basic Income</Text>
        <Text style={styles.headerDescription}>
          SHURIUM provides regular income to all verified participants, 
          funded by network emissions and treasury. No conditions, 
          no means testing - just financial inclusion for everyone.
        </Text>
      </View>

      {/* Identity Status */}
      <View style={styles.identityCard}>
        <Text style={styles.sectionTitle}>Identity Status</Text>
        
        <View style={styles.identityStatus}>
          <View style={[styles.verificationBadge, { backgroundColor: verificationLevel.color }]}>
            <Text style={styles.verificationText}>{verificationLevel.label}</Text>
          </View>
          <Text style={styles.verificationDescription}>{verificationLevel.description}</Text>
        </View>

        {ubiInfo?.identityId ? (
          <View style={styles.identityDetails}>
            <View style={styles.identityRow}>
              <Text style={styles.identityLabel}>Identity ID</Text>
              <Text style={styles.identityValue} numberOfLines={1}>
                {ubiInfo.identityId}
              </Text>
            </View>
            <View style={styles.identityRow}>
              <Text style={styles.identityLabel}>Registered</Text>
              <Text style={[styles.identityValue, { color: '#4CAF50' }]}>Yes</Text>
            </View>
            <View style={styles.identityRow}>
              <Text style={styles.identityLabel}>UBI Eligible</Text>
              <Text style={[styles.identityValue, { 
                color: ubiInfo.isEligible ? '#4CAF50' : '#F44336' 
              }]}>
                {ubiInfo.isEligible ? 'Yes' : 'No'}
              </Text>
            </View>
          </View>
        ) : (
          <View style={styles.notRegistered}>
            <Text style={styles.notRegisteredText}>
              Your identity is not yet registered on the SHURIUM network.
            </Text>
            <TouchableOpacity style={styles.registerButton}>
              <Text style={styles.registerButtonText}>Register Identity</Text>
            </TouchableOpacity>
            <Text style={styles.registerHint}>
              Identity registration requires zero-knowledge proof of uniqueness
            </Text>
          </View>
        )}
      </View>

      {/* Claim UBI */}
      {ubiInfo?.isEligible && (
        <View style={styles.claimCard}>
          <Text style={styles.sectionTitle}>Claim Your UBI</Text>
          
          <View style={styles.claimAmount}>
            <Text style={styles.claimAmountLabel}>Available to Claim</Text>
            <Text style={styles.claimAmountValue}>
              {ubiInfo.claimableAmount || '0'} SHR
            </Text>
          </View>

          <TouchableOpacity
            style={[
              styles.claimButton,
              (parseFloat(ubiInfo.claimableAmount) <= 0 || isClaiming) && styles.claimButtonDisabled
            ]}
            onPress={handleClaimUBI}
            disabled={parseFloat(ubiInfo.claimableAmount) <= 0 || isClaiming}
          >
            {isClaiming ? (
              <ActivityIndicator color="#fff" />
            ) : (
              <Text style={styles.claimButtonText}>
                {parseFloat(ubiInfo.claimableAmount) > 0 
                  ? `Claim ${ubiInfo.claimableAmount} SHR`
                  : 'No UBI Available'}
              </Text>
            )}
          </TouchableOpacity>

          {/* Claim timing info */}
          <View style={styles.claimTiming}>
            <View style={styles.timingRow}>
              <Text style={styles.timingLabel}>Last Claim</Text>
              <Text style={styles.timingValue}>{formatDate(ubiInfo.lastClaimTime)}</Text>
            </View>
            <View style={styles.timingRow}>
              <Text style={styles.timingLabel}>Next Claim</Text>
              <Text style={styles.timingValue}>
                {getTimeUntilNextClaim() || 'Register identity'}
              </Text>
            </View>
          </View>
        </View>
      )}

      {/* Statistics */}
      <View style={styles.statsCard}>
        <Text style={styles.sectionTitle}>Your UBI Statistics</Text>
        
        <View style={styles.statsGrid}>
          <View style={styles.statItem}>
            <Text style={styles.statValue}>{ubiInfo?.totalClaimed || '0'}</Text>
            <Text style={styles.statLabel}>Total Claimed (SHR)</Text>
          </View>
          <View style={styles.statItem}>
            <Text style={styles.statValue}>
              {ubiInfo?.claimHistory?.length || 0}
            </Text>
            <Text style={styles.statLabel}>Claims Made</Text>
          </View>
        </View>
      </View>

      {/* Claim History */}
      {ubiInfo?.claimHistory && ubiInfo.claimHistory.length > 0 && (
        <View style={styles.historyCard}>
          <Text style={styles.sectionTitle}>Claim History</Text>
          
          {ubiInfo.claimHistory.map((claim, index) => (
            <View key={index} style={styles.historyItem}>
              <View style={styles.historyInfo}>
                <Text style={styles.historyAmount}>+{claim.amount} SHR</Text>
                <Text style={styles.historyDate}>
                  {new Date(claim.timestamp * 1000).toLocaleDateString()}
                </Text>
              </View>
              <Text style={styles.historyTxid} numberOfLines={1}>
                {claim.txid.substring(0, 12)}...
              </Text>
            </View>
          ))}
        </View>
      )}

      {/* How UBI Works */}
      <View style={styles.infoCard}>
        <Text style={styles.sectionTitle}>How SHURIUM UBI Works</Text>
        
        <View style={styles.infoItem}>
          <Text style={styles.infoNumber}>1</Text>
          <View style={styles.infoContent}>
            <Text style={styles.infoTitle}>Register Your Identity</Text>
            <Text style={styles.infoDescription}>
              Create a privacy-preserving identity using zero-knowledge proofs. 
              Your personal data never leaves your device.
            </Text>
          </View>
        </View>

        <View style={styles.infoItem}>
          <Text style={styles.infoNumber}>2</Text>
          <View style={styles.infoContent}>
            <Text style={styles.infoTitle}>Get Verified</Text>
            <Text style={styles.infoDescription}>
              Higher verification levels unlock higher UBI rates. 
              Verification protects against Sybil attacks.
            </Text>
          </View>
        </View>

        <View style={styles.infoItem}>
          <Text style={styles.infoNumber}>3</Text>
          <View style={styles.infoContent}>
            <Text style={styles.infoTitle}>Claim Regularly</Text>
            <Text style={styles.infoDescription}>
              UBI accrues over time. Claim daily, weekly, or whenever 
              you need it. Unclaimed UBI remains available.
            </Text>
          </View>
        </View>

        <View style={styles.infoItem}>
          <Text style={styles.infoNumber}>4</Text>
          <View style={styles.infoContent}>
            <Text style={styles.infoTitle}>Use or Stake</Text>
            <Text style={styles.infoDescription}>
              Spend your UBI, save it, or stake it to earn additional rewards. 
              It's your money - use it as you wish.
            </Text>
          </View>
        </View>
      </View>

      {/* Verification Levels Info */}
      <View style={styles.levelsCard}>
        <Text style={styles.sectionTitle}>Verification Levels</Text>
        
        {Object.entries(VERIFICATION_LEVELS).filter(([key]) => key !== 'none').map(([key, level]) => (
          <View key={key} style={styles.levelItem}>
            <View style={[styles.levelDot, { backgroundColor: level.color }]} />
            <View style={styles.levelInfo}>
              <Text style={styles.levelName}>{level.label}</Text>
              <Text style={styles.levelDescription}>{level.description}</Text>
            </View>
          </View>
        ))}
      </View>
    </ScrollView>
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
  headerCard: {
    backgroundColor: '#9C27B0',
    padding: 20,
    margin: 16,
    marginBottom: 0,
    borderRadius: 16,
  },
  headerTitle: {
    color: '#fff',
    fontSize: 22,
    fontWeight: 'bold',
    marginBottom: 8,
  },
  headerDescription: {
    color: 'rgba(255,255,255,0.9)',
    fontSize: 14,
    lineHeight: 20,
  },
  sectionTitle: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
    marginBottom: 16,
  },
  identityCard: {
    backgroundColor: '#1E1E1E',
    padding: 20,
    margin: 16,
    borderRadius: 16,
  },
  identityStatus: {
    alignItems: 'center',
    marginBottom: 20,
  },
  verificationBadge: {
    paddingHorizontal: 16,
    paddingVertical: 8,
    borderRadius: 20,
    marginBottom: 8,
  },
  verificationText: {
    color: '#fff',
    fontSize: 14,
    fontWeight: 'bold',
  },
  verificationDescription: {
    color: '#888',
    fontSize: 14,
  },
  identityDetails: {
    borderTopWidth: 1,
    borderTopColor: '#333',
    paddingTop: 16,
  },
  identityRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 8,
  },
  identityLabel: {
    color: '#888',
    fontSize: 14,
  },
  identityValue: {
    color: '#fff',
    fontSize: 14,
    flex: 1,
    textAlign: 'right',
    marginLeft: 16,
  },
  notRegistered: {
    alignItems: 'center',
    paddingTop: 16,
  },
  notRegisteredText: {
    color: '#888',
    fontSize: 14,
    textAlign: 'center',
    marginBottom: 16,
  },
  registerButton: {
    backgroundColor: '#9C27B0',
    paddingHorizontal: 32,
    paddingVertical: 14,
    borderRadius: 12,
    marginBottom: 12,
  },
  registerButtonText: {
    color: '#fff',
    fontSize: 16,
    fontWeight: 'bold',
  },
  registerHint: {
    color: '#666',
    fontSize: 12,
    textAlign: 'center',
  },
  claimCard: {
    backgroundColor: '#1E1E1E',
    padding: 20,
    marginHorizontal: 16,
    marginBottom: 16,
    borderRadius: 16,
  },
  claimAmount: {
    alignItems: 'center',
    marginBottom: 20,
  },
  claimAmountLabel: {
    color: '#888',
    fontSize: 14,
    marginBottom: 4,
  },
  claimAmountValue: {
    color: '#4CAF50',
    fontSize: 36,
    fontWeight: 'bold',
  },
  claimButton: {
    backgroundColor: '#9C27B0',
    padding: 18,
    borderRadius: 12,
    alignItems: 'center',
    marginBottom: 16,
  },
  claimButtonDisabled: {
    opacity: 0.5,
  },
  claimButtonText: {
    color: '#fff',
    fontSize: 18,
    fontWeight: 'bold',
  },
  claimTiming: {
    borderTopWidth: 1,
    borderTopColor: '#333',
    paddingTop: 16,
  },
  timingRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 4,
  },
  timingLabel: {
    color: '#888',
    fontSize: 14,
  },
  timingValue: {
    color: '#fff',
    fontSize: 14,
  },
  statsCard: {
    backgroundColor: '#1E1E1E',
    padding: 20,
    marginHorizontal: 16,
    marginBottom: 16,
    borderRadius: 16,
  },
  statsGrid: {
    flexDirection: 'row',
    gap: 16,
  },
  statItem: {
    flex: 1,
    backgroundColor: '#2A2A2A',
    padding: 16,
    borderRadius: 12,
    alignItems: 'center',
  },
  statValue: {
    color: '#fff',
    fontSize: 24,
    fontWeight: 'bold',
  },
  statLabel: {
    color: '#888',
    fontSize: 12,
    marginTop: 4,
    textAlign: 'center',
  },
  historyCard: {
    backgroundColor: '#1E1E1E',
    padding: 20,
    marginHorizontal: 16,
    marginBottom: 16,
    borderRadius: 16,
  },
  historyItem: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#333',
  },
  historyInfo: {
    flex: 1,
  },
  historyAmount: {
    color: '#4CAF50',
    fontSize: 16,
    fontWeight: 'bold',
  },
  historyDate: {
    color: '#888',
    fontSize: 12,
  },
  historyTxid: {
    color: '#666',
    fontSize: 12,
    fontFamily: 'monospace',
  },
  infoCard: {
    backgroundColor: '#1E1E1E',
    padding: 20,
    marginHorizontal: 16,
    marginBottom: 16,
    borderRadius: 16,
  },
  infoItem: {
    flexDirection: 'row',
    marginBottom: 16,
  },
  infoNumber: {
    width: 28,
    height: 28,
    borderRadius: 14,
    backgroundColor: '#9C27B0',
    color: '#fff',
    fontSize: 14,
    fontWeight: 'bold',
    textAlign: 'center',
    lineHeight: 28,
    marginRight: 12,
    overflow: 'hidden',
  },
  infoContent: {
    flex: 1,
  },
  infoTitle: {
    color: '#fff',
    fontSize: 14,
    fontWeight: 'bold',
    marginBottom: 4,
  },
  infoDescription: {
    color: '#888',
    fontSize: 13,
    lineHeight: 18,
  },
  levelsCard: {
    backgroundColor: '#1E1E1E',
    padding: 20,
    marginHorizontal: 16,
    marginBottom: 32,
    borderRadius: 16,
  },
  levelItem: {
    flexDirection: 'row',
    alignItems: 'center',
    paddingVertical: 12,
    borderBottomWidth: 1,
    borderBottomColor: '#333',
  },
  levelDot: {
    width: 12,
    height: 12,
    borderRadius: 6,
    marginRight: 12,
  },
  levelInfo: {
    flex: 1,
  },
  levelName: {
    color: '#fff',
    fontSize: 14,
    fontWeight: '500',
  },
  levelDescription: {
    color: '#888',
    fontSize: 12,
  },
});

export default UBIScreen;
