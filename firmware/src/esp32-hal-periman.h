#pragma once
// Stub: satisfies Arduino_GFX's Arduino_ESP32SPI.h which requires esp32-hal-periman.h
// from Arduino-ESP32 >=2.0.3. Our platform is older so we provide empty shims.
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP32_BUS_TYPE_INIT        = 0,
    ESP32_BUS_TYPE_GPIO        = 1,
    ESP32_BUS_TYPE_UART        = 2,
    ESP32_BUS_TYPE_SIGMADELTA  = 3,
    ESP32_BUS_TYPE_RMT         = 4,
    ESP32_BUS_TYPE_PSRAM       = 5,
    ESP32_BUS_TYPE_I2S_STD     = 6,
    ESP32_BUS_TYPE_I2S_TDM     = 7,
    ESP32_BUS_TYPE_I2S_PDM_TX  = 8,
    ESP32_BUS_TYPE_I2S_PDM_RX  = 9,
    ESP32_BUS_TYPE_I2C         = 10,
    ESP32_BUS_TYPE_SPI_MASTER  = 11,
    ESP32_BUS_TYPE_LEDC        = 12,
    ESP32_BUS_TYPE_TWAI        = 13,
    ESP32_BUS_TYPE_USB_DM      = 14,
    ESP32_BUS_TYPE_USB_DP      = 15,
    ESP32_BUS_TYPE_ETH_RMII    = 16,
    ESP32_BUS_TYPE_ETH_CLK     = 17,
    ESP32_BUS_TYPE_MAX
} peripheral_bus_type_t;

typedef bool (*perimanFreeCallback)(void *bus);

inline bool perimanSetPinBus(uint8_t pin, peripheral_bus_type_t type, void *bus) { return true; }
inline bool perimanSetPinBus(uint8_t pin, peripheral_bus_type_t type, void *bus, int8_t n, int8_t ch) { return true; }
inline void *perimanGetPinBus(uint8_t pin, peripheral_bus_type_t type) { return nullptr; }
inline bool perimanClearPinBus(uint8_t pin) { return true; }
inline const char *perimanGetTypeName(peripheral_bus_type_t type) { return ""; }
inline bool perimanPinIsValid(uint8_t pin) { return true; }

#ifdef __cplusplus
}
#endif
