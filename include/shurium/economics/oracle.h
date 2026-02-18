// SHURIUM - Decentralized Price Oracle System
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements a decentralized oracle system for obtaining reliable price data.
//
// Key features:
// - Multiple independent oracle sources
// - Reputation-based oracle weighting
// - Fraud detection and slashing
// - Aggregation with outlier rejection
// - On-chain price commitments

#ifndef SHURIUM_ECONOMICS_ORACLE_H
#define SHURIUM_ECONOMICS_ORACLE_H

#include <shurium/core/types.h>
#include <shurium/crypto/keys.h>
#include <shurium/economics/stability.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace shurium {
namespace economics {

// Forward declarations
class OracleRegistry;
class PriceAggregator;

// ============================================================================
// Oracle Constants
// ============================================================================

/// Minimum number of oracles for valid price
constexpr size_t MIN_ORACLE_SOURCES = 3;

/// Maximum allowed price deviation between oracles (basis points)
constexpr int64_t MAX_ORACLE_DEVIATION_BPS = 500; // 5%

/// Oracle heartbeat interval (seconds)
constexpr int64_t ORACLE_HEARTBEAT_SECONDS = 300; // 5 minutes

/// Oracle timeout (seconds without heartbeat)
constexpr int64_t ORACLE_TIMEOUT_SECONDS = 900; // 15 minutes

/// Minimum stake required to be an oracle
constexpr Amount MIN_ORACLE_STAKE = 10000 * COIN;

/// Slashing penalty for malicious behavior (percentage of stake)
constexpr int ORACLE_SLASH_PERCENT = 10;

/// Price update cooldown per oracle (blocks)
constexpr int ORACLE_UPDATE_COOLDOWN = 6; // ~3 minutes

// ============================================================================
// Oracle Types
// ============================================================================

/// Unique identifier for an oracle
using OracleId = Hash256;

/// Oracle status
enum class OracleStatus {
    /// Oracle is active and providing data
    Active,
    
    /// Oracle is registered but not yet active
    Pending,
    
    /// Oracle is temporarily suspended
    Suspended,
    
    /// Oracle has been slashed/banned
    Slashed,
    
    /// Oracle has voluntarily withdrawn
    Withdrawn,
    
    /// Oracle is offline (no recent heartbeat)
    Offline
};

/// Convert status to string
const char* OracleStatusToString(OracleStatus status);

// ============================================================================
// Oracle Registration
// ============================================================================

/**
 * Information about a registered oracle.
 */
struct OracleInfo {
    /// Unique oracle identifier
    OracleId id;
    
    /// Oracle's public key (for signature verification)
    PublicKey publicKey;
    
    /// Operator address (for rewards/slashing)
    Hash160 operatorAddress;
    
    /// Stake amount locked
    Amount stakedAmount{0};
    
    /// Current status
    OracleStatus status{OracleStatus::Pending};
    
    /// Registration block height
    int registrationHeight{0};
    
    /// Last active block height
    int lastActiveHeight{0};
    
    /// Last heartbeat timestamp
    std::chrono::system_clock::time_point lastHeartbeat;
    
    /// Reputation score (0-1000)
    int reputation{500}; // Start neutral
    
    /// Total price submissions
    uint64_t submissionCount{0};
    
    /// Successful submissions (included in aggregation)
    uint64_t successfulSubmissions{0};
    
    /// Outlier submissions (rejected as outliers)
    uint64_t outlierCount{0};
    
    /// Slash events
    uint32_t slashCount{0};
    
    /// Human-readable name (optional)
    std::string name;
    
    /// External URL (optional)
    std::string url;
    
    /// Calculate accuracy rate
    double AccuracyRate() const;
    
    /// Check if oracle is eligible to submit
    bool CanSubmit(int currentHeight) const;
    
    /// Check if oracle should be marked offline
    bool IsTimedOut() const;
    
    /// Get weight for aggregation (based on reputation)
    double GetWeight() const;
    
