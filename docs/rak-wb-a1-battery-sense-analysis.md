# RAK WB_A1 External Battery Sense - Analysis

This document is the Phase-1 inspection of the upstream MeshCore repository
(commit `a3a1aa5e3be34b42d8ac8c2cc244d30af6cdd71e`, default branch `main`,
latest relevant tag `room-server-v1.16.0`).

It captures what the fork intends to do, what the current code actually does,
and where the opt-in `WB_A1` external-divider support can be added safely.

## 1. Upstream / environment inventory

| Field | Value |
| --- | --- |
| Upstream URL | `https://github.com/meshcore-dev/MeshCore` |
| Upstream default branch | `main` |
| Fork branch for this work | `feature/rak-wb-a1-battery-sense` |
| Cloned commit SHA | `a3a1aa5e3be34b42d8ac8c2cc244d30af6cdd71e` |
| Latest tag (firmware-versioned) | `room-server-v1.16.0` |
| Latest tag (room firmware family) | `room-v6` |
| Submodules | None (`.gitmodules` is absent) |
| PlatformIO | 6.1.19 in use locally; upstream expects `platformio` Python package |
| Python | Upstream workflow pins `3.13`; locally verified with `3.14` |
| MCU families in CI | `ESP32_PLATFORM`, `NRF52_PLATFORM`, `STM32_PLATFORM`, `RP2040_PLATFORM` |
| Existing workflows | `pr-build-check.yml`, `run-unit-tests.yml`, `firmware-builder.yml`, `build-companion-firmware.yml`, `build-repeater-firmware.yml`, `build-room-server-firmware.yml`, `github-pages.yml`, `stale-bot.yml` |
| Composite action | `.github/actions/setup-build-environment/action.yml` (pip-installs PlatformIO, sets `GIT_TAG_VERSION`) |
| Native unit test env | `[env:native]` (GoogleTest) under `test/test_utils/test_tohex.cpp` |

## 2. RAK hardware compatibility matrix

The "all RAK firmwares" scope was determined by enumerating every
`variants/rak*` directory and listing every `env:` defined in each
`platformio.ini`.

| Variant dir | MCU | Core | `WB_A1` defined in `variant.h` | Pin number for `WB_A1` | ADC capable | NRF52 power mgmt | Standard envs | `WB_A1` candidate? |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `rak3401` | nRF52840 | Adafruit nRF52 (DCDC) | Not named `WB_A1`, but `PIN_A1` / `PIN_A5` = `31` and board is WisBlock-class | `31` (`AIN7`) | Yes (12-bit effective, default SAADC input) | Yes (`PWRMGT_LPCOMP_AIN=3`, `REFSEL=4`) | `RAK_3401_repeater`, `RAK_3401_room_server`, `RAK_3401_companion_radio_usb`, `RAK_3401_companion_radio_ble`, `RAK_3401_terminal_chat`, `RAK_3401_sensor`, `RAK_3401_kiss_modem` | **Yes (primary target)** |
| `rak4631` | nRF52840 | Adafruit nRF52 (DCDC) | Yes (`WB_A1 = 31`) | `31` (`AIN7`) | Yes | Yes (`PWRMGT_LPCOMP_AIN=3`, `REFSEL=4`) | `RAK_4631_repeater`, `RAK_4631_repeater_bridge_rs232_serial1`, `RAK_4631_repeater_bridge_rs232_serial2`, `RAK_4631_room_server`, `RAK_4631_companion_radio_usb`, `RAK_4631_companion_radio_ble`, `RAK_4631_terminal_chat`, `RAK_4631_sensor`, `RAK_4631_kiss_modem` | **Yes** |
| `rak_wismesh_tag` | nRF52840 | Adafruit nRF52 (DCDC) | Not defined; pinout is WisMesh-Tag specific (no IO_SLOT `WB_A1` exposed in variant.h) | n/a | n/a | No (no `NRF52_POWER_MANAGEMENT`) | `RAK_WisMesh_Tag_repeater`, `RAK_WisMesh_Tag_room_server`, `RAK_WisMesh_Tag_companion_radio_usb`, `RAK_WisMesh_Tag_companion_radio_ble`, `RAK_WisMesh_Tag_sensor`, `RAK_WisMesh_Tag_kiss_modem` | **No** (board exposes no `WB_A1`; pin 31 not declared as analog input) |
| `rak11310` | RP2040 | `earlephilhower` | No `WB_*` symbols | n/a | n/a | n/a | `RAK_11310_repeater`, `RAK_11310_repeater_bridge_rs232`, `RAK_11310_room_server`, `RAK_11310_companion_radio_usb`, `RAK_11310_terminal_chat`, `RAK_11310_kiss_modem` | **No** (different MCU; no `WB_A1`) |
| `rak3112` | ESP32-S3 | ESP32 Arduino | No `WB_*` symbols | n/a | n/a | n/a | `RAK_3112_repeater`, `RAK_3112_repeater_bridge_rs232`, `RAK_3112_repeater_bridge_espnow`, `RAK_3112_room_server`, `RAK_3112_terminal_chat`, `RAK_3112_companion_radio_usb`, `RAK_3112_companion_radio_ble`, `RAK_3112_companion_radio_wifi`, `RAK_3112_sensor`, `RAK_3112_kiss_modem` | **No** (different MCU; no `WB_A1`) |
| `rak3x72` | STM32WLE5 (built-in SX126x) | STM32duino | No `WB_*` symbols | n/a | n/a | n/a | `RAK_3x72_repeater`, `RAK_3x72_sensor`, `RAK_3x72_companion_radio_usb`, `RAK_3x72_kiss_modem` | **No** (different MCU; no `WB_A1`) |

