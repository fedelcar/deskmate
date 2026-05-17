#pragma once
#include <lvgl.h>
#include <stdint.h>

void display_init();
void display_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p);
void display_set_brightness(uint8_t level);  // 0=off, 255=max
