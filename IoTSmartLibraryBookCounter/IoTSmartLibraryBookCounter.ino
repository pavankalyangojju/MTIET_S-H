
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
// OLED SETTINGS
// =================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// =================================
// IR SENSOR PINS
// =================================
#define IR1 18
#define IR2 19
#define IR3 23

// =================================
// SENSOR STATUS
// =================================
bool book1, book2, book3;

unsigned long lastTelegramCheck = 0;
const unsigned long telegramInterval = 1000;

// =================================
// READ SENSORS
// =================================
void readSensors() {

  // Assumes IR output LOW when book detected
  book1 = (digitalRead(IR1) == LOW);
  book2 = (digitalRead(IR2) == LOW);
  book3 = (digitalRead(IR3) == LOW);
}

// =================================
// OLED DISPLAY
// =================================
void updateDisplay() {

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(1);
  display.setCursor(15, 0);
  display.println("SMART LIBRARY");

  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  display.setCursor(0, 20);
  display.print("Book 1: ");
  display.println(book1 ? "PRESENT" : "NOT PRESENT");

  display.setCursor(0, 34);
  display.print("Book 2: ");
  display.println(book2 ? "PRESENT" : "NOT PRESENT");

  display.setCursor(0, 48);
  display.print("Book 3: ");
  display.println(book3 ? "PRESENT" : "NOT PRESENT");

  display.display();
}

// =================================
// TELEGRAM STATUS
// =================================
String getBookStatus() {

  readSensors();

  String message = "SMART LIBRARY BOOK STATUS\n\n";

  message += "Book 1: ";
  message += book1 ? "PRESENT\n" : "NOT PRESENT\n";

  message += "Book 2: ";
  message += book2 ? "PRESENT\n" : "NOT PRESENT\n";

  message += "Book 3: ";
  message += book3 ? "PRESENT\n" : "NOT PRESENT\n";

  int totalPresent = book1 + book2 + book3;

  message += "\nTotal Books Present: ";
  message += String(totalPresent);

  return message;
}

// =================================
// TELEGRAM COMMANDS
// =================================
void handleTelegram() {

  int numberOfMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numberOfMessages) {

    for (int i = 0; i < numberOfMessages; i++) {

      String chat_id = bot.messages[i].chat_id;
      String text = bot.messages[i].text;

      // Only authorized user
      if (chat_id != CHAT_ID) {
        continue;
      }

      if (text == "/start") {

        bot.sendMessage(
          CHAT_ID,
          "Welcome to Smart Library!\n\nSend /status to check book availability.",
          ""
        );
      }

      else if (text == "/status") {

        String statusMessage = getBookStatus();

        bot.sendMessage(
          CHAT_ID,
          statusMessage,
          ""
        );
      }
    }

    numberOfMessages = bot.getUpdates(
      bot.last_message_received + 1
    );
  }
}

// =================================
// SETUP
// =================================
void setup() {

  Serial.begin(115200);

  pinMode(IR1, INPUT_PULLUP);
  pinMode(IR2, INPUT_PULLUP);
  pinMode(IR3, INPUT_PULLUP);

  Wire.begin(21, 22);

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        0x3C
      )) {

    Serial.println("OLED NOT FOUND");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 25);
  display.println("Connecting WiFi...");
  display.display();

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");

  client.setInsecure();

  display.clearDisplay();
  display.setCursor(15, 25);
  display.println("SMART LIBRARY READY");
  display.display();

  delay(1500);
}

// =================================
// MAIN LOOP
// =================================
void loop() {

  readSensors();

  // Update OLED continuously
  updateDisplay();

  // Check Telegram periodically
  if (millis() - lastTelegramCheck >= telegramInterval) {

    lastTelegramCheck = millis();

    if (WiFi.status() == WL_CONNECTED) {
      handleTelegram();
    }
  }

  delay(100);
}