    /// String representation
    std::string ToString() const;
};

// ============================================================================
// Price Submission
// ============================================================================

/**
 * A price submission from an oracle.
 */
struct PriceSubmission {
    /// Oracle that submitted
    OracleId oracleId;
    
    /// Submitted price (millicents)
    PriceMillicents price{0};
    
    /// Block height of submission
    int blockHeight{0};
    
    /// Timestamp
    std::chrono::system_clock::time_point timestamp;
    
    /// Signature of the submission
    std::vector<Byte> signature;
    
    /// Optional confidence score from oracle
    int confidence{100};
    
    /// Hash of submission (for uniqueness)
    Hash256 GetHash() const;
    
    /// Create signature message
    std::vector<Byte> GetSignatureMessage() const;
    
    /// Verify signature
    bool VerifySignature(const PublicKey& pubkey) const;
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    static std::optional<PriceSubmission> Deserialize(const Byte* data, size_t len);
    
    /// String representation
    std::string ToString() const;
};

// ============================================================================
// Price Commitment (Commit-Reveal Scheme)
// ============================================================================

/**
 * Commitment for commit-reveal oracle scheme.
 * 
 * Oracles first submit a commitment, then reveal the actual price.
 * This prevents front-running and price manipulation.
 */
struct PriceCommitment {
    /// Oracle that committed
    OracleId oracleId;
    
    /// Hash of (price || salt)
    Hash256 commitment;
    
    /// Block height of commitment
    int commitHeight{0};
    
    /// Reveal deadline (block height)
    int revealDeadline{0};
    
    /// Whether revealed
    bool revealed{false};
    
    /// Revealed price (set after reveal)
    PriceMillicents revealedPrice{0};
    
    /// Salt used in commitment
    Hash256 salt;
    
    /// Create a new commitment
    static PriceCommitment Create(
        const OracleId& oracle,
        PriceMillicents price,
        int commitHeight,
        int revealWindow = 10
    );
    
    /// Verify a reveal matches commitment
    bool VerifyReveal(PriceMillicents price, const Hash256& revealSalt) const;
    
    /// Check if reveal deadline passed
    bool IsExpired(int currentHeight) const;
    
    /// Serialize
    std::vector<Byte> Serialize() const;
    
    /// Deserialize
    static std::optional<PriceCommitment> Deserialize(const Byte* data, size_t len);
};

// ============================================================================
// Oracle Registry
// ============================================================================

/**
 * Manages oracle registration and status.
 */
class OracleRegistry {
public:
    OracleRegistry();
    ~OracleRegistry();
    
    // ========================================================================
    // Registration
    // ========================================================================
    
    /**
     * Register a new oracle.
     * 
     * @param pubkey Oracle's public key
     * @param operatorAddr Operator address for rewards
     * @param stake Initial stake amount
     * @param height Registration block height
     * @param name Optional human-readable name
     * @return Oracle ID if successful
     */
    std::optional<OracleId> Register(
        const PublicKey& pubkey,
        const Hash160& operatorAddr,
        Amount stake,
        int height,
        const std::string& name = ""
    );
    
    /**
     * Add stake to an existing oracle.
     * 
     * @param id Oracle ID
     * @param amount Additional stake
     * @return true if successful
     */
    bool AddStake(const OracleId& id, Amount amount);
    
    /**
     * Withdraw an oracle (voluntary exit).
     * 
     * @param id Oracle ID
     * @param height Current block height
     * @return Stake amount to return
     */
    Amount Withdraw(const OracleId& id, int height);
    
    // ========================================================================
    // Lookup
    // ========================================================================
    
    /// Get oracle info by ID
    const OracleInfo* GetOracle(const OracleId& id) const;
    
    /// Get oracle by public key
    const OracleInfo* GetOracleByPubkey(const PublicKey& pubkey) const;
    
    /// Get all active oracles
    std::vector<const OracleInfo*> GetActiveOracles() const;
    
