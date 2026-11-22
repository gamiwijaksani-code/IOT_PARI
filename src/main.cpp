// library.properties
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

// 1. KONFIGURASI WIFI & BROKER EMQX ===
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// Disesuaikan lagi dengan Broker Address dari dashboard EMQX Cloud yang dimiliki
const char* mqtt_server = "vf24f1fb.ala.us-east-1.emqxsl.com";
const int mqtt_port = 8883;
const char* mqtt_user = "parilah";
const char* mqtt_pass = "ikanpari";

const char* topic_telemetry = "smarthome/telemetry";
const char* topic_control   = "smarthome/control";

// 2. PEMETAAN PIN SESUAI diagram.json ===
#define PIN_DHT     15
#define DHTTYPE     DHT22
#define PIN_PIR     13
#define PIN_LDR     34
#define PIN_BTN     32
#define PIN_SERVO   14
#define PIN_BUZZER  25
#define PIN_LED_Y   27  // Lampu Teras (LED Kuning)
#define PIN_LED_R   33  // Indikator Alarm (LED Merah)

// Ambang batas kondisi lingkungan Wokwi
#define LDR_DARK_THRESHOLD  2000  // >= 2000 dianggap mendung/gelap
#define HUMIDITY_RAIN_LIMIT 80.0  // >= 80% dianggap potensi hujan
#define TEMP_HOT_LIMIT      40.0  // > 40°C dianggap panas ekstrem = kanopi tutup

// 3. INISIALISASI OBJEK & VARIABEL STATUS ===
WiFiClientSecure espClient;
PubSubClient client(espClient);
DHT dht(PIN_DHT, DHTTYPE);
Servo canopyServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

unsigned long lastMsg = 0;
bool autoMode = true;        // Default: Mode Otomatis aktif
int currentServoAngle = 0;   // 90 = Kanopi Buka (Jemur), 0 = Kanopi Tutup (Teduh)
bool lastBtnState = HIGH;

// 4. FUNGSI KONEKSI WIFI & MQTT ===
void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Perintah masuk: ");
  Serial.println(message);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, message);
  if (!error) {
    if (doc["auto"].is<int>()) {
      autoMode = (doc["auto"].as<int>() == 1);
    }
    if (doc["led_kuning"].is<int>()) {
      autoMode = false;
      digitalWrite(PIN_LED_Y, doc["led_kuning"].as<int>());
    }
    if (doc["led_merah"].is<int>()) {
      autoMode = false;
      digitalWrite(PIN_LED_R, doc["led_merah"].as<int>());
    }
    if (doc["servo"].is<int>()) {
      autoMode = false;
      currentServoAngle = doc["servo"].as<int>();
      canopyServo.write(currentServoAngle);
    }
    if (doc["buzzer"].is<int>()) {
      autoMode = false;
      if (doc["buzzer"].as<int>() == 1) tone(PIN_BUZZER, 1000);
      else noTone(PIN_BUZZER);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32-SmartPatio-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
      client.subscribe(topic_control);
    } else {
      delay(3000);
    }
  }
}

