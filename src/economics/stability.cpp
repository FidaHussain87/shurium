// SHURIUM - Algorithmic Stability System Implementation
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <shurium/economics/stability.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace shurium {
namespace economics {

// ============================================================================
// PriceObservation
// ============================================================================

int64_t PriceObservation::DeviationBps() const {
    return CalculateDeviationBps(price, TARGET_PRICE_MILLICENTS);
}

bool PriceObservation::IsInBand() const {
    return price >= LOWER_PRICE_THRESHOLD && price <= UPPER_PRICE_THRESHOLD;
}

std::string PriceObservation::ToString() const {
    std::ostringstream ss;
    ss << "PriceObservation {"
       << " price: " << MillicentsToString(price)
       << ", deviation: " << DeviationBps() << " bps"
       << ", source: " << source
       << ", confidence: " << confidence
       << " }";
    return ss.str();
}

// ============================================================================
// AggregatedPrice
// ============================================================================

bool AggregatedPrice::IsReliable() const {
    // Reliable if spread is less than 2% and confidence is high
    return spreadBps < 200 && avgConfidence >= 70 && sourceCount >= 3;
}

std::string AggregatedPrice::ToString() const {
    std::ostringstream ss;
    ss << "AggregatedPrice {"
       << " median: " << MillicentsToString(medianPrice)
       << ", weighted: " << MillicentsToString(weightedPrice)
       << ", sources: " << sourceCount
       << ", spread: " << spreadBps << " bps"
       << ", confidence: " << avgConfidence
       << " }";
    return ss.str();
}

// ============================================================================
// StabilityAction Utilities
// ============================================================================

const char* StabilityActionToString(StabilityAction action) {
    switch (action) {
        case StabilityAction::None:             return "None";
        case StabilityAction::ExpandSupply:     return "ExpandSupply";
        case StabilityAction::ContractSupply:   return "ContractSupply";
        case StabilityAction::EmergencyExpand:  return "EmergencyExpand";
        case StabilityAction::EmergencyContract: return "EmergencyContract";
        case StabilityAction::Pause:            return "Pause";
        default:                                return "Unknown";
    }
}

// ============================================================================
// StabilityDecision
// ============================================================================

std::string StabilityDecision::ToString() const {
    std::ostringstream ss;
    ss << "StabilityDecision {"
       << " action: " << StabilityActionToString(action)
       << ", adjustment: " << adjustmentBps << " bps"
       << ", deviation: " << deviationBps << " bps"
       << ", confidence: " << confidence
       << ", reason: \"" << reason << "\""
       << " }";
    return ss.str();
}

// ============================================================================
// ExponentialMovingAverage
// ============================================================================

ExponentialMovingAverage::ExponentialMovingAverage(double alpha)
    : alpha_(std::clamp(alpha, 0.001, 1.0)) {
}

void ExponentialMovingAverage::Update(PriceMillicents value) {
    if (!initialized_) {
        currentValue_ = value;
        initialized_ = true;
    } else {
        currentValue_ = static_cast<PriceMillicents>(
            alpha_ * value + (1.0 - alpha_) * currentValue_
        );
    }
}

void ExponentialMovingAverage::Reset() {
    currentValue_ = 0;
    initialized_ = false;
}

// ============================================================================
// TimeWeightedAveragePrice
// ============================================================================

TimeWeightedAveragePrice::TimeWeightedAveragePrice(std::chrono::seconds window)
    : window_(window) {
}

void TimeWeightedAveragePrice::AddObservation(const PriceObservation& obs) {
    observations_.push_back(obs);
    Prune();
}

PriceMillicents TimeWeightedAveragePrice::Calculate() const {
    if (observations_.empty()) {
        return 0;
    }
    
    if (observations_.size() == 1) {
        return observations_.front().price;
    }
    
    // Calculate time-weighted average
    int64_t totalWeightedPrice = 0;
    int64_t totalDuration = 0;
    
    for (size_t i = 0; i < observations_.size() - 1; ++i) {
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            observations_[i + 1].timestamp - observations_[i].timestamp
        ).count();
        
        if (duration > 0) {
            totalWeightedPrice += observations_[i].price * duration;
            totalDuration += duration;
        }
    }
    
    // Add the last observation with remaining time
    auto now = std::chrono::system_clock::now();
    auto lastDuration = std::chrono::duration_cast<std::chrono::seconds>(
        now - observations_.back().timestamp
    ).count();
    
    if (lastDuration > 0 && lastDuration < window_.count()) {
        totalWeightedPrice += observations_.back().price * lastDuration;
        totalDuration += lastDuration;
    }
    
    return totalDuration > 0 ? totalWeightedPrice / totalDuration : observations_.back().price;
}

void TimeWeightedAveragePrice::Prune() {
    auto cutoff = std::chrono::system_clock::now() - window_;
    
    while (!observations_.empty() && observations_.front().timestamp < cutoff) {
        observations_.pop_front();
    }
}

// ============================================================================
// StabilityController
// ============================================================================

StabilityController::StabilityController()
    : config_(),
      ema_(config_.emaSmoothingAlpha),
      twap_(std::make_unique<TimeWeightedAveragePrice>(
          std::chrono::seconds(config_.twapWindowSeconds))) {
}

StabilityController::StabilityController(const Config& config)
    : config_(config),
      ema_(config.emaSmoothingAlpha),
      twap_(std::make_unique<TimeWeightedAveragePrice>(
          std::chrono::seconds(config.twapWindowSeconds))) {
}

StabilityController::~StabilityController() = default;

void StabilityController::OnPriceUpdate(const PriceObservation& obs) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    ema_.Update(obs.price);
    twap_->AddObservation(obs);
}

