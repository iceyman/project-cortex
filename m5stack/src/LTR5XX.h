// ════════════════════════════════════════════════════════════════
//  LTR5XX.h — Direct I2C driver for LTR-553ALS-WA
//  Uses m5::In_I2C (internal bus) — NO Wire.begin() call
//  M5StackChan.begin() already owns I2C init
// ════════════════════════════════════════════════════════════════
#pragma once
#include <M5Unified.h>  // for m5::In_I2C

#define LTR5XX_LED_PULSE_FREQ_40KHZ      0x02
#define LTR5XX_PS_MEASUREMENT_RATE_50MS  0x02
#define LTR5XX_ALS_GAIN_48X              0x03
#define LTR5XX_PS_ACTIVE_MODE            0x03
#define LTR5XX_ALS_ACTIVE_MODE           0x01
#define LTR553_ADDR                      0x23

struct Ltr5xx_Init_Basic_Para {
    uint8_t ps_led_pulse_freq;
    uint8_t ps_measurement_rate;
    uint8_t als_gain;
};
#define LTR5XX_BASE_PARA_CONFIG_DEFAULT { LTR5XX_LED_PULSE_FREQ_40KHZ, LTR5XX_PS_MEASUREMENT_RATE_50MS, LTR5XX_ALS_GAIN_48X }

class LTR5XX {
private:
    bool _ok = false;

    void writeReg(uint8_t reg, uint8_t val) {
        m5::In_I2C.writeRegister8(LTR553_ADDR, reg, val, 100000);
    }
    uint8_t readReg(uint8_t reg) {
        return m5::In_I2C.readRegister8(LTR553_ADDR, reg, 100000);
    }

public:
    bool begin(Ltr5xx_Init_Basic_Para* params) {
        // Check part ID (should be 0x92 or 0x93)
        uint8_t id = readReg(0x86);
        _ok = (id == 0x92 || id == 0x93);
        Serial.printf("[LTR553] Part ID=0x%02X  %s\n", id, _ok ? "OK" : "NOT FOUND");
        return _ok;
    }
    void setPsMode(uint8_t mode)  { if (_ok) writeReg(0x81, mode); }
    void setAlsMode(uint8_t mode) { if (_ok) writeReg(0x80, mode); }

    uint16_t getPsValue() {
        if (!_ok) return 0;
        uint8_t lo = readReg(0x88);
        uint8_t hi = readReg(0x89);
        return ((uint16_t)(hi & 0x07) << 8) | lo;
    }
    uint16_t getAlsValue() {
        if (!_ok) return 0;
        uint8_t lo = readReg(0x8A);
        uint8_t hi = readReg(0x8B);
        return ((uint16_t)hi << 8) | lo;
    }
};
