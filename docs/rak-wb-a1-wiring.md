# RAK WB_A1 External Battery Sense - Wiring and Calibration Guide

> **WARNING - read first.**
> A 1S lithium-ion cell at full charge delivers up to 4.2 V. Never
> connect a 1S Li-ion pack (or any voltage above 3.0 V) directly to a
> WisBlock analog input or any other MCU GPIO. The WB_A1 pin on the
> RAK4631 and RAK3401 is *not* 5 V tolerant. Always wire a correctly
> designed voltage divider, connect the pack ground and the WisBlock
> ground together, and verify the ADC pin voltage with a calibrated
> multimeter before connecting it to the RAK board.
>
> The pack described in this guide is a 1S3P (three parallel 18650
> cells) Li-ion pack, not 0S3P. A 0S pack has no cells and provides
> no energy.

## 1. Why this fork exists

A RAK WisBlock nRF52 board is normally powered from a regulated 3.3 V
buck-boost supply. The on-board `WB_A0` (pin 5) battery-sense input is
fed by an internal divider that assumes a single Li-ion cell directly
on `VBAT`. If you power the board from a regulated supply and connect
the raw battery only through that sense path, the reported voltage is
clipped to the supply rail and does not represent the actual cell
voltage.

This fork adds an opt-in firmware build that reads the raw battery
voltage from a user-supplied external divider wired to `WB_A1`
(IO_SLOT pin 31 = `AIN7` on the nRF52840 SAADC). The on-board `WB_A0`
sense path is preserved so the existing nRF52 power-management logic
(boot lockout, LPCOMP wake, SYSTEMOFF recovery, VBUS detection)
keeps behaving as upstream intended.

## 2. Supported boards and firmware roles

| Board | Architecture | `WB_A1` ADC pin | Roles with custom build |
| --- | --- | --- | --- |
| RAK3401 | nRF52840 | P0.31 (`AIN7`) | repeater, room_server, companion_radio_usb, companion_radio_ble, terminal_chat, sensor |
| RAK4631 | nRF52840 | P0.31 (`AIN7`) | repeater, room_server, companion_radio_usb, companion_radio_ble, terminal_chat, sensor |

Each role gets a `*_wb_a1` PlatformIO environment. There is no
`kiss_modem_wb_a1` environment because the kiss-modem role is a small
host-side serial bridge that does not maintain telemetry on its own
and does not benefit from this change.

## 3. Unsupported boards

| Board | Reason |
| --- | --- |
| RAK WisMesh Tag | No `WB_A1` analog input exposed in `variant.h`. Pin 31 is not declared as an ADC input on this module. |
| RAK11310 | RP2040 - no `WB_*` symbols, different MCU. |
| RAK3112 | ESP32-S3 - no `WB_*` symbols, different MCU. |
| RAK3x72 | STM32WLE5 - no `WB_*` symbols, different MCU. |

If a future RAK board exposes `WB_A1` and ships a `variant.h` that
declares it, add a new `*_wb_a1` environment by following the existing
patterns in `variants/rak3401/platformio.ini` and
`variants/rak4631/platformio.ini`, and append the environment to
`tools/wb_a1_build_matrix.txt`.

## 3.1 Recommended WisBlock Base board

The Core module is only half the story. The `WB_A1` signal (P0.31 / AIN7)
travels to **Core connector pin 22** (40-pin board-to-board header) which
is exposed by every modern WisBlock Base as the `AIN1` net. Whether you
can actually solder to it externally depends on which Base board you
have:

