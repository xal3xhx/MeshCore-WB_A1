# RAK WB_A1 Hardware Test Procedure

This document describes a manual bench procedure for verifying a
`RAK_3401_*_wb_a1` or `RAK_4631_*_wb_a1` build on real hardware.

If a piece of equipment in this document is not available, mark that
step as `SKIPPED` and note why. The fork cannot perform hardware-in-the-loop
validation in this environment.

## 1. Equipment

- One RAK3401 *or* RAK4631 WisBlock core + base board.
- One USB-C cable for power and flashing.
- One current-limited bench DC power supply (>= 5 V, <= 1 A).
- One calibrated multimeter (>= 0.5 % DC accuracy).
- Two equal-value resistors for the 2:1 divider (100 kOhm recommended,
  1 % or better, from the same batch so they match closely).
- One 100 nF ceramic capacitor (optional) for `C_dec`.
- Hookup wire, breadboard or solder fixtures.
- A second MeshCore-compatible node (any) for radio-loopback testing.

## 2. Wiring (preliminary checks)

1. With the bench supply OFF and the USB cable DISCONNECTED, wire the
   divider as described in `docs/rak-wb-a1-wiring.md` section 4.
2. Set the bench supply to 0 V, current limit 100 mA, output OFF.
3. Connect the bench supply positive lead to the battery-side of the
   divider.
4. Connect bench supply negative lead to the WisBlock ground.
5. Set the multimeter to DC volts and connect it to the `WB_A1` pin
   (or to the divider mid-point).
6. Power on the bench supply. Increase the output to 3.00 V.
7. Confirm the multimeter reads 1.500 V (within the resistor
   tolerance, e.g. 1.485 V to 1.515 V for 1 % resistors).
8. Repeat at 3.30 V (expect 1.650 V) and 4.20 V (expect 2.100 V).
9. Power the bench supply OFF before continuing.

## 3. Flash the custom firmware

1. Double-tap the RAK reset button to enter the UF2 bootloader. A
   USB mass-storage device named `RAK3401` or `RAK4631` should appear.
2. Copy the `RAK_3401_repeater_wb_a1-<version>.uf2` (or `_4631_`)
   file from the CI artifacts to the bootloader drive. The board
   reboots into the new firmware.
3. Open a serial console at 115200 baud on the USB CDC port.
4. Confirm the firmware banner identifies the build as
   `RAK_3401_repeater_wb_a1-<version>` (or `RAK_4631_...`).

## 4. Telemetry verification

For each test voltage below:

1. Set the bench supply to the indicated voltage.
2. Wait 5 seconds for the divider to settle.
3. Read the multimeter to capture the actual `V_meas_mV`.
4. Read the firmware's reported voltage. The easiest path is to
   enable MESH_DEBUG=1 in the build and read the
   `get pwrmgt.bootmv` CLI value, or to look at the `telemetry`
   packet on a serial monitor.
5. Record `V_report_mV`, the deviation
   `delta = V_report_mV - V_meas_mV`, and the
   percent error `100 * delta / V_meas_mV`.

| Step | Bench supply | Expected `V_adc` | Allowed `|delta|` |
| --- | --- | --- | --- |
| 1 | 3.000 V | 1.500 V | <= 30 mV (1 %) |
| 2 | 3.300 V | 1.650 V | <= 33 mV (1 %) |
| 3 | 3.700 V | 1.850 V | <= 37 mV (1 %) |
| 4 | 4.000 V | 2.000 V | <= 40 mV (1 %) |
| 5 | 4.200 V | 2.100 V | <= 42 mV (1 %) |

If the error is outside the allowed range, recompute the calibration
Q-format value as described in `docs/rak-wb-a1-wiring.md` section 5
and push it to the running firmware via the serial console
(`set ext.batt.cal <new_q>`). No rebuild is required — the value
persists to `/com_prefs` and survives reboot.

## 5. ADC input range safety

1. With the bench supply OFF, configure the multimeter to monitor the
   `WB_A1` pin in DC volts mode.
2. Set the bench supply to 4.200 V. Confirm the multimeter reads
   2.100 V. This must NEVER exceed 3.000 V (the nRF52 SAADC
   reference). If it does, stop the test and re-check the divider
   values.
3. Briefly set the bench supply to 4.300 V (above the maximum
   expected cell voltage). Confirm the multimeter reads 2.150 V.
   This is still inside the ADC range. If your resistor tolerance is
   worse than 1 %, the reading may rise above 2.2 V - that is still
   safe.

## 6. Radio / PA sanity

1. From the companion app or web flasher, send a test message to a
   second MeshCore node.
2. Confirm the message is acknowledged.
3. Repeat with the bench supply at 3.0 V and again at 4.2 V. The
   radio should continue to function identically to a standard RAK
   build.

## 7. Reboot / power cycle

1. Power the bench supply OFF.
2. Wait 10 seconds.
3. Power ON. Confirm the board boots into the WB_A1 firmware
   and reports a battery voltage close to the bench supply voltage.
4. Disconnect the USB cable. Power the bench supply OFF.
5. Wait 10 seconds.
6. Power the bench supply ON. Confirm the board boots again.
7. (Optional) If `AUTO_SHUTDOWN_MILLIVOLTS` is enabled in the
   companion UI build, lower the bench supply to 3.300 V and confirm
   the auto-shutdown does not trigger (since the divider reports
   ~3.300 V too).

## 8. Reporting

Record all measured values in a CSV or markdown table and attach it
to the fork's pull request. A template:

```csv
board,bench_v,multimeter_vadc,firmware_v,delta_mv,pct_error
RAK_3401,3.000,1.498,2.992,-8,-0.27
RAK_3401,3.300,1.652,3.296,-4,-0.12
...
```

## 9. Known limitations

- The `WB_A0` on-board sense input still drives
  `PWRMGT_VOLTAGE_BOOTLOCK` and LPCOMP wake. With a regulated 5 V
  supply, `WB_A0` may be at a fixed ~3.0 V to ~4.2 V (depending on
  the regulator topology), which can trigger the boot lockout
  unexpectedly. This is upstream behaviour and is not changed by
  this fork.
- The `PIN_USER_BTN_ANA = 31` analog button on the companion UI is
  now reading the divider mid-point instead of the joystick button.
  This is a hardware-level conflict; if you need the analog button
  on a companion build, use a different analog input or a different
  physical button.
