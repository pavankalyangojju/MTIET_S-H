
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// =====================================
// WIFI DETAILS
// =====================================

const char* ssid = "PK";
const char* password = "244466666";

// =====================================
// TELEGRAM BOT DETAILS
// =====================================

#define BOT_TOKEN "9188319138:AAFfHsgnRnpzlkZ9FFo9-WV1m2OB9EU-N0U"
#define CHAT_ID "8753067838"

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// =====================================
// PIN CONFIGURATION
// =====================================

#define IR_SENSOR_1 26
#define IR_SENSOR_2 27

#define SERVO_PIN 18
#define BUZZER_PIN 25

#define RED_LED 32
#define GREEN_LED 33

// =====================================
// SERVO SETTINGS
// =====================================

#define GATE_OPEN 0
#define GATE_CLOSED 90

Servo railwayGate;

// =====================================
// TIMING SETTINGS
// =====================================

const unsigned long ARRIVAL_DURATION = 5000;
const unsigned long DEPARTURE_DURATION = 500;

// =====================================
// BUZZER MODES
// =====================================

enum BuzzerMode {
  BUZZER_OFF,
  ARRIVAL_BUZZER,
  DEPARTURE_BUZZER
};

BuzzerMode buzzerMode = BUZZER_OFF;

unsigned long buzzerStartTime = 0;

// =====================================
// SYSTEM VARIABLES
// =====================================

bool trainCycleActive = false;
bool sensor2Armed = false;
bool waitingForSensor1Clear = false;

// =====================================
// TELEGRAM FUNCTION
// =====================================

void sendTelegram(String message) {

  if (WiFi.status() == WL_CONNECTED) {

    bool sent = bot.sendMessage(
      CHAT_ID,
      message,
      ""
    );

    if (sent) {
      Serial.println("Telegram notification sent!");
    }
    else {
      Serial.println("Telegram notification failed!");
    }
  }
  else {
    Serial.println("WiFi disconnected!");
  }
}

// =====================================
// ARRIVAL BUZZER
// =====================================

void startArrivalBuzzer() {

  buzzerMode = ARRIVAL_BUZZER;
  buzzerStartTime = millis();

  digitalWrite(BUZZER_PIN, HIGH);

  Serial.println("Arrival Buzzer ON for 5 seconds");
}

// =====================================
// DEPARTURE BUZZER
// =====================================

void startDepartureBuzzer() {

  buzzerMode = DEPARTURE_BUZZER;
  buzzerStartTime = millis();

  digitalWrite(BUZZER_PIN, HIGH);

  Serial.println("Departure Buzzer: 2 Beeps");
}

// =====================================
// BUZZER CONTROL
// =====================================

void updateBuzzer() {

  unsigned long elapsed =
    millis() - buzzerStartTime;

  // Arrival buzzer: 5 seconds continuous

  if (buzzerMode == ARRIVAL_BUZZER) {

    if (elapsed >= ARRIVAL_DURATION) {

      digitalWrite(BUZZER_PIN, LOW);

      buzzerMode = BUZZER_OFF;

      Serial.println("Arrival Buzzer OFF");
    }
  }

  // Departure buzzer: 2 beeps within 3 seconds

  else if (buzzerMode == DEPARTURE_BUZZER) {

    if (elapsed < 500 ||
        (elapsed >= 1500 && elapsed < 2000)) {

      digitalWrite(BUZZER_PIN, HIGH);
    }
    else {

      digitalWrite(BUZZER_PIN, LOW);
    }

    if (elapsed >= DEPARTURE_DURATION) {

      digitalWrite(BUZZER_PIN, LOW);

      buzzerMode = BUZZER_OFF;

      Serial.println("Two Beeps Completed");
    }
  }
}

// =====================================
// TRAIN ARRIVAL FUNCTION
// =====================================

void trainArrival() {

  Serial.println();
  Serial.println("============================");
  Serial.println("TRAIN ARRIVED AT STATION!");
  Serial.println("============================");

  // Close railway gate

  railwayGate.write(GATE_CLOSED);

  // Red LED ON

  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);

  // Buzzer ON for 5 seconds

  startArrivalBuzzer();

  // Telegram notification

  sendTelegram(
    "TRAIN ARRIVED AT THE STATION!\n\n"
    
    "Railway Gate: CLOSED\n"
    
  );
}

// =====================================
// TRAIN DEPARTURE FUNCTION
// =====================================

void trainDeparture() {

  Serial.println();
  Serial.println("============================");
  Serial.println("TRAIN LEFT THE STATION!");
  Serial.println("============================");

  // Open railway gate

  railwayGate.write(GATE_OPEN);

  // Green LED ON

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  // Buzzer: 2 beeps within 3 seconds

  startDepartureBuzzer();

  // Telegram notification

  sendTelegram(
    "TRAIN LEFT THE STATION!\n\n"
   
    
    "Railway Gate: OPEN\n"
   
  );
}

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);

  // Sensor configuration

  pinMode(IR_SENSOR_1, INPUT);
  pinMode(IR_SENSOR_2, INPUT);

  // LED configuration

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Buzzer configuration

  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  // Initial LED condition

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  // Servo initialization

  railwayGate.setPeriodHertz(50);

  railwayGate.attach(
    SERVO_PIN,
    500,
    2400
  );

  // Initial gate position

  railwayGate.write(GATE_OPEN);

  Serial.println("============================");
  Serial.println("SMART RAILWAY STATION");
  Serial.println("============================");

  Serial.println("Gate OPEN");


  // WiFi connection

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  // Telegram SSL

  client.setInsecure();

  // Startup notification

  sendTelegram(
    "SMART RAILWAY STATION\n\n"
   
    "Gate Status: OPEN\n"
   
    "Waiting for Train Arrival."
  );

  Serial.println("System Ready!");
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

  // Update buzzer timing

  updateBuzzer();

  // Read IR sensors

  int sensor1 = digitalRead(IR_SENSOR_1);
  int sensor2 = digitalRead(IR_SENSOR_2);

  // =================================
  // RESET SENSOR 1 LOCK
  // =================================

  if (waitingForSensor1Clear &&
      sensor1 == HIGH) {

    waitingForSensor1Clear = false;

    Serial.println("Sensor 1 cleared.");
    Serial.println("Ready for next train.");
  }

  // =================================
  // TRAIN ARRIVAL - IR SENSOR 1
  // =================================

  if (sensor1 == LOW &&
      !trainCycleActive &&
      !waitingForSensor1Clear) {

    trainCycleActive = true;

    // Arm Sensor 2 only when it is clear

    sensor2Armed = (sensor2 == HIGH);

    trainArrival();
  }

  // =================================
  // ARM IR SENSOR 2
  // =================================

  if (trainCycleActive &&
      sensor2 == HIGH) {

    sensor2Armed = true;
  }

  // =================================
  // TRAIN DEPARTURE - IR SENSOR 2
  // =================================

  if (trainCycleActive &&
      sensor2Armed &&
      sensor2 == LOW) {

    trainDeparture();

    // Reset train cycle

    trainCycleActive = false;
    sensor2Armed = false;

    waitingForSensor1Clear = true;

    Serial.println("System Ready for Next Train.");
  }

  delay(50);
}
