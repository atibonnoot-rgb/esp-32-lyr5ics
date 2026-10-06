#include "display.h"
#include "config.h"

DisplayUI::DisplayUI() : 
    _u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ I2C_SCL, /* data=*/ I2C_SDA),
    _animFrame(0),
    _lastAnimTime(0),
    _marqueeOffset(0),
    _lastMarqueeTime(0),
    _lastCurIdx(-1)
{}

void DisplayUI::begin() {
    _u8g2.begin();
    _u8g2.clearBuffer();
}

String DisplayUI::formatTime(uint32_t ms) {
    uint32_t totalSec = ms / 1000;
    uint32_t min = totalSec / 60;
    uint32_t sec = totalSec % 60;
    char buf[10];
    snprintf(buf, sizeof(buf), "%02u:%02u", min, sec);
    return String(buf);
}

void DisplayUI::drawEqualizer(int x, int y, bool isPlaying) {
    static const uint8_t heights[8][4] = {
        {2, 5, 7, 3},
        {4, 7, 3, 6},
        {7, 4, 6, 2},
        {5, 2, 7, 5},
        {3, 6, 2, 7},
        {6, 3, 5, 4},
        {4, 7, 4, 3},
        {2, 4, 6, 5}
    };

    if (isPlaying) {
        if (millis() - _lastAnimTime > 120) {
            _animFrame = (_animFrame + 1) % 8;
            _lastAnimTime = millis();
        }
    }

    for (int i = 0; i < 4; i++) {
        uint8_t h = isPlaying ? heights[_animFrame][i] : 2;
        _u8g2.drawVLine(x + (i * 3), y + (7 - h), h);
        _u8g2.drawVLine(x + (i * 3) + 1, y + (7 - h), h);
    }
}