| Base board | External `AIN1` access | Recommended for this fork? | Notes |
| --- | --- | --- | --- |
| **RAK19007** (2nd gen, Type-C) | **J11 pin 1** = `AIN1`, J11 pin 4 = `VBAT`, J10 pin 2 = `GND` | **Yes (recommended)** | 2.54 mm pitch header; divider can be built entirely on the header with no Core-side soldering. |
| **RAK19003** (Mini Base) | No 2.54 mm header exposes `AIN1`. Pin 22 reaches the 40-pin Core connector only. | **no** | `AIN1` is not broken out; tapping the connector castellation on pin 22 is not supported here. Upgrade to a RAK19007 (or RAK19011). |
| **RAK19011** (3rd gen, Dual IO with Power slot) | **J19 / J20 headers** expose all Core-connector pins including pin 22 `AIN1`. Pin 21 is internally tied to battery via `ADC_VBAT` net — **do not** treat pin 21 as the divider top. | Yes | Pin numbers 21/22 are reused across sensor and IO slots; only the J19/J20 Core-header pins map to `AIN1`. |
| **RAK5005-O** (1st gen, Micro-USB) | J11 pin 1 = `AIN0` (= `WB_A0` / P0.05 / on-board VBAT divider). `AIN1` is **not** broken out to a header. | **No** | Wiring to J11 pin 1 will conflict with the on-board battery-sense divider. The RAK19007 datasheet explicitly notes: "J11 header Analog input changed from AIN0 to AIN1" — this fix is *not* retro-applied to RAK5005-O. |
| RAK19026 | Pin map not verified in this fork's research window | Unknown | See [Adding a new base board](#10-adding-a-new-base-board) before wiring. |

#### RAK5005-O, in two sentences

The RAK19007 datasheet's *"Improvements from RAK5005-O Base board"* note states verbatim: **"J11 header Analog input changed from AIN0 to AIN1"** ([source](https://docs.rakwireless.com/product-categories/wisblock/rak19007/datasheet/)). So RAK5005-O's J11 pin 1 is the legacy `AIN0` — the same net the on-board VBAT divider taps on every WisBlock Base, including the RAK19003, RAK19007, RAK19011. Wiring an external divider to RAK5005-O J11 pin 1 collides with `WB_A0` on the RAK4631 / RAK3401; the only safe option is to upgrade the Base board to a RAK19007 (or RAK19011).

The MCU-side `WB_A1` symbol maps cleanly through every Base board's
40-pin Core connector to connector pin 22. **The fork's firmware does
not change based on the Base board** — only the manual wiring does. The
MCU will sample P0.31 regardless of what is attached to it. Your
responsibility is to make sure whatever is on that pin is the divider
midpoint you designed.

## 3.1.1 Sanctioned wiring point (RAK19007 J11)

With a RAK19007 Base board, all three connections for the divider are
on the same 2.54 mm pitch header family. Use a 3-pin JST-EH or
hand-soldered leads:

```
                       RAK19007 silk screen
   Top of board side                 Bottom of board side
    1  2  3  4   <- J10 (BOOT,UART)   J11  1  2  3  4  <- AIN1, IO1, IO2, VBAT
                       GND = J10 pin 2
                       VBAT = J11 pin 4   (top of divider)
                       AIN1 = J11 pin 1   (mid-point of divider -> WB_A1 / P0.31)
```

The full divider assembly on a RAK19007:

```
   Battery + ---- R_top (100 k) ----+---- J11 pin 1 (AIN1 = WB_A1)
                                     |
                                     C_dec (100 nF, optional)
                                     |
                                     +---- J10 pin 2 (GND)
                                     |
   Battery - -----------------------+---- J10 pin 2 (GND) (also pack -)
```

No wire is required to the Core module. No wire is required to the
RAK3401 / RAK4631 module itself. The whole divider lives on the two
header rows.

> **RAK5005-O owners:** the same divider will *not* work on RAK5005-O
> J11 because pin 1 is `AIN0` (the on-board VBAT sense divider is also
> on this pin). Upgrading the Base board to a RAK19007 is the supported
> path. Bridging the existing on-board divider defeats both readings.

## 4. Wiring

```
                +--------------------+
   Battery + ---+ R_top   (100 kOhm) +----+---- WB_A1  (P0.31 / AIN7)
                +--------------------+    |
                                         |
                                         C_dec  (100 nF, optional)
                                         |
                                         +---- GND
                                         |
   Battery - -----------------------------+---- GND (WisBlock)
```

