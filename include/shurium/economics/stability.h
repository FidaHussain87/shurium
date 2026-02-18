// SHURIUM - Algorithmic Stability System
// Copyright (c) 2024 SHURIUM Developers
// MIT License
//
// Implements the SHURIUM algorithmic stability mechanism to reduce price volatility.
//
// Key features:
// - Elastic supply adjustments based on market conditions
// - Target price band maintenance using stability reserve
// - Smoothing algorithms to prevent abrupt changes
// - Oracle integration for real-time price data
// - Emergency mechanisms for extreme market conditions

#ifndef SHURIUM_ECONOMICS_STABILITY_H
#define SHURIUM_ECONOMICS_STABILITY_H

#include <shurium/core/types.h>
#include <shurium/economics/reward.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace shurium {
namespace economics {

// Forward declarations
class PriceOracle;
class StabilityReserve;

// ============================================================================
// Stability Constants
// ============================================================================

/// Target price in USD millicents (1 NXS = $1.00 = 100000 millicents)
constexpr int64_t TARGET_PRICE_MILLICENTS = 100000;

/// Price band width (+/- 5% from target)
constexpr int64_t PRICE_BAND_PERCENT = 5;

/// Upper price threshold (105% of target)
constexpr int64_t UPPER_PRICE_THRESHOLD = TARGET_PRICE_MILLICENTS * (100 + PRICE_BAND_PERCENT) / 100;

/// Lower price threshold (95% of target)
constexpr int64_t LOWER_PRICE_THRESHOLD = TARGET_PRICE_MILLICENTS * (100 - PRICE_BAND_PERCENT) / 100;

/// Maximum supply adjustment per block (0.1%)
constexpr int64_t MAX_ADJUSTMENT_RATE_BPS = 10; // basis points

/// Minimum blocks between adjustments
constexpr int MIN_ADJUSTMENT_INTERVAL = 10;

/// Price smoothing window (number of price samples)
constexpr size_t PRICE_SMOOTHING_WINDOW = 144; // ~12 hours at 5-min samples

/// Emergency threshold (price deviation > 20%)
constexpr int64_t EMERGENCY_DEVIATION_PERCENT = 20;

// ============================================================================
// Price Types
// ============================================================================

/// Price in millicents (1/100000 of $1)
using PriceMillicents = int64_t;

/// Price timestamp
using PriceTimestamp = std::chrono::system_clock::time_point;

/// A single price observation
struct PriceObservation {
    /// Price in millicents
    PriceMillicents price{0};
    
    /// Timestamp of observation
    PriceTimestamp timestamp;
    
    /// Block height (if from on-chain oracle)
    int blockHeight{0};
    
    /// Source oracle identifier
    std::string source;
    
    /// Confidence score (0-100)
    int confidence{0};
    
    /// Calculate deviation from target (in basis points)
    int64_t DeviationBps() const;
    
    /// Check if within price band
    bool IsInBand() const;
    
    /// String representation
    std::string ToString() const;
};

/// Aggregated price from multiple oracles
struct AggregatedPrice {
    /// Median price
    PriceMillicents medianPrice{0};
    
    /// Weighted average price
    PriceMillicents weightedPrice{0};
    
    /// Number of oracle sources
    size_t sourceCount{0};
    
    /// Minimum price across sources
    PriceMillicents minPrice{0};
    
    /// Maximum price across sources
    PriceMillicents maxPrice{0};
    
    /// Spread between min and max (in basis points)
    int64_t spreadBps{0};
    
    /// Average confidence
    int avgConfidence{0};
    
    /// Timestamp
    PriceTimestamp timestamp;
    
    /// Check if price data is reliable (low spread, high confidence)
    bool IsReliable() const;
    
    /// String representation
    std::string ToString() const;
};

// ============================================================================
// Stability Action
// ============================================================================

/// Type of stability action to take
enum class StabilityAction {
    /// No action needed - price within band
    None,
    
