/*
 * Unit tests for the external-divider battery sense helper.
 *
 * These tests exercise the pure-C++ math in
 * src/helpers/ExternalBatterySense.cpp. The helper is intentionally
 * host-compilable (no Arduino types) so the test_build_src environment
 * in the top-level platformio.ini can link it directly.
 */
#include <gtest/gtest.h>

extern "C" {
  #include "helpers/ExternalBatterySense.h"
}

/*
 * Sanity check the formula with the hardcoded 2:1 divider against a
 * 3.0V nRF52 SAADC reference.
 */
TEST(ExternalBatterySense, ZeroInputReturnsZeroMillivolts) {
  EXPECT_EQ(rakExternalBatteryMilliVolts(0), 0);
}

TEST(ExternalBatterySense, ThreeVoltBattery) {
  // 3.0V battery with 2:1 divider, 12-bit ADC at 3.0V reference.
  // raw = 3.0V / 2 / 3.0V * 4096 = 2048
  // Vbattery_mV = 2048 * 3000 * 2 * 1000 / (4096 * 1000) = 3000
  EXPECT_NEAR(rakExternalBatteryMilliVolts(2048), 3000, 2);
}

TEST(ExternalBatterySense, ThreePointThreeVoltBattery) {
  // 3.3V battery -> raw = 3.3 / 2 / 3.0 * 4096 = 2252
  uint16_t raw = (uint16_t)(3300 * 4096 / 3000 / 2);
  EXPECT_NEAR(rakExternalBatteryMilliVolts(raw), 3300, 2);
}

TEST(ExternalBatterySense, ThreePointSevenVoltBattery) {
  uint16_t raw = (uint16_t)(3700 * 4096 / 3000 / 2);
  EXPECT_NEAR(rakExternalBatteryMilliVolts(raw), 3700, 2);
}

TEST(ExternalBatterySense, FourPointTwoVoltBattery) {
  uint16_t raw = (uint16_t)(4200 * 4096 / 3000 / 2);
  EXPECT_NEAR(rakExternalBatteryMilliVolts(raw), 4200, 2);
}

TEST(ExternalBatterySense, FullScaleClampedToMaxRepresentable) {
  // raw=4095 -> 4095 * 3000 * 2 * 1000 / (4096 * 1000) = ~5999 mV
  // (lithium pack at 4.2V would never produce this - it is the natural
  // saturation point of the divider+ADC combo)
  uint16_t result = rakExternalBatteryMilliVolts(4095);
  EXPECT_GE(result, 5998);
  EXPECT_LE(result, 6000);
}

TEST(ExternalBatterySense, NullSamplesReturnsZero) {
  EXPECT_EQ(rakExternalBatteryMilliVoltsAvg(NULL, 8), 0);
  uint16_t samples[8] = {2048,2048,2048,2048,2048,2048,2048,2048};
  EXPECT_EQ(rakExternalBatteryMilliVoltsAvg(samples, 0), 0);
}

TEST(ExternalBatterySense, AveragedSamplesMatchSingleSample) {
  uint16_t samples[8] = {2048, 2048, 2048, 2048, 2048, 2048, 2048, 2048};
  uint16_t result = rakExternalBatteryMilliVoltsAvg(samples, 8);
  EXPECT_NEAR(result, 3000, 2);
}

TEST(ExternalBatterySense, AveragedSamplesWithNoise) {
  // Slightly noisy samples around 2048 should still average close to 3.0V
  uint16_t samples[8] = {2046, 2047, 2048, 2049, 2050, 2049, 2048, 2047};
  uint16_t result = rakExternalBatteryMilliVoltsAvg(samples, 8);
  EXPECT_NEAR(result, 3000, 5);
}

// ---------------------------------------------------------------------------
// Runtime calibration override tests (CommonCLI `set ext.batt.cal`).
// ---------------------------------------------------------------------------

TEST(ExternalBatterySenseRuntime, ResetToDefaultsUsesBuildTimeDefaults) {
  rakExternalBatterySetCalibration(0);
  EXPECT_NEAR(rakExternalBatteryMilliVolts(2048), 3000, 2);
}

TEST(ExternalBatterySenseRuntime, CalOverride1050_BumpsThreeVoltTo3150) {
  rakExternalBatterySetCalibration(1050);
  EXPECT_NEAR(rakExternalBatteryMilliVolts(2048), 3150, 2);
}

TEST(ExternalBatterySenseRuntime, CalOverride950_DropsThreeVoltTo2850) {
  rakExternalBatterySetCalibration(950);
  EXPECT_NEAR(rakExternalBatteryMilliVolts(2048), 2850, 2);
}

TEST(ExternalBatterySenseRuntime, ZeroCalibrationResetsToDefault) {
  // 0 is the "use compile-time default" sentinel: after set(1000) then
  // set(0), the override is cleared and the math falls back to the
  // build-time RAK_EXTERNAL_BATTERY_CALIBRATION_Q (1000 = unity).
  rakExternalBatterySetCalibration(1000);
  rakExternalBatterySetCalibration(0);
  EXPECT_EQ(rakExternalBatteryGetCalibration(), 0);
  EXPECT_NEAR(rakExternalBatteryMilliVolts(2048), 3000, 2);
}

TEST(ExternalBatterySenseRuntime, GetterReportsZeroAfterReset) {
  // After a hard reset (no `set` calls) the global store starts zeroed
  // and the getter reports zero (so the CLI can show
  // "compile-time default" upstream).
  rakExternalBatterySetCalibration(0);
  EXPECT_EQ(rakExternalBatteryGetCalibration(), 0);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
