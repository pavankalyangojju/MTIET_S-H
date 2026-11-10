
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>

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

// =================================
// RFID PINS
// =================================

#define SS_PIN 5
#define RST_PIN 4

MFRC522 rfid(SS_PIN, RST_PIN);

// =================================
// LED AND BUZZER PINS
// =================================

#define GREEN_LED 25
#define RED_LED 26
#define BUZZER 27

// =================================
// OLED SETTINGS
// =================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// =================================
// TELEGRAM CLIENT
// =================================

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// =================================
// STUDENT DATABASE
// =================================

struct Student {
  String uid;
  String name;
  String regNo;
};

Student students[] = {
  {"444CF905", "Pavan", "17HR1A0777"},
  {"A5141F06", "Gojju", "17HR1A0333"},
  {"E50B1C06", "Bheer", "17HR1A0555"},
  {"EA351806", "shyam", "17HR1A0888"}
};

const int totalStudents =
  sizeof(students) / sizeof(students[0]);

// =================================
// ONE HOUR DUPLICATE ATTENDANCE
// =================================

const unsigned long ONE_HOUR = 3600000UL;

// Store last attendance time for each student
unsigned long lastAttendanceMillis[totalStudents] = {0};

// Check whether student has already scanned
bool hasAttendance[totalStudents] = {false};

// =================================
// RFID QUICK SCAN PROTECTION
// =================================

String lastUID = "";
unsigned long lastTapTime = 0;

const unsigned long scanDelay = 2000;

// =================================
// BUZZER FUNCTION
// =================================

void beepBuzzer(int count, int duration) {

  for (int i = 0; i < count; i++) {

    digitalWrite(BUZZER, HIGH);
    delay(duration);

    digitalWrite(BUZZER, LOW);
    delay(150);
  }
}

// =================================
// OLED HOME SCREEN
// =================================

void showHomeScreen() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(15, 5);
  display.println("7ROBOLABS");

  display.drawLine(0, 18, 127, 18, SSD1306_WHITE);

  display.setCursor(10, 27);
  display.println("RFID ATTENDANCE");

  display.setCursor(10, 43);
  display.println("System Ready!");

  display.setCursor(10, 55);
  display.println("Tap Your RFID Card");

  display.display();
}

// =================================
// OLED STARTUP SCREEN
// =================================

void showStartupScreen() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(15, 5);
  display.println("7ROBOLABS");

  display.drawLine(0, 20, 127, 20, SSD1306_WHITE);

  display.setCursor(15, 30);
  display.println("RFID ATTENDANCE");

  display.setCursor(25, 45);
  display.println("Starting...");

  display.display();
}

// =================================
// OLED VALID STUDENT SCREEN
// =================================

void showAttendance(
  String name,
  String regNo,
  String currentTime,
  String currentDate
) {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("STUDENT PRESENT");

  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);

  display.setCursor(0, 15);
  display.print("Name: ");
  display.println(name);

  display.setCursor(0, 27);
  display.print("Reg: ");
  display.println(regNo);

  display.setCursor(0, 39);
  display.print("Time: ");
  display.println(currentTime);

  display.setCursor(0, 51);
  display.print("Date: ");
  display.println(currentDate);

  display.display();
}

// =================================
// OLED ALREADY SCANNED SCREEN
// =================================

void showAlreadyScanned(String name, int remainingMinutes) {

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(5, 2);
  display.println("ALREADY SCANNED!");

  display.drawLine(0, 14, 127, 14, SSD1306_WHITE);

  display.setCursor(0, 20);
  display.println("Already scanned");

  display.setCursor(0, 31);
  display.println("for this class.");

  display.setCursor(0, 43);
  display.println("Please come after");

  display.setCursor(0, 55);
  display.print(remainingMinutes);
  display.println(" minutes.");

  display.display();
}
// =================================
// OLED WRONG CARD SCREEN
// =================================

void showWrongCard(String uid) {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(15, 5);
  display.println("ACCESS DENIED!");

  display.drawLine(0, 18, 127, 18, SSD1306_WHITE);

  display.setCursor(15, 28);
  display.println("Wrong RFID Card");

  display.setCursor(0, 45);
  display.println("Unknown Card UID:");

  display.setCursor(0, 56);
  display.println(uid);

  display.display();
}

// =================================
// SETUP
// =================================

void setup() {

  Serial.begin(115200);

  // LED setup

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  // Buzzer setup

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  // OLED initialization

  Wire.begin(21, 22);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS)) {

    Serial.println("OLED NOT FOUND!");

    while (1) {
      delay(1000);
    }
  }

  Serial.println("OLED Initialized Successfully!");

  showStartupScreen();

  delay(2000);

  // RFID initialization

  SPI.begin(18, 19, 23, SS_PIN);

  rfid.PCD_Init();

  Serial.println("RFID Initialized!");

  // WiFi connection

  WiFi.begin(ssid, password);

  Serial.print("Connecting WiFi");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("Connecting WiFi...");
  display.display();

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  // Telegram SSL

  client.setInsecure();

  // Indian date and time

  configTime(
    19800,
    0,
    "pool.ntp.org",
    "time.google.com"
  );

  display.clearDisplay();
  display.setCursor(10, 25);
  display.println("Getting Date Time");
  display.display();

  struct tm timeinfo;

  while (!getLocalTime(&timeinfo)) {
    delay(500);
  }

  Serial.println("Time Synchronized!");

  showHomeScreen();

  Serial.println("======================");
  Serial.println("7ROBOLABS ATTENDANCE");
  Serial.println("System Ready!");
  Serial.println("Tap RFID Card");
  Serial.println("======================");
}

