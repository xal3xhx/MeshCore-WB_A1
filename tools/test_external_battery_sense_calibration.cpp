// Standalone test for the external battery sense helper when configured
// with non-default calibration Q-format values. Compile with:
//
//   cl /std:c++17 /EHsc /DRAK_EXTERNAL_BATTERY_CALIBRATION_Q=1050 \
//      /I src /Fe:tools\test_calibration_above.exe \
//      tools\test_external_battery_sense_calibration.cpp \
//      src\helpers\ExternalBatterySense.cpp
//
//   cl /std:c++17 /EHsc /DRAK_EXTERNAL_BATTERY_CALIBRATION_Q=950  \
//      /I src /Fe:tools\test_calibration_below.exe \
//      tools\test_external_battery_sense_calibration.cpp \
//      src\helpers\ExternalBatterySense.cpp
#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
  #include "helpers/ExternalBatterySense.h"
}

#ifndef RAK_EXTERNAL_BATTERY_CALIBRATION_Q
  #define RAK_EXTERNAL_BATTERY_CALIBRATION_Q 1000
#endif

int main() {
  // raw=2048 -> 3.0V at 1.0 calibration
  // 1.05 calibration -> 3.15V ; 0.95 calibration -> 2.85V
  uint16_t raw = 2048;
  uint16_t result = rakExternalBatteryMilliVolts(raw);
  int diff_pct = ((int)result - 3000) * 100 / 3000;

  std::printf("Calibration Q=%d, raw=%u -> %u mV (diff %+d%%)\n",
              RAK_EXTERNAL_BATTERY_CALIBRATION_Q, raw, result, diff_pct);

  if (RAK_EXTERNAL_BATTERY_CALIBRATION_Q == 1050) {
    // +5 % expected
    if (result >= 3140 && result <= 3160) {
      std::printf("PASS calibration_above_1_0 (1.05x) -> %u mV\n", result);
      return 0;
    } else {
      std::printf("FAIL calibration_above_1_0 (1.05x) -> %u mV, expected 3140..3160\n", result);
      return 1;
    }
  } else if (RAK_EXTERNAL_BATTERY_CALIBRATION_Q == 950) {
    // -5 % expected
    if (result >= 2840 && result <= 2860) {
      std::printf("PASS calibration_below_1_0 (0.95x) -> %u mV\n", result);
      return 0;
    } else {
      std::printf("FAIL calibration_below_1_0 (0.95x) -> %u mV, expected 2840..2860\n", result);
      return 1;
    }
  } else {
    std::printf("FAIL: unknown calibration Q value %d (use 1050 or 950)\n",
                RAK_EXTERNAL_BATTERY_CALIBRATION_Q);
    return 1;
  }
}