    /// Get oracle count by status
    size_t GetOracleCount(OracleStatus status) const;
    
    /// Check if oracle exists
    bool HasOracle(const OracleId& id) const;
    
    // ========================================================================
    // Status Management
    // ========================================================================
    
    /// Update oracle status
    void UpdateStatus(const OracleId& id, OracleStatus status);
    
    /// Record a heartbeat
    void RecordHeartbeat(const OracleId& id, int height);
    
    /// Record a successful submission
    void RecordSubmission(const OracleId& id, bool wasIncluded);
    
    /// Mark timed-out oracles as offline
    void UpdateTimeouts();
    
    // ========================================================================
    // Reputation
    // ========================================================================
    
    /// Increase reputation (for good behavior)
    void IncreaseReputation(const OracleId& id, int amount);
    
    /// Decrease reputation (for bad behavior)
    void DecreaseReputation(const OracleId& id, int amount);
    
    /**
     * Slash an oracle for malicious behavior.
     * 
     * @param id Oracle ID
     * @param reason Reason for slashing
     * @return Amount slashed
     */
    Amount Slash(const OracleId& id, const std::string& reason);
    
    // ========================================================================
    // Serialization
    // ========================================================================
    
    std::vector<Byte> Serialize() const;
    bool Deserialize(const Byte* data, size_t len);

private:
    std::map<OracleId, OracleInfo> oracles_;
    std::map<PublicKey, OracleId> pubkeyIndex_;
    mutable std::mutex mutex_;
    
    /// Generate oracle ID from public key
    static OracleId GenerateId(const PublicKey& pubkey);
};

// ============================================================================
// Price Aggregator
// ============================================================================

/**
 * Aggregates prices from multiple oracles.
 * 
 * Uses weighted median with outlier rejection for robust price aggregation.
 */
class PriceAggregator {
public:
    /// Aggregation configuration
    struct Config {
        /// Minimum sources required
        size_t minSources{MIN_ORACLE_SOURCES};
        
        /// Maximum deviation for outlier detection (basis points)
        int64_t maxDeviationBps{MAX_ORACLE_DEVIATION_BPS};
        
        /// Whether to weight by reputation
        bool useReputationWeights{true};
        
        /// Whether to use commit-reveal scheme
        bool useCommitReveal{true};
        
        /// Commit-reveal window (blocks)
        int commitRevealWindow{10};
    };
    
    /// Create aggregator with default config
    explicit PriceAggregator(OracleRegistry& registry);
    
    /// Create aggregator with custom config
    PriceAggregator(OracleRegistry& registry, const Config& config);
    
    // ========================================================================
    // Submission Processing
    // ========================================================================
    
    /**
     * Process a price submission.
     * 
     * @param submission The submission to process
     * @return true if submission was accepted
     */
    bool ProcessSubmission(const PriceSubmission& submission);
    
    /**
     * Process a price commitment.
     * 
     * @param commitment The commitment
     * @return true if commitment was accepted
     */
    bool ProcessCommitment(const PriceCommitment& commitment);
    
    /**
     * Process a price reveal.
     * 
     * @param oracleId Oracle revealing
     * @param price Revealed price
     * @param salt Reveal salt
     * @return true if reveal was valid
     */
    bool ProcessReveal(const OracleId& oracleId, PriceMillicents price, const Hash256& salt);
    
    // ========================================================================
    // Aggregation
    // ========================================================================
    
    /**
     * Aggregate current submissions into a single price.
     * 
     * @param currentHeight Current block height
     * @return Aggregated price or nullopt if insufficient data
     */
    std::optional<AggregatedPrice> Aggregate(int currentHeight);
    
    /**
     * Get the latest aggregated price.
     */
    std::optional<AggregatedPrice> GetLatestPrice() const;
    
    /// Get number of pending submissions
    size_t GetPendingSubmissionCount() const;
    
    /// Get submissions for current round
    std::vector<PriceSubmission> GetCurrentSubmissions() const;
    
