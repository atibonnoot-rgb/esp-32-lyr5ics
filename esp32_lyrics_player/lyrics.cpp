#include "lyrics.h"
#include "config.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "esp_a2dp_api.h"

LyricsManager::LyricsManager() : _hasLyrics(false), _statusMessage("Ready") {}

void LyricsManager::clear() {
    _lines.clear();
    _lines.shrink_to_fit();
    _hasLyrics = false;
    _statusMessage = "No lyrics loaded";
}

int LyricsManager::getLineCount() const {
    return _lines.size();
}

const LyricLine* LyricsManager::getLine(int index) const {
    if (index >= 0 && index < (int)_lines.size()) {
        return &_lines[index];
    }
    return nullptr;
}

bool LyricsManager::hasLyrics() const {
    return _hasLyrics && !_lines.empty();
}

String LyricsManager::getStatusMessage() const {
    return _statusMessage;
}

String LyricsManager::cleanArtist(const String& rawArtist) {
    String cleaned = rawArtist;
    cleaned.replace(" - Topic", "");
    cleaned.replace(" - Official", "");
    cleaned.replace(" Official", "");
    cleaned.trim();
    return cleaned;
}

String LyricsManager::cleanSongTitle(const String& rawTitle) {
    String cleaned = rawTitle;
    
    const char* patterns[] = {
        "(Official Music Video)", "(Official Video)", "[Official Video]",
        "(Official Audio)", "[Official Audio]", "(Lyric Video)", "[Lyric Video]",
        "(Audio)", "[Audio]", "(Lyrics)", "[Lyrics]", "(HD)", "[4K]", "(Visualizer)",
        "(Official)", "[Official]", "(Music Video)", "[Music Video]",
        "(Vertical Video)", "[Vertical Video]", "(Lyrical Video)", "[Lyrical Video]"
    };
    
    for (const char* pattern : patterns) {
        int idx = -1;
        while ((idx = cleaned.indexOf(pattern)) >= 0) {
            cleaned.remove(idx, strlen(pattern));
        }
    }

    int dashIdx = cleaned.indexOf(" - ");
    if (dashIdx > 0 && dashIdx < (int)cleaned.length() - 3) {
        cleaned = cleaned.substring(dashIdx + 3);
    }

    cleaned.trim();
    return cleaned;
}

String LyricsManager::urlEncode(const String& str) {
    String encoded = "";
    char c;
    char code0;
    char code1;
    for (unsigned int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (isalnum(c)) {
            encoded += c;
        } else if (c == ' ') {
            encoded += "%20";
        } else {
            code1 = (c & 0xf) + '0';
            if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
            c = (c >> 4) & 0xf;
            code0 = c + '0';
            if (c > 9) code0 = c - 10 + 'A';
            encoded += '%';
            encoded += code0;
            encoded += code1;
        }
    }
    return encoded;
}

String LyricsManager::queryLrcLib(const String& path) {
    if (WiFi.status() != WL_CONNECTED) {
        _statusMessage = "Wi-Fi disconnected";
        return "";
    }

#if PROXY_PORT == 443
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(10000);
    const unsigned long reqTimeout = 10000;
#else
    WiFiClient client;
    client.setTimeout(3500);
    const unsigned long reqTimeout = 3500;
#endif

    Serial.printf("[HTTP] Connecting to fast proxy %s:%d...\n", PROXY_HOST, PROXY_PORT);
    if (!client.connect(PROXY_HOST, PROXY_PORT)) {
        _statusMessage = "Proxy Conn Err";
        Serial.printf("[HTTP] Connection to %s:%d failed!\n", PROXY_HOST, PROXY_PORT);
        return "";
    }

    // Send HTTP GET request
    String req = "GET " + path + " HTTP/1.1\r\n" +
                 "Host: " + String(PROXY_HOST) + ":" + String(PROXY_PORT) + "\r\n" +
                 "Connection: close\r\n\r\n";
    client.print(req);

    // Wait for response header
    unsigned long startRead = millis();
    while (!client.available() && client.connected() && (millis() - startRead < reqTimeout)) {
        delay(10);
    }

    if (!client.available()) {
        _statusMessage = "Read Timeout";
        client.stop();
        return "";
    }

    // Read status line
    String statusLine = client.readStringUntil('\n');
    Serial.println("[HTTP] " + statusLine);

    if (statusLine.indexOf("200") == -1) {
        if (statusLine.indexOf("404") != -1) {
            _statusMessage = "No Synced Lyrics";
        } else {
            _statusMessage = "HTTP Err";
        }
        client.stop();
        return "";
    }

    // Skip HTTP headers (find empty line)
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) break;
    }

    // The rest of the stream is pure plain-text LRC!
    String lrc = client.readString();
    client.stop();
    return lrc;
}

