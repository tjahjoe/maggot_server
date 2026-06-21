#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#define DHTTYPE DHT22

// ================= DHT =================
#define DHT1_PIN 15
#define DHT2_PIN 16
#define DHT3_PIN 17

DHT dht1(DHT1_PIN, DHTTYPE);
DHT dht2(DHT2_PIN, DHTTYPE);
DHT dht3(DHT3_PIN, DHTTYPE);

// ================= ZC =================
#define ZC1 13
#define ZC2 26
#define ZC3 25

// ================= DIMMER =================
#define DIM1 14
#define DIM2 27
#define DIM3 33

// ================= FAN =================
#define FAN 5

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// ================= WIFI =================
const char* ssid = "Yudha";
const char* wifiPassword = "Mentari.27";

// ================= MQTT =================
const char* mqttHost = "4815f5e014b641d384ceff6b1570e225.s1.eu.hivemq.cloud";
const int mqttPort = 8883;
const char* mqttUser = "maggot";
const char* mqttPass = "Maggot123";
const char* mqttClientId = "maggot-device01";

const char* topicSensor = "maggot/device01/sensor";
const char* topicStatus = "maggot/device01/status";
const char* topicCmd = "maggot/device01/cmd";

// ================= FASTAPI =================
const char* apiSensorUrl = "https://maggot-server-kappa.vercel.app/sensor/insert";
const char* apiStatusUrl = "https://maggot-server-kappa.vercel.app/status/insert";
const char* deviceId = "device01";

// ================= CONTROL =================
float setPoint = 33.0;
int dimDelay = 500;
bool heaterEnable = true;
bool manualMode = false;

// ================= DIMMER =================
volatile bool zc1Flag = false;
volatile bool zc2Flag = false;
volatile bool zc3Flag = false;

volatile uint32_t zc1Count = 0;
volatile uint32_t zc2Count = 0;
volatile uint32_t zc3Count = 0;

volatile unsigned long lastZC1 = 0;
volatile unsigned long lastZC2 = 0;
volatile unsigned long lastZC3 = 0;

// ================= SENSOR =================
float t1 = 0, t2 = 0, t3 = 0;
float avgTemp = 0;

// ================= TIMER =================
unsigned long lastSensor = 0;
unsigned long lastLCD = 0;
unsigned long lastDebug = 0;
unsigned long lastMQTT = 0;
unsigned long lastRecon = 0;

// ================= FLAG POST =================
bool pendingSensorPost = false;
bool pendingStatusPost = false;

// ================= WIFI / MQTT =================
WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

// ================= ISR =================
void IRAM_ATTR zc1ISR() {
  unsigned long now = micros();
  if (now - lastZC1 > 3000) {
    zc1Flag = true;
    zc1Count++;
    lastZC1 = now;
  }
}

void IRAM_ATTR zc2ISR() {
  unsigned long now = micros();
  if (now - lastZC2 > 3000) {
    zc2Flag = true;
    zc2Count++;
    lastZC2 = now;
  }
}

void IRAM_ATTR zc3ISR() {
  unsigned long now = micros();
  if (now - lastZC3 > 3000) {
    zc3Flag = true;
    zc3Count++;
    lastZC3 = now;
  }
}

// ================= TRIAC =================
void fireTriac(int pin, int delayUs) {
  delayMicroseconds(delayUs);
  digitalWrite(pin, HIGH);
  delayMicroseconds(50);
  digitalWrite(pin, LOW);
}

// ================= HTTP POST =================
void postToServer(const char* url, const char* body) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int code = http.POST(body);
  if (code > 0) {
    Serial.printf("[HTTP] POST %s -> %d\n", url, code);
  } else {
    Serial.printf("[HTTP] POST %s gagal: %s\n", url, http.errorToString(code).c_str());
  }
  http.end();
}

// ================= BUILD SENSOR JSON =================
void buildSensorJson(char* buf, size_t size) {
  StaticJsonDocument<256> doc;
  doc["device_id"] = deviceId;
  doc["t1"] = serialized(String(t1, 1));
  doc["t2"] = serialized(String(t2, 1));
  doc["t3"] = serialized(String(t3, 1));
  doc["avg_temp"] = serialized(String(avgTemp, 1));
  doc["set_point"] = serialized(String(setPoint, 1));
  doc["dim_delay_us"] = dimDelay;
  serializeJson(doc, buf, size);
}

// ================= BUILD STATUS JSON =================
void buildStatusJson(char* buf, size_t size) {
  StaticJsonDocument<128> doc;
  doc["device_id"] = deviceId;
  doc["heater"] = heaterEnable ? "on" : "off";
  doc["fan"] = digitalRead(FAN) ? "on" : "off";
  doc["mode"] = manualMode ? "manual" : "auto";
  serializeJson(doc, buf, size);
}