- `R` is the value of **both** resistors. Any equal-value pair works —
  the ratio is always 2:1 regardless of the absolute value. Default:
  100 kOhm, 1 % or better. See §4.3 for a current-draw table that
  helps you pick a value.
- `C_dec` is an optional 100 nF ceramic capacitor from `WB_A1` to
  ground. It forms a one-pole low-pass filter with the parallel
  combination of the two resistors (tau ~= R/2 * C), removing RF and
  supply-noise pickup on the analog line.
- Battery ground and WisBlock ground MUST be tied together.
- Do not add any other protection circuitry in series with the ADC
  pin unless you understand how it interacts with the SAADC input
  impedance.

### 4.1 Divider math

```
V_adc    = V_battery / 2
V_battery= V_adc * 2
```

With two equal-value resistors the ratio is exactly **2:1** (output is
half the input). The maximum expected `V_adc` at 4.2 V battery is
2.1 V, well within the 0 – 3.0 V input range of the nRF52 SAADC with
`AR_INTERNAL_3_0` reference.

### 4.2 ADC input limits

| Quantity | Value |
| --- | --- |
| nRF52 SAADC reference (`AR_INTERNAL_3_0`) | 3.0 V |
| Maximum allowed `V_adc` | 3.0 V |
| Maximum expected `V_adc` with the default divider at 4.2 V | 2.1 V |
| Maximum expected `V_adc` with a 3.0 V battery | 1.5 V |
| 12-bit ADC full scale | 4095 counts |

### 4.3 Resistor value and current draw

Any equal-value pair works — the ratio is always 2:1. The absolute
value only affects **idle current draw** and **ADC source impedance**:

| Equal pair | Total R | @4.2 V (full) | @3.7 V (nom) | Source Z (R/2) |
|---|---|---|---|---|
| 10 k + 10 k | 20 kΩ | 210 µA | 185 µA | 5 kΩ |
| 47 k + 47 k | 94 kΩ | 45 µA | 39 µA | 23.5 kΩ |
| **100 k + 100 k** | **200 kΩ** | **21 µA** | **18.5 µA** | **50 kΩ** |
| 220 k + 220 k | 440 kΩ | 9.5 µA | 8.4 µA | 110 kΩ |
| 470 k + 470 k | 940 kΩ | 4.5 µA | 3.9 µA | 235 kΩ |
| 1 M + 1 M | 2 MΩ | 2.1 µA | 1.85 µA | 500 kΩ |

**100 k + 100 k is the recommended default.** 21 µA is negligible next
to the radio's sleep draw, and 50 kΩ source impedance is comfortably
within the nRF52 SAADC's happy range. Very high values (≥470 kΩ)
cut current further but raise source impedance where you may need the
optional 100 nF decoupling capacitor and/or a longer ADC acquisition
time. Calibration (`set ext.batt.cal`) corrects for real-world
resistor tolerance regardless of the chosen pair.