void StabilityController::OnAggregatedPrice(const AggregatedPrice& price) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    latestPrice_ = price;
    ema_.Update(price.medianPrice);
    
    // Create observation from aggregated price
    PriceObservation obs;
    obs.price = price.medianPrice;
    obs.timestamp = price.timestamp;
    obs.confidence = price.avgConfidence;
    obs.source = "aggregated";
    
    twap_->AddObservation(obs);
}

PriceMillicents StabilityController::GetSmoothedPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return ema_.GetValue();
}

PriceMillicents StabilityController::GetTWAP() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return twap_->Calculate();
}

std::optional<AggregatedPrice> StabilityController::GetLatestPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latestPrice_;
}

StabilityDecision StabilityController::CalculateDecision(int currentHeight) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    StabilityDecision decision;
    decision.blockHeight = currentHeight;
    
    // Check if we have price data
    if (!ema_.IsInitialized() || !latestPrice_) {
        decision.action = StabilityAction::Pause;
        decision.reason = "Insufficient price data";
        decision.confidence = 0;
        return decision;
    }
    
    // Check if data is reliable
    if (!latestPrice_->IsReliable()) {
        decision.action = StabilityAction::Pause;
        decision.reason = "Price data unreliable";
        decision.confidence = 30;
        return decision;
    }
    
    // Calculate deviation from target
    PriceMillicents currentPrice = ema_.GetValue();
    decision.deviationBps = CalculateDeviationBps(currentPrice, config_.targetPrice);
    
    // Check for emergency conditions
    if (IsEmergencyCondition(decision.deviationBps)) {
        if (decision.deviationBps < 0) {
            decision.action = StabilityAction::EmergencyExpand;
            decision.reason = "Emergency: Price significantly below target";
        } else {
            decision.action = StabilityAction::EmergencyContract;
            decision.reason = "Emergency: Price significantly above target";
        }
        decision.adjustmentBps = config_.maxAdjustmentBps * 2; // Double adjustment for emergency
        decision.confidence = 90;
        return decision;
    }
    
    // Calculate thresholds
    int64_t lowerThresholdBps = -static_cast<int64_t>(config_.bandWidthPercent) * 100;
    int64_t upperThresholdBps = static_cast<int64_t>(config_.bandWidthPercent) * 100;
    
    // Determine action based on deviation
    if (decision.deviationBps < lowerThresholdBps) {
        // Price too low - need to expand supply or buy
        decision.action = StabilityAction::ExpandSupply;
        decision.adjustmentBps = CalculateAdjustmentMagnitude(decision.deviationBps);
        decision.reason = "Price below target band";
        decision.confidence = 80;
    } else if (decision.deviationBps > upperThresholdBps) {
        // Price too high - need to contract supply or sell
        decision.action = StabilityAction::ContractSupply;
        decision.adjustmentBps = CalculateAdjustmentMagnitude(decision.deviationBps);
        decision.reason = "Price above target band";
        decision.confidence = 80;
    } else {
        // Price within band
        decision.action = StabilityAction::None;
        decision.adjustmentBps = 0;
        decision.reason = "Price within target band";
        decision.confidence = 100;
    }
    
    return decision;
}

bool StabilityController::CanAdjust(int currentHeight) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentHeight >= lastAdjustmentHeight_ + config_.minAdjustmentInterval;
}

void StabilityController::RecordAdjustment(int height, const StabilityDecision& decision) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    lastAdjustmentHeight_ = height;
    stats_.totalAdjustments++;
    
    if (decision.action == StabilityAction::ExpandSupply || 
        decision.action == StabilityAction::EmergencyExpand) {
        stats_.expansions++;
    } else if (decision.action == StabilityAction::ContractSupply ||
               decision.action == StabilityAction::EmergencyContract) {
        stats_.contractions++;
    }
    
    if (decision.action == StabilityAction::EmergencyExpand ||
        decision.action == StabilityAction::EmergencyContract) {
        stats_.emergencyActions++;
    }
    
    // Update max deviation
    int64_t absDeviation = std::abs(decision.deviationBps);
    if (absDeviation > stats_.maxDeviationBps) {
        stats_.maxDeviationBps = absDeviation;
    }
}