// ================= PUBLISH SENSOR =================
void publishAndPostSensor() {
  char buf[256];
  buildSensorJson(buf, sizeof(buf));

  if (mqttClient.connected())
    mqttClient.publish(topicSensor, buf);

  postToServer(apiSensorUrl, buf);

  Serial.println("[SENSOR] " + String(buf));
}

// ================= PUBLISH STATUS =================
void publishAndPostStatus() {
  char buf[128];
  buildStatusJson(buf, sizeof(buf));

  if (mqttClient.connected())
    mqttClient.publish(topicStatus, buf);

  postToServer(apiStatusUrl, buf);

  Serial.println("[STATUS] " + String(buf));
}

// ================= MQTT CALLBACK =================
// Terima CMD dari mobile, eksekusi, lalu langsung publish & post status balik
//
// Format CMD:
//   {"cmd":"setpoint","value":34.0}
//   {"cmd":"heater","value":"on"}   / "off"
//   {"cmd":"fan","value":"on"}      / "off"
//   {"cmd":"dim","value":3000}
//   {"cmd":"auto"}                  → kembali ke mode otomatis
void mqttCallback(char* topic, byte* payload, unsigned int length) {

  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("[CMD] " + msg);

  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, msg)) {
    Serial.println("JSON parse error");
    return;
  }

  const char* cmd = doc["cmd"];
  if (!cmd) return;

  bool statusChanged = true;
  bool sensorChanged = true;

  if (strcmp(cmd, "setpoint") == 0) {
    setPoint = doc["value"].as<float>();
    manualMode = false;
    statusChanged = false;
    Serial.printf("Setpoint -> %.1f\n", setPoint);
  } else if (strcmp(cmd, "heater") == 0 && manualMode) {
    String val = doc["value"].as<String>();
    heaterEnable = (val == "on");
    // manualMode = true;
    sensorChanged = false;
    Serial.println("Heater manual -> " + val);
  } else if (strcmp(cmd, "fan") == 0 && manualMode) {
    String val = doc["value"].as<String>();
    digitalWrite(FAN, val == "on" ? HIGH : LOW);
    // manualMode = true;
    sensorChanged = false;
    Serial.println("Fan manual -> " + val);
  } else if (strcmp(cmd, "dim") == 0 && manualMode) {
    dimDelay = doc["value"].as<int>();
    // manualMode = true;
    statusChanged = false;
    Serial.printf("DimDelay manual -> %d us\n", dimDelay);
  } else if (strcmp(cmd, "mode") == 0) {
    String val = doc["value"].as<String>();
    if (val == "auto") {
      manualMode = false;
      sensorChanged = false;
      Serial.println("Mode -> AUTO");
    } else if (val == "manual") {
      manualMode = true;
      sensorChanged = false;
      Serial.println("Mode -> MANUAL");
    } else {
      Serial.println("Invalid mode");
      statusChanged = false;
      sensorChanged = false;
    }
  } else {
    statusChanged = false;
    sensorChanged = false;
  }

  if (statusChanged || sensorChanged) {
    publishAndPostStatus();
    publishAndPostSensor();
  }
}

// ================= WIFI =================
void connectWifi() {
  Serial.print("Connecting WiFi");
  lcd.setCursor(0, 0);
  lcd.print("WiFi connecting...  ");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifiPassword);

  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK: " + WiFi.localIP().toString());
    lcd.setCursor(0, 0);
    lcd.print("WiFi OK             ");
  } else {
    Serial.println("\nWiFi GAGAL");
    lcd.setCursor(0, 0);
    lcd.print("WiFi GAGAL          ");
  }
}

// ================= MQTT CONNECT =================
void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  wifiClient.setInsecure();
  mqttClient.setServer(mqttHost, mqttPort);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(30);

  Serial.print("Connecting MQTT...");

  if (mqttClient.connect(mqttClientId, mqttUser, mqttPass)) {
    Serial.println("OK");
    mqttClient.subscribe(topicCmd);

    publishAndPostStatus();
  } else {
    Serial.printf("GAGAL rc=%d\n", mqttClient.state());
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  dht1.begin();
  dht2.begin();
  dht3.begin();

  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.clear();

  pinMode(FAN, OUTPUT);
  digitalWrite(FAN, LOW);

  pinMode(ZC1, INPUT_PULLUP);
  pinMode(ZC2, INPUT_PULLUP);
  pinMode(ZC3, INPUT_PULLUP);

  pinMode(DIM1, OUTPUT);
  pinMode(DIM2, OUTPUT);
  pinMode(DIM3, OUTPUT);
  digitalWrite(DIM1, LOW);
  digitalWrite(DIM2, LOW);
  digitalWrite(DIM3, LOW);

  attachInterrupt(digitalPinToInterrupt(ZC1), zc1ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ZC2), zc2ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ZC3), zc3ISR, RISING);

  connectWifi();
  connectMQTT();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SYSTEM READY");
  Serial.println("SYSTEM READY");

  publishAndPostStatus();
  publishAndPostSensor();
}

