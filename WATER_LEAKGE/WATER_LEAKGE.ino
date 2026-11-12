#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// ---------- USER SETTINGS ----------
const char* WIFI_SSID     = "PK";
const char* WIFI_PASSWORD = "244466666";
#define BOT_TOKEN  "9191078773:AAF_3xVEtq_b021a4jEtq9kPWI7qgUUgecY"   // from @BotFather
#define CHAT_ID    "8719633048"                  // your chat ID (from @myidbot)

// ---------- PINS ----------
#define FLOW_PIN   4
#define RELAY_PIN  26

// Swap these if your pump behaves the opposite way
#define PUMP_ON   HIGH
#define PUMP_OFF  LOW

// ---------- FLOW DETECTION SETTINGS ----------
const unsigned long PULSE_THRESHOLD   = 20;    // pulses per second to confirm flow
const unsigned long WINDOW_MS         = 1000;  // counting window
const unsigned long MIN_PULSE_GAP_US  = 2000;  // ignore pulses closer than 2 ms (noise)
const unsigned long STARTUP_IGNORE_MS = 3000;  // ignore flow right after pump starts

// ---------- TELEGRAM SETTINGS ----------
const unsigned long BOT_INTERVAL_MS = 2000;    // how often to check for new messages

WiFiClientSecure securedClient;
UniversalTelegramBot bot(BOT_TOKEN, securedClient);

volatile unsigned long pulseCount = 0;
volatile unsigned long lastPulseUs = 0;

bool pumpRunning = false;
unsigned long windowStart = 0;
unsigned long pumpStartTime = 0;
unsigned long lastBotCheck = 0;

void IRAM_ATTR countPulse() {
  unsigned long now = micros();
  if (now - lastPulseUs >= MIN_PULSE_GAP_US) {
    pulseCount++;
    lastPulseUs = now;
  }
}

void sendMsg(const String& text) {
  if (WiFi.status() == WL_CONNECTED) {
    bot.sendMessage(CHAT_ID, text, "");
  }
}

void pumpOn() {
  digitalWrite(RELAY_PIN, PUMP_ON);
  pumpRunning = true;
  noInterrupts();
  pulseCount = 0;
  interrupts();
  pumpStartTime = millis();
  windowStart = millis();
  Serial.println("Pump ON - waiting for flow...");
}

void pumpOff(bool byFlow) {
  digitalWrite(RELAY_PIN, PUMP_OFF);
  pumpRunning = false;
  if (byFlow) {
    Serial.println("Flow detected - pump OFF");
    sendMsg("💧 Flow detected - Pump turned OFF.\nSend /on to start the pump again.");
  } else {
    Serial.println("Pump OFF (manual)");
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi not connected (pump control still works)");
  }
}

void handleMessages(int numNew) {
  for (int i = 0; i < numNew; i++) {
    String chatId = String(bot.messages[i].chat_id);
    String text   = bot.messages[i].text;

    // Only accept commands from your own chat
    if (chatId != CHAT_ID) {
      bot.sendMessage(chatId, "Unauthorized user.", "");
      continue;
    }

    if (text == "/start" || text == "/help") {
      sendMsg("Commands:\n/on - Turn pump ON\n/off - Turn pump OFF\n/status - Pump status");
    }
    else if (text == "/on") {
      if (pumpRunning) {
        sendMsg("Pump is already ON.");
      } else {
        pumpOn();
        sendMsg("✅ Pump turned ON. Waiting for flow...");
      }
    }
    else if (text == "/off") {
      if (!pumpRunning) {
        sendMsg("Pump is already OFF.");
      } else {
        pumpOff(false);
        sendMsg("🛑 Pump turned OFF manually.");
      }
    }
    else if (text == "/status") {
      sendMsg(pumpRunning ? "Pump is ON (waiting for flow)." : "Pump is OFF.");
    }
    else {
      sendMsg("Unknown command. Send /help");
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, PUMP_OFF);
  pinMode(FLOW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_PIN), countPulse, FALLING);

  pumpOn();   // pump ON immediately, even before WiFi connects

  connectWiFi();
  securedClient.setInsecure();   // skip certificate check (simple setup)

  sendMsg("🔌 Device started. Pump is ON.\nSend /help for commands.");
}

void loop() {
  unsigned long now = millis();

  // ---------- FLOW DETECTION ----------
  if (pumpRunning) {
    if (now - pumpStartTime < STARTUP_IGNORE_MS) {
      noInterrupts();
      pulseCount = 0;
      interrupts();
      windowStart = now;
    }
    else if (now - windowStart >= WINDOW_MS) {
      noInterrupts();
      unsigned long pulses = pulseCount;
      pulseCount = 0;
      interrupts();

      unsigned long elapsed = now - windowStart;
      windowStart = now;

      // Normalize to pulses per second (in case the loop was delayed by Telegram)
      unsigned long rate = pulses * 1000UL / elapsed;

      Serial.print("Pulses per second: ");
      Serial.println(rate);

      if (rate >= PULSE_THRESHOLD) {
        pumpOff(true);
      }
    }
  }

  // ---------- WIFI RECONNECT ----------
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastTry = 0;
    if (now - lastTry > 10000) {
      lastTry = now;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  // ---------- TELEGRAM ----------
  if (WiFi.status() == WL_CONNECTED && now - lastBotCheck >= BOT_INTERVAL_MS) {
    lastBotCheck = now;
    int numNew = bot.getUpdates(bot.last_message_received + 1);
    while (numNew) {
      handleMessages(numNew);
      numNew = bot.getUpdates(bot.last_message_received + 1);
    }
  }

  delay(10);
}