    /// Expand supply to lower price
    ExpandSupply,
    
    /// Contract supply to raise price
    ContractSupply,
    
    /// Emergency expansion (price way below target)
    EmergencyExpand,
    
    /// Emergency contraction (price way above target)
    EmergencyContract,
    
    /// Pause - insufficient data or conflicting signals
    Pause
};

/// Convert action to string
const char* StabilityActionToString(StabilityAction action);

/// Result of a stability calculation
struct StabilityDecision {
    /// Recommended action
    StabilityAction action{StabilityAction::None};
    
    /// Adjustment magnitude (basis points)
    int64_t adjustmentBps{0};
    
    /// Current price deviation from target (basis points)
    int64_t deviationBps{0};
    
    /// Confidence in this decision (0-100)
    int confidence{0};
    
    /// Reason for this decision
    std::string reason;
    
    /// Block height at decision time
    int blockHeight{0};
    
    /// String representation
    std::string ToString() const;
};

// ============================================================================
// Price Smoothing
// ============================================================================

/**
 * Exponential moving average for price smoothing.
 */
class ExponentialMovingAverage {
public:
    /// Create with smoothing factor (0 < alpha <= 1)
    explicit ExponentialMovingAverage(double alpha = 0.1);
    
    /// Add a new value
    void Update(PriceMillicents value);
    
    /// Get current smoothed value
    PriceMillicents GetValue() const { return currentValue_; }
    
    /// Check if initialized (has at least one value)
    bool IsInitialized() const { return initialized_; }
    
    /// Reset the EMA
    void Reset();

private:
    double alpha_;
    PriceMillicents currentValue_{0};
    bool initialized_{false};
};

/**
 * Time-weighted average price (TWAP).
 */
class TimeWeightedAveragePrice {
public:
    /// Create with window duration
    explicit TimeWeightedAveragePrice(std::chrono::seconds window);
    
    /// Add a price observation
    void AddObservation(const PriceObservation& obs);
    
    /// Calculate TWAP for current window
    PriceMillicents Calculate() const;
    
    /// Get number of observations in window
    size_t ObservationCount() const { return observations_.size(); }
    
    /// Clear old observations
    void Prune();

private:
    std::chrono::seconds window_;
    std::deque<PriceObservation> observations_;
};

// ============================================================================
// Stability Controller
// ============================================================================

/**
 * Main stability controller.
 * 
 * Monitors price data, calculates supply adjustments, and coordinates
 * with the stability reserve to maintain price stability.
 */
class StabilityController {
public:
    /// Configuration for the controller
    struct Config {
        /// Target price in millicents
        PriceMillicents targetPrice{TARGET_PRICE_MILLICENTS};
        
        /// Price band width (percent)
        int64_t bandWidthPercent{PRICE_BAND_PERCENT};
        
        /// Maximum adjustment per block (basis points)
        int64_t maxAdjustmentBps{MAX_ADJUSTMENT_RATE_BPS};
        
        /// Minimum blocks between adjustments
        int minAdjustmentInterval{MIN_ADJUSTMENT_INTERVAL};
        
        /// TWAP window in seconds
        int twapWindowSeconds{3600}; // 1 hour
        
        /// EMA smoothing factor
        double emaSmoothingAlpha{0.1};
        
        /// Minimum oracle sources required
        size_t minOracleSources{3};
        
        /// Minimum confidence threshold
        int minConfidence{70};
    };
    
    /// Create controller with default config
    StabilityController();
    
    /// Create controller with custom config
    explicit StabilityController(const Config& config);
    
    ~StabilityController();
    
    // ========================================================================
    // Price Updates
    // ========================================================================
    
    /// Process a new price observation
    void OnPriceUpdate(const PriceObservation& obs);
    
    /// Process an aggregated price from oracle system
    void OnAggregatedPrice(const AggregatedPrice& price);
    
