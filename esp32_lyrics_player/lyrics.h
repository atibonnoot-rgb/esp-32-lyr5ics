#pragma once
#include <Arduino.h>
#include <vector>

struct LyricLine {
    uint32_t timestampMs;
    String text;
};

class LyricsManager {
public:
    LyricsManager();
    void clear();
    bool fetchLyrics(const String& artist, const String& track, uint32_t durationSec = 0);
    int getCurrentIndex(uint32_t elapsedMs) const;

    int getLineCount() const;
    const LyricLine* getLine(int index) const;
    bool hasLyrics() const;
    String getStatusMessage() const;

private:
    std::vector<LyricLine> _lines;
    bool _hasLyrics;
    String _statusMessage;

    void parseLrc(const String& lrcData);
    String cleanSongTitle(const String& rawTitle);
    String cleanArtist(const String& rawArtist);
    String urlEncode(const String& str);
    String queryLrcLib(const String& url);
};