void StabilityController::UpdateConfig(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    ema_ = ExponentialMovingAverage(config.emaSmoothingAlpha);
    twap_ = std::make_unique<TimeWeightedAveragePrice>(
        std::chrono::seconds(config.twapWindowSeconds)
    );
}

PriceMillicents StabilityController::GetTargetPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.targetPrice;
}

PriceMillicents StabilityController::GetUpperThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.targetPrice * (100 + config_.bandWidthPercent) / 100;
}

PriceMillicents StabilityController::GetLowerThreshold() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.targetPrice * (100 - config_.bandWidthPercent) / 100;
}

StabilityController::Stats StabilityController::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

int64_t StabilityController::CalculateAdjustmentMagnitude(int64_t deviationBps) const {
    // Proportional adjustment based on deviation
    // More deviation = larger adjustment (up to max)
    int64_t absDeviation = std::abs(deviationBps);
    
    // Linear scaling: at 5% deviation, use max adjustment
    // At smaller deviations, use proportionally less
    int64_t bandBps = config_.bandWidthPercent * 100;
    
    if (bandBps == 0) {
        return config_.maxAdjustmentBps;
    }
    
    int64_t scaledAdjustment = (absDeviation * config_.maxAdjustmentBps) / bandBps;
    return std::min(scaledAdjustment, config_.maxAdjustmentBps);
}

bool StabilityController::IsEmergencyCondition(int64_t deviationBps) const {
    return std::abs(deviationBps) > EMERGENCY_DEVIATION_PERCENT * 100;
}

// ============================================================================
// StabilityReserve
// ============================================================================

StabilityReserve::StabilityReserve() = default;

void StabilityReserve::AddFunds(Amount amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    balance_ += amount;
}

bool StabilityReserve::RemoveFunds(Amount amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (balance_ < amount) {
        return false;
    }
    
    balance_ -= amount;
    return true;
}

bool StabilityReserve::HasMinimumBalance() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return balance_ >= minimumBalance_;
}

void StabilityReserve::SetMinimumBalance(Amount amount) {
    std::lock_guard<std::mutex> lock(mutex_);
    minimumBalance_ = amount;
}

Amount StabilityReserve::GetSpendableAmount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return balance_ > minimumBalance_ ? balance_ - minimumBalance_ : 0;
}

void StabilityReserve::RecordBuy(Amount spent, Amount acquired) {
    std::lock_guard<std::mutex> lock(mutex_);
    totalSpent_ += spent;
    totalBought_ += acquired;
}

void StabilityReserve::RecordSell(Amount sold, Amount received) {
    std::lock_guard<std::mutex> lock(mutex_);
    totalSold_ += sold;
    totalReceived_ += received;
}

std::vector<Byte> StabilityReserve::Serialize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<Byte> data;
    data.reserve(48);
    
    // Serialize each Amount (8 bytes each)
    auto writeAmount = [&data](Amount val) {
        for (int i = 0; i < 8; ++i) {
            data.push_back(static_cast<Byte>((val >> (i * 8)) & 0xFF));
        }
    };
    
    writeAmount(balance_);
    writeAmount(minimumBalance_);
    writeAmount(totalBought_);
    writeAmount(totalSold_);
    writeAmount(totalSpent_);
    writeAmount(totalReceived_);
    
    return data;
}

bool StabilityReserve::Deserialize(const Byte* data, size_t len) {
    if (len < 48) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t offset = 0;
    
    auto readAmount = [&data, &offset]() -> Amount {
        Amount val = 0;
        for (int i = 0; i < 8; ++i) {
            val |= static_cast<Amount>(data[offset++]) << (i * 8);
        }
        return val;
    };
    
    balance_ = readAmount();
    minimumBalance_ = readAmount();
    totalBought_ = readAmount();
    totalSold_ = readAmount();
    totalSpent_ = readAmount();
    totalReceived_ = readAmount();
    
    return true;
}

// ============================================================================
// SupplyAdjuster
// ============================================================================

SupplyAdjuster::SupplyAdjuster(const RewardCalculator& calculator)
    : calculator_(calculator) {
}

