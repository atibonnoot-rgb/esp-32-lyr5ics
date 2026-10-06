#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "lyrics.h"

class DisplayUI {
public:
    DisplayUI();
    void begin();
    
    void drawBootScreen(const String& status);
    void drawWaitingScreen(bool btConnected, bool wifiConnected);
    void drawLoadingLyrics(const String& title);
    void drawPlayer(
        const String& title,
        const String& artist,
        const LyricsManager& lyrics,
        uint32_t elapsedMs,
        uint32_t durationMs,
        bool isPlaying,
        bool btConnected,
        bool wifiConnected
    );

private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C _u8g2;
    int _animFrame;
    unsigned long _lastAnimTime;
    int _marqueeOffset;
    unsigned long _lastMarqueeTime;
    int _lastCurIdx;

    void drawDecoratedHeader(bool btConnected, bool wifiConnected, bool isPlaying);
    void drawEqualizer(int x, int y, bool isPlaying);
    void drawProgressBar(int x, int y, int width, int height, uint32_t elapsedMs, uint32_t durationMs);
    String formatTime(uint32_t ms);
};