Implications:

- Only `rak3401` and `rak4631` are RAK WisBlock-class nRF52 boards whose
  `variant.h` exposes pin `31` as an analog-capable input (mapped to `AIN7`
  on the nRF52840 SAADC).
- `rak3401` does not declare a symbolic `WB_A1` constant. To stay aligned
  with the upstream naming used on `rak4631` we will add the alias.
- `rak_wismesh_tag`, `rak11310`, `rak3112`, and `rak3x72` are kept
  unchanged. They are documented as "Not compatible" in the README.

## 3. Existing battery telemetry call path

The single source of truth is the virtual method
`mesh::MainBoard::getBattMilliVolts()` declared in
`src/MeshCore.h:47` and overridden in every board implementation. It is
called from the following consumers:

| Consumer file | Use |
| --- | --- |
| `src/helpers/StatsFormatHelper.h:14` | `"battery_mv"` in `formatCoreStats` JSON reply |
| `src/helpers/NRF52Board.cpp:102` / `:109` | Boot-voltage read for `PWRMGT_VOLTAGE_BOOTLOCK` |
| `examples/simple_sensor/SensorMesh.cpp:180` and `:924` | `telemetry.addVoltage(...)` |
| `examples/simple_room_server/MyMesh.cpp:142` | `stats.batt_milli_volts = ...` |
| `examples/simple_room_server/MyMesh.cpp:168` | `telemetry.addVoltage(...)` |
| `examples/simple_repeater/MyMesh.cpp:221` | `stats.batt_milli_volts = ...` |
| `examples/simple_repeater/MyMesh.cpp:247` | `telemetry.addVoltage(...)` |
| `examples/kiss_modem/KissModem.cpp:543` | KISS modem local stats |
| `examples/companion_radio/ui-orig/UITask.cpp:236` | `renderBatteryIndicator(...)` for on-screen icon |
| `examples/companion_radio/ui-new/UITask.cpp:162` / `:201` / `:833` | CayenneLPP voltage + on-screen icon |
| `examples/companion_radio/ui-tiny/UITask.cpp:125` / `:431` / `:728` / `:747` | CayenneLPP voltage + cached battery for low-battery shutdown |
| `examples/companion_radio/MyMesh.cpp:653` | `telemetry.addVoltage(...)` |

