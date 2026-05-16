#include "touch.h"
#include "config.h"
#include <Wire.h>

// CST9217 register map (I2C, addr 0x1A)
#define CST9217_FINGER_NUM   0x02
#define CST9217_TOUCH_DATA   0x03  // 6 bytes per finger: status, xH, xL, yH, yL, pressure

static uint16_t touch_x = 0, touch_y = 0;
static bool     touch_pressed = false;

static bool cst9217_read_raw() {
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

    if (TOUCH_INT >= 0) {
        pinMode(TOUCH_INT, INPUT_PULLUP);
    }
    if (TOUCH_RST >= 0) {
        pinMode(TOUCH_RST, OUTPUT);
        digitalWrite(TOUCH_RST, LOW);
        delay(20);
        digitalWrite(TOUCH_RST, HIGH);
        delay(100);
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
