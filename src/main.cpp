#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>

// ── WiFi ──────────────────────────────────────────────────────────────────────
const char* ssid     = "FRITZ!Box 5530 HC";
const char* password = "63581191284534172960";

// ── MAX98357A → I2S pins ──────────────────────────────────────────────────────
#define I2S_BCLK  4   // GPIO4 → BCLK
#define I2S_LRC   5   // GPIO5 → LRC
#define I2S_DOUT  6   // GPIO6 → DIN

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
void playStation() {
    Serial.printf("[Radio] Station %d: %s\n", currentStation + 1, stations[currentStation]);
    audio.connecttohost(stations[currentStation]);
}

void connectWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("[WiFi]  Verbinde");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.printf("\n[WiFi]  Verbunden – IP: %s\n", WiFi.localIP().toString().c_str());
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    for (auto& b : btns) pinMode(b.pin, INPUT_PULLUP);

    connectWiFi();

    audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
    audio.setVolume(volume);
    playStation();
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    audio.loop();

    // WiFi-Reconnect
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi]  Verbindung verloren – reconnect ...");
        connectWiFi();
        playStation();
    }

    // Nächster Sender
    if (pressed(btns[0])) {
        currentStation = (currentStation + 1) % NUM_STATIONS;
        playStation();
    }

    // Mute-Toggle
    if (pressed(btns[1])) {
        muted = !muted;
        audio.setVolume(muted ? 0 : volume);
        Serial.printf("[Radio] %s\n", muted ? "Muted" : "Unmuted");
    }

    // Lauter
    if (pressed(btns[2]) && !muted && volume < 21) {
        audio.setVolume(++volume);
        Serial.printf("[Radio] Lautstärke: %d\n", volume);
    }

    // Leiser
    if (pressed(btns[3]) && !muted && volume > 0) {
        audio.setVolume(--volume);
        Serial.printf("[Radio] Lautstärke: %d\n", volume);
    }
}

// ── schreibfaul Callbacks ─────────────────────────────────────────────────────
void audio_info(const char* info)            { Serial.printf("[Info]    %s\n", info); }
void audio_showstation(const char* info)     { Serial.printf("[Sender]  %s\n", info); }
void audio_showstreamtitle(const char* info) { Serial.printf("[Titel]   %s\n", info); }
void audio_eof_stream(const char* info)      { Serial.printf("[EOF]     %s – reconnect\n", info); playStation(); }