Battery percentage is purely derived from those millivolts inside the UI
helpers (`BATT_MIN_MILLIVOLTS=3000`, `BATT_MAX_MILLIVOLTS=4200`), so
**the on-screen percentage automatically benefits from a corrected sense
reading** without any extra changes.

The low-voltage auto-shutdown path (`AUTO_SHUTDOWN_MILLIVOLTS`) lives in
the companion UIs and only ever reads `getBattMilliVolts()` (`ui-tiny`
`UITask.cpp:729`, `ui-new` `UITask.cpp:834`). For every board that does
not define `AUTO_SHUTDOWN_MILLIVOLTS` this path is compiled out.

The boot-protection path is separate and lives in
`src/helpers/NRF52Board.cpp:98` (`checkBootVoltage`). It explicitly
**calls `getBattMilliVolts()` directly**, so any change we make to that
override is observed by the boot-lock as well.

## 4. ADC reference and resolution by RAK architecture

### nRF52 (RAK3401, RAK4631, RAK WisMesh Tag)
- `analogReadResolution(12)` is hard-coded in every RAK nRF52 board's
  `getBattMilliVolts()` (`RAK3401Board.h:26`, `RAK4631Board.h:24`,
  `RAKWismeshTagBoard.h:28`).
- Internal ADC reference is `AR_INTERNAL_3_0` per `rak3401/variant.h:206`
  (`VBAT_AR_INTERNAL`) and per `AR_INTERNAL_3_0` overrides used by every
  other nRF52 variant in the same Adafruit BSP fork.
- `ADC_RESOLUTION = 14` is the BSP-default but the board code requests
  12 effective bits for `getBattMilliVolts()`.
- The voltage-divider constant is embedded in `ADC_MULTIPLIER`:
  `3 * 1.73 * 1.187 * 1000` (3.0 V reference, ~1.73 resistive divider,
  ~1.187 calibration trim), yielding `~6157.5 mV per raw count` × raw/4096.

### RP2040 (RAK11310)
- `analogReadResolution(12)` in `RAK11310Board.h:30`.
- Reference handled in the BSP; the divider math is folded into
  `ADC_MULTIPLIER = (VBAT_DIVIDER_COMP * VBAT_MV_PER_LSB) ≈ 1.487`.
- Internal `analogRead` is not exposed as volts; the multiplier does the
  whole conversion.

### ESP32 (RAK3112)
- `analogReadResolution(12)` in `RAK3112Board.h:82`.
- Default reference via `analogReadMilliVolts(...)` in the BSP; multiplier
  `3 * 1.73 * 1.187 * 1000` is applied to the millivolts result.
- Note: the generic `ESP32Board.h` `getBattMilliVolts()` uses `analogReadMilliVolts` and
  a hard-coded `2 * raw` divisor, but `RAK3112Board` overrides it with raw
  counts to match the rest of the nRF52-style flow.

### STM32 (RAK3x72)
- `analogReadResolution(12)` in `rak3x72/target.h:27`.
- RAK3172 BSP-specific ADC path; not affected by our changes.

## 5. Confirmed `WB_A1` mappings

| Board | `WB_A1` value | Notes |
| --- | --- | --- |
| RAK3401 | `31` (`AIN7`, P0.31). Alias `WB_A1` to be added in this fork's `variant.h` | Pin 31 is also aliased as `PIN_A1` / `PIN_A5`. Default configuration does not name it `WB_A1`; we add the alias. |
| RAK4631 | `31` (`AIN7`, P0.31) | Already aliased in `rak4631/variant.h:52`. |
| RAK WisMesh Tag | not exposed | No WB slot on the tag module. |
| RAK11310 / RAK3112 / RAK3x72 | n/a | Non-nRF52 or no WisBlock connector. |

## 5.1 WisBlock Base board compatibility matrix

The fork's firmware reads `WB_A1`, which the variant maps to MCU pin
P0.31 (AIN7) on a RAK4631 or RAK3401. From the MCU's perspective the
Wiring diagram is identical regardless of which WisBlock Base board
hosts the Core module. What changes between base boards is whether
`AIN1` (Core connector pin 22) is broken out to a solderable,
user-accessible header.

