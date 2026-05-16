#include "display.h"
#include "config.h"
#include <Arduino.h>
// Selective includes — avoids Arduino_ESP32SPI.h which needs esp32-hal-periman.h
#include <Arduino_GFX.h>
#include <databus/Arduino_ESP32QSPI.h>
#include <display/Arduino_CO5300.h>

static Arduino_DataBus *bus = nullptr;
static Arduino_GFX     *gfx = nullptr;

void display_init() {
    bus = new Arduino_ESP32QSPI(
        LCD_CS, LCD_SCLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
    // Default CO5300 constructor uses 466×466; don't pass LCD_W/H to avoid uint8_t overflow
    gfx = new Arduino_CO5300(bus, LCD_RST);

    if (!gfx->begin(40000000)) {
        Serial.println("[display] init failed — check wiring");
        while (1) delay(1000);
    }
    Serial.println("[display] CO5300 ready");

    // Red flash: confirms hardware SPI path works before LVGL takes over
    gfx->fillScreen(0xF800);  // RED in RGB565
    delay(800);
    gfx->fillScreen(0x0000);  // BLACK
    Serial.println("[display] test pattern done");
}

void display_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    // LV_COLOR_16_SWAP=1 → LVGL buffer is big-endian RGB565, matches draw16bitBeRGBBitmap
    gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
    lv_disp_flush_ready(drv);
}
