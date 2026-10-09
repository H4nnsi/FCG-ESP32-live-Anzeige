#include <Adafruit_Protomatter.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include "time.h"

#define BUTTON_UP   6
#define BUTTON_DOWN 7

WebServer server(80);

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;       // UTC +1
const int   daylightOffset_sec = 3600;  // Sommerzeit

#define WIDTH 64
#define HEIGHT 32

uint8_t rgbPins[]  = {42, 41, 40, 38, 39, 37};
uint8_t addrPins[] = {45, 36, 48, 35};
uint8_t clockPin   = 2;
uint8_t latchPin   = 47;
uint8_t oePin      = 14;

Adafruit_Protomatter matrix(
  WIDTH, 4, 1, rgbPins, 4, addrPins,
  clockPin, latchPin, oePin, true
);

bool isLive = false; 
unsigned long lastDisplayUpdate = 0;

// Totmannschalter (Heartbeat)
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_TIMEOUT = 10000; // 10 Sek. Timeout

void updateDisplay();
void startLiveSetup();

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);

  ProtomatterStatus status = matrix.begin();
  if (status != PROTOMATTER_OK) {
    while(1);
  }
  matrix.setTextWrap(false);

  matrix.fillScreen(0);
  matrix.setTextSize(1);
  matrix.setCursor(2, 12);
  matrix.setTextColor(matrix.color565(255, 255, 0));
  matrix.print("Verbinde...");
  matrix.show();

  WiFiManager wm;
  wm.setConnectTimeout(10);
  
  if (!wm.autoConnect("MatrixPortal-Setup")) {
    startLiveSetup();
    return;
  }

  server.on("/", []() {
    String html = "<html><head><title>ESP32 OBS Status</title></head><body>";
    html += "<h1>Matrix Display Status: " + String(isLive ? "LIVE" : "OFFLINE") + "</h1>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/live", []() {
    if (server.hasArg("state")) {
      String state = server.arg("state");
      
      if (state == "on") {
        isLive = true;
        lastHeartbeat = millis();
        server.send(200, "text/plain", "OBS State: ON");
      } 
      else if (state == "keepalive") {
        if (isLive) {
          lastHeartbeat = millis();
        }
        server.send(200, "text/plain", "Heartbeat received");
      } 
      else if (state == "off") {
        isLive = false;
        server.send(200, "text/plain", "OBS State: OFF");
      } 
      else {
        server.send(400, "text/plain", "Invalid state");
      }
      updateDisplay();
    } else {
      server.send(400, "text/plain", "Missing state parameter");
    }
  });
  
  server.begin(); 

  matrix.fillScreen(0);
  matrix.setTextSize(1);
  matrix.setCursor(2, 12);
  matrix.setTextColor(matrix.color565(0, 255, 0));
  matrix.print("Zeit-Sync");
  matrix.show();

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  server.handleClient();

  if (isLive && (millis() - lastHeartbeat > HEARTBEAT_TIMEOUT)) {
    isLive = false;
    Serial.println("WARNUNG: OBS Heartbeat verloren!");
    updateDisplay();
  }

  if (digitalRead(BUTTON_UP) == LOW) {
    unsigned long pressStart = millis();
    bool heldLongEnough = false;

    while (digitalRead(BUTTON_UP) == LOW) {
      if (millis() - pressStart >= 2000) {
        heldLongEnough = true;
        break;
      }
      delay(50);
    }

    if (heldLongEnough) {
      startLiveSetup();
    }
  }

  if (millis() - lastDisplayUpdate >= 1000) {
    lastDisplayUpdate = millis();
    updateDisplay();
  }
}

void startLiveSetup() {
  matrix.fillScreen(0);
  matrix.setTextSize(1);
  matrix.setCursor(2, 12);
  matrix.setTextColor(matrix.color565(255, 165, 0));
  matrix.print("Web-Portal");
  matrix.show();

  WiFi.disconnect(true, true);
  delay(500);

  WiFiManager wm;
  wm.resetSettings();

  if (!wm.startConfigPortal("MatrixPortal-Setup")) {
    ESP.restart();
  }

  ESP.restart();
}

void updateDisplay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }

  matrix.fillScreen(0);

  // Formatiere Uhrzeit (HH:MM) und Datum ohne Jahr (DD.MM)
  char timeHM[6];  // HH:MM
  char dateDM[6];  // DD.MM

  strftime(timeHM, sizeof(timeHM), "%H:%M", &timeinfo);
  strftime(dateDM, sizeof(dateDM), "%d.%m", &timeinfo);

  if (isLive) {
    uint16_t red = matrix.color565(255, 0, 0);

    // 1. Große Uhrzeit oben zentriert (Größe 2)
    matrix.setTextSize(2);
    matrix.setCursor(2, 2);
    matrix.setTextColor(red);
    matrix.print(timeHM);

    // 2. Unten: "LIVE" links (Rot), Datum rechts (Weiß) (Größe 1)
    matrix.setTextSize(1);
    matrix.setCursor(2, 21);
    matrix.setTextColor(red);
    matrix.print("LIVE!");

    matrix.setCursor(34, 21);
    matrix.setTextColor(matrix.color565(255, 255, 255)); // Weißes Datum
    matrix.print(dateDM);

    // Roter Warnrahmen
    matrix.drawRect(0, 0, 64, 32, red);

  } else {
    uint16_t cyan = matrix.color565(0, 255, 200);

    // 1. Große Uhrzeit oben zentriert (Größe 2)
    matrix.setTextSize(2);
    matrix.setCursor(2, 2);
    matrix.setTextColor(cyan);
    matrix.print(timeHM);

    // 2. Datum unten exakt zentriert (Größe 1)
    matrix.setTextSize(1);
    matrix.setCursor(17, 21);
    matrix.setTextColor(matrix.color565(180, 180, 180)); // Silber/Grau
    matrix.print(dateDM);
  }

  matrix.show();
}
