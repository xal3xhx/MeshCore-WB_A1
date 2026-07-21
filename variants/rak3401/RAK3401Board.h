#pragma once

#include <MeshCore.h>
#include <Arduino.h>
#include <helpers/NRF52Board.h>
#if defined(RAK_EXTERNAL_BATTERY_SENSE) && RAK_EXTERNAL_BATTERY_SENSE
  #include <helpers/ExternalBatterySense.h>
#endif

// built-ins
#define  PIN_VBAT_READ    5
#define  ADC_MULTIPLIER   (3 * 1.73 * 1.187 * 1000)

#define PIN_3V3_EN (34)
#define WB_IO2 PIN_3V3_EN

// WisBlock IO_SLOT analog input. On RAK3401 this pin is not named
// `WB_A1` in the upstream variant.h - it is exposed as `PIN_A1 = 31`
// (also aliased as `PIN_A5 = 31`). The fork adds the symbolic alias
// so the new external-divider feature can use the canonical name
// used on RAK4631.
#ifndef WB_A1
  #define WB_A1 31
#endif

class RAK3401Board : public NRF52BoardDCDC {
protected:
#ifdef NRF52_POWER_MANAGEMENT
  void initiateShutdown(uint8_t reason) override;
#endif
public:
  RAK3401Board() : NRF52Board("RAK3401_OTA") {}
  void begin();

  #define BATTERY_SAMPLES 8

  uint16_t getBattMilliVolts() override {
#if defined(RAK_EXTERNAL_BATTERY_SENSE) && RAK_EXTERNAL_BATTERY_SENSE
    // External voltage-divider sense on WB_A1.
    // The on-board WB_A0 path is intentionally preserved for boot-lock
    // and SYSTEMOFF wake (NRF52_POWER_MANAGEMENT). See
    // docs/rak-wb-a1-battery-sense-analysis.md.
    analogReadResolution(12);

#ifndef RAK_EXTERNAL_BATTERY_PIN
    #define RAK_EXTERNAL_BATTERY_PIN WB_A1
#endif

    uint16_t samples[BATTERY_SAMPLES];
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      samples[i] = (uint16_t)analogRead(RAK_EXTERNAL_BATTERY_PIN);
    }
    return rakExternalBatteryMilliVoltsAvg(samples, BATTERY_SAMPLES);
#else
    // Standard on-board battery sense (unchanged from upstream).
    analogReadResolution(12);

    uint32_t raw = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
      raw += analogRead(PIN_VBAT_READ);
    }
    raw = raw / BATTERY_SAMPLES;

    return (ADC_MULTIPLIER * raw) / 4096;
#endif
  }

  const char* getManufacturerName() const override {
    return "RAK 3401";
  }

  // TX/RX switching is handled by SX1262 DIO2 -> SKY66122 CTX (hardware-timed).
  // No onBeforeTransmit/onAfterTransmit overrides needed.
};
