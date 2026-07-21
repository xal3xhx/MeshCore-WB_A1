/*
 * External voltage-divider battery sense helper - implementation.
 *
 * See ExternalBatterySense.h for the contract.
 *
 * Build-time configuration (set by PlatformIO build flags):
 *   RAK_EXTERNAL_BATTERY_VREF_MV         ADC reference in millivolts
 *                                        (default 3000 - matches nRF52
 *                                        AR_INTERNAL_3_0; the board code
 *                                        MUST call
 *                                        analogReference(AR_INTERNAL_3_0)
 *                                        or the core's 3.6 V power-on
 *                                        default skews every reading)
 *   RAK_EXTERNAL_BATTERY_ADC_FULL_SCALE  ADC full-scale count
 *                                        (default 4096 for 12-bit)
 *   RAK_EXTERNAL_BATTERY_CALIBRATION_Q   Calibration factor in Q-format
 *                                        where 1000 == 1.000
 *                                        (default 1000 = unity)
 *
 * Runtime configuration (CommonCLI `set ext.batt.cal`):
 *   A calibration override is kept in process-local storage and takes
 *   effect when non-zero. The CLI writes through
 *   rakExternalBatterySetCalibration() and persists via /com_prefs so
 *   the override survives reboot.
 *
 * The divider is fixed at 2:1 (two equal-value resistors):
 *   Vadc_mV     = raw * Vref / ADC_FULL_SCALE
 *   Vbattery_mV = Vadc_mV * 2 * cal_q / 1000
 */
#include "ExternalBatterySense.h"

#ifndef RAK_EXTERNAL_BATTERY_VREF_MV
  #define RAK_EXTERNAL_BATTERY_VREF_MV 3000UL
#endif

#ifndef RAK_EXTERNAL_BATTERY_ADC_FULL_SCALE
  #define RAK_EXTERNAL_BATTERY_ADC_FULL_SCALE 4096UL
#endif

#ifndef RAK_EXTERNAL_BATTERY_CALIBRATION_Q
  #define RAK_EXTERNAL_BATTERY_CALIBRATION_Q 1000
#endif

/* Fixed 2:1 divider: two equal resistors halve the battery voltage,
 * so the reconstruction multiplies the ADC-node voltage by 2. */
#define RAK_EXTERNAL_BATTERY_DIVIDER_NUM 2UL

#if (RAK_EXTERNAL_BATTERY_ADC_FULL_SCALE == 0UL)
  #error "RAK_EXTERNAL_BATTERY_ADC_FULL_SCALE must be greater than zero"
#endif

#if (RAK_EXTERNAL_BATTERY_CALIBRATION_Q < 1)
  #error "RAK_EXTERNAL_BATTERY_CALIBRATION_Q must be positive (use 1000 for 1.000)"
#endif

/*
 * Runtime calibration override. 0 means "fall back to compile-time
 * default". Touched only from the main thread (CommonCLI command
 * handler called from the serial console task, before/after the
 * telemetry loop reads it via rakExternalBatteryMilliVolts).
 */
static volatile uint16_t g_runtime_calibration_q = 0;

void rakExternalBatterySetCalibration(uint16_t calibration_q) {
  g_runtime_calibration_q = calibration_q;
}

uint16_t rakExternalBatteryGetCalibration(void) {
  return g_runtime_calibration_q;
}

uint16_t rakExternalBatteryMilliVolts(uint16_t raw_adc) {
  uint32_t raw     = (uint32_t)raw_adc;
  uint32_t vref_mv = (uint32_t)RAK_EXTERNAL_BATTERY_VREF_MV;
  uint32_t adc_fs  = (uint32_t)RAK_EXTERNAL_BATTERY_ADC_FULL_SCALE;
  uint32_t cal_q   = (uint32_t)RAK_EXTERNAL_BATTERY_CALIBRATION_Q;

  /* Runtime override: 0 means "use compile-time default". */
  uint16_t rt_cal = g_runtime_calibration_q;
  if (rt_cal != 0U) {
    cal_q = (uint32_t)rt_cal;
  }

  /* Use uint64_t intermediate to avoid any overflow. Worst case
   * (raw=4095, vref=5000, x2, cal_q=65535) fits comfortably in 64 bits. */
  uint64_t numerator   = (uint64_t)raw * (uint64_t)vref_mv
                       * (uint64_t)RAK_EXTERNAL_BATTERY_DIVIDER_NUM
                       * (uint64_t)cal_q;
  uint64_t denominator = (uint64_t)adc_fs * 1000ULL;
  uint64_t result = numerator / denominator;

  if (result > 65535ULL) {
    return 65535U;
  }
  return (uint16_t)result;
}

uint16_t rakExternalBatteryMilliVoltsAvg(const uint16_t* raw_samples, size_t count) {
  if (raw_samples == NULL || count == 0) {
    return 0U;
  }

  /* Sum first into uint32_t (up to 4095 * 64 = ~262k - safe), then take
   * a 12-bit-rounded average before converting. A small rounding
   * constant (+ count/2) keeps the result unbiased. */
  uint32_t acc = 0;
  for (size_t i = 0; i < count; i++) {
    acc += (uint32_t)raw_samples[i];
  }
  uint32_t avg = (acc + (uint32_t)(count / 2U)) / (uint32_t)count;
  if (avg > 65535UL) {
    avg = 65535UL;
  }
  return rakExternalBatteryMilliVolts((uint16_t)avg);
}