The helper's compile-time default is a calibration factor of `1000`
(Q-format, 1.000). The `RAK_EXTERNAL_BATTERY_CALIBRATION_Q` flag can
override this at build time. It is **overridable at runtime** via the
CommonCLI without rebuilding — see
[§5.3 Post-flash runtime configuration](#53-post-flash-runtime-configuration)
and the
[README "Configuring the divider math post-flash"](../README.md#configuring-the-divider-math-post-flash)
section.

## 5. Calibration

The conversion in `src/helpers/ExternalBatterySense.cpp` is:

```
V_battery_mV = raw_adc * V_ref_mV * 2 * CAL_q
             / (ADC_FULL_SCALE * 1000)
```

with `CAL_q` in Q-format where `1000` represents a calibration factor
of exactly 1.000. This integer math is deterministic on Cortex-M4 and
does not need the FPU.

The only runtime knob is `CAL_q`. It is stored as a
[CommonCLI `NodePrefs`](../../meshcore/src/helpers/CommonCLI.h)
field and pushed to `rakExternalBatterySetCalibration(...)`
post-flash, so calibration never requires a rebuild. The compile-time
default baked into the `*_wb_a1` PlatformIO environments is simply
the seed value; the runtime `set ext.batt.cal` command overrides it
in NVS.

### 5.1 Calibration procedure (runtime, no rebuild)

1. Power the board from a current-limited bench supply set to a known
   voltage (e.g. 3.700 V) and connected through the divider.
2. Measure the actual battery-side voltage with a calibrated
   multimeter (>= 0.5 % accuracy). Call it `V_meas_mV`.
3. Read the firmware's reported voltage. Easiest path: enable
   `MESH_DEBUG=1` in the build, open the serial console, and run
   `get pwrmgt.bootmv` (still reads the on-board `WB_A0` path — see
   §9 for the relationship). For an accurate read of the external
   divider, add a temporary log line that calls `getBattMilliVolts()`
   once and prints the value, or read the
   `telemetry.addVoltage(...)` CayenneLPP field from a companion.
4. The ideal reading would be `V_meas_mV`. The actual reading is
   `V_report_mV`. The correction factor is
   `CAL_q = round(1000 * V_meas_mV / V_report_mV)`.
5. Push the calibration to the running firmware over the serial
   console — no rebuild required:
   ```
   set ext.batt.cal <new_cal_q>
   ```
   The value is persisted to `/com_prefs` immediately and survives
   reboot.
6. Re-test at 3.000 V, 3.300 V, 3.700 V, 4.000 V, 4.200 V. If the
   deviation at any of those points is still outside the table in
   [`docs/rak-wb-a1-hardware-test.md`](./rak-wb-a1-hardware-test.md#4-telemetry-verification),
   repeat from step 4.

### 5.2 Example

Suppose the bench supply is at 3.700 V and the firmware reports
3.640 V. Then `CAL_q = round(1000 * 3700 / 3640) = round(1016.48) =
1016`. From the serial console:

```
set ext.batt.cal 1016
```

The firmware now reports ~3.700 V at the same bench supply without a
rebuild. To revert to the compile-time default at any time:

```
set ext.batt.cal 0
```

### 5.3 Post-flash runtime configuration

After flashing any `*_wb_a1` UF2, the serial console exposes the
calibration trim without rebuilding:

```
set ext.batt.cal    <q>      // Q-format cal multiplier, 1000 = 1.000 (default 1000)
get ext.batt                 // shows effective cal
get ext.batt.cal             // individual read
```

`set ext.batt.cal 0` reverts to the compile-time default. Values persist
in `/com_prefs` and survive reboot. The full list of post-flash CLI
commands is in the
[README §"Configuring the divider math post-flash"](../README.md#configuring-the-divider-math-post-flash).

## 6. Build commands

The fork uses PlatformIO. From the repository root:

```bash
# Standard (unchanged) RAK3401 repeater
pio run -e RAK_3401_repeater

# Custom WB_A1 RAK3401 repeater
pio run -e RAK_3401_repeater_wb_a1

# Custom WB_A1 RAK4631 room server
pio run -e RAK_4631_room_server_wb_a1

# All WB_A1 RAK3401 + RAK4631 envs
while read env; do
  [ -z "$env" ] && continue
  [[ "$env" =~ ^# ]] && continue
  pio run -e "$env"
done < tools/wb_a1_build_matrix.txt
```

## 7. Artifact naming

The CI workflow produces one artifact per `*_wb_a1` environment plus a
combined archive on tagged releases. Each artifact filename is

```
<PIO_ENV>-<FIRMWARE_VERSION>-<SHORT_COMMIT>.<ext>
```

where `<ext>` is `uf2` (nRF52), `bin` (RP2040/STM32), or `hex` (STM32).
The combined release archive is

```
rak-wb-a1-firmware.tar.gz
rak-wb-a1-firmware.tar.gz.sha256
manifest.json
```

with `manifest.json` listing every artifact, its `sha256`, its
`pio_env`, its `rak_model`, its `firmware_role`, and the build
timestamp in UTC.

## 8. Flashing

Use the same flashing procedure as upstream MeshCore for the RAK3401
or RAK4631. The custom UF2 / HEX files are byte-equivalent except for
the optional `RAK_EXTERNAL_BATTERY_*` compile-time flags and the
include of `src/helpers/ExternalBatterySense.cpp` in the linked image.

For nRF52 boards:

- Drag the `.uf2` onto the `RAK3401` or `RAK4631` USB mass-storage
  bootloader (double-tap reset to enter it).
- Or use `nrfjprog --program firmware.hex --sectorerase` if you have
  a J-Link.

For ESP32-S3 / RP2040 / STM32, follow the standard `pio run -t upload`
procedure.

## 9. Telemetry vs. low-voltage protection

The custom build changes **only** the source of `getBattMilliVolts()`.
It does **not** modify any of the following safety paths:

- `PWRMGT_VOLTAGE_BOOTLOCK` (3.30 V on RAK3401 / RAK4631) still
  uses the on-board `WB_A0` divider via `getBattMilliVolts()`.
- The LPCOMP wake from SYSTEMOFF is still configured to AIN3
  (`WB_A0` / P0.05) with REFSEL 4 (5/8 VDD).
- VBUS detection (`isExternalPowered()`) still works via the USB
  regulator.
- The optional `AUTO_SHUTDOWN_MILLIVOLTS` (companion UI) is now
  evaluated against the external-divider reading. If you are using a
  regulated 5 V supply and want the auto-shutdown threshold to
  compare against the *raw battery*, you are now correctly protected.

If the board is powered via the USB-C port and a 5 V buck-boost is
attached to the raw battery, `isExternalPowered()` returns true and
the boot lockout is bypassed by the upstream logic.

## 10. Adding a new base board

The fork's firmware is independent of the WisBlock Base model: every
Base board uses the same 40-pin Core connector pinout for pins 21/22
(`AIN0`/`AIN1`). The fork will read whatever is on pin 22 the moment
the firmware is built with `RAK_EXTERNAL_BATTERY_SENSE=1
RAK_EXTERNAL_BATTERY_PIN=WB_A1`.

If you want to validate a base board that is not in the table above
(RAK19026, future revisions, third-party bases):

1. Confirm the 40-pin Core connector carries `AIN1` on pin 22.
   RAK publishes connector pinouts in every Base-board datasheet.
2. Identify how (or if) `AIN1` is broken out to an external pad /
   header / IO slot. If it is connector-only, the build is functional
   but practical wiring is harder.
3. Verify the Base's on-board `WB_A0` / `AIN0` net is *not* tapping
   into your divider mid-point. RAK19011 calls pin 21 `ADC_VBAT`
   ("Connected to a battery") — that net is the on-board sense divider
   and should not be reused for our external divider.
4. Add a row to the table in [Recommended WisBlock Base board](#31-recommended-wisblock-base-board).
5. Open a PR with the table update. No firmware change is required if
   the Core module is a RAK3401 or RAK4631.

## 11. Adding a new RAK environment

To add a new RAK environment to the WB_A1 build matrix:

1. Verify the board's `variant.h` exposes `WB_A1` and that the pin is
   not in conflict with SPI/I2C/GPS/display/flash/NFC/power-enable.
2. Update the board's `*Board.h` `getBattMilliVolts()` to honour
   `RAK_EXTERNAL_BATTERY_SENSE` (mirror the RAK3401 / RAK4631
   pattern).
3. Add the new `*_wb_a1` env to the board's `platformio.ini`
   following the existing pattern (use `env:NAME` extends and
   `${env:NAME.build_flags}` for variable interpolation).
4. Append the new env name to `tools/wb_a1_build_matrix.txt`.
5. Open a PR with all four changes plus updated analysis/hardware
   docs.

The CI matrix will fail until `tools/wb_a1_build_matrix.txt` is in
sync with the actual `platformio.ini` envs.