    // ========================================================================
    // Round Management
    // ========================================================================
    
    /// Start a new aggregation round
    void StartNewRound(int height);
    
    /// Finalize current round
    void FinalizeRound();
    
    /// Get current round height
    int GetCurrentRoundHeight() const { return currentRoundHeight_; }
    
    /// Clear old submissions
    void Prune(int keepRounds = 10);
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    const Config& GetConfig() const { return config_; }
    void UpdateConfig(const Config& config);

private:
    OracleRegistry& registry_;
    Config config_;
    
    /// Current aggregation round height
    int currentRoundHeight_{0};
    
    /// Submissions for current round
    std::vector<PriceSubmission> currentSubmissions_;
    
    /// Pending commitments (oracle -> commitment)
    std::map<OracleId, PriceCommitment> pendingCommitments_;
    
    /// Latest aggregated price
    std::optional<AggregatedPrice> latestPrice_;
    
    /// Historical prices
    std::deque<AggregatedPrice> priceHistory_;
    
    mutable std::mutex mutex_;
    
    /// Calculate weighted median
    PriceMillicents CalculateWeightedMedian(
        const std::vector<std::pair<PriceMillicents, double>>& weightedPrices
    ) const;
    
    /// Detect and remove outliers
    std::vector<PriceSubmission> RemoveOutliers(
        const std::vector<PriceSubmission>& submissions
    ) const;
    
    /// Calculate spread
    int64_t CalculateSpread(const std::vector<PriceSubmission>& submissions) const;
};

// ============================================================================
// Oracle Price Feed
// ============================================================================

/**
 * High-level interface for price data.
 * 
 * Integrates oracle registry, aggregator, and stability controller.
 */
class OraclePriceFeed {
public:
    OraclePriceFeed();
    ~OraclePriceFeed();
    
    /// Initialize with external registry
    void Initialize(std::shared_ptr<OracleRegistry> registry);
    
    /// Process a block (triggers aggregation if needed)
    void ProcessBlock(int height);
    
    /// Get current price
    std::optional<AggregatedPrice> GetCurrentPrice() const;
    
    /// Get price at a specific height
    std::optional<AggregatedPrice> GetPriceAtHeight(int height) const;
    
    /// Get price history
    std::vector<AggregatedPrice> GetPriceHistory(size_t count = 100) const;
    
    /// Register a price update callback
    using PriceCallback = std::function<void(const AggregatedPrice&)>;
    void OnPriceUpdate(PriceCallback callback);
    
    /// Get the aggregator
    PriceAggregator& GetAggregator() { return *aggregator_; }
    
    /// Get the registry
    OracleRegistry& GetRegistry() { return *registry_; }

private:
    std::shared_ptr<OracleRegistry> registry_;
    std::unique_ptr<PriceAggregator> aggregator_;
    std::vector<PriceCallback> callbacks_;
    mutable std::mutex mutex_;
};

// ============================================================================
// Oracle Reward Calculator
// ============================================================================

/**
 * Calculates rewards for oracle operators.
 */
class OracleRewardCalculator {
public:
    /// Calculate reward for an oracle based on participation
    static Amount CalculateReward(
        const OracleInfo& oracle,
        Amount totalRewardPool,
        size_t totalActiveOracles
    );
    
    /// Calculate penalty for missed submissions
    static Amount CalculatePenalty(
        const OracleInfo& oracle,
        int missedSubmissions
    );
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Validate a price submission
bool ValidateSubmission(const PriceSubmission& submission, const OracleRegistry& registry);

/// Calculate oracle ID from public key
OracleId CalculateOracleId(const PublicKey& pubkey);

/// Check if price is reasonable (within bounds)
bool IsPriceReasonable(PriceMillicents price, PriceMillicents reference, int64_t maxDeviationBps);

} // namespace economics
} // namespace shurium

#endif // SHURIUM_ECONOMICS_ORACLE_H
