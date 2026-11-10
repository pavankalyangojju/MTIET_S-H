
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ESP32Servo.h>

// =================================
// WIFI DETAILS
// =================================

const char* ssid = "PK";
const char* password = "244466666";

// =================================
// TELEGRAM DETAILS
// =================================

#define BOT_TOKEN "9197802903:AAFXnBIC3t4GNEsFGNUrV3zlumLGX7sR2kU"
#define CHAT_ID "8719633048"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// =================================
// PIN CONFIGURATION
// =================================

// Lid detection ultrasonic
#define LID_TRIG 5
#define LID_ECHO 18

// Garbage level ultrasonic
#define BIN_TRIG 19
#define BIN_ECHO 21

// Servo
#define SERVO_PIN 13

// =================================
// OBJECTS
// =================================

Servo dustbinServo;

// =================================
// SETTINGS
// =================================

const int PERSON_DISTANCE = 20;

const int LID_OPEN_ANGLE = 90;
const int LID_CLOSE_ANGLE = 0;

const unsigned long LID_OPEN_TIME = 3000;

// Adjust these according to your dustbin
const float EMPTY_DISTANCE_CM = 25.0;
const float FULL_DISTANCE_CM = 5.0;

const int FULL_THRESHOLD = 80;
const int RESET_THRESHOLD = 70;

// =================================
// VARIABLES
// =================================

bool lidOpen = false;
bool binFullNotified = false;

unsigned long lidCloseTime = 0;
unsigned long lastBinCheck = 0;

const unsigned long BIN_CHECK_INTERVAL = 1000;

// =================================
// ULTRASONIC FUNCTION
// =================================

float getDistance(int trigPin, int echoPin) {

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration =
      pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return -1;
  }

  float distance = duration * 0.0343 / 2;

  return distance;
}

// =================================
// TELEGRAM FUNCTION
// =================================

void sendTelegram(String message) {

  if (WiFi.status() == WL_CONNECTED) {

    bool result = bot.sendMessage(
      CHAT_ID,
      message,
      ""
    );

    if (result) {
      Serial.println("Telegram Message Sent!");
    } else {
      Serial.println("Telegram Message Failed!");
    }

  } else {
    Serial.println("WiFi Disconnected!");
  }
}

// =================================
// LID CONTROL
// =================================

void controlLid() {

  float distance = getDistance(
    LID_TRIG,
    LID_ECHO
  );

  if (distance > 0 && distance <= PERSON_DISTANCE) {

    if (!lidOpen) {

      Serial.println("Person Detected!");
      Serial.println("Dustbin Lid OPEN");

      dustbinServo.write(LID_OPEN_ANGLE);
      lidOpen = true;
    }

    // Keep lid open while person is nearby
    lidCloseTime = millis() + LID_OPEN_TIME;
  }

  // Automatically close lid
  if (lidOpen &&
      (long)(millis() - lidCloseTime) >= 0) {

    dustbinServo.write(LID_CLOSE_ANGLE);

    lidOpen = false;

    Serial.println("Dustbin Lid CLOSED");
  }
}

// =================================
// GARBAGE LEVEL MONITORING
// =================================

void monitorGarbage() {

  if (millis() - lastBinCheck >= BIN_CHECK_INTERVAL) {

    lastBinCheck = millis();

    float distance = getDistance(
      BIN_TRIG,
      BIN_ECHO
    );

    if (distance <= 0) {
      Serial.println("Garbage sensor: No Echo");
      return;
    }

    // Calculate garbage percentage
    float percentage =
      (EMPTY_DISTANCE_CM - distance) /
      (EMPTY_DISTANCE_CM - FULL_DISTANCE_CM) * 100.0;

    percentage = constrain(percentage, 0, 100);

    Serial.print("Garbage Level: ");
    Serial.print(percentage);
    Serial.println("%");

    // Dustbin full alert
    if (percentage >= FULL_THRESHOLD &&
        !binFullNotified) {

      Serial.println("DUSTBIN FULL!");

      sendTelegram(
        "🚨 SMART DUSTBIN ALERT!\n\n"
        "Dustbin is FULL!\n"
        "Garbage Level: " +
        String(percentage, 1) +
        "%\n\n"
        "Please arrange cleaning."
      );

      binFullNotified = true;
    }

    // Reset alert after dustbin is emptied
    if (percentage <= RESET_THRESHOLD &&
        binFullNotified) {

      binFullNotified = false;

      Serial.println("Dustbin Emptied!");

      sendTelegram(
        "✅ SMART DUSTBIN UPDATE!\n\n"
        "Dustbin has been emptied.\n"
        "Ready for use."
      );
    }
  }
}

// =================================
// SETUP
// =================================

void setup() {

  Serial.begin(115200);

  // Ultrasonic pins
  pinMode(LID_TRIG, OUTPUT);
  pinMode(LID_ECHO, INPUT);

  pinMode(BIN_TRIG, OUTPUT);
  pinMode(BIN_ECHO, INPUT);

  // Servo setup
  dustbinServo.setPeriodHertz(50);
  dustbinServo.attach(SERVO_PIN, 500, 2400);

  dustbinServo.write(LID_CLOSE_ANGLE);

  // WiFi connection
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  client.setInsecure();

  // Telegram startup notification
  sendTelegram(
    "🗑️ Smart Dustbin System Started!\n"
    "ESP32 is Online.\n"
    "Automatic Lid and Garbage Monitoring Active."
  );

  Serial.println("Smart Dustbin Ready!");
}

// =================================
// MAIN LOOP
// =================================

void loop() {

  controlLid();

  monitorGarbage();

  delay(50);
}
