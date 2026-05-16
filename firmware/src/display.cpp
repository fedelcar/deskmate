#include "display.h"
#include "config.h"
#include <Arduino_GFX_Library.h>

static Arduino_DataBus *bus = nullptr;
static Arduino_GFX    *gfx = nullptr;

void display_init() {
    // QSPI bus — four data lines for high-speed AMOLED transfer
    bus = new Arduino_ESP32QSPI(
        LCD_CS, LCD_SCLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3);

    // CO5300 driver — 466×466 round AMOLED, no backlight pin needed
    // If this class is missing, install Waveshare's GFX fork:
    //   https://github.com/waveshareteam/Arduino_GFX (see README)
    gfx = new Arduino_CO5300(bus, LCD_RST, 0 /* rotation */, false, LCD_W, LCD_H);

    if (!gfx->begin(80000000)) {
        Serial.println("[display] begin() failed — check wiring and library");
        while (1) delay(1000);
    }
    gfx->fillScreen(BLACK);
}

void display_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
    lv_disp_flush_ready(drv);
}