| Base board | Generation | USB | External `AIN1` access | External `VBAT` (divider top) | External `GND` (divider bottom) | Sanctioned for this fork? |
| --- | --- | --- | --- | --- | --- | --- |
| **RAK19007** | 2nd-gen | Type-C | **J11 pin 1** | J11 pin 4 | J10 pin 2 | **Yes (recommended)** |
| **RAK19003** | Mini Base | Type-C | Not broken out (Core connector pin 22 only) | Battery connector | Battery connector | **No** — `AIN1` not broken out; castellation tap not supported |
| **RAK19011** | 3rd-gen Dual IO | Type-C | **J19/J20 Core IO header pin 22** | Internal to `ADC_VBAT` net (pin 21) — do not reuse | Header GND | Yes |
| **RAK5005-O** | 1st-gen | Micro-B | J11 pin 1 = `AIN0` (= `WB_A0`); `AIN1` is **not** broken out | Battery connector | Battery connector | **No** — J11 conflicts with on-board VBAT divider |
| **RAK19026** | (datasheet URL returned 404 during this fork's research) | — | **Not verified** | — | — | Unknown |

References (RAK Wireless documentation, with section anchors):

- **RAK19007 datasheet, "Improvements from RAK5005-O Base board" note** —
  states verbatim: *"J11 header Analog input changed from AIN0 to AIN1."*
  This is the primary citation that RAK5005-O's J11 pin 1 is the legacy
  `AIN0` (= on-board VBAT-divider net), not `AIN1` as one might guess
  from the more recent naming convention.
  <https://docs.rakwireless.com/product-categories/wisblock/rak19007/datasheet/>
- **RAK19007 datasheet, `J11 Header Pinout` table** — pin 1 = `AIN1`,
  pin 2 = `IO1`, pin 3 = `IO2`, pin 4 = `VBAT`.
- **RAK5005-O datasheet, `J11 Pin Definition` table** — pin 1 = `AIN,
  ADC input signal` (the legacy `AIN0` per the improvement note above,
  with no `AIN0`/`AIN1` suffix because the rename happened later on
  RAK19007). Pins 2 and 3 are unpopulated in the table.
  <https://docs.rakwireless.com/product-categories/wisblock/rak5005-o/datasheet/>
- **RAK19003 datasheet, "Connector for WisBlock Core"** — pin 22 =
  `AIN1`. The Mini Base exposes no 2.54 mm header to that pin; only the
  castellation on the 40-pin board-to-board connector is reachable.
- **RAK19011 datasheet, J19/J20 description** — "forty 2.54 mm pitch
  headers for IO access and extension... pin arrangement is based on
  the 40-pin WisConnector of the WisBlock Core". Core-connector pin
  table: pin 21 = `ADC_VBAT` ("Connected to a battery"), pin 22 =
  `AIN1`. Pin 21 is the on-board VBAT-divider input — leave it alone.
  <https://docs.rakwireless.com/product-categories/wisblock/rak19011/datasheet/>
- **RAK4631 (Core) datasheet, "RAK4631 Module Pinout"** — module
  pin 39 = P0.31/AIN7 and the WisConnector pin table — pin 22 of the
  Core connector = `AIN1`. Confirms the MCU-to-base mapping the fork
  relies on.

### 5.1.1 RAK5005-O proof trail

End-to-end chain from datasheet to MCU pin, suitable for pasting into
an issue or PR:

1. **RAK5005-O datasheet** — `J11 Pin Definition`, pin 1 reads
   `AIN, ADC input signal`. URL:
   <https://docs.rakwireless.com/product-categories/wisblock/rak5005-o/datasheet/#j11-pin-definition>
2. **RAK19007 datasheet** — "Improvements from RAK5005-O Base board"
   reads verbatim *"J11 header Analog input changed from AIN0 to
   AIN1."* URL:
   <https://docs.rakwireless.com/product-categories/wisblock/rak19007/datasheet/#overview>
   ⇒ step 1's `AIN` is the legacy `AIN0`, and step 1's J11 pin 1 is
   therefore the same physical net as the on-board VBAT divider.
3. **RAK19007 datasheet** — `Connector for WisBlock Core` pin table,
   pin 21 = `AIN0`, pin 22 = `AIN1`. The RAK5005-O table has the same
   pinout (verified against the RAK5005-O datasheet pinout table at
   the same anchor). `AIN0` (pin 21) is the on-board VBAT-divider
   input net.
4. **`variants/rak4631/variant.h:111`** — comment reads *"AIN3 = P0.05
   = PIN_A0 / PIN_VBAT_READ"*. The internal divider feeds the MCU's
   `AIN3` input, which is the same physical net as the Base board's
   `AIN0` connector pin 21.
5. **`variants/rak4631/RAK4631Board.h:11`** — `#define PIN_VBAT_READ
   5` and `getBattMilliVolts()` reads `analogRead(PIN_VBAT_READ)`
   (line 49), proving that the upstream on-board VBAT-sense path
   lives on `WB_A0 = PIN_VBAT_READ = 5 = AIN3 = P0.05` — the same
   net RAK5005-O exposes on J11 pin 1.

So: anything the user wires to RAK5005-O J11 pin 1 conflicts with the
on-board divider that `getBattMilliVolts()` reads on the standard
firmware build. No additional board surgery would fix that without
breaking the upstream sense path; the only safe option is to upgrade
the Base to a RAK19007 (or RAK19011).

Implications:

- The fork's firmware image for a given `*_wb_a1` environment is
  byte-identical regardless of the Base board. Only the user's
  wiring diagram changes.
- For a RAK19007-based build (`-t upload` or drag-and-drop UF2 on
  RAK4631/RAK3401 + RAK19007) the wiring guide in
  `docs/rak-wb-a1-wiring.md#311-sanctioned-wiring-point-rak19007-j11`
  is the canonical reference.
- On a RAK5005-O Base board the user's only path is to either replace
  the Base or skip the external-divider feature and continue using the
  on-board `WB_A0` divider (i.e. do not enable `RAK_EXTERNAL_BATTERY_SENSE`).
- On a RAK19011 Base, **pin 21 is internally connected to a battery
  divider** (datasheet name `ADC_VBAT`). Wiring to pin 22 only is
  safe and uses the standard `AIN1` net. Pin 21 must be left untouched.

## 6. Pin conflict review

For RAK3401 / RAK4631 the candidate pin `31` (`AIN7`) is currently used
by the **companion UI** as `PIN_USER_BTN_ANA` (analog joystick button
input). All companion environments set `-D PIN_USER_BTN_ANA=31`. The
analog button driver (`MomentaryButton`) and the ADC battery sampler are
separate call sites, so they do not collide on the wire. However, the
following caveats apply:

- If the user wires an external divider onto `WB_A1` **and** still wants
  the analog joystick button to function, the analog input will read the
  divider midpoint instead of the button. This is a hardware-level
  conflict that the firmware cannot resolve.
- For non-companion roles (`repeater`, `room_server`, `sensor`,
  `terminal_chat`, `kiss_modem`) `PIN_USER_BTN_ANA` is not used, so pin
  31 is free for our external divider.
- Pin 31 (`AIN7`) is also reachable by the nRF52 LPCOMP peripheral, but
  `PWRMGT_LPCOMP_AIN` is hard-coded to `3` (`AIN3`, P0.05 = `WB_A0` /
  `PIN_VBAT_READ`) on RAK3401 and RAK4631 (`variant.h:86`). We therefore
  keep the LPCOMP boot-lock and wake on the original `WB_A0`/`AIN3`
  input. **Boot lockout, brownout recovery, and SYSTEMOFF wake behavior
  are NOT moved to `WB_A1`.** The custom build only redirects the
  `getBattMilliVolts()` telemetry path used by stats, telemetry packets,
  battery indicator and the optional companion `AUTO_SHUTDOWN_MILLIVOLTS`
  shutdown.

Implications of powering the board through regulated 5 V while leaving
`WB_A0` floating:

- `NRF52Board::isExternalPowered()` checks `USBREGSTATUS.VBUSDETECT`. A
  5 V buck-boost supply that does not drive VBUS will **not** look
  external-powered. The boot-lock check is then performed against
  whatever `getBattMilliVolts()` returns from `WB_A0` (which may now be
  floating or tied to the 5 V rail via the on-board divider). This is a
  hardware reality and is documented as a known caveat. With our
  `WB_A1` build we keep `WB_A0` sampling untouched, so the user inherits
  the upstream behaviour.
- If the user wants the board to treat USB power as "external" they can
  power it via the WisBlock USB-C port (or wire 5 V onto VBUS through a
  resistive network as appropriate to their hardware).

## 7. Fork maintenance (manual)

The user opted out of an automatic upstream-sync workflow. To keep
the fork in step with upstream MeshCore:

1. Add the upstream remote once (locally):
   ```bash
   git remote add upstream https://github.com/meshcore-dev/MeshCore.git
   git fetch upstream
   ```
2. From the fork's `main` branch:
   ```bash
   git checkout main
   git merge --no-ff upstream/main
   git push origin main
   ```
3. Rebase the feature branch on the updated `main`:
   ```bash
   git checkout feature/rak-wb-a1-battery-sense
   git rebase main
   # resolve any conflicts, then
   git push --force-with-lease origin feature/rak-wb-a1-battery-sense
   ```

The fork MUST NOT be force-pushed. Force-push with lease is only
acceptable on the feature branch and only after a successful rebase
or squash.

## 8. Risks and assumptions

- **Assumption A1**: With the regulated 5 V supply, `WB_A0` no longer
  represents battery voltage. The user's harness must be sure that the
  5 V buck-boost does not back-feed `WB_A0` through the on-board divider
  to a value above the LPCOMP `5/8 VDD` reference (~3.4 V). The boot-lock
  may engage unexpectedly. We document this without changing the upstream
  boot-lock path.
- **Assumption A2**: `AIN7` (P0.31) is free from SPI, I2C, GPS, display,
  radio, flash, NFC, and power-enable functions on RAK3401 and RAK4631.
  Verified by reconciling every defined pin in `variants/rak3401/variant.h`
  and `variants/rak4631/variant.h` (SPI on 43/44/45, I2C on 13/14,
  display reset on 21, GPS PPS on 17, LoRa DIO on 10/9/26/4). The only
  collision is `PIN_USER_BTN_ANA = 31` on companion roles.
- **Assumption A3**: The fork only changes the telemetry path; we never
  retarget LPCOMP to `AIN7`.
- **Assumption A4**: The user is OK with integer math. We use
  fixed-point (microvolt-aware) arithmetic in a new helper to avoid
  unnecessary floating-point on Cortex-M4.
- **Risk R1**: `PIN_USER_BTN_ANA = 31` on companion roles. Documented.
  We do not change upstream; we provide the option.
- **Risk R2**: Powering a 1S3P Li-ion pack above 4.2 V or below 3.0 V
  can damage the cells. Documented in the wiring guide. The divider math
  clamps at compile time when the configured divider ratio plus
  maximum battery voltage exceeds 3.0 V (`AR_INTERNAL_3_0`).
- **Risk R3**: Host-side native test environment cannot exercise the
  Arduino `analogRead` call. We factor the divider math into a
  standalone, pure-C++ helper so it can be unit-tested on the host
  without an MCU.

## 8. Implementation plan

1. **Helpers** (new files under `src/helpers/`):
   - `src/helpers/ExternalBatterySense.h` / `.cpp`: pure helper exposing
     `extern_battery_millivolts(raw_adc, r_top, r_bottom, calibration)`
     returning battery millivolts as a `uint16_t`. Integer math, scale
     factor `(R_top + R_bottom) * 1000 / R_bottom`, then apply a 12-bit
     ADC full scale against the board's reference voltage (3.0 V for
     nRF52 SAADC `AR_INTERNAL_3_0`).
   - The helper compiles on host (`#if defined(ARDUINO)` guard) so the
     native test target can link it.

