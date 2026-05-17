#pragma once

// ─── WiFi ────────────────────────────────────────────────────────────────────
#define WIFI_SSID   "freebox_flc"
#define WIFI_PASS   "wififede"

// ─── Server ──────────────────────────────────────────────────────────────────
// mDNS hostname advertised by the Node server ("deskmate.local")
#define SERVER_HOST "deskmate.local"
#define SERVER_HOST_IP "192.168.0.15"  // Mac fallback if mDNS fails
#define SERVER_PORT 3001
#define FETCH_INTERVAL_MS 60000   // refresh every 60 seconds

// ─── Display pins  (Waveshare ESP32-S3-Touch-AMOLED-1.75) ───────────────────
// Source: waveshareteam/ESP32-S3-Touch-AMOLED-1.75 pin_config.h
#define LCD_CS    12
#define LCD_SCLK  38
#define LCD_D0    4
#define LCD_D1    5
#define LCD_D2    6
#define LCD_D3    7
#define LCD_RST   39
#define LCD_W     466
#define LCD_H     466

// ─── Touch pins  (CST9217, I2C) ─────────────────────────────────────────────
#define TOUCH_SDA   15
#define TOUCH_SCL   14
#define TOUCH_INT   11
#define TOUCH_RST   40
#define TOUCH_ADDR  0x1A

// ─── Buttons  (Waveshare ESP32-S3-Touch-AMOLED-1.75) ────────────────────────
// BOOT button (active LOW) — short press: next screen
#define BUTTON1_PIN  0
// PA/KEY2 pin — short press: cycle brightness
#define BUTTON2_PIN  46

// ─── LVGL ────────────────────────────────────────────────────────────────────
// Two full-screen draw buffers in PSRAM for tear-free rendering
#define LV_BUF_LINES  80    // lines per buffer (80 × 466 × 2 bytes ≈ 74 KB each)
