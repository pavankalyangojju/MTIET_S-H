#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// ---------- Settings ----------
#define USE_TELEGRAM true          // set false to run OLED only

const char* ssid     = "YG";
const char* password = "1234567890";
#define BOT_TOKEN "9248187263:AAEbaQAgI5nJf-Rzn-so9sjyjbkQNLPY37s"
#define CHAT_ID   "9226671534"

#define PULSE_PIN 34
#define SCREEN_W 128
#define SCREEN_H 64

const unsigned long telegramInterval = 30000;  // send BPM every 30 s
const int highBpm = 120;
const int lowBpm  = 50;

// ---------- Objects ----------
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// ---------- Signal / BPM ----------
int wave[SCREEN_W];                 // scrolling waveform buffer
int bpm = 0;
unsigned long lastBeat = 0;
unsigned long lastSample = 0;
unsigned long lastTelegram = 0;
unsigned long lastBeatSeen = 0;
bool aboveThreshold = false;
int bpmHistory[5] = {0};
int bpmIndex = 0;
int sampleCount = 0;

int readPulse() {
  long sum = 0;
  for (int i = 0; i < 4; i++) sum += analogRead(PULSE_PIN);
  return sum / 4;
}

void drawScreen(bool fingerPresent, int minV, int maxV) {
  display.clearDisplay();

  if (!fingerPresent) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(18, 20);
    display.println("Place finger on");
    display.setCursor(30, 34);
    display.println("the sensor");
    display.display();
    return;
  }

  // Waveform (top area, y = 0..43)
  int range = max(maxV - minV, 1);
  for (int x = 1; x < SCREEN_W; x++) {
    int y1 = 43 - ((wave[x - 1] - minV) * 43L / range);
    int y2 = 43 - ((wave[x]     - minV) * 43L / range);
    display.drawLine(x - 1, y1, x, y2, SSD1306_WHITE);
  }

  // BPM text (bottom)
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 48);
  if (bpm > 0) {
    display.print(bpm);
    display.print(" BPM");
  } else {
    display.print("-- BPM");
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(PULSE_PIN, ADC_11db);

  Wire.begin(21, 22);
  Wire.setClock(400000);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found! Check wiring / address (try 0x3D)");
    while (true) delay(1000);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 28);
  display.println("IoT Heart Rate");
  display.display();

  for (int i = 0; i < SCREEN_W; i++) wave[i] = 2000;

#if USE_TELEGRAM
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? "\nWiFi Connected" : "\nWiFi FAILED");
  client.setInsecure();
  if (WiFi.status() == WL_CONNECTED) {
    bool ok = bot.sendMessage(CHAT_ID, "❤️ IoT Heart Rate Monitor Started", "");
    Serial.println(ok ? "Startup message SENT" : "Startup message FAILED");
  }
#endif
}

void loop() {
  // Sample every 20 ms (50 Hz)
  if (millis() - lastSample < 20) return;
  lastSample = millis();

  int v = readPulse();

  // Shift waveform left and add new sample
  memmove(wave, wave + 1, (SCREEN_W - 1) * sizeof(int));
  wave[SCREEN_W - 1] = v;

  // Min / max over the visible window
  int minV = 4095, maxV = 0;
  for (int i = 0; i < SCREEN_W; i++) {
    if (wave[i] < minV) minV = wave[i];
    if (wave[i] > maxV) maxV = wave[i];
  }

  // A real finger pulse gives a clear swing; a bare sensor does not
  bool fingerPresent = (maxV - minV) > 150;

  // Beat detection: rising edge across 60% of the range
  int threshold = minV + (maxV - minV) * 6 / 10;
  if (fingerPresent) {
    if (v > threshold && !aboveThreshold && millis() - lastBeat > 300) {
      unsigned long now = millis();
      if (lastBeat > 0) {
        unsigned long ibi = now - lastBeat;
        int instBpm = 60000 / ibi;
        if (instBpm >= 40 && instBpm <= 180) {
          bpmHistory[bpmIndex++ % 5] = instBpm;
          int sum = 0, n = 0;
          for (int i = 0; i < 5; i++) if (bpmHistory[i] > 0) { sum += bpmHistory[i]; n++; }
          bpm = sum / n;
          lastBeatSeen = now;
        }
      }
      lastBeat = now;
      aboveThreshold = true;
    }
    if (v < threshold) aboveThreshold = false;

    // Clear BPM if no valid beat for 5 s
    if (bpm > 0 && millis() - lastBeatSeen > 5000) bpm = 0;
  } else {
    bpm = 0;
    lastBeat = 0;
    for (int i = 0; i < 5; i++) bpmHistory[i] = 0;
  }

  // Redraw every 2nd sample (~25 fps)
  if (++sampleCount % 2 == 0) drawScreen(fingerPresent, minV, maxV);

  Serial.println(v);  // open Serial Plotter to see the raw waveform

#if USE_TELEGRAM
  if (bpm > 0 && WiFi.status() == WL_CONNECTED &&
      millis() - lastTelegram >= telegramInterval) {
    String msg = "❤️ Heart Rate: " + String(bpm) + " BPM";
    if (bpm > highBpm) msg += "\n⚠️ High heart rate!";
    if (bpm < lowBpm)  msg += "\n⚠️ Low heart rate!";
    bool ok = bot.sendMessage(CHAT_ID, msg, "");
    Serial.println(ok ? "Telegram SENT" : "Telegram FAILED");
    lastTelegram = millis();
  }
#endif
}