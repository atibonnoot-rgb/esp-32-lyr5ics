#pragma once
#include <Arduino.h>

#define WIFI_SSID     "realme"
#define WIFI_PASSWORD "12345678"

#define BT_DEVICE_NAME "ESP32-Lyrics-Display"

#define I2C_SDA 21
#define I2C_SCL 22
#define OLED_ADDR 0x3C

#define MAX_LYRIC_LINES 150

#define PROXY_HOST "esp-32-lyr5ics.onrender.com"
#define PROXY_PORT 443

// Fine-tuning offset in milliseconds (+900ms shifts lyrics earlier so text updates as the singer starts)
#define LYRIC_OFFSET_MS 900




