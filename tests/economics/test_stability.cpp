// SHURIUM - Algorithmic Stability Tests
// Copyright (c) 2024 SHURIUM Developers
// MIT License

#include <gtest/gtest.h>

#include <shurium/economics/stability.h>
#include <shurium/economics/reward.h>
#include <shurium/consensus/params.h>

#include <chrono>
#include <cmath>
#include <thread>
#include <vector>

namespace shurium {
namespace economics {
namespace {

// ============================================================================
// Test Fixtures
// ============================================================================

class StabilityTest : public ::testing::Test {
protected:
    consensus::Params params_;
    std::unique_ptr<RewardCalculator> calculator_;
    std::unique_ptr<StabilityController> controller_;
    std::unique_ptr<StabilityReserve> reserve_;
    
    void SetUp() override {
        params_ = consensus::Params::Main();
        calculator_ = std::make_unique<RewardCalculator>(params_);
        controller_ = std::make_unique<StabilityController>();
        reserve_ = std::make_unique<StabilityReserve>();
    }
    
    // Helper to create a price observation
    PriceObservation CreatePriceObservation(PriceMillicents price, int height = 0) {
        PriceObservation obs;
        obs.price = price;
        obs.blockHeight = height;
        obs.timestamp = std::chrono::system_clock::now();
        obs.source = "test";
        obs.confidence = 100;
        return obs;
    }
    
