#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* ssid = "PK";
const char* password = "244466666";

#define BOT_TOKEN "9191078773:AAF_3xVEtq_b021a4jEtq9kPWI7qgUUgecY"
#define CHAT_ID "8719633048"

// Pins
#define SOUND_PIN 27
#define GREEN_LED 25
#define RED_LED   2
#define BUZZER    33

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

const unsigned long notificationInterval = 5000;  // min gap between Telegram messages
const unsigned long alertHoldTime = 3000;         // keep alert on this long after last noise

unsigned long lastNotification = 0;
unsigned long lastNoiseTime = 0;
bool firstAlert = true;
bool noiseEverSeen = false;

void setAlertOutputs(bool alertActive) {
  digitalWrite(RED_LED, alertActive ? HIGH : LOW);
  digitalWrite(GREEN_LED, alertActive ? LOW : HIGH);

  if (alertActive) {
    // Beep pattern: 200 ms on, 200 ms off
    digitalWrite(BUZZER, (millis() / 200) % 2 == 0 ? HIGH : LOW);
  } else {
    digitalWrite(BUZZER, LOW);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(SOUND_PIN, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  setAlertOutputs(false);  // green on at startup

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());

  client.setInsecure();

  bool ok = bot.sendMessage(CHAT_ID,
    "🏫 IoT Smart Classroom System Started!\nNoise Monitoring Active.", "");
  Serial.println(ok ? "Startup message SENT" : "Startup message FAILED");
}

void loop() {
  // Reconnect WiFi if dropped (local alerts still work below)
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
  }

  bool noiseDetected = (digitalRead(SOUND_PIN) == LOW);  // change to HIGH if your module is active HIGH

  if (noiseDetected) {
    lastNoiseTime = millis();
    noiseEverSeen = true;
  }

  bool alertActive = noiseEverSeen && (millis() - lastNoiseTime < alertHoldTime);

  // Update LEDs and buzzer first so they react instantly
  setAlertOutputs(alertActive);

  // Telegram notification (rate limited)
  if (noiseDetected && WiFi.status() == WL_CONNECTED &&
      (firstAlert || millis() - lastNotification >= notificationInterval)) {

    Serial.println("Noise detected, sending...");
    bool ok = bot.sendMessage(CHAT_ID,
      "🔊 Classroom Noise Detected!\n⚠️ Students, please maintain silence.", "");
    Serial.println(ok ? "Telegram notification SENT" : "Telegram notification FAILED");

    lastNotification = millis();
    firstAlert = false;
  }

  delay(20);
}