#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include "time.h"

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -18000; // Adjust for your timezone (e.g., -5hrs for EST)
const int   daylightOffset_sec = 3600;

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASS ""
#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883

// I2C Addresses defined in your chip.c
#define ADDR_TOF 0x29
#define ADDR_LOAD 0x42

#define BUTT_1 5
#define BUTT_2 17
#define I2C_SDA 21
#define I2C_SCL 22

bool butt1Pressed = false;
bool butt2Pressed = false;
uint32_t sensorDistance = 0;
uint32_t weightRaw = 0;

WiFiClient espClient;
PubSubClient mqtt(espClient);

void connectWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "esp32-hubert-" + String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str())) {
      Serial.println("Connected!");
    } else {
      delay(5000);
    }
  }
}

void pollButton() {
  if (digitalRead(BUTT_1) == HIGH) butt1Pressed = true;
  if (digitalRead(BUTT_2) == HIGH) butt2Pressed = true;
}

// Helper to read 8-bit values from the custom chip
uint32_t read32FromI2C(uint8_t addr) {
  // Clear any old data
  while(Wire.available()) Wire.read(); 

  uint8_t count = Wire.requestFrom(addr, (uint8_t)4); 
  if (count == 4) {
    uint32_t b1 = Wire.read();
    uint32_t b2 = Wire.read();
    uint32_t b3 = Wire.read();
    uint32_t b4 = Wire.read();
    return (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
  }
  return 0;
}

void readSensorsFromChip() {
  sensorDistance = read32FromI2C(ADDR_TOF);
  delay(10);
  weightRaw = read32FromI2C(ADDR_LOAD);
  
  Serial.printf("Dist (raw): %u  | Weight (g): %u\n", sensorDistance, weightRaw);
}

String getTimeString() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    return "1970-01-01T00:00:00Z"; // Fallback if time sync fails
  }
  char timeStringBuff[25]; 
  // Formats to: 2026-04-12T22:05:04
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &timeinfo);
  return String(timeStringBuff);
}

void publishSensorData() {
  readSensorsFromChip();

  String json = "{";
  json += "\"device_id\":\"esp32-node-01\",";
  json += "\"timestamp\":\"" + getTimeString() + "\",";
  json += "\"sensors\":{";
  json += "\"weight\":{\"value\":" + String(weightRaw) + "},";
  json += "\"distance\":{\"value\":" + String(sensorDistance) + "},";
  json += "\"cancel\":{\"value\":" + String(butt1Pressed) + "},";
  json += "\"confirm\":{\"value\":" + String(butt2Pressed) + "}";
  json += "}}";

  mqtt.publish("eece5155/Hubert/data", json.c_str());
  
  butt1Pressed = false;
  butt2Pressed = false;
}

void setup() {
  Serial.begin(115200);
  
  // Initialize I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  
  pinMode(BUTT_1, INPUT_PULLDOWN);
  pinMode(BUTT_2, INPUT_PULLDOWN);
  
  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

unsigned long lastTime = 0;
unsigned long timerDelay = 1000;

void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
  pollButton();

  if ((millis() - lastTime) > timerDelay) {
    publishSensorData();
    lastTime = millis();
  }
}