Amount SupplyAdjuster::CalculateAdjustedReward(
    Amount baseReward, 
    const StabilityDecision& decision
) const {
    if (decision.action == StabilityAction::None ||
        decision.action == StabilityAction::Pause) {
        return baseReward;
    }
    
    // Calculate adjustment as a percentage of base reward
    int64_t adjustmentPercent = decision.adjustmentBps / 100;
    
    if (decision.action == StabilityAction::ExpandSupply ||
        decision.action == StabilityAction::EmergencyExpand) {
        // Increase reward to expand supply
        return baseReward + (baseReward * adjustmentPercent) / 100;
    } else {
        // Decrease reward to contract supply
        Amount reduction = (baseReward * adjustmentPercent) / 100;
        return baseReward > reduction ? baseReward - reduction : 0;
    }
}

int64_t SupplyAdjuster::CalculateSupplyChange(
    const StabilityDecision& decision,
    Amount currentSupply
) const {
    if (decision.action == StabilityAction::None ||
        decision.action == StabilityAction::Pause) {
        return 0;
    }
    
    // Calculate supply change based on decision
    int64_t changeBps = decision.adjustmentBps;
    int64_t change = (static_cast<int64_t>(currentSupply) * changeBps) / 10000;
    
    if (decision.action == StabilityAction::ContractSupply ||
        decision.action == StabilityAction::EmergencyContract) {
        return -change;
    }
    
    return change;
}

void SupplyAdjuster::RecordAdjustment(int64_t amount, int height) {
    cumulativeAdjustment_ += amount;
    lastAdjustmentHeight_ = height;
}

// ============================================================================
// StabilityMetrics
// ============================================================================

void StabilityMetrics::AddObservation(const PriceObservation& obs) {
    observations_.push_back(obs);
    
    // Limit size
    while (observations_.size() > MAX_OBSERVATIONS) {
        observations_.pop_front();
    }
}

double StabilityMetrics::CalculateVolatility(size_t windowSize) const {
    if (observations_.size() < 2) {
        return 0.0;
    }
    
    size_t count = std::min(windowSize, observations_.size());
    auto startIt = observations_.end() - count;
    
    // Calculate returns
    std::vector<double> returns;
    returns.reserve(count - 1);
    
    for (auto it = startIt + 1; it != observations_.end(); ++it) {
        auto prev = std::prev(it);
        if (prev->price > 0) {
            double ret = static_cast<double>(it->price - prev->price) / prev->price;
            returns.push_back(ret);
        }
    }
    
    if (returns.empty()) {
        return 0.0;
    }
    
    // Calculate standard deviation
    double mean = std::accumulate(returns.begin(), returns.end(), 0.0) / returns.size();
    double variance = 0.0;
    for (double r : returns) {
        variance += (r - mean) * (r - mean);
    }
    variance /= returns.size();
    
    return std::sqrt(variance);
}

double StabilityMetrics::CalculateMomentum() const {
    if (observations_.size() < 2) {
        return 0.0;
    }
    
    // Simple momentum: current price vs price N observations ago
    size_t lookback = std::min(size_t(24), observations_.size() - 1);
    auto oldIt = observations_.end() - lookback - 1;
    auto newIt = observations_.end() - 1;
    
    if (oldIt->price == 0) {
        return 0.0;
    }
    
    return static_cast<double>(newIt->price - oldIt->price) / oldIt->price;
}

int64_t StabilityMetrics::GetAverageDeviation() const {
    if (observations_.empty()) {
        return 0;
    }
    
    int64_t totalDeviation = 0;
    for (const auto& obs : observations_) {
        totalDeviation += obs.DeviationBps();
    }
    
    return totalDeviation / static_cast<int64_t>(observations_.size());
}

int64_t StabilityMetrics::GetMaxDeviation() const {
    int64_t maxDev = 0;
    
    for (const auto& obs : observations_) {
        int64_t absDeviation = std::abs(obs.DeviationBps());
        if (absDeviation > maxDev) {
            maxDev = absDeviation;
        }
    }
    
    return maxDev;
}

double StabilityMetrics::GetTimeInBand() const {
    if (observations_.empty()) {
        return 0.0;
    }
    
    size_t inBandCount = 0;
    for (const auto& obs : observations_) {
        if (obs.IsInBand()) {
            inBandCount++;
        }
    }
    
    return static_cast<double>(inBandCount) / observations_.size() * 100.0;
}

void StabilityMetrics::Prune(PriceTimestamp cutoff) {
    while (!observations_.empty() && observations_.front().timestamp < cutoff) {
        observations_.pop_front();
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string MillicentsToString(PriceMillicents price) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(5);
    ss << "$" << (static_cast<double>(price) / 100000.0);
    return ss.str();
}

std::optional<PriceMillicents> ParsePrice(const std::string& str) {
    std::string s = str;
    
    // Remove $ prefix if present
    if (!s.empty() && s[0] == '$') {
        s = s.substr(1);
    }
    
    try {
        double price = std::stod(s);
        return static_cast<PriceMillicents>(price * 100000.0);
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace economics
} // namespace shurium