    /// Get current smoothed price
    PriceMillicents GetSmoothedPrice() const;
    
    /// Get current TWAP
    PriceMillicents GetTWAP() const;
    
    /// Get latest aggregated price
    std::optional<AggregatedPrice> GetLatestPrice() const;
    
    // ========================================================================
    // Stability Decisions
    // ========================================================================
    
    /**
     * Calculate stability decision for current conditions.
     * 
     * @param currentHeight Current block height
     * @return Decision with action and magnitude
     */
    StabilityDecision CalculateDecision(int currentHeight) const;
    
    /**
     * Check if an adjustment is allowed at this block.
     * 
     * @param currentHeight Current block height
     * @return true if adjustment is allowed
     */
    bool CanAdjust(int currentHeight) const;
    
    /// Get last adjustment height
    int GetLastAdjustmentHeight() const { return lastAdjustmentHeight_; }
    
    /// Record that an adjustment was made
    void RecordAdjustment(int height, const StabilityDecision& decision);
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /// Get current configuration
    const Config& GetConfig() const { return config_; }
    
    /// Update configuration
    void UpdateConfig(const Config& config);
    
    /// Get target price
    PriceMillicents GetTargetPrice() const;
    
    /// Get upper band threshold
    PriceMillicents GetUpperThreshold() const;
    
    /// Get lower band threshold
    PriceMillicents GetLowerThreshold() const;
    
    // ========================================================================
    // Statistics
    // ========================================================================
    
    /// Stability statistics
    struct Stats {
        /// Total adjustments made
        uint64_t totalAdjustments{0};
        
        /// Expansion adjustments
        uint64_t expansions{0};
        
        /// Contraction adjustments
        uint64_t contractions{0};
        
        /// Average deviation from target (basis points)
        int64_t avgDeviationBps{0};
        
        /// Time within band (percentage)
        double timeInBandPercent{0.0};
        
        /// Maximum deviation observed
        int64_t maxDeviationBps{0};
        
        /// Emergency actions triggered
        uint64_t emergencyActions{0};
    };
    
    Stats GetStats() const;

private:
    Config config_;
    
    /// EMA for price smoothing
    ExponentialMovingAverage ema_;
    
    /// TWAP calculator
    std::unique_ptr<TimeWeightedAveragePrice> twap_;
    
    /// Latest aggregated price
    std::optional<AggregatedPrice> latestPrice_;
    
    /// Last adjustment block height
    int lastAdjustmentHeight_{0};
    
    /// Statistics
    Stats stats_;
    
    /// Mutex for thread safety
    mutable std::mutex mutex_;
    
    /// Calculate adjustment magnitude based on deviation
    int64_t CalculateAdjustmentMagnitude(int64_t deviationBps) const;
    
    /// Check if in emergency condition
    bool IsEmergencyCondition(int64_t deviationBps) const;
};

// ============================================================================
// Stability Reserve
// ============================================================================

/**
 * Manages the stability reserve fund.
 * 
 * The stability reserve holds NXS tokens that can be used to
 * buy or sell to maintain price stability.
 */
class StabilityReserve {
public:
    /// Create reserve
    StabilityReserve();
    
    /// Get current reserve balance
    Amount GetBalance() const { return balance_; }
    
    /// Add funds to reserve (from block rewards)
    void AddFunds(Amount amount);
    
    /// Remove funds from reserve (for market operations)
    bool RemoveFunds(Amount amount);
    
    /// Check if reserve has minimum required balance
    bool HasMinimumBalance() const;
    
    /// Get minimum required balance (for safety)
    Amount GetMinimumBalance() const { return minimumBalance_; }
    
    /// Set minimum balance threshold
    void SetMinimumBalance(Amount amount);
    
    /// Calculate maximum spendable amount (balance - minimum)
    Amount GetSpendableAmount() const;
    
    /// Record a buy operation (spent reserve to buy NXS)
    void RecordBuy(Amount spent, Amount acquired);
    
