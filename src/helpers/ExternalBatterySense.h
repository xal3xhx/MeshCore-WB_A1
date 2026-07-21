/*
 * External voltage-divider battery sense helper.
 *
 * Used by RAK WisBlock nRF52 boards (RAK3401, RAK4631) when
 * RAK_EXTERNAL_BATTERY_SENSE=1 is set. The WisBlock WB_A1 analog input
 * is connected to the mid-point of a user-supplied 2:1 resistive
 * divider - two EQUAL-value resistors - that halves the raw battery
 * voltage into the nRF52 ADC input range.
 *
 * The divider ratio is fixed at 2:1 by design. Any equal-value pair
 * divides by exactly 2 (Vout = Vin * R/(R+R) = Vin/2), so the resistor
 * VALUE is not a firmware concern - it only affects the divider's
 * passive current draw and source impedance. The one tunable is a
 * calibration factor (Q-format, 1000 = 1.000) that trims out resistor
 * tolerance and ADC reference error. It has a compile-time seed
 * (RAK_EXTERNAL_BATTERY_CALIBRATION_Q) and a runtime override driven
 * by the CommonCLI `set ext.batt.cal` command, persisted to /com_prefs
 * so it survives reboot without a rebuild.
 *
 * The math runs entirely in integer arithmetic so it stays
 * deterministic and avoids pulling in the FPU on Cortex-M4.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Convert one 12-bit ADC reading (0..ADC_FULL_SCALE-1) to the
 * estimated battery millivolts using the fixed 2:1 divider and the
 * effective calibration factor.
 *
 * Returns battery millivolts, clamped to 0..65535.
 */
uint16_t rakExternalBatteryMilliVolts(uint16_t raw_adc);

/*
 * Average a series of raw ADC readings and convert them to battery
 * millivolts. Returns 0 if `count` is zero.
 */
uint16_t rakExternalBatteryMilliVoltsAvg(const uint16_t* raw_samples, size_t count);

/*
 * Set the runtime calibration override (Q-format, 1000 = 1.000).
 * Passing 0 means "use the compile-time default" (i.e. reset).
 */
void rakExternalBatterySetCalibration(uint16_t calibration_q);

/*
 * Read the currently-stored runtime override. 0 means no override is
 * active (the compile-time default is in effect).
 */
uint16_t rakExternalBatteryGetCalibration(void);

#ifdef __cplusplus
}
#endif
