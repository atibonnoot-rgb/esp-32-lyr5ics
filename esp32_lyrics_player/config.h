#pragma once
#include <Arduino.h>

#define WIFI_SSID     "Sreedhar2g"
#define WIFI_PASSWORD "abram123"

#define BT_DEVICE_NAME "ESP32-Lyrics-Display"

#define I2C_SDA 21
#define I2C_SCL 22
#define OLED_ADDR 0x3C

#define MAX_LYRIC_LINES 150

#define PROXY_HOST "192.168.29.56"
#define PROXY_PORT 8080

// Fine-tuning offset in milliseconds (+900ms shifts lyrics earlier so text updates as the singer starts)
#define LYRIC_OFFSET_MS 900