    /// Record a sell operation (sold NXS for reserve)
    void RecordSell(Amount sold, Amount received);
    
    /// Get total bought all-time
    Amount GetTotalBought() const { return totalBought_; }
    
    /// Get total sold all-time
    Amount GetTotalSold() const { return totalSold_; }
    
    /// Serialize state
    std::vector<Byte> Serialize() const;
    
    /// Deserialize state
    bool Deserialize(const Byte* data, size_t len);

private:
    Amount balance_{0};
    Amount minimumBalance_{1000 * COIN}; // Default 1000 NXS minimum
    Amount totalBought_{0};
    Amount totalSold_{0};
    Amount totalSpent_{0};
    Amount totalReceived_{0};
    mutable std::mutex mutex_;
};

// ============================================================================
// Supply Adjuster
// ============================================================================

/**
 * Implements supply adjustment mechanisms.
 * 
 * Works with the stability controller to execute supply changes:
 * - Expansion: Mint additional coins (distributed as rewards)
 * - Contraction: Reduce future rewards / burn mechanism
 */
class SupplyAdjuster {
public:
    /// Create adjuster with reward calculator
    explicit SupplyAdjuster(const RewardCalculator& calculator);
    
    /**
     * Calculate adjusted block reward based on stability decision.
     * 
     * @param baseReward Base reward from RewardCalculator
     * @param decision Stability decision
     * @return Adjusted reward
     */
    Amount CalculateAdjustedReward(Amount baseReward, 
                                    const StabilityDecision& decision) const;
    
    /**
     * Calculate supply adjustment amount.
     * 
     * @param decision Stability decision
     * @param currentSupply Current total supply
     * @return Amount to mint (positive) or reduce (negative as target)
     */
    int64_t CalculateSupplyChange(const StabilityDecision& decision,
                                   Amount currentSupply) const;
    
    /// Get cumulative supply adjustment
    int64_t GetCumulativeAdjustment() const { return cumulativeAdjustment_; }
    
    /// Record an executed adjustment
    void RecordAdjustment(int64_t amount, int height);

private:
    const RewardCalculator& calculator_;
    int64_t cumulativeAdjustment_{0};
    int lastAdjustmentHeight_{0};
};

// ============================================================================
// Stability Metrics
// ============================================================================

/**
 * Calculates and tracks stability metrics.
 */
class StabilityMetrics {
public:
    /// Add a price observation for metrics calculation
    void AddObservation(const PriceObservation& obs);
    
    /// Calculate volatility (standard deviation of returns)
    double CalculateVolatility(size_t windowSize = 24) const;
    
    /// Calculate price momentum (rate of change)
    double CalculateMomentum() const;
    
    /// Get average deviation from target
    int64_t GetAverageDeviation() const;
    
    /// Get maximum deviation in observation window
    int64_t GetMaxDeviation() const;
    
    /// Get time spent within price band (percentage)
    double GetTimeInBand() const;
    
    /// Clear old observations
    void Prune(PriceTimestamp cutoff);

private:
    std::deque<PriceObservation> observations_;
    static constexpr size_t MAX_OBSERVATIONS = 1000;
};

// ============================================================================
// Utility Functions
// ============================================================================

/// Convert millicents to dollars string
std::string MillicentsToString(PriceMillicents price);

/// Parse price string to millicents
std::optional<PriceMillicents> ParsePrice(const std::string& str);

/// Calculate basis points deviation
inline int64_t CalculateDeviationBps(PriceMillicents price, PriceMillicents target) {
    if (target == 0) return 0;
    return ((price - target) * 10000) / target;
}

/// Calculate percentage deviation
inline double CalculateDeviationPercent(PriceMillicents price, PriceMillicents target) {
    return static_cast<double>(CalculateDeviationBps(price, target)) / 100.0;
}

} // namespace economics
} // namespace shurium

#endif // SHURIUM_ECONOMICS_STABILITY_H