// 5. SETUP UTAMA =====
void setup() {
  Serial.begin(115200);

  pinMode(PIN_PIR, INPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_LED_Y, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  dht.begin();
  canopyServo.attach(PIN_SERVO);
  canopyServo.write(0);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Patio IoT");
  lcd.setCursor(0, 1);
  lcd.print("Connecting...");

  setup_wifi();

  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setBufferSize(512);
  client.setKeepAlive(60);
  client.setCallback(callback);

  lcd.clear();
}

// 6. LOOP UTAMA =====
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Sakelar fisik di dinding (GPIO 32) untuk beralih Mode AUTO / MANUAL
  bool currentBtnState = digitalRead(PIN_BTN);
  if (lastBtnState == HIGH && currentBtnState == LOW) {
    autoMode = !autoMode;
    delay(200);
  }
  lastBtnState = currentBtnState;

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;

    float suhu = dht.readTemperature();
    float lembap = dht.readHumidity();
    int gerakan = digitalRead(PIN_PIR);
    int cahaya = analogRead(PIN_LDR);

    if (isnan(suhu)) suhu = 25.0;
    if (isnan(lembap)) lembap = 50.0;

    bool isDark     = (cahaya >= LDR_DARK_THRESHOLD);
    bool isRainRisk = (lembap >= HUMIDITY_RAIN_LIMIT);
    bool isTooHot   = (suhu > TEMP_HOT_LIMIT);

    // LOGIKA OTOMATIS SMART PATIO & JEMURAN ===
    if (autoMode) {
      // 1. Kanopi Jemuran (Servo): Buka (90°) hanya jika CERAH, KERING, dan TIDAK TERLALU PANAS
      if (!isDark && !isRainRisk && !isTooHot) {
        currentServoAngle = 90;
      } else {
        currentServoAngle = 0; // Tutup kanopi (melindungi dari hujan, malam, atau panas > 40°C)
      }
      canopyServo.write(currentServoAngle);

      // 2. Lampu Teras (LED Kuning): Nyala otomatis saat gelap/malam
      digitalWrite(PIN_LED_Y, isDark ? HIGH : LOW);

      // 3. Keamanan Teras (PIR -> LED Merah & Buzzer)
      if (gerakan == HIGH) {
        digitalWrite(PIN_LED_R, HIGH);
        ledcWriteTone(7, 1200);
      } else {
        digitalWrite(PIN_LED_R, LOW);
        ledcWriteTone(7, 0);
      }
    }

    // TAMPILAN INFORMASI DI LCD 16x2 ===
    lcd.setCursor(0, 0);
    lcd.print("T:");
    lcd.print(suhu, 0);
    lcd.print("C H:");
    lcd.print(lembap, 0);
    lcd.print("% ");
    lcd.print(autoMode ? "AUTO" : "MAN ");

    lcd.setCursor(0, 1);
    lcd.print("ATAP:");
    lcd.print(currentServoAngle == 90 ? "BUKA " : "TUTUP");
    lcd.print(gerakan ? " !GERAK" : "  AMAN ");

   // KIRIM DATA JSON SESUAI FORMAT KE WEBSITE JS ===
    JsonDocument doc;
    doc["temperature"] = suhu;
    doc["humidity"] = lembap;
    doc["ldr"] = cahaya;
    doc["pir"] = gerakan;
    doc["servo"] = currentServoAngle;
    doc["led_kuning"] = digitalRead(PIN_LED_Y);
    doc["led_merah"] = digitalRead(PIN_LED_R);
    doc["buzzer"] = (autoMode && gerakan == HIGH) ? 1 : 0;
    doc["auto"] = autoMode ? 1 : 0;

    // 1. Kirim JSON ringkas ke EMQX & Website
    char jsonBuffer[256];
    serializeJsonPretty(doc, jsonBuffer);
    client.publish(topic_telemetry, jsonBuffer);

    // 2. Tampilkan log rapi & rata kiri di Serial Monitor
    const char* statusCuaca = "Cerah & Normal";
    if (isRainRisk) statusCuaca = "Potensi Hujan (Lembap)";
    else if (isTooHot) statusCuaca = "Panas Ekstrem (>40C)";
    else if (isDark) statusCuaca = "Gelap / Mendung";

    Serial.print("==========================================\r\n");
    Serial.printf("[MODE: %s] | Suhu: %.1f C | Lembap: %.0f%%\r\n", autoMode ? "OTOMATIS" : "MANUAL", suhu, lembap);
    Serial.printf("Kondisi      : %s (LDR: %d)\r\n", statusCuaca, cahaya);
    Serial.printf("Gerakan (PIR): %s\r\n", gerakan ? "TERDETEKSI!" : "Aman");
    Serial.printf("Kanopi Servo : %d deg (%s)\r\n", currentServoAngle, currentServoAngle > 0 ? "Terbuka" : "Tertutup");
    Serial.printf("Lampu Kuning : %s | Alarm Merah: %s\r\n", digitalRead(PIN_LED_Y) ? "ON" : "OFF", digitalRead(PIN_LED_R) ? "ON" : "OFF");
    Serial.print("Status MQTT  : Terkirim ke smarthome/telemetry\r\n");
    Serial.print("==========================================\r\n\r\n");
  }
}