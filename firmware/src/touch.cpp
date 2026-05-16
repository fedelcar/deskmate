#include "touch.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

// CST9217 register map (I2C, addr 0x1A)
#define CST9217_FINGER_NUM   0x02
#define CST9217_TOUCH_DATA   0x03  // 6 bytes per finger: status, xH, xL, yH, yL, pressure

static uint16_t touch_x = 0, touch_y = 0;
static bool     touch_pressed = false;

static void cst9217_wake() {
    // Toggle INT low briefly to wake the IC from low-power mode
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, LOW);
    delayMicroseconds(5);
    pinMode(TOUCH_INT, INPUT_PULLUP);
    delayMicroseconds(50);
}

static bool cst9217_read_raw() {
    cst9217_wake();
    Wire.beginTransmission(TOUCH_ADDR);
    Wire.write(CST9217_TOUCH_DATA);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(TOUCH_ADDR, 6) != 6) return false;

    uint8_t status = Wire.read();
    uint8_t xh     = Wire.read();
    uint8_t xl     = Wire.read();
    uint8_t yh     = Wire.read();
    uint8_t yl     = Wire.read();
    Wire.read(); // pressure — ignored

    bool pressed = (status & 0x0F) > 0;
    if (pressed) {
        touch_x = ((xh & 0x0F) << 8) | xl;
        touch_y = ((yh & 0x0F) << 8) | yl;
    }
    return pressed;
}

void touch_init() {
    Wire.begin(TOUCH_SDA, TOUCH_SCL, 400000);

    if (TOUCH_RST >= 0) {
        pinMode(TOUCH_RST, OUTPUT);
        digitalWrite(TOUCH_RST, LOW);
        delay(20);
        digitalWrite(TOUCH_RST, HIGH);
        delay(200);  // extra settle time
    }
    if (TOUCH_INT >= 0) {
        pinMode(TOUCH_INT, INPUT_PULLUP);
    }

    // I2C scan — logs any device found
    Serial.println("[touch] I2C scan:");
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("[touch]   found 0x%02X\n", addr);
        }
    }
}

void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    touch_pressed = cst9217_read_raw();
    if (touch_pressed) {
        data->point.x = touch_x;
        data->point.y = touch_y;
        data->state   = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
