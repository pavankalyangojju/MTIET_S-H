
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// WiFi Details
const char* ssid = "PK";
const char* password = "244466666";

// Telegram Bot Details
#define BOT_TOKEN "9197802903:AAFXnBIC3t4GNEsFGNUrV3zlumLGX7sR2kU"
#define CHAT_ID "8719633048"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Pin Configuration
#define PIR_PIN 27
#define GREEN_LED 25
#define RED_LED 26
#define BUZZER 14

// Timing
const unsigned long ALERT_DURATION = 10000;

// Sensor and alert variables
bool previousMotion = false;
bool alertActive = false;

unsigned long alertStartTime = 0;

// Telegram notification flags
bool telegramPending = false;
String telegramMessage = "";

void sendTelegram(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    if (bot.sendMessage(CHAT_ID, message, "")) {
      Serial.println("Telegram notification sent!");
    } else {
      Serial.println("Telegram notification failed!");
    }
  }
}

void setup() {

  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  digitalWrite(GREEN_LED, HIGH);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);

  // WiFi Connection
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");

  client.setInsecure();

  // PIR warm-up
  Serial.println("PIR Sensor Warming Up...");
  delay(30000);

  // Initialize sensor state
  previousMotion = digitalRead(PIR_PIN);

  if (previousMotion == HIGH) {
    Serial.println("Motion already detected at startup!");
    alertActive = true;
    alertStartTime = millis();

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER, HIGH);
  } else {
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
  }

  Serial.println("System Ready!");

telegramPending = true;

telegramMessage =
    "🚨 MOTION DETECTED!\n"
    "Movement detected in the monitored area.\n"
    "Red LED and buzzer activated for 10 seconds.";
}

void loop() {

  unsigned long currentTime = millis();

  // Read PIR sensor continuously
  bool motion = digitalRead(PIR_PIN);

  // Detect new motion immediately
  if (motion == HIGH && previousMotion == LOW) {

    Serial.println("MOTION DETECTED!");

    // Start alert
    alertActive = true;
    alertStartTime = currentTime;

    // Activate Red LED and buzzer
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);
    digitalWrite(BUZZER, HIGH);

    // Queue Telegram message
    telegramPending = true;
    telegramMessage =
      "MOTION DETECTED!\n"
      "Movement detected in the monitored area.\n"
      "Red LED and buzzer activated for 10 seconds.";
  }

  // Detect when motion stops
  if (motion == LOW && previousMotion == HIGH) {
    Serial.println("No Motion Detected!");
  }

  previousMotion = motion;

  // Turn off alert after 10 seconds
  if (alertActive &&
      currentTime - alertStartTime >= ALERT_DURATION) {

    digitalWrite(RED_LED, LOW);
    digitalWrite(BUZZER, LOW);
    digitalWrite(GREEN_LED, HIGH);

    alertActive = false;

    Serial.println("10 Seconds Completed!");
    Serial.println("Green LED ON - System Ready");
  }

  // Send Telegram notification separately
  // Only send when WiFi is connected
  if (telegramPending && WiFi.status() == WL_CONNECTED) {

    telegramPending = false;

    sendTelegram(telegramMessage);
  }

  // No unnecessary delay
}
