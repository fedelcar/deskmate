#include "display.h"
#include "config.h"
#include "CO5300.h"
#include <Arduino.h>

static CO5300 *lcd = nullptr;

void display_init() {
    lcd = new CO5300(LCD_CS, LCD_SCLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3, LCD_RST, LCD_W, LCD_H);
    if (!lcd->begin(40000000)) {
        Serial.println("[display] CO5300 init failed — check wiring and config.h pins");
        while (1) delay(1000);
    }
    Serial.println("[display] CO5300 ready");

    // Red flash: confirms SPI path before LVGL takes over
    lcd->fillScreen(0xF800);   // red
    delay(800);
    lcd->fillScreen(0x0000);   // black
    Serial.println("[display] test pattern done");
}

void display_set_brightness(uint8_t level) {
    lcd->setBrightness(level);
}

void display_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint16_t x0 = area->x1, y0 = area->y1;
    uint16_t x1 = area->x2, y1 = area->y2;
    lcd->setWindow(x0, y0, x1, y1);
    lcd->pushColors((uint16_t *)color_p, (uint32_t)(x1 - x0 + 1) * (y1 - y0 + 1));
    lv_disp_flush_ready(drv);
}
