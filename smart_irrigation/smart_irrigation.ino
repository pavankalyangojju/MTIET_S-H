
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// =================================
// WIFI DETAILS
// =================================

const char* ssid = "PK";
const char* password = "244466666";

// Telegram details
#define BOT_TOKEN "9238638826:AAFletb1yHo-MPoT2g0B1cAkai-A_hpkAjQ"
#define CHAT_ID "1160527711"

// =================================
// PIN CONFIGURATION
// =================================

#define SOIL_PIN 34
#define RELAY_PIN 26

// =================================
// SOIL THRESHOLD
// =================================

int dryThreshold = 2500;

// =================================
// RELAY CONFIGURATION
// =================================

// Active LOW relay
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// =================================
// TELEGRAM OBJECT
// =================================

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// =================================
// VARIABLES
// =================================

bool pumpState = false;
bool previousDryState = false;

unsigned long lastCheck = 0;

const unsigned long checkInterval = 1000;

// =================================
// MOTOR CONTROL FUNCTIONS
// =================================

void pumpON() {

  digitalWrite(RELAY_PIN, RELAY_ON);
  pumpState = true;

  Serial.println("MOTOR PUMP ON");
}

void pumpOFF() {

  digitalWrite(RELAY_PIN, RELAY_OFF);
  pumpState = false;

  Serial.println("MOTOR PUMP OFF");
}

// =================================
// SETUP
// =================================

void setup() {

  Serial.begin(115200);

  pinMode(SOIL_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Initially motor OFF
  pumpOFF();

  // Connect WiFi
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");

  client.setInsecure();

  // Startup Telegram message
  bot.sendMessage(
    CHAT_ID,
    "🌱 Smart Irrigation System Started!\n\n"
    "Motor Pump: OFF\n\n"
    "Available Commands:\n"
    "/on - Turn ON Pump\n"
    "/off - Turn OFF Pump\n"
    "/status - Check Status",
    ""
  );
}

// =================================
// MAIN LOOP
// =================================

void loop() {

  // Read soil sensor
  int soilValue = analogRead(SOIL_PIN);

  bool soilIsDry = (soilValue > dryThreshold);

  // Serial Monitor
  Serial.print("Soil Value: ");
  Serial.print(soilValue);

  Serial.print(" | Soil: ");
  Serial.print(soilIsDry ? "DRY" : "WET");

  Serial.print(" | Pump: ");
  Serial.println(pumpState ? "ON" : "OFF");

  // =================================
  // DRY SOIL NOTIFICATION
  // =================================

  if (soilIsDry && !previousDryState) {

    bot.sendMessage(
      CHAT_ID,
      "⚠️ Soil is Dry!\n"
      "Please turn ON the motor pump.",
      ""
    );

    Serial.println("Dry soil notification sent!");
  }

  // =================================
  // WET SOIL AUTOMATIC PUMP OFF
  // =================================

  if (!soilIsDry && previousDryState) {

    if (pumpState) {

      pumpOFF();

      bot.sendMessage(
        CHAT_ID,
        "✅ Soil is Wet!\n"
        "Motor pump automatically turned OFF.",
        ""
      );
    }
    else {

      bot.sendMessage(
        CHAT_ID,
        "✅ Soil is Wet!\n"
        "No need to turn ON the motor pump.",
        ""
      );
    }

    Serial.println("Wet soil detected!");
  }

  previousDryState = soilIsDry;

  // =================================
  // TELEGRAM COMMANDS
  // =================================

  if (millis() - lastCheck >= checkInterval) {

    int messages = bot.getUpdates(
      bot.last_message_received + 1
    );

    while (messages) {

      for (int i = 0; i < messages; i++) {

        String chat_id = bot.messages[i].chat_id;
        String message = bot.messages[i].text;

        // Security: Allow only configured chat
        if (chat_id != CHAT_ID) {
          continue;
        }

        // =================================
        // MOTOR ON COMMAND
        // =================================

        if (message == "/on") {

          int currentSoil = analogRead(SOIL_PIN);

          if (currentSoil > dryThreshold) {

            pumpON();

            bot.sendMessage(
              CHAT_ID,
              "💧 Motor Pump turned ON!\n"
              "Soil is Dry.",
              ""
            );
          }
          else {

            pumpOFF();

            bot.sendMessage(
              CHAT_ID,
              "⚠️ Soil is Wet!\n"
              "Motor pump cannot be turned ON.",
              ""
            );
          }
        }

        // =================================
        // MOTOR OFF COMMAND
        // =================================

        else if (message == "/off") {

          pumpOFF();

          bot.sendMessage(
            CHAT_ID,
            "🛑 Motor Pump turned OFF manually.",
            ""
          );
        }

        // =================================
        // STATUS COMMAND
        // =================================

        else if (message == "/status") {

          int currentSoil = analogRead(SOIL_PIN);

          bool currentDry =
            (currentSoil > dryThreshold);

          String status = "🌱 IRRIGATION STATUS\n\n";

          status += "Soil Value: ";
          status += String(currentSoil);
          status += "\n";

          status += currentDry ?
            "Soil: DRY\n" :
            "Soil: WET\n";

          status += pumpState ?
            "Motor Pump: ON" :
            "Motor Pump: OFF";

          bot.sendMessage(
            CHAT_ID,
            status,
            ""
          );
        }

      }

      messages = bot.getUpdates(
        bot.last_message_received + 1
      );
    }

    lastCheck = millis();
  }

  delay(100);
}
