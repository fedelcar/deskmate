#include "display.h"
#include "config.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

static Arduino_DataBus *bus = nullptr;
static Arduino_GFX     *gfx = nullptr;

void display_init() {
    bus = new Arduino_ESP32QSPI(
        LCD_CS, LCD_SCLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);
    gfx = new Arduino_CO5300(bus, LCD_RST, 0 /* rotation */, false /* IPS */,
                              LCD_W, LCD_H);

    if (!gfx->begin(40000000)) {
        Serial.println("[display] init failed — check wiring");
        while (1) delay(1000);
    }
    Serial.println("[display] CO5300 ready");

    // Hardware test: red flash confirms display and SPI are alive
    gfx->fillScreen(RED);
    delay(800);
    gfx->fillScreen(BLACK);
    Serial.println("[display] test pattern done");
}

void display_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint16_t w = area->x2 - area->x1 + 1;
    uint16_t h = area->y2 - area->y1 + 1;
    gfx->startWrite();
    gfx->setAddrWindow(area->x1, area->y1, w, h);
    // LV_COLOR_16_SWAP=1 → buffer is big-endian; pass false so GFX doesn't double-swap
    gfx->writePixels((uint16_t *)color_p, (uint32_t)w * h, false);
    gfx->endWrite();
    lv_disp_flush_ready(drv);
}
