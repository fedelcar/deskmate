#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <lvgl.h>

void touch_init();
void touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