void DisplayUI::drawDecoratedHeader(bool btConnected, bool wifiConnected, bool isPlaying) {
    drawEqualizer(2, 2, isPlaying);

    _u8g2.setFont(u8g2_font_5x7_tf);
    _u8g2.drawRBox(38, 1, 52, 9, 2);
    _u8g2.setDrawColor(0);
    _u8g2.drawStr(43, 8, "YT MUSIC");
    _u8g2.setDrawColor(1);

    int wx = 104;
    int wy = 8;
    if (wifiConnected) {
        _u8g2.drawPixel(wx, wy);
        _u8g2.drawCircle(wx, wy, 3, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
        _u8g2.drawCircle(wx, wy, 6, U8G2_DRAW_UPPER_RIGHT | U8G2_DRAW_UPPER_LEFT);
    } else {
        _u8g2.setFont(u8g2_font_5x7_tf);
        _u8g2.drawStr(wx - 2, wy, "X");
    }

    _u8g2.setFont(u8g2_font_4x6_tf);
    if (btConnected) {
        _u8g2.drawFrame(115, 1, 12, 8);
        _u8g2.drawStr(117, 7, "BT");
    } else {
        _u8g2.drawStr(115, 7, "--");
    }

    _u8g2.drawHLine(0, 11, 128);
}

void DisplayUI::drawProgressBar(int x, int y, int width, int height, uint32_t elapsedMs, uint32_t durationMs) {
    if (durationMs == 0) durationMs = 1;
    if (elapsedMs > durationMs) elapsedMs = durationMs;

    _u8g2.drawRFrame(x, y, width, height, 1);

    int fillW = (int)(((float)elapsedMs / (float)durationMs) * (width - 2));
    if (fillW > 0) {
        _u8g2.drawBox(x + 1, y + 1, fillW, height - 2);
    }
}

void DisplayUI::drawBootScreen(const String& status) {
    _u8g2.clearBuffer();
    _u8g2.drawRFrame(0, 0, 128, 64, 4);
    _u8g2.drawRFrame(2, 2, 124, 60, 2);

    _u8g2.setFont(u8g2_font_7x14B_tf);
    _u8g2.drawStr(22, 22, "ESP32 LYRICS");

    _u8g2.setFont(u8g2_font_5x7_tf);
    _u8g2.drawStr(34, 34, "Starting Up...");
    
    _u8g2.setFont(u8g2_font_4x6_tf);
    _u8g2.drawStr(10, 50, status.c_str());

    _u8g2.sendBuffer();
}

void DisplayUI::drawWaitingScreen(bool btConnected, bool wifiConnected) {
    _u8g2.clearBuffer();
    drawDecoratedHeader(btConnected, wifiConnected, false);

    _u8g2.drawRFrame(6, 16, 116, 44, 3);
    _u8g2.drawCircle(32, 38, 10);
    _u8g2.drawCircle(96, 38, 10);
    _u8g2.drawCircle(32, 38, 4);
    _u8g2.drawCircle(96, 38, 4);
    _u8g2.drawHLine(42, 38, 44);

    _u8g2.setFont(u8g2_font_5x7_tf);
    if (!btConnected) {
        _u8g2.drawStr(18, 25, "Waiting for Phone...");
        _u8g2.setFont(u8g2_font_4x6_tf);
        _u8g2.drawStr(22, 55, "Pair: " BT_DEVICE_NAME);
    } else {
        _u8g2.drawStr(26, 25, "Connected to BT!");
        _u8g2.setFont(u8g2_font_4x6_tf);
        _u8g2.drawStr(24, 55, "Play a song in YTM");
    }

    _u8g2.sendBuffer();
}

void DisplayUI::drawLoadingLyrics(const String& title) {
    _u8g2.clearBuffer();
    drawDecoratedHeader(true, true, true);

    _u8g2.drawRFrame(0, 14, 128, 48, 3);
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.drawStr(12, 30, "Fetching Lyrics...");

    _u8g2.setFont(u8g2_font_5x7_tf);
    String displayTitle = title.length() > 20 ? title.substring(0, 18) + ".." : title;
    _u8g2.drawStr(10, 48, displayTitle.c_str());

    _u8g2.sendBuffer();
}

void DisplayUI::drawPlayer(
    const String& title,
    const String& artist,
    const LyricsManager& lyrics,
    uint32_t elapsedMs,
    uint32_t durationMs,
    bool isPlaying,
    bool btConnected,
    bool wifiConnected
) {
    _u8g2.clearBuffer();

    drawDecoratedHeader(btConnected, wifiConnected, isPlaying);

    _u8g2.drawRFrame(0, 13, 128, 39, 2);
    _u8g2.drawBox(0, 13, 3, 3);
    _u8g2.drawBox(125, 13, 3, 3);
    _u8g2.drawBox(0, 49, 3, 3);
    _u8g2.drawBox(125, 49, 3, 3);

    if (lyrics.hasLyrics()) {
        int curIdx = lyrics.getCurrentIndex(elapsedMs);

        if (curIdx < 0) {
            // Intro before first lyric line
            _u8g2.setFont(u8g2_font_6x10_tf);
            _u8g2.drawStr(18, 30, "~ Intro / Music ~");
            if (lyrics.getLineCount() > 0) {
                const LyricLine* first = lyrics.getLine(0);
                if (first) {
                    _u8g2.setFont(u8g2_font_4x6_tf);
                    String preview = "Next: " + first->text;
                    if (preview.length() > 24) preview = preview.substring(0, 22) + "..";
                    _u8g2.drawStr(6, 45, preview.c_str());
                }
            }
        } else {
            const LyricLine* current = lyrics.getLine(curIdx);
            if (current) {
                _u8g2.setFont(u8g2_font_6x12_tf);
                int textWidth = _u8g2.getStrWidth(current->text.c_str());

                if (textWidth <= 120) {
                    // Line fits in single line: show previous, current, next
                    if (curIdx > 0) {
                        const LyricLine* prev = lyrics.getLine(curIdx - 1);
                        if (prev) {
                            _u8g2.setFont(u8g2_font_4x6_tf);
                            String pText = prev->text.length() > 24 ? prev->text.substring(0, 22) + ".." : prev->text;
                            _u8g2.drawStr(6, 21, pText.c_str());
                        }
                    }

                    _u8g2.setFont(u8g2_font_6x12_tf);
                    int x = (128 - textWidth) / 2;
                    if (x < 4) x = 4;
                    _u8g2.drawStr(x, 34, current->text.c_str());

                    if (curIdx + 1 < lyrics.getLineCount()) {
                        const LyricLine* next = lyrics.getLine(curIdx + 1);
                        if (next) {
                            _u8g2.setFont(u8g2_font_4x6_tf);
                            String nText = next->text.length() > 24 ? next->text.substring(0, 22) + ".." : next->text;
                            _u8g2.drawStr(6, 47, nText.c_str());
                        }
                    }
                } else {
                    // Long line: wrap into 2 lines cleanly so ALL words appear immediately!
                    String fullText = current->text;
                    int mid = fullText.length() / 2;
                    int splitIdx = fullText.indexOf(' ', mid - 3);
                    if (splitIdx == -1 || splitIdx > mid + 7) {
                        splitIdx = fullText.lastIndexOf(' ', mid);
                    }
                    if (splitIdx <= 0) splitIdx = mid;

                    String line1 = fullText.substring(0, splitIdx);
                    String line2 = fullText.substring(splitIdx);
                    line1.trim();
                    line2.trim();

                    _u8g2.setFont(u8g2_font_6x12_tf);
                    int w1 = _u8g2.getStrWidth(line1.c_str());
                    int w2 = _u8g2.getStrWidth(line2.c_str());
                    int x1 = (128 - w1) / 2; if (x1 < 4) x1 = 4;
                    int x2 = (128 - w2) / 2; if (x2 < 4) x2 = 4;

                    _u8g2.drawStr(x1, 27, line1.c_str());
                    _u8g2.drawStr(x2, 43, line2.c_str());
                }
            }
        }
    } else {
        _u8g2.setFont(u8g2_font_6x10_tf);
        String dispTitle = title.length() > 18 ? title.substring(0, 16) + ".." : title;
        _u8g2.drawStr(6, 26, dispTitle.c_str());

        _u8g2.setFont(u8g2_font_5x7_tf);
        String dispArtist = artist.length() > 20 ? artist.substring(0, 18) + ".." : artist;
        _u8g2.drawStr(6, 37, dispArtist.c_str());

        // Display exact live status message (e.g. Wi-Fi not connected / HTTP code / Searching)
        _u8g2.setFont(u8g2_font_4x6_tf);
        String status = "[" + lyrics.getStatusMessage() + "]";
        _u8g2.drawStr(6, 48, status.c_str());
    }

    _u8g2.setFont(u8g2_font_4x6_tf);
    String tElapsed = formatTime(elapsedMs);
    String tTotal = formatTime(durationMs);

    _u8g2.drawStr(2, 60, tElapsed.c_str());
    drawProgressBar(26, 56, 76, 5, elapsedMs, durationMs);
    _u8g2.drawStr(106, 60, tTotal.c_str());

    _u8g2.sendBuffer();
}