// =================================
// MAIN LOOP
// =================================

void loop() {

  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  // Read RFID UID

  String cardUID = "";

  for (byte i = 0; i < rfid.uid.size; i++) {

    if (rfid.uid.uidByte[i] < 0x10) {
      cardUID += "0";
    }

    cardUID += String(
      rfid.uid.uidByte[i],
      HEX
    );
  }

  cardUID.toUpperCase();

  Serial.print("Card UID: ");
  Serial.println(cardUID);

  // Prevent immediate repeated reads

  if (cardUID != lastUID ||
      millis() - lastTapTime > scanDelay) {

    lastUID = cardUID;
    lastTapTime = millis();

    checkStudent(cardUID);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  delay(200);
}

// =================================
// CHECK STUDENT
// =================================

void checkStudent(String uid) {

  bool found = false;

  for (int i = 0; i < totalStudents; i++) {

    if (students[i].uid == uid) {

      found = true;

      String studentName = students[i].name;
      String regNo = students[i].regNo;

      // =================================
      // CHECK ONE HOUR DUPLICATE
      // =================================

      if (hasAttendance[i] &&
          millis() - lastAttendanceMillis[i] < ONE_HOUR) {

        unsigned long elapsed =
          millis() - lastAttendanceMillis[i];

        unsigned long remaining =
          ONE_HOUR - elapsed;

        int remainingMinutes =
          (remaining + 59999UL) / 60000UL;

        Serial.println("ALREADY SCANNED!");
        Serial.print("Student: ");
        Serial.println(studentName);

        Serial.print("Remaining minutes: ");
        Serial.println(remainingMinutes);

        // Red LED for duplicate scan

        digitalWrite(GREEN_LED, LOW);
        digitalWrite(RED_LED, HIGH);

        // Two short beeps

        beepBuzzer(2, 150);

        // OLED message

        showAlreadyScanned(
          studentName,
          remainingMinutes
        );

        // Telegram notification

        if (WiFi.status() == WL_CONNECTED) {

          String duplicateMessage =
            "ALREADY SCANNED!\n\n"
            "Name: " + studentName +
            "\nRegister Number: " + regNo +
            "\nMessage: Please come back after 1 hour." +
            "\nRemaining Time: " +
            String(remainingMinutes) + " minutes";

          bool sent = bot.sendMessage(
            CHAT_ID,
            duplicateMessage,
            ""
          );

          if (sent) {
            Serial.println("Duplicate message sent!");
          } else {
            Serial.println("Telegram message failed!");
          }
        }

        delay(3000);

        digitalWrite(RED_LED, LOW);

        showHomeScreen();

        return;
      }

      // =================================
      // NEW ATTENDANCE
      // =================================

      // Save attendance scan time

      hasAttendance[i] = true;
      lastAttendanceMillis[i] = millis();

      // Green LED ON

      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);

      // One beep for successful attendance

      beepBuzzer(1, 300);

      // Get current date and time

      struct tm timeinfo;

      if (!getLocalTime(&timeinfo)) {

        Serial.println("Time unavailable!");

        digitalWrite(GREEN_LED, LOW);

        showHomeScreen();

        return;
      }

      char dateBuffer[15];
      char timeBuffer[15];

      strftime(
        dateBuffer,
        sizeof(dateBuffer),
        "%d-%m-%Y",
        &timeinfo
      );

      strftime(
        timeBuffer,
        sizeof(timeBuffer),
        "%I:%M:%S %p",
        &timeinfo
      );

      String currentTime = String(timeBuffer);
      String currentDate = String(dateBuffer);

      // OLED attendance display

      showAttendance(
        studentName,
        regNo,
        currentTime,
        currentDate
      );

      // Telegram message

      String message =
        "STUDENT PRESENT\n\n"
        "Name of the student : " + studentName +
        "\nRegister Number : " + regNo +
        "\nTime : " + currentTime +
        "\nDate : " + currentDate;

      Serial.println(message);

      if (WiFi.status() == WL_CONNECTED) {

        bool sent = bot.sendMessage(
          CHAT_ID,
          message,
          ""
        );

        if (sent) {
          Serial.println("Telegram Message Sent!");
        } else {
          Serial.println("Telegram Message Failed!");
        }

      } else {

        Serial.println("WiFi Disconnected!");
      }

      delay(3000);

      // Green LED OFF

      digitalWrite(GREEN_LED, LOW);

      showHomeScreen();

      break;
    }
  }

  // =================================
  // UNKNOWN RFID CARD
  // =================================

  if (!found) {

    digitalWrite(GREEN_LED, LOW);
    digitalWrite(RED_LED, HIGH);

    // Three beeps for unknown card

    beepBuzzer(3, 150);

    showWrongCard(uid);

    Serial.println("ACCESS DENIED!");
    Serial.println("Unknown RFID Card!");

    if (WiFi.status() == WL_CONNECTED) {

      String errorMessage =
        "ACCESS DENIED!\n\n"
        "Unknown RFID Card\n"
        "UID: " + uid;

      bot.sendMessage(
        CHAT_ID,
        errorMessage,
        ""
      );
    }

    delay(3000);

    digitalWrite(RED_LED, LOW);

    showHomeScreen();
  }
}
