<div align="center">

# WB_A1 MeshCore Fork

**An opt-in `WB_A1` external battery sense build for RAK3401 and
RAK4631 / RAK4630 WisBlock cores.**

[![firmware: 12 envs](https://img.shields.io/badge/firmware-12%20envs-blue?style=flat-square)](#supported-firmware)
[![base board: RAK19007](https://img.shields.io/badge/base-RAK19007-blue?style=flat-square)](#supported-hardware)
[![license: MIT](https://img.shields.io/badge/license-MIT-blue?style=flat-square)](./license.txt)

[Flash via web flasher](https://A1Flasher.clankerhub.net/) &middot;
[Build from source](#build-from-source) &middot;
[Wiring guide](./docs/rak-wb-a1-wiring.md) &middot;
[Analysis](./docs/rak-wb-a1-battery-sense-analysis.md) &middot;
[Hardware test](./docs/rak-wb-a1-hardware-test.md)

</div>

---

## AI usage disclosure

This fork was developed with AI assistance. The **documentation**
(`docs/` and this README) is **100% AI generated**. The **code** is a
combination of AI-generated and hand-written changes that has gone
through thorough manual review and on-hardware testing (see
[Hardware test](./docs/rak-wb-a1-hardware-test.md)).

---

## What is this?

This is a single-purpose fork of [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore).
It adds one feature: **an opt-in `WB_A1` external battery-sensing
build** for the two WisBlock Core modules that expose a second analog
input (`WB_A1` = nRF52 P0.31 / AIN7): the **RAK3401** and the
**RAK4631 / RAK4630**.

The on-board `WB_A0` sense path is preserved unchanged. Boot lockout,
LPCOMP wake from `SYSTEMOFF`, brownout recovery, and `isExternalPowered()`
behave exactly as upstream. Only `getBattMilliVolts()` is retargeted at
the new pin, so the **telemetry, packet payloads, battery indicator, and
`BATT_MIN/BATT_MAX` percentage mapping all benefit automatically**.

The `*_wb_a1` PlatformIO environments are opt-in. Standard `RAK_*`
environments are byte-identical to upstream. The divider ratio is
fixed at 2:1 (any equal-value resistor pair); after flashing, the
`set ext.batt.cal` CLI command trims out resistor tolerance and ADC
reference error live in NVS without rebuilding the firmware.

---

## Why?

Some in the MeshCore community have opted for a solar / off-grid
pattern where an MPPT buck/boost charge controller feeds a stable
**5 V** rail to a RAK WisBlock base, which then powers the core and
(when present) the **1 W booster module** like the RAK13302. That setup
gives reliable day-and-night operation, but **the on-board
battery-sense divider is bypassed** — there's no way for the firmware
to see the actual battery voltage anymore.

There are two paths people use in practice. The 1 W variant has the
easy method:

> For the 1 W board (RAK19007 base + RAK4631 core + RAK13302 LoRa),
> power the booster from the 5 V connector on the back of the booster
> module, **remove the jumper**, and connect the battery to *both* the
> external charge controller *and* the LiPo battery connector. The
> on-board divider is no longer in the power path, so the firmware
> sees ground / 0 V — but you can wire the booster's passthrough battery
> terminal back to the LiPo connector to keep both circuits live.

This works for the 1 W setup because the LiPo connector is a real,
exposed battery terminal you can re-attach to. It does **not** work for
any non-1 W configuration (no booster, no exposed LiPo terminal, no
way to break the on-board divider cleanly).

For everything else, the standard workaround is **a 2:1 voltage
divider** made from two equal-value resistors wired between battery B+ and `WB_A1`:

```text
Battery B+ ── 100 kΩ ──+── WB_A1   (P0.31 / AIN7)
                       |
                     100 kΩ
                       |
Battery B-/GND ─────────+── RAK GND
```

You then need custom firmware to read that input instead of the
clipped on-board `WB_A0` divider, apply the divider math, and feed the
result into the existing telemetry / stats / display paths so the rest
of MeshCore can use it unchanged. **That's what this fork is.** A
repeater mounted at 5 V from a buck/boost MPPT with a 1S3P 18650 pack
can now report its real battery voltage, just like a stock USB-powered
unit, with no extra wiring beyond the divider.

(Background confirmed via RAK19007 / RAK19011 / RAK5005-O datasheets
and the MeshCore Discord #hardware channel; see
[Analysis §5](./docs/rak-wb-a1-battery-sense-analysis.md) for the
hardware compatibility matrix.)

---

## Supported hardware

| Role | MCU | `WB_A1` pin | Base board recommended | Notes |
|---|---|---|---|---|
| **RAK3401** | nRF52840 + SX1262 | P0.31 / `AIN7` | RAK19007 (Type-C, J11) | `WB_A1` alias added in this fork's `variant.h` |
| **RAK4631 / RAK4630** | nRF52840 + SX1262 | P0.31 / `AIN7` | RAK19007 (Type-C, J11) | Already aliased in upstream `variant.h` |
| WisMesh Tag, RAK11310, RAK3112, RAK3x72 | various | n/a | n/a | not supported |

### WisBlock Base boards (where you actually solder)

The MCU pin is the same on every WisBlock Base — it sits on
**Core connector pin 22 (`AIN1`)**. What varies is whether that pin is
broken out to a 2.54 mm header you can reach with a soldering iron,
and whether the on-board divider already taps that net:

| Base board | External `AIN1` | Recommended? | Notes |
|---|---|---|---|
| **RAK19007** (2nd gen, Type-C) | **J11 pin 1** | yes | J10 pin 2 = GND, J11 pin 4 = VBAT |
| **RAK19011** (3rd gen, Dual IO) | J19/J20 Core IO header pin 22 | yes | pin 21 is internally `ADC_VBAT`, do not reuse |
| RAK19003 (Mini Base) | connector only | **no** | `AIN1` is not broken out; tapping the connector castellation is not supported here |
| **RAK5005-O** (1st gen, Micro-B) | J11 pin 1 = `AIN0` (= on-board VBAT divider input) | **no** | conflicts with the on-board 2:1 divider that feeds the same node; cited at [Analysis §5.1](./docs/rak-wb-a1-battery-sense-analysis.md#51-wisblock-base-board-compatibility-matrix) |
| RAK19026 | not verified | unknown | see [wiring guide §3.1.1](./docs/rak-wb-a1-wiring.md#311-sanctioned-wiring-point-rak19007-j11) |

#### RAK5005-O, in two sentences

RAK19007's datasheet explicitly states under "Improvements from RAK5005-O Base board": **"J11 header Analog input changed from AIN0 to AIN1"** ([source](https://docs.rakwireless.com/product-categories/wisblock/rak19007/datasheet/)). That is, RAK5005-O's J11 pin 1 is the legacy `AIN0` — the same net the on-board VBAT divider taps on every WisBlock Base, including the RAK19003, RAK19007, RAK19011. So a divider wired to RAK5005-O J11 pin 1 collides with `WB_A0` on the RAK4631 / RAK3401. The datasheet itself documents J11 pin 1 as "AIN, ADC input signal" with no `AIN0`/`AIN1` suffix because that rename happened on RAK19007 ([source](https://docs.rakwireless.com/product-categories/wisblock/rak5005-o/datasheet/)).

---

## Quick-flash (recommended)

The fork's flasher is deployed at:

**https://A1Flasher.clankerhub.net/**

Open it in Chrome or Edge (WebSerial is required), pick **RAK3401** or
**RAK4631 / RAK4630**, pick a role (`repeater`, `room_server`,
`companion_radio_usb`, `companion_radio_ble`, `terminal_chat`,
`sensor`), double-tap reset on the board to enter DFU, and click
**Flash!**.

The site only ever lists WB_A1 builds. UF2 binaries come from this
fork's CI ([`.github/workflows/build-rak-wb-a1.yml`](./.github/workflows/build-rak-wb-a1.yml))
attaching each per-env UF2 individually as a GitHub release asset.

---

## Quick-flash (manual)

If you have a UF2 already:

1. Double-tap reset on the board; it appears as a USB mass-storage
   device named `RAK3401` or `RAK4631`.
2. Drag the chosen UF2 onto the drive.
3. Wait for the copy to complete and the board to reboot.

UF2s for each `*_wb_a1` environment are published by this fork's CI as
per-environment GitHub release assets (see [Build from source](#build-from-source)).

---

## Configuring the divider math post-flash

The divider ratio is **hardcoded to 2:1** (equal-value resistors).
Any matched pair works — 47k+47k, 100k+100k, 220k+220k, etc. The
only runtime knob is calibration (`CAL_q`), which absorbs real-world
resistor tolerance and ADC reference error:

```
set ext.batt.cal    <q>      // Q-format calibration multiplier, 1000 = 1.000
```

Each `set` is followed by `savePrefs` automatically by the CLI; the
value survives reboot via the existing `/com_prefs` file in NVS.

Read it back at any time:

```
get ext.batt         // shows effective cal
get ext.batt.cal     // individual read
```

Reset to factory: `format /com_prefs` then reboot; the compile-time
default will be used until you `set ext.batt.cal` again. See
[Wiring guide §5 Calibration](./docs/rak-wb-a1-wiring.md#5-calibration)
for the Q-format math and the bench-`V_meas` procedure used to derive
`CAL` for a specific unit.

---

## Build from source

```bash
# Standard (unchanged) RAK3401 repeater
pio run -e RAK_3401_repeater

# WB_A1 RAK3401 repeater
pio run -e RAK_3401_repeater_wb_a1

# All twelve WB_A1 environments at once
while read env; do
  [ -z "$env" ] && continue
  [[ "$env" =~ ^# ]] && continue
  pio run -e "$env" -t create_uf2
done < tools/wb_a1_build_matrix.txt
```

The custom build flag is the compile-time calibration default; runtime
overrides are accepted via the CLI above and persist to `/com_prefs`:

```
-DRAK_EXTERNAL_BATTERY_SENSE=1
-DRAK_EXTERNAL_BATTERY_PIN=WB_A1
-DRAK_EXTERNAL_BATTERY_CALIBRATION_Q=1000   // overridden by `set ext.batt.cal`
```

The feature is off by default. To bake in a different default
 calibration (e.g. for a specific batch with known resistor tolerance),
append the corresponding `-DRAK_EXTERNAL_BATTERY_CALIBRATION_Q`
override to the env's `build_flags` line in `variants/rak3401/platformio.ini` or
`variants/rak4631/platformio.ini`. End users can still override at
runtime via the CLI on top of any baked value.

---

## Documentation

- [Wiring and calibration guide](./docs/rak-wb-a1-wiring.md) — schematic,
  divider math, recommended base-board wiring (RAK19007 J11), calibration
  procedure, troubleshooting.
- [Hardware-test procedure](./docs/rak-wb-a1-hardware-test.md) —
  bench-test procedure with the bench PSU.
- [Analysis](./docs/rak-wb-a1-battery-sense-analysis.md) — the
  compatibility matrix, call path, ADC reference, base-board mapping, and
  implementation rationale.

### Original upstream MeshCore docs (preserved for reference)

The standard MeshCore user, CLI, protocol, packet format, and payload
docs are kept intact under `docs/`:

- [`docs/cli_commands.md`](./docs/cli_commands.md), [`docs/kiss_modem_protocol.md`](./docs/kiss_modem_protocol.md),
  [`docs/companion_protocol.md`](./docs/companion_protocol.md), [`docs/payloads.md`](./docs/payloads.md),
  [`docs/packet_format.md`](./docs/packet_format.md), [`docs/qr_codes.md`](./docs/qr_codes.md),
  [`docs/nrf52_power_management.md`](./docs/nrf52_power_management.md), [`docs/index.md`](./docs/index.md),
  [`docs/docs.md`](./docs/docs.md), [`docs/number_allocations.md`](./docs/number_allocations.md),
  [`docs/stats_binary_frames.md`](./docs/stats_binary_frames.md), [`docs/terminal_chat_cli.md`](./docs/terminal_chat_cli.md)

---

## Project layout

```
.
├── README.md                          you are here
├── license.txt                        inherited from upstream (MIT)
├── platformio.ini                     standard envs + [env:native] tests
├── src/helpers/ExternalBatterySense.h/cpp   runtime-aware battery math
├── test/test_external_battery_sense/  10 GoogleTest cases
├── tools/wb_a1_build_matrix.txt       CI matrix source-of-truth
├── tools/test_external_battery_sense*.cpp   local MSVC harnesses
├── variants/rak3401/                  + WB_A1 alias + 6 *_wb_a1 envs
├── variants/rak4631/                  + 6 *_wb_a1 envs
├── src/helpers/CommonCLI.{h,cpp}      + ext.batt.cal CLI command
├── .github/workflows/build-rak-wb-a1.yml   CI matrix, manifest.json, release
├── docs/rak-wb-a1-*.md                fork-specific docs (read in order below)
└── flasher/                           web flasher site (deploys to A1Flasher.clankerhub.net)
```

---

## Tests

Host-side GoogleTest suite for the integer math:

```bash
pio test --environment native --verbose
```

The native env compiles `src/helpers/ExternalBatterySense.cpp`
(guarded so it builds without Arduino). With no `g++` in your local
`PATH` (e.g. on a Windows box without WSL), `pio test -e native` will
skip. In that case compile the MSVC harnesses under `tools/` directly.

---

## Upstream attribution

This fork stands on two upstream projects. **No patches are sent
upstream and none are requested.**

- **Firmware**: [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore),
  MIT, copyright 2025 Scott Powell / rippleradios.com. See [`license.txt`](./license.txt).
- **Web flasher SPA**: [meshcore-dev/flasher.meshcore.io](https://github.com/meshcore-dev/flasher.meshcore.io),
  MIT, copyright 2025 Rastislav Vysoky.
- **Vendor libraries** under the flasher's `lib/` are third-party; see the
  upstream flasher README for per-file licences.

The fork-specific work — `src/helpers/ExternalBatterySense.{h,cpp}`,
the WB_A1 PlatformIO envs, the CI workflow, the docs/, and the new
`ext.batt.*` CLI commands — is original to this fork, MIT, copyright
2026.

---

## License

MIT, matching upstream. See [`license.txt`](./license.txt).
