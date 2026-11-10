
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
// PIN CONFIGURATION
// =====================================

// IR Sensors
#define IR1 13
#define IR2 18

// Slot 1 LEDs
#define GREEN1 5
#define RED1   15

// Slot 2 LEDs
#define GREEN2 19
#define RED2   4

// =====================================
// PARKING VARIABLES
// =====================================

bool slot1Occupied = false;
bool slot2Occupied = false;

unsigned long lastTelegramCheck = 0;
const unsigned long telegramInterval = 1000;

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);

  pinMode(IR1, INPUT);
  pinMode(IR2, INPUT);

  pinMode(GREEN1, OUTPUT);
  pinMode(RED1, OUTPUT);

  pinMode(GREEN2, OUTPUT);
  pinMode(RED2, OUTPUT);

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

  // Telegram SSL connection
  client.setInsecure();

  // Read initial sensor states
  readParkingSensors();
  updateLEDs();

  Serial.println("Smart Parking System Started");
  Serial.println("Send /status on Telegram");
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

  // Update parking LEDs automatically
  readParkingSensors();
  updateLEDs();

  // Check Telegram commands
  handleTelegram();

  delay(100);
}

// =====================================
// READ IR SENSORS
// =====================================

void readParkingSensors() {

  // Assumption: LOW means vehicle detected
  slot1Occupied = (digitalRead(IR1) == LOW);
  slot2Occupied = (digitalRead(IR2) == LOW);
}

// =====================================
// UPDATE LED INDICATION
// =====================================

void updateLEDs() {

  // Parking Slot 1
  if (slot1Occupied) {
    digitalWrite(RED1, HIGH);
    digitalWrite(GREEN1, LOW);
  }
  else {
    digitalWrite(RED1, LOW);
    digitalWrite(GREEN1, HIGH);
  }

  // Parking Slot 2
  if (slot2Occupied) {
    digitalWrite(RED2, HIGH);
    digitalWrite(GREEN2, LOW);
  }
  else {
    digitalWrite(RED2, LOW);
    digitalWrite(GREEN2, HIGH);
  }
}

// =====================================
// PARKING STATUS MESSAGE
// =====================================

String getParkingStatus() {

  int availableSlots = 0;

  if (!slot1Occupied) availableSlots++;
  if (!slot2Occupied) availableSlots++;

  String message = "SMART PARKING STATUS\n";
  message += "------------------------\n";

  message += "Slot 1: ";
  message += slot1Occupied ? "OCCUPIED" : "AVAILABLE";

  message += "\n";

  message += "Slot 2: ";
  message += slot2Occupied ? "OCCUPIED" : "AVAILABLE";

  message += "\n\n";

  message += "Available Slots: ";
  message += String(availableSlots);
  message += "/2";

  message += "\n";

  message += "Occupied Slots: ";
  message += String(2 - availableSlots);
  message += "/2";

  return message;
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

      // Respond only to your configured chat
      if (receivedChatID != CHAT_ID) {
        continue;
      }

      // Status command
      if (receivedText == "/status") {

        // Read fresh sensor values
        readParkingSensors();
        updateLEDs();

        String statusMessage = getParkingStatus();

        bot.sendMessage(
          receivedChatID,
          statusMessage,
          ""
        );

        Serial.println("Parking status requested");
        Serial.println(statusMessage);
      }
    }

    numMessages = bot.getUpdates(
      bot.last_message_received + 1
    );
  }
}
