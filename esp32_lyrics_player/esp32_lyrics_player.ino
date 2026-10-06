#include <Arduino.h>
#include <WiFi.h>
#include "esp_bt.h"
#include "BluetoothA2DPSink.h"
#include "config.h"
#include "display.h"
#include "lyrics.h"

// Objects
BluetoothA2DPSink a2dp_sink;
DisplayUI display;
LyricsManager lyricsMgr;

// State Variables
String currentTitle = "";
String currentArtist = "";
String pendingTitle = "";
String pendingArtist = "";
bool metadataChanged = false;
unsigned long lastMetadataTime = 0;

uint32_t trackDurationMs = 0;
uint32_t currentPositionMs = 0;
unsigned long lastPositionUpdate = 0;
unsigned long lastWifiRetry = 0;
bool isPlaying = false;
bool btConnected = false;

// AVRCP Metadata Callback (called when song changes)
void avrc_metadata_callback(uint8_t id, const uint8_t *text) {
    String val = String((const char*)text);
    
    if (id == ESP_AVRC_MD_ATTR_TITLE) {
        Serial.printf("[AVRCP] Title: %s\n", val.c_str());
        pendingTitle = val;
        metadataChanged = true;
        lastMetadataTime = millis();
    } else if (id == ESP_AVRC_MD_ATTR_ARTIST) {
        Serial.printf("[AVRCP] Artist: %s\n", val.c_str());
        pendingArtist = val;
        metadataChanged = true;
        lastMetadataTime = millis();
    } else if (id == ESP_AVRC_MD_ATTR_PLAYING_TIME) {
        trackDurationMs = val.toInt();
        metadataChanged = true;
        lastMetadataTime = millis();
        Serial.printf("[AVRCP] Duration: %u ms\n", trackDurationMs);
    }
}

// AVRCP Playback Position Callback (called periodically by phone)
void avrc_play_pos_callback(uint32_t play_pos) {
    currentPositionMs = play_pos;
    lastPositionUpdate = millis();
    Serial.printf("[AVRCP POS] %u ms\n", play_pos);
}

// Bluetooth Connection State Callback
void connection_state_changed(esp_a2d_connection_state_t state, void *ptr) {
    if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
        btConnected = true;
        Serial.println("[BT] Connected to device!");
    } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
        btConnected = false;
        isPlaying = false;
        currentTitle = "";
        currentArtist = "";
        lyricsMgr.clear();
        Serial.println("[BT] Disconnected.");
    }
}

// Bluetooth Audio State Callback (Play / Pause)
void audio_state_changed(esp_a2d_audio_state_t state, void *ptr) {
    if (state == ESP_A2D_AUDIO_STATE_STARTED) {
        isPlaying = true;
        lastPositionUpdate = millis();
        Serial.println("[BT] Audio Playing");
    } else {
        isPlaying = false;
        Serial.println("[BT] Audio Paused / Stopped");
    }
}

// Dummy audio reader callback: discards incoming PCM audio packets
// This avoids initializing I2S on GPIO 22 (which is already used by the OLED I2C SCL)
void dummy_audio_reader(const uint8_t *data, uint32_t length) {
    (void)data;
    (void)length;
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n--- ESP32 YouTube Music Lyrics Display ---");

    // 0. Release unused BLE Controller RAM to recover ~25KB contiguous internal heap for TLS
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);

    // 1. Initialize 0.96" OLED
    display.begin();
    display.drawBootScreen("Connecting Wi-Fi...");

    // 2. Connect to Wi-Fi
    WiFi.useStaticBuffers(true);
    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, IPAddress(8, 8, 8, 8), IPAddress(1, 1, 1, 1));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(WIFI_SSID);

    unsigned long wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart < 12000)) {
        delay(250);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[Wi-Fi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        display.drawBootScreen("Wi-Fi Connected!");
    } else {
        Serial.println("\n[Wi-Fi] Failed to connect (will retry in background)");
        display.drawBootScreen("Wi-Fi Offline");
    }
    delay(800);

    // 3. Configure Bluetooth A2DP Sink & AVRCP
    a2dp_sink.set_event_queue_size(5);
    a2dp_sink.set_event_stack_size(2048);
    a2dp_sink.set_avrc_metadata_callback(avrc_metadata_callback);
    a2dp_sink.set_avrc_rn_play_pos_callback(avrc_play_pos_callback, 1);
    a2dp_sink.set_on_connection_state_changed(connection_state_changed);
    a2dp_sink.set_on_audio_state_changed(audio_state_changed);

    // Disable I2S output so it doesn't take over GPIO 22 (OLED SCL)
    a2dp_sink.set_stream_reader(dummy_audio_reader, false);

    // Start Bluetooth Sink with configured name
    a2dp_sink.start(BT_DEVICE_NAME);
    Serial.printf("[BT] Started listening as '%s'\n", BT_DEVICE_NAME);
}

void loop() {
    bool wifiOk = (WiFi.status() == WL_CONNECTED);

    // Reconnect Wi-Fi in background if connection dropped
    if (!wifiOk && (millis() - lastWifiRetry > 10000)) {
        lastWifiRetry = millis();
        Serial.println("[Wi-Fi] Reconnecting...");
        WiFi.reconnect();
    }

    // Check if new song metadata arrived and settled (600ms debounce)
    if (metadataChanged && (millis() - lastMetadataTime > 600)) {
        metadataChanged = false;

        if (pendingTitle.length() > 0 && (pendingTitle != currentTitle || pendingArtist != currentArtist)) {
            currentTitle = pendingTitle;
            currentArtist = pendingArtist;
            currentPositionMs = 0;
            lastPositionUpdate = millis();

            // Immediately show loading on screen
            display.drawLoadingLyrics(currentTitle);

            Serial.printf("[LYRICS] Fetching for '%s' by '%s'...\n", currentTitle.c_str(), currentArtist.c_str());
            // Fetch lyrics directly in loop (uses loop's stack, 0 extra RAM)
            lyricsMgr.fetchLyrics(currentArtist, currentTitle, trackDurationMs / 1000);
            Serial.printf("[LYRICS] Loaded %d lines\n", lyricsMgr.getLineCount());
            lastPositionUpdate = millis();
        }
    }

    // Calculate interpolated playback position for smooth lyric scrolling
    uint32_t elapsedMs = currentPositionMs;
    if (isPlaying) {
        elapsedMs += (millis() - lastPositionUpdate);
        if (trackDurationMs > 0 && elapsedMs > trackDurationMs) {
            elapsedMs = trackDurationMs;
        }
    }
#ifdef LYRIC_OFFSET_MS
    if ((int32_t)elapsedMs + LYRIC_OFFSET_MS > 0) {
        elapsedMs = (uint32_t)((int32_t)elapsedMs + LYRIC_OFFSET_MS);
    }
#endif

    // Render OLED screen based on current state
    if (!btConnected || currentTitle.length() == 0) {
        display.drawWaitingScreen(btConnected, wifiOk);
    } else {
        display.drawPlayer(
            currentTitle,
            currentArtist,
            lyricsMgr,
            elapsedMs,
            trackDurationMs,
            isPlaying,
            btConnected,
            wifiOk
        );
    }

    // ~30 FPS frame rate
    delay(33);
}