2. **Board header**:
   - `variants/rak3401/RAK3401Board.h` (and `rak4631/RAK4631Board.h`)
     gain an `#ifdef RAK_EXTERNAL_BATTERY_SENSE` branch inside
     `getBattMilliVolts()` that selects the external pin, averages
     `BATTERY_SAMPLES` samples, and delegates the math to the helper.
     The original implementation is kept under `#else` and remains the
     default.
   - `variants/rak3401/variant.h` gains `#define WB_A1 (31)` for
     consistency with `rak4631`.

3. **PlatformIO custom environments**:
   - For every RAK3401 and RAK4631 environment listed in section 2 we
     add a `*_wb_a1` counterpart that `extends` the existing env and
     appends the build flag
     `-D RAK_EXTERNAL_BATTERY_SENSE=1` plus the resistor / calibration
     overrides. No other flag changes.
   - Upstream environment names remain byte-identical.

4. **Tests** (new files under `test/test_utils/`):
   - `test_external_battery_sense.cpp` exercises the helper for the
     nine required cases (0 V, 3.0 V, 3.3 V, 3.7 V, 4.2 V, full-scale,
     zero bottom resistance, overflow resistance, calibration > 1 and
     < 1).
   - The native test entry in `platformio.ini` already includes
     `src/Utils.cpp`; we extend it to also build `src/helpers/ExternalBatterySense.cpp`
     via `build_src_filter` so the test can link the helper without an
     Arduino dependency.

