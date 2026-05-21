/*
 * ESP32-S3 Internet Radio
 * ════════════════════════════════════════════════════════════════
 * Board   : ESP32-S3-DevKitC-1  N16R8  (16 MB Flash, 8 MB PSRAM)
 * Library : schreibfaul1 / ESP32-audioI2S
 *
 * MAX98357A (I2S Verstärker)
 *   GPIO4  → BCLK
 *   GPIO5  → LRC
 *   GPIO6  → DIN
 *   3.3 V  → VIN
 *   GND    → GND   (SD-Pin floating lassen!)
 *
 * Taster (→ GND, INPUT_PULLUP)
 *   GPIO1  → Senderwechsel
 *   GPIO2  → Mute
 *   GPIO41 → Volume +
 *   GPIO42 → Volume -
 * ════════════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
const char* ssid     = "FRITZ!Box 5530 HC";
const char* password = "63581191284534172960";

// ── MAX98357A → I2S pins ──────────────────────────────────────────────────────
#define I2S_BCLK  4
#define I2S_LRC   5
#define I2S_DOUT  6

// ── Taster (INPUT_PULLUP, active LOW) ─────────────────────────────────────────
#define BTN_NEXT     1
#define BTN_MUTE     2
#define BTN_VOL_UP   41
#define BTN_VOL_DOWN 42

// ── Sender ────────────────────────────────────────────────────────────────────
const char* stations[] = {
    "http://radiorecord.hostingradio.ru/rr_main96.aacp",
    "http://mp3.ffh.de/radioffh/hqlivestream.mp3",
    "http://mp3.planetradio.de/planetradio/hqlivestream.mp3",
    "http://radiorecord.hostingradio.ru/rus96.aacp"
};
const int NUM_STATIONS = sizeof(stations) / sizeof(stations[0]);

// ── Zustand ───────────────────────────────────────────────────────────────────
Audio audio;
int   currentStation = 0;
int   volume         = 12;   // 0–21
bool  muted          = false;

// Gesetzt vom EOF-Callback, ausgewertet im Loop (verhindert Re-Entrancy)
volatile bool pendingStreamEnd = false;

// ── Taster-Debounce ───────────────────────────────────────────────────────────
struct Btn { uint8_t pin; bool last = HIGH, state = HIGH; uint32_t t = 0; };
Btn btns[] = {{BTN_NEXT}, {BTN_MUTE}, {BTN_VOL_UP}, {BTN_VOL_DOWN}};

bool pressed(Btn& b) {
    bool r = digitalRead(b.pin);
    if (r != b.last) b.t = millis();
    b.last = r;
    if ((millis() - b.t) > 50 && r != b.state) {
        b.state = r;
        if (b.state == LOW) return true;
    }
    return false;
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────
void printSeparator() { Serial.println("─────────────────────────────────────────"); }

void playStation() {
    printSeparator();
    Serial.printf("[Radio] ▶ Station %d/%d\n", currentStation + 1, NUM_STATIONS);
    Serial.printf("[Radio]   %s\n", stations[currentStation]);
    printSeparator();
    audio.stopSong();
    audio.connecttohost(stations[currentStation]);
    audio.setVolume(muted ? 0 : volume);
}

void connectWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("[WiFi]  Verbinde mit \"");
    Serial.print(ssid);
    Serial.print("\"");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    WiFi.setSleep(false);
    Serial.printf("\n[WiFi]  ✓ Verbunden\n");
    Serial.printf("[WiFi]  IP  : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi]  RSSI: %d dBm\n", WiFi.RSSI());
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    printSeparator();
    Serial.println("[Boot]  ESP32-S3 Internet Radio");
    Serial.printf("[Boot]  Chip: %s  Rev: %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("[Boot]  Flash: %d MB  PSRAM: %d KB\n",
                  ESP.getFlashChipSize() / (1024 * 1024),
                  ESP.getPsramSize() / 1024);
    Serial.printf("[Boot]  Free Heap: %d Bytes\n", ESP.getFreeHeap());
    printSeparator();

    for (auto& b : btns) pinMode(b.pin, INPUT_PULLUP);

    connectWiFi();

    Serial.println("[Audio] Initialisiere I2S ...");
    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setAudioTaskCore(1);  // Core 1 = weg vom WiFi-Stack (Core 0) → kein Stottern
    audio.setVolume(volume);
    Serial.printf("[Audio] BCLK=GPIO%d  LRC=GPIO%d  DOUT=GPIO%d\n", I2S_BCLK, I2S_LRC, I2S_DOUT);
    Serial.printf("[Audio] Startlautstärke: %d/21\n", volume);

    playStation();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    audio.loop();

    // ── Debug: alle 5s Status ausgeben ────────────────────────────────────────
    static uint32_t loopCount = 0;
    static uint32_t lastDebug = 0;
    loopCount++;
    if (millis() - lastDebug >= 5000) {
        lastDebug = millis();
        Serial.printf("[Debug] Loops/s: %5lu  Heap: %6d  RSSI: %3d dBm  Audio: %s\n",
            loopCount / 5,
            ESP.getFreeHeap(),
            WiFi.RSSI(),
            audio.isRunning() ? "läuft" : "STOP");
        loopCount = 0;
    }

    if (pendingStreamEnd) {
        pendingStreamEnd = false;
        // Nur reconnecten, kein stopSong/Volume-Reset → kein Lautstärke-Zyklus
        audio.connecttohost(stations[currentStation]);
    }

    if (WiFi.status() != WL_CONNECTED) {
        printSeparator();
        Serial.println("[WiFi]  ✗ Verbindung verloren – reconnect ...");
        connectWiFi();
        playStation();
    }

    if (pressed(btns[0])) {
        currentStation = (currentStation + 1) % NUM_STATIONS;
        Serial.println("[BTN]   Senderwechsel gedrückt");
        playStation();
    }

    if (pressed(btns[1])) {
        muted = !muted;
        audio.setVolume(muted ? 0 : volume);
        Serial.printf("[BTN]   Mute → %s\n", muted ? "AN (stumm)" : "AUS (Ton)");
    }

    if (pressed(btns[2])) {
        if (!muted && volume < 21) {
            audio.setVolume(++volume);
            Serial.printf("[BTN]   Volume +  → %d/21\n", volume);
        } else if (volume >= 21) {
            Serial.println("[BTN]   Volume +  → Maximum erreicht (21)");
        }
    }

    if (pressed(btns[3])) {
        if (!muted && volume > 0) {
            audio.setVolume(--volume);
            Serial.printf("[BTN]   Volume -  → %d/21\n", volume);
        } else if (volume <= 0) {
            Serial.println("[BTN]   Volume -  → Minimum erreicht (0)");
        }
    }
}

// ── Callbacks ─────────────────────────────────────────────────────────────────
void audio_info(const char* info)            { Serial.printf("[Info]    %s\n", info); }
void audio_showstation(const char* info)     { Serial.printf("[Sender]  %s\n", info); }
void audio_showstreamtitle(const char* info) { Serial.printf("[Titel]   %s\n", info); }
void audio_bitrate(const char* info)         { Serial.printf("[Bitrate] %s\n", info); }
void audio_commercial(const char* info)      { Serial.printf("[Werbung] %s\n", info); }
void audio_icyurl(const char* info)          { Serial.printf("[ICY-URL] %s\n", info); }
void audio_lasthost(const char* info)        { Serial.printf("[Host]    %s\n", info); }
void audio_id3data(const char* info)         { Serial.printf("[ID3]     %s\n", info); }
void audio_eof_stream(const char* info)      { Serial.printf("[EOF]     %s\n", info); pendingStreamEnd = true; }
void audio_error(const char* info)           { Serial.printf("[ERROR]   %s\n", info); }