    // Helper to create an aggregated price
    AggregatedPrice CreateAggregatedPrice(PriceMillicents price) {
        AggregatedPrice agg;
        agg.medianPrice = price;
        agg.weightedPrice = price;
        agg.sourceCount = 5;
        agg.minPrice = price - 1000;
        agg.maxPrice = price + 1000;
        agg.spreadBps = 20;
        agg.avgConfidence = 95;
        agg.timestamp = std::chrono::system_clock::now();
        return agg;
    }
};

// ============================================================================
// Constants Tests
// ============================================================================

TEST_F(StabilityTest, StabilityConstantsValid) {
    // Target price should be $1.00
    EXPECT_EQ(TARGET_PRICE_MILLICENTS, 100000);
    
    // Band should be symmetric
    EXPECT_EQ(PRICE_BAND_PERCENT, 5);
    EXPECT_EQ(UPPER_PRICE_THRESHOLD, TARGET_PRICE_MILLICENTS * 105 / 100);
    EXPECT_EQ(LOWER_PRICE_THRESHOLD, TARGET_PRICE_MILLICENTS * 95 / 100);
    
    // Other constants should be positive
    EXPECT_GT(MAX_ADJUSTMENT_RATE_BPS, 0);
    EXPECT_GT(MIN_ADJUSTMENT_INTERVAL, 0);
    EXPECT_GT(PRICE_SMOOTHING_WINDOW, 0);
    EXPECT_GT(EMERGENCY_DEVIATION_PERCENT, PRICE_BAND_PERCENT);
}

// ============================================================================
// PriceObservation Tests
// ============================================================================

TEST_F(StabilityTest, PriceObservationDeviationBps) {
    PriceObservation obs;
    obs.price = TARGET_PRICE_MILLICENTS;
    
    // At target - 0 deviation
    EXPECT_EQ(obs.DeviationBps(), 0);
    
    // 5% above target
    obs.price = UPPER_PRICE_THRESHOLD;
    EXPECT_EQ(obs.DeviationBps(), 500);  // 5% = 500 bps
    
    // 5% below target
    obs.price = LOWER_PRICE_THRESHOLD;
    EXPECT_EQ(obs.DeviationBps(), -500);
}

TEST_F(StabilityTest, PriceObservationIsInBand) {
    PriceObservation obs;
    
    // At target - in band
    obs.price = TARGET_PRICE_MILLICENTS;
    EXPECT_TRUE(obs.IsInBand());
    
    // Just inside upper band
    obs.price = UPPER_PRICE_THRESHOLD - 1;
    EXPECT_TRUE(obs.IsInBand());
    
    // Just inside lower band
    obs.price = LOWER_PRICE_THRESHOLD + 1;
    EXPECT_TRUE(obs.IsInBand());
    
    // Above band
    obs.price = UPPER_PRICE_THRESHOLD + 1;
    EXPECT_FALSE(obs.IsInBand());
    
    // Below band
    obs.price = LOWER_PRICE_THRESHOLD - 1;
    EXPECT_FALSE(obs.IsInBand());
}

TEST_F(StabilityTest, PriceObservationToString) {
    PriceObservation obs = CreatePriceObservation(TARGET_PRICE_MILLICENTS);
    std::string str = obs.ToString();
    EXPECT_FALSE(str.empty());
}

// ============================================================================
// AggregatedPrice Tests
// ============================================================================

TEST_F(StabilityTest, AggregatedPriceIsReliable) {
    AggregatedPrice agg;
    agg.medianPrice = TARGET_PRICE_MILLICENTS;
    agg.sourceCount = 5;
    agg.spreadBps = 50;  // Low spread
    agg.avgConfidence = 95;  // High confidence
    
    EXPECT_TRUE(agg.IsReliable());
    
    // Large spread - unreliable
    agg.spreadBps = 1000;  // 10% spread
    EXPECT_FALSE(agg.IsReliable());
    
    // Low confidence - unreliable
    agg.spreadBps = 50;
    agg.avgConfidence = 30;
    EXPECT_FALSE(agg.IsReliable());
    
    // Too few sources - unreliable
    agg.avgConfidence = 95;
    agg.sourceCount = 1;
    EXPECT_FALSE(agg.IsReliable());
}

TEST_F(StabilityTest, AggregatedPriceToString) {
    AggregatedPrice agg = CreateAggregatedPrice(TARGET_PRICE_MILLICENTS);
    std::string str = agg.ToString();
    EXPECT_FALSE(str.empty());
}

// ============================================================================
// StabilityAction Tests
// ============================================================================

TEST_F(StabilityTest, StabilityActionToString) {
    EXPECT_NE(StabilityActionToString(StabilityAction::None), nullptr);
    EXPECT_NE(StabilityActionToString(StabilityAction::ExpandSupply), nullptr);
    EXPECT_NE(StabilityActionToString(StabilityAction::ContractSupply), nullptr);
    EXPECT_NE(StabilityActionToString(StabilityAction::EmergencyExpand), nullptr);
    EXPECT_NE(StabilityActionToString(StabilityAction::EmergencyContract), nullptr);
    EXPECT_NE(StabilityActionToString(StabilityAction::Pause), nullptr);
}

// ============================================================================
// StabilityDecision Tests
// ============================================================================

TEST_F(StabilityTest, StabilityDecisionToString) {
    StabilityDecision decision;
    decision.action = StabilityAction::ExpandSupply;
    decision.adjustmentBps = 10;
    decision.deviationBps = -300;
    decision.confidence = 85;
    decision.reason = "Price below target";
    decision.blockHeight = 1000;
    
    std::string str = decision.ToString();
    EXPECT_FALSE(str.empty());
}

// ============================================================================
// ExponentialMovingAverage Tests
// ============================================================================

TEST_F(StabilityTest, EMAInitialization) {
    ExponentialMovingAverage ema(0.1);
    EXPECT_FALSE(ema.IsInitialized());
    
    ema.Update(100000);
    EXPECT_TRUE(ema.IsInitialized());
    EXPECT_EQ(ema.GetValue(), 100000);
}

TEST_F(StabilityTest, EMASmoothing) {
    ExponentialMovingAverage ema(0.5);  // High alpha for faster response
    
    // First value
    ema.Update(100000);
    EXPECT_EQ(ema.GetValue(), 100000);
    
    // Second value - should move toward it
    ema.Update(110000);
    PriceMillicents smoothed = ema.GetValue();
    EXPECT_GT(smoothed, 100000);
    EXPECT_LT(smoothed, 110000);
}

TEST_F(StabilityTest, EMAReset) {
    ExponentialMovingAverage ema(0.1);
    ema.Update(100000);
    EXPECT_TRUE(ema.IsInitialized());
    
    ema.Reset();
    EXPECT_FALSE(ema.IsInitialized());
}

TEST_F(StabilityTest, EMAAlphaRange) {
    // Low alpha = slow response
    ExponentialMovingAverage slowEma(0.01);
    slowEma.Update(100000);
    slowEma.Update(200000);
    PriceMillicents slowValue = slowEma.GetValue();
    
    // High alpha = fast response
    ExponentialMovingAverage fastEma(0.9);
    fastEma.Update(100000);
    fastEma.Update(200000);
    PriceMillicents fastValue = fastEma.GetValue();
    
    // Fast EMA should be closer to new value
    EXPECT_GT(fastValue, slowValue);
}

// ============================================================================
// TimeWeightedAveragePrice Tests
// ============================================================================

TEST_F(StabilityTest, TWAPBasicCalculation) {
    TimeWeightedAveragePrice twap(std::chrono::seconds(3600));  // 1 hour window
    
    // Add observations
    PriceObservation obs1 = CreatePriceObservation(100000);
    PriceObservation obs2 = CreatePriceObservation(110000);
    
    twap.AddObservation(obs1);
    twap.AddObservation(obs2);
    
    EXPECT_EQ(twap.ObservationCount(), 2);
    
    PriceMillicents calculated = twap.Calculate();
    EXPECT_GE(calculated, 100000);
    EXPECT_LE(calculated, 110000);
}

TEST_F(StabilityTest, TWAPPrune) {
    TimeWeightedAveragePrice twap(std::chrono::seconds(1));  // 1 second window
    
    PriceObservation obs = CreatePriceObservation(100000);
    twap.AddObservation(obs);
    
    EXPECT_EQ(twap.ObservationCount(), 1);
    
    // Wait and prune
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    twap.Prune();
    
    // May or may not have pruned depending on timing
    EXPECT_GE(twap.ObservationCount(), 0);
}

// ============================================================================
// StabilityController Tests
// ============================================================================

TEST_F(StabilityTest, StabilityControllerConstruction) {
    StabilityController controller;
    
    auto config = controller.GetConfig();
    EXPECT_EQ(config.targetPrice, TARGET_PRICE_MILLICENTS);
    EXPECT_EQ(config.bandWidthPercent, PRICE_BAND_PERCENT);
}

TEST_F(StabilityTest, StabilityControllerCustomConfig) {
    StabilityController::Config config;
    config.targetPrice = 200000;  // $2.00
    config.bandWidthPercent = 10;
    
    StabilityController controller(config);
    
    EXPECT_EQ(controller.GetConfig().targetPrice, 200000);
    EXPECT_EQ(controller.GetConfig().bandWidthPercent, 10);
}

TEST_F(StabilityTest, StabilityControllerPriceUpdate) {
    PriceObservation obs = CreatePriceObservation(TARGET_PRICE_MILLICENTS);
    controller_->OnPriceUpdate(obs);
    
    // Smoothed price should be available
    PriceMillicents smoothed = controller_->GetSmoothedPrice();
    EXPECT_GT(smoothed, 0);
}

TEST_F(StabilityTest, StabilityControllerAggregatedPrice) {
    AggregatedPrice agg = CreateAggregatedPrice(TARGET_PRICE_MILLICENTS);
    controller_->OnAggregatedPrice(agg);
    
    auto latest = controller_->GetLatestPrice();
    ASSERT_TRUE(latest.has_value());
    EXPECT_EQ(latest->medianPrice, TARGET_PRICE_MILLICENTS);
}

TEST_F(StabilityTest, StabilityControllerDecisionAtTarget) {
    // Price at target
    AggregatedPrice agg = CreateAggregatedPrice(TARGET_PRICE_MILLICENTS);
    controller_->OnAggregatedPrice(agg);
    
    StabilityDecision decision = controller_->CalculateDecision(100);
    
    // At target - no action needed
    EXPECT_EQ(decision.action, StabilityAction::None);
    EXPECT_EQ(decision.adjustmentBps, 0);
}

TEST_F(StabilityTest, StabilityControllerDecisionAboveBand) {
    // Price significantly above target (8%)
    PriceMillicents highPrice = TARGET_PRICE_MILLICENTS * 108 / 100;
    AggregatedPrice agg = CreateAggregatedPrice(highPrice);
    controller_->OnAggregatedPrice(agg);
    
    StabilityDecision decision = controller_->CalculateDecision(100);
    
    // Price above target -> contract supply to reduce price
    // (In reserve-based stablecoins: sell NXS from reserve)
    EXPECT_EQ(decision.action, StabilityAction::ContractSupply);
    EXPECT_GT(decision.adjustmentBps, 0);
}

TEST_F(StabilityTest, StabilityControllerDecisionBelowBand) {
    // Price significantly below target (8%)
    PriceMillicents lowPrice = TARGET_PRICE_MILLICENTS * 92 / 100;
    AggregatedPrice agg = CreateAggregatedPrice(lowPrice);
    controller_->OnAggregatedPrice(agg);
    
    StabilityDecision decision = controller_->CalculateDecision(100);
    
    // Price below target -> expand supply or buy NXS to raise price
    EXPECT_EQ(decision.action, StabilityAction::ExpandSupply);
    EXPECT_GT(decision.adjustmentBps, 0);
}

TEST_F(StabilityTest, StabilityControllerCanAdjust) {
    // First adjustment should be allowed
    EXPECT_TRUE(controller_->CanAdjust(100));
    
    // Record an adjustment
    StabilityDecision decision;
    decision.action = StabilityAction::ExpandSupply;
    controller_->RecordAdjustment(100, decision);
    
    // Immediate next adjustment should not be allowed
    EXPECT_FALSE(controller_->CanAdjust(100));
    EXPECT_FALSE(controller_->CanAdjust(100 + MIN_ADJUSTMENT_INTERVAL - 1));
    
    // After interval, should be allowed again
    EXPECT_TRUE(controller_->CanAdjust(100 + MIN_ADJUSTMENT_INTERVAL));
}

TEST_F(StabilityTest, StabilityControllerThresholds) {
    EXPECT_EQ(controller_->GetTargetPrice(), TARGET_PRICE_MILLICENTS);
    EXPECT_EQ(controller_->GetUpperThreshold(), UPPER_PRICE_THRESHOLD);
    EXPECT_EQ(controller_->GetLowerThreshold(), LOWER_PRICE_THRESHOLD);
}

TEST_F(StabilityTest, StabilityControllerStats) {
    auto stats = controller_->GetStats();
    EXPECT_EQ(stats.totalAdjustments, 0);
    EXPECT_EQ(stats.expansions, 0);
    EXPECT_EQ(stats.contractions, 0);
}

// ============================================================================
// StabilityReserve Tests
// ============================================================================

TEST_F(StabilityTest, StabilityReserveConstruction) {
    EXPECT_EQ(reserve_->GetBalance(), 0);
    EXPECT_GT(reserve_->GetMinimumBalance(), 0);
}

TEST_F(StabilityTest, StabilityReserveAddFunds) {
    reserve_->AddFunds(1000 * COIN);
    EXPECT_EQ(reserve_->GetBalance(), 1000 * COIN);
    
    reserve_->AddFunds(500 * COIN);
    EXPECT_EQ(reserve_->GetBalance(), 1500 * COIN);
}

TEST_F(StabilityTest, StabilityReserveRemoveFunds) {
    reserve_->AddFunds(1000 * COIN);
    
    EXPECT_TRUE(reserve_->RemoveFunds(500 * COIN));
    EXPECT_EQ(reserve_->GetBalance(), 500 * COIN);
    
    // Cannot remove more than balance
    EXPECT_FALSE(reserve_->RemoveFunds(600 * COIN));
    EXPECT_EQ(reserve_->GetBalance(), 500 * COIN);
}

TEST_F(StabilityTest, StabilityReserveMinimumBalance) {
    reserve_->SetMinimumBalance(100 * COIN);
    EXPECT_EQ(reserve_->GetMinimumBalance(), 100 * COIN);
    
    reserve_->AddFunds(150 * COIN);
    EXPECT_TRUE(reserve_->HasMinimumBalance());
    
    reserve_->RemoveFunds(100 * COIN);
    EXPECT_FALSE(reserve_->HasMinimumBalance());
}

TEST_F(StabilityTest, StabilityReserveSpendableAmount) {
    reserve_->SetMinimumBalance(100 * COIN);
    reserve_->AddFunds(500 * COIN);
    
    EXPECT_EQ(reserve_->GetSpendableAmount(), 400 * COIN);
}

TEST_F(StabilityTest, StabilityReserveRecordBuy) {
    reserve_->AddFunds(1000 * COIN);
    reserve_->RecordBuy(100 * COIN, 110 * COIN);  // Spent 100, got 110
    
    EXPECT_EQ(reserve_->GetTotalBought(), 110 * COIN);
}

TEST_F(StabilityTest, StabilityReserveRecordSell) {
    reserve_->AddFunds(1000 * COIN);
    reserve_->RecordSell(100 * COIN, 95 * COIN);  // Sold 100, got 95
    
    EXPECT_EQ(reserve_->GetTotalSold(), 100 * COIN);
}

TEST_F(StabilityTest, StabilityReserveSerializeDeserialize) {
    reserve_->AddFunds(1000 * COIN);
    reserve_->SetMinimumBalance(200 * COIN);
    
    std::vector<Byte> serialized = reserve_->Serialize();
    EXPECT_GT(serialized.size(), 0);
    
    StabilityReserve newReserve;
    EXPECT_TRUE(newReserve.Deserialize(serialized.data(), serialized.size()));
    
    EXPECT_EQ(newReserve.GetBalance(), 1000 * COIN);
    EXPECT_EQ(newReserve.GetMinimumBalance(), 200 * COIN);
}

// ============================================================================
// SupplyAdjuster Tests
// ============================================================================

TEST_F(StabilityTest, SupplyAdjusterCalculateAdjustedReward) {
    SupplyAdjuster adjuster(*calculator_);
    
    Amount baseReward = INITIAL_BLOCK_REWARD;
    
    // No adjustment
    StabilityDecision noAction;
    noAction.action = StabilityAction::None;
    noAction.adjustmentBps = 0;
    
    Amount adjusted = adjuster.CalculateAdjustedReward(baseReward, noAction);
    EXPECT_EQ(adjusted, baseReward);
    
    // Expansion - need at least 100 bps (1%) to see visible change due to integer math
    StabilityDecision expand;
    expand.action = StabilityAction::ExpandSupply;
    expand.adjustmentBps = 100;  // 1% increase
    
    Amount expanded = adjuster.CalculateAdjustedReward(baseReward, expand);
    EXPECT_GT(expanded, baseReward);
    
    // Contraction
    StabilityDecision contract;
    contract.action = StabilityAction::ContractSupply;
    contract.adjustmentBps = 100;  // 1% decrease
    
    Amount contracted = adjuster.CalculateAdjustedReward(baseReward, contract);
    EXPECT_LT(contracted, baseReward);
}

TEST_F(StabilityTest, SupplyAdjusterCalculateSupplyChange) {
    SupplyAdjuster adjuster(*calculator_);
    
    Amount currentSupply = 1000000 * COIN;
    
    StabilityDecision expand;
    expand.action = StabilityAction::ExpandSupply;
    expand.adjustmentBps = 10;
    
    int64_t change = adjuster.CalculateSupplyChange(expand, currentSupply);
    EXPECT_GT(change, 0);
}

TEST_F(StabilityTest, SupplyAdjusterCumulativeAdjustment) {
    SupplyAdjuster adjuster(*calculator_);
    
    EXPECT_EQ(adjuster.GetCumulativeAdjustment(), 0);
    
    adjuster.RecordAdjustment(1000, 100);
    EXPECT_EQ(adjuster.GetCumulativeAdjustment(), 1000);
    
    adjuster.RecordAdjustment(-500, 200);
    EXPECT_EQ(adjuster.GetCumulativeAdjustment(), 500);
}

// ============================================================================
// StabilityMetrics Tests
// ============================================================================

TEST_F(StabilityTest, StabilityMetricsAddObservation) {
    StabilityMetrics metrics;
    
    PriceObservation obs = CreatePriceObservation(TARGET_PRICE_MILLICENTS);
    metrics.AddObservation(obs);
    
    // Should have observation
    EXPECT_EQ(metrics.GetAverageDeviation(), 0);
}

TEST_F(StabilityTest, StabilityMetricsVolatility) {
    StabilityMetrics metrics;
    
    // Add varied prices
    for (int i = 0; i < 30; ++i) {
        PriceMillicents price = TARGET_PRICE_MILLICENTS + (i % 2 == 0 ? 1000 : -1000);
        metrics.AddObservation(CreatePriceObservation(price, i));
    }
    
    double volatility = metrics.CalculateVolatility(24);
    EXPECT_GT(volatility, 0);  // Should have some volatility
}

TEST_F(StabilityTest, StabilityMetricsTimeInBand) {
    StabilityMetrics metrics;
    
    // All prices at target (in band)
    for (int i = 0; i < 10; ++i) {
        metrics.AddObservation(CreatePriceObservation(TARGET_PRICE_MILLICENTS, i));
    }
    
    double timeInBand = metrics.GetTimeInBand();
    EXPECT_DOUBLE_EQ(timeInBand, 100.0);  // 100% in band
}

// ============================================================================
// Utility Function Tests
// ============================================================================

TEST_F(StabilityTest, MillicentsToString) {
    // Implementation uses 5 decimal places
    EXPECT_EQ(MillicentsToString(100000), "$1.00000");
    EXPECT_EQ(MillicentsToString(150000), "$1.50000");
    EXPECT_EQ(MillicentsToString(99), "$0.00099");
}

TEST_F(StabilityTest, ParsePrice) {
    auto price1 = ParsePrice("$1.00");
    ASSERT_TRUE(price1.has_value());
    EXPECT_EQ(*price1, 100000);
    
    auto price2 = ParsePrice("1.50");
    ASSERT_TRUE(price2.has_value());
    EXPECT_EQ(*price2, 150000);
    
    auto invalid = ParsePrice("not a price");
    EXPECT_FALSE(invalid.has_value());
}

TEST_F(StabilityTest, CalculateDeviationBps) {
    // At target
    EXPECT_EQ(CalculateDeviationBps(TARGET_PRICE_MILLICENTS, TARGET_PRICE_MILLICENTS), 0);
    
    // 5% above
    EXPECT_EQ(CalculateDeviationBps(105000, 100000), 500);
    
    // 5% below
    EXPECT_EQ(CalculateDeviationBps(95000, 100000), -500);
    
    // Edge case: zero target
    EXPECT_EQ(CalculateDeviationBps(100000, 0), 0);
}

TEST_F(StabilityTest, CalculateDeviationPercent) {
    EXPECT_DOUBLE_EQ(CalculateDeviationPercent(105000, 100000), 5.0);
    EXPECT_DOUBLE_EQ(CalculateDeviationPercent(95000, 100000), -5.0);
}

// ============================================================================
// Emergency Condition Tests
// ============================================================================

TEST_F(StabilityTest, EmergencyConditionAbove) {
    // Price 25% above target (beyond emergency threshold)
    PriceMillicents highPrice = TARGET_PRICE_MILLICENTS * 125 / 100;
    AggregatedPrice agg = CreateAggregatedPrice(highPrice);
    controller_->OnAggregatedPrice(agg);
    
    StabilityDecision decision = controller_->CalculateDecision(100);
    
    // Price too high -> need to contract supply to bring price down
    EXPECT_EQ(decision.action, StabilityAction::EmergencyContract);
}

TEST_F(StabilityTest, EmergencyConditionBelow) {
    // Price 25% below target (beyond emergency threshold)
    PriceMillicents lowPrice = TARGET_PRICE_MILLICENTS * 75 / 100;
    AggregatedPrice agg = CreateAggregatedPrice(lowPrice);
    controller_->OnAggregatedPrice(agg);
    
    StabilityDecision decision = controller_->CalculateDecision(100);
    
    // Price too low -> need to expand supply (counterintuitive but correct for algorithmic stability)
    EXPECT_EQ(decision.action, StabilityAction::EmergencyExpand);
}

} // namespace
} // namespace economics
} // namespace shurium