5. **GitHub Actions**:
   - `.github/workflows/build-rak-wb-a1.yml`: matrix over every
     `*_wb_a1` env, Ubuntu `ubuntu-24.04`, pinned `actions/checkout@<sha>`,
     pinned `actions/setup-python@v6` with Python 3.13, PlatformIO via
     pip, caches, per-env artifacts, combined archive, machine-readable
     `manifest.json` with SHA-256, concurrency group, explicit
     `permissions: contents: read`. Pull-request builds run with
     `secrets: inherit: never`. Tag pushes can flip `contents: write` on
     the release job.
   - Reuses `.github/actions/setup-build-environment` to keep the cache
     and `GIT_TAG_VERSION` behaviour consistent with upstream.
   - `.github/workflows/sync-upstream.yml`: scheduled (weekly) +
     `workflow_dispatch`, fetches upstream, opens a PR via `peter-evans/create-pull-request`,
     never force-pushes, never deletes branches.

6. **Documentation**:
   - `docs/rak-wb-a1-battery-sense-analysis.md` (this file).
   - `docs/rak-wb-a1-hardware-test.md` for bench-test procedure.
   - `docs/rak-wb-a1-wiring.md` for divider schematic, calibration,
     flashing, fork synchronisation.

## 9. Test plan

### 9.1 Unit (host, GoogleTest)

| Case | Input ADC raw (12-bit) | Expected millivolts (±2 mV) |
| --- | --- | --- |
| 0 V battery | 0 | 0 |
| 3.0 V battery | 2048 | 3000 |
| 3.3 V battery | 2253 | 3300 |
| 3.7 V battery | 2527 | 3700 |
| 4.2 V battery | 2867 | 4200 |
| Full-scale | 4095 | ≤ 6000 |
| Zero bottom resistance (`R_BOTTOM=0`) | any | compile error via `static_assert` |
| Calibration > 1.0 (`CAL=1.05`) | 2048 | 3150 |
| Calibration < 1.0 (`CAL=0.95`) | 2048 | 2850 |
| Overflow safety | raw=4095 with 2:1 divider, cal=1.0 | ≈ 6000 (clamped) |

### 9.2 Build verification

- `pio run -e RAK_3401_repeater` succeeds with no custom flags (no
  drift from upstream).
- Every `*_wb_a1` env compiles via the GitHub Actions matrix.
- `pio test -e native` passes with the new test suite.

### 9.3 Hardware-in-the-loop

Out of scope in this environment. Procedure documented in
`docs/rak-wb-a1-hardware-test.md`.