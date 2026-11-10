#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// =====================================
// WIFI DETAILS
// =====================================

const char* ssid = "PK";
const char* password = "244466666";

// =================================
// TELEGRAM DETAILS
// =================================

#define BOT_TOKEN "9197802903:AAFXnBIC3t4GNEsFGNUrV3zlumLGX7sR2kU"
#define CHAT_ID "8719633048"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// =====================================
// ESP32 PIN CONFIGURATION
// =====================================

#define MQ2_PIN 34
#define FLAME_PIN 27

#define BUZZER_PIN 25
#define RED_LED 4
#define GREEN_LED 15

// =====================================
// SENSOR SETTINGS
// =====================================

// Adjust this using your Serial Monitor readings
int smokeThreshold = 1500;

// Most flame sensor modules output LOW
// when flame is detected.
#define FLAME_DETECTED LOW

// =====================================
// GLOBAL VARIABLES
// =====================================

bool previousSmokeState = false;
bool previousFlameState = false;

unsigned long lastTelegramCheck = 0;
const unsigned long telegramInterval = 1000;

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);

  analogReadResolution(12);
  analogSetPinAttenuation(MQ2_PIN, ADC_11db);

  pinMode(MQ2_PIN, INPUT);
  pinMode(FLAME_PIN, INPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Initial SAFE indication
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  // Connect to WiFi
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  client.setInsecure();

  Serial.println("================================");
  Serial.println("IoT FIRE & SMOKE ALERT SYSTEM");
  Serial.println("System Ready");
  Serial.println("================================");
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

  int smokeValue = analogRead(MQ2_PIN);
  int flameValue = digitalRead(FLAME_PIN);

  // Sensor detection
  bool smokeDetected = smokeValue > smokeThreshold;
  bool flameDetected = (flameValue == FLAME_DETECTED);

  bool alertState = smokeDetected || flameDetected;

  // ===================================
  // SERIAL MONITOR
  // ===================================

  Serial.print("Smoke Value: ");
  Serial.print(smokeValue);

  Serial.print(" | Threshold: ");
  Serial.print(smokeThreshold);

  Serial.print(" | Smoke: ");
  Serial.print(smokeDetected ? "DETECTED" : "NORMAL");

  Serial.print(" | Flame: ");
  Serial.print(flameDetected ? "DETECTED" : "NORMAL");

  Serial.print(" | Overall: ");
  Serial.println(alertState ? "DANGER" : "SAFE");

  // ===================================
  // LED AND BUZZER CONTROL
  // ===================================

  if (alertState) {

    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER_PIN, HIGH);

  }
  else {

    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BUZZER_PIN, LOW);

  }

  // ===================================
  // SMOKE DETECTION TELEGRAM ALERT
  // ===================================

  if (smokeDetected && !previousSmokeState) {

    String message = "🚨 SMOKE ALERT! 🚨\n\n";

    message += "🔥 SMOKE DETECTED!\n";
    message += "Smoke Value: ";
    message += String(smokeValue);

    message += "\n\nRed LED: ON";
    message += "\nBuzzer: ON";

    message += "\n\nPlease check immediately!";

    if (WiFi.status() == WL_CONNECTED) {
      bot.sendMessage(CHAT_ID, message, "");
    }

    Serial.println("SMOKE ALERT SENT!");
  }

  // ===================================
  // FLAME DETECTION TELEGRAM ALERT
  // ===================================

  if (flameDetected && !previousFlameState) {

    String message = "🚨 FIRE ALERT! 🚨\n\n";

    message += "🔥 FLAME DETECTED!\n";
    message += "\nRed LED: ON";
    message += "\nBuzzer: ON";

    message += "\n\nPlease check immediately!";

    if (WiFi.status() == WL_CONNECTED) {
      bot.sendMessage(CHAT_ID, message, "");
    }

    Serial.println("FIRE ALERT SENT!");
  }

  // Save previous sensor states
  previousSmokeState = smokeDetected;
  previousFlameState = flameDetected;

  // ===================================
  // TELEGRAM COMMAND HANDLER
  // ===================================

  handleTelegram();

  delay(300);
}

// =====================================
// TELEGRAM COMMAND HANDLER
// =====================================

void handleTelegram() {

  if (millis() - lastTelegramCheck < telegramInterval) {
    return;
  }

  lastTelegramCheck = millis();

  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  int numMessages = bot.getUpdates(
    bot.last_message_received + 1
  );

  while (numMessages) {

    for (int i = 0; i < numMessages; i++) {

      String receivedText = bot.messages[i].text;
      String receivedChatID = bot.messages[i].chat_id;

      if (receivedChatID != CHAT_ID) {
        continue;
      }

      // =================================
      // STATUS COMMAND
      // =================================

      if (receivedText == "/status") {

        int smoke = analogRead(MQ2_PIN);

        bool smokeDetected = smoke > smokeThreshold;

        bool flameDetected =
          (digitalRead(FLAME_PIN) == FLAME_DETECTED);

        bool danger = smokeDetected || flameDetected;

        // Update outputs
        digitalWrite(RED_LED, danger ? HIGH : LOW);
        digitalWrite(GREEN_LED, danger ? LOW : HIGH);
        digitalWrite(BUZZER_PIN, danger ? HIGH : LOW);

        String message = "🔥 FIRE & SMOKE SYSTEM STATUS\n\n";

        message += "Smoke Value: ";
        message += String(smoke);

        message += "\nSmoke: ";
        message += smokeDetected ? "DETECTED" : "NORMAL";

        message += "\nFlame: ";
        message += flameDetected ? "DETECTED" : "NOT DETECTED";

        message += "\n\nOverall Status: ";
        message += danger ? "DANGER" : "SAFE";

        message += "\nRed LED: ";
        message += danger ? "ON" : "OFF";

        message += "\nGreen LED: ";
        message += danger ? "OFF" : "ON";

        message += "\nBuzzer: ";
        message += danger ? "ON" : "OFF";

        bot.sendMessage(
          receivedChatID,
          message,
          ""
        );

        Serial.println("Status requested from Telegram");
      }
    }

    numMessages = bot.getUpdates(
      bot.last_message_received + 1
    );
  }
}