bool LyricsManager::fetchLyrics(const String& rawArtist, const String& rawTrack, uint32_t durationSec) {
    if (WiFi.status() != WL_CONNECTED) {
        _statusMessage = "Wi-Fi disconnected";
        Serial.println("[LYRICS] Wi-Fi not connected!");
        return false;
    }

    String track = cleanSongTitle(rawTrack);
    String artist = cleanArtist(rawArtist);
    _statusMessage = "Searching...";
    clear();

    Serial.printf("[LYRICS] Fetching: '%s' by '%s' (dur: %u s)\n", track.c_str(), artist.c_str(), durationSec);

    // Single request: proxy performs duration match, exact match, and search fallback in milliseconds
    String path = "/api/get?artist_name=" + urlEncode(artist) + 
                  "&track_name=" + urlEncode(track) + 
                  "&duration=" + String(durationSec);

    String lrcContent = queryLrcLib(path);

    if (lrcContent.length() > 0 && lrcContent.indexOf('[') != -1) {
        parseLrc(lrcContent);
        if (!_lines.empty()) {
            _hasLyrics = true;
            _statusMessage = "Synced!";
            Serial.printf("[LYRICS] Loaded %d lines\n", (int)_lines.size());
            return true;
        }
    }

    if (_statusMessage == "Searching...") {
        _statusMessage = "No Synced Lyrics";
    }
    Serial.printf("[LYRICS] Final status: %s\n", _statusMessage.c_str());
    return false;
}

void LyricsManager::parseLrc(const String& lrcData) {
    _lines.clear();
    int startIdx = 0;

    while (startIdx < (int)lrcData.length() && (int)_lines.size() < MAX_LYRIC_LINES) {
        int endIdx = lrcData.indexOf('\n', startIdx);
        if (endIdx == -1) endIdx = lrcData.length();

        String line = lrcData.substring(startIdx, endIdx);
        line.trim();

        if (line.startsWith("[") && line.indexOf(']') > 0) {
            int closeBracket = line.indexOf(']');
            String timeTag = line.substring(1, closeBracket);
            String lyricText = line.substring(closeBracket + 1);
            lyricText.trim();

            int colonIdx = timeTag.indexOf(':');
            if (colonIdx > 0) {
                int min = timeTag.substring(0, colonIdx).toInt();
                float sec = timeTag.substring(colonIdx + 1).toFloat();
                uint32_t ms = (uint32_t)((min * 60 + sec) * 1000.0f);

                if (lyricText.length() > 0) {
                    LyricLine item;
                    item.timestampMs = ms;
                    item.text = lyricText;
                    _lines.push_back(item);
                }
            }
        }

        startIdx = endIdx + 1;
    }
}

int LyricsManager::getCurrentIndex(uint32_t elapsedMs) const {
    if (_lines.empty()) return -1;
    if (elapsedMs < _lines[0].timestampMs) return -1;

    int low = 0;
    int high = _lines.size() - 1;
    int ans = 0;

    while (low <= high) {
        int mid = low + (high - low) / 2;
        if (_lines[mid].timestampMs <= elapsedMs) {
            ans = mid;
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return ans;
}