// ================= LOOP =================
void loop() {

  // ================= MQTT LOOP =================
  if (mqttClient.connected()) {
    mqttClient.loop();
  } else if (millis() - lastRecon > 5000) {
    lastRecon = millis();
    Serial.println("Reconnect MQTT...");
    connectMQTT();
  }

  // ================= BACA SENSOR =================
  if (millis() - lastSensor > 2000) {
    lastSensor = millis();

    float r1 = dht1.readTemperature();
    float r2 = dht2.readTemperature();
    float r3 = dht3.readTemperature();

    if (!isnan(r1)) t1 = r1;
    if (!isnan(r2)) t2 = r2;
    if (!isnan(r3)) t3 = r3;

    avgTemp = (t1 + t2 + t3) / 3.0;

    // ================= KONTROL HEATER AUTO =================
    if (!manualMode) {
      bool prevHeater = heaterEnable;
      bool prevFan = digitalRead(FAN);

      if (avgTemp < setPoint - 5) {
        heaterEnable = true;
        dimDelay = 500;
        digitalWrite(FAN, LOW);
      } else if (avgTemp < setPoint - 4) {
        heaterEnable = true;
        dimDelay = 2500;
      } else if (avgTemp < setPoint - 2) {
        heaterEnable = true;
        dimDelay = 5000;
      } else if (avgTemp < setPoint - 1) {
        heaterEnable = true;
        dimDelay = 8000;
      } else if (avgTemp < setPoint) {
        heaterEnable = true;
        dimDelay = 9500;
      } else {
        heaterEnable = false;
        digitalWrite(FAN, HIGH);
      }

      if (prevHeater != heaterEnable || prevFan != (bool)digitalRead(FAN)) {
        publishAndPostStatus();
      }
    }
  }

  // ================= DIMMER =================
  if (zc1Flag) {
    zc1Flag = false;
    if (heaterEnable) fireTriac(DIM1, dimDelay);
  }
  if (zc2Flag) {
    zc2Flag = false;
    if (heaterEnable) fireTriac(DIM2, dimDelay);
  }
  if (zc3Flag) {
    zc3Flag = false;
    if (heaterEnable) fireTriac(DIM3, dimDelay);
  }

  // ================= PUBLISH SENSOR INTERVAL (5 detik) =================
  // Sensor publish & post ke server setiap 5 detik untuk history
  if (millis() - lastMQTT > 5000) {
    lastMQTT = millis();
    publishAndPostSensor();
    publishAndPostStatus();
  }

  // ================= LCD =================
  if (millis() - lastLCD > 500) {
    lastLCD = millis();

    lcd.setCursor(0, 0);
    lcd.print("AVG:");
    lcd.print(avgTemp, 1);
    lcd.print("C SP:");
    lcd.print(setPoint, 1);
    lcd.print("  ");

    lcd.setCursor(0, 1);
    lcd.print("T1:");
    lcd.print(t1, 1);
    lcd.print(" T2:");
    lcd.print(t2, 1);
    lcd.print("  ");

    lcd.setCursor(0, 2);
    lcd.print("T3:");
    lcd.print(t3, 1);
    lcd.print(manualMode ? " MAN" : " AUT");
    lcd.print("    ");

    lcd.setCursor(0, 3);
    lcd.print("HT:");
    lcd.print(heaterEnable ? "ON " : "OFF");
    lcd.print(" FAN:");
    lcd.print(digitalRead(FAN) ? "ON " : "OFF");
    lcd.print(" M:");
    lcd.print(mqttClient.connected() ? "OK" : "ER");
  }

  // ================= DEBUG SERIAL =================
  if (millis() - lastDebug > 1000) {
    lastDebug = millis();

    noInterrupts();
    uint32_t c1 = zc1Count, c2 = zc2Count, c3 = zc3Count;
    zc1Count = 0;
    zc2Count = 0;
    zc3Count = 0;
    interrupts();

    Serial.println("================================");
    Serial.printf("T1:%.1f T2:%.1f T3:%.1f AVG:%.1f C\n", t1, t2, t3, avgTemp);
    Serial.printf("SETPOINT : %.1f C\n", setPoint);
    Serial.printf("MODE     : %s\n", manualMode ? "MANUAL" : "AUTO");
    Serial.printf("HEATER   : %s\n", heaterEnable ? "ON" : "OFF");
    Serial.printf("FAN      : %s\n", digitalRead(FAN) ? "ON" : "OFF");
    Serial.printf("DIM DELAY: %d us\n", dimDelay);
    Serial.printf("ZC1/s:%u ZC2/s:%u ZC3/s:%u\n", c1, c2, c3);
    Serial.printf("WiFi:%s MQTT:%s\n",
                  WiFi.status() == WL_CONNECTED ? "OK" : "DISC",
                  mqttClient.connected() ? "OK" : "DISC");
  }
}