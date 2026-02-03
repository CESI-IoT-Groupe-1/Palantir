#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <DHT.h>
#include <AES.h>
#include <SHA256.h>
#include <vl53l5cx_class.h>
#include <math.h>

const int ROOM_ID = 212;
const char* SECRET_KEY = "ZIGBEE_SECRET_KEY_2026_PRO_SAFE!";
const unsigned long SEND_EVERY_MS = 3000;

// Pins & Capteurs
#define LPN_PIN 5
#define I2C_RST_PIN 3
#define PWREN_PIN A3
#define DEV_I2C Wire
#define DHTPIN 2
#define DHTTYPE DHT11
const int lightPin = A0;
const int MIC_AO_PIN = A1;
const unsigned long MIC_WIN_MS = 50;
const unsigned long MIC_SAMPLE_PERIOD_MS = 1;

// Micro
const unsigned long MIC_CALIB_MS = 3000;
const float MIC_EPS = 1e-3f;
float micDbOffset = 40.0f
float micDbSmoothed = 0.0f;
const float MIC_EMA_ALPHA = 0.25f;

// Accumulateurs RMS
unsigned long micLastMs = 0;
long micSum = 0;
long micSumSq = 0;
long micN = 0;

// Référence auto-calibrée
float micRefRmsDynamic = 0.0f;
bool micRefReady = false;
unsigned long micCalibStart = 0;
float calibRmsSum = 0.0f;
long calibCount = 0;
float micDbLast = 0.0f;

// Objets Capteurs
VL53L5CX sensor_vl53(&DEV_I2C, LPN_PIN, I2C_RST_PIN);
VL53L5CX_Motion_Configuration motion_config;
DHT dht(DHTPIN, DHTTYPE);
Adafruit_BME280 bme;
bool bmeOK = false;

// Objets Crypto
AES256 aes256;
SHA256 sha256;
uint8_t keyBytes[32];

// Variables Globales
int number_of_people = 0;
String state = "nothing";
unsigned long lastSend = 0;

// Init des capteurs
void initVL53() {
  pinMode(PWREN_PIN, OUTPUT);
  digitalWrite(PWREN_PIN, HIGH);
  delay(10);

  DEV_I2C.begin();
  DEV_I2C.setClock(400000);

  sensor_vl53.begin();
  sensor_vl53.init_sensor();
  sensor_vl53.vl53l5cx_set_resolution(VL53L5CX_RESOLUTION_8X8);

  sensor_vl53.vl53l5cx_motion_indicator_init(&motion_config, VL53L5CX_RESOLUTION_8X8);
  sensor_vl53.vl53l5cx_motion_indicator_set_resolution(&motion_config, VL53L5CX_RESOLUTION_8X8);
  sensor_vl53.vl53l5cx_motion_indicator_set_distance_motion(&motion_config, 400, 1500);

  sensor_vl53.vl53l5cx_set_ranging_frequency_hz(10);
  sensor_vl53.vl53l5cx_start_ranging();
  Serial.println(F("VL53L5CX Ready."));
}

// Logique de comptage
void updatePeopleCount() {
  VL53L5CX_ResultsData Results;
  uint8_t ready = 0;
  sensor_vl53.vl53l5cx_check_data_ready(&ready);
  if (!ready) return;

  sensor_vl53.vl53l5cx_get_ranging_data(&Results);

  int a_zone_count = 0;
  int b_zone_count = 0;
  for (int i = 0; i < 64; i++) {
    if (Results.distance_mm[i] > 200 && Results.distance_mm[i] < 1200) {
      if (i < 32) a_zone_count++;
      else b_zone_count++;
    }
  }

  uint8_t a_result = (a_zone_count > 5) ? 1 : 0;
  uint8_t b_result = (b_zone_count > 5) ? 1 : 0;

  if (a_result == 1 && b_result == 0) {
    if (state == "nothing") state = "A First";
    else if (state == "B First") {
      number_of_people--;
      state = "nothing";
      Serial.println("Event: SORTIE");
    }
  }
  if (a_result == 0 && b_result == 1) {
    if (state == "nothing") state = "B First";
    else if (state == "A First") {
      number_of_people++;
      state = "nothing";
      Serial.println("Event: ENTREE");
    }
  }
  if (a_result == 0 && b_result == 0) state = "nothing";
  if (number_of_people < 0) number_of_people = 0;
}

// Fonctions crypto et envoi
void printHex2(uint8_t val) {
  if (val < 16) Serial1.print('0');
  Serial1.print(val, HEX);
}

void updateMicDb() {
  unsigned long now = millis();
  if (now - micLastMs < MIC_SAMPLE_PERIOD_MS) return;
  micLastMs = now;

  int x = analogRead(MIC_AO_PIN);
  micSum += x;
  micSumSq += (long)x * (long)x;
  micN++;

  static unsigned long winStart = 0;
  if (winStart == 0) winStart = now;
  if (now - winStart >= MIC_WIN_MS) {
    if (micN >= 2) {
      float mean = (float)micSum / (float)micN;
      float ex2  = (float)micSumSq / (float)micN;
      float var  = ex2 - mean * mean;
      if (var < 0) var = 0;
      float rms = sqrtf(var);

      if (!micRefReady) {
        if (now - micCalibStart <= MIC_CALIB_MS) {
          calibRmsSum += rms;
          calibCount++;
        } else {
          micRefRmsDynamic = (calibCount > 0) ? (calibRmsSum / (float)calibCount) : 1.0f;
          if (micRefRmsDynamic < MIC_EPS) micRefRmsDynamic = 1.0f;
          micRefReady = true;
          Serial.print(F("[MIC] Ref RMS calib = "));
          Serial.println(micRefRmsDynamic, 4);
        }
      }

      float ref = micRefReady ? micRefRmsDynamic : 1.0f;
      float dbRel = 20.0f * log10f((rms + MIC_EPS) / (ref + MIC_EPS));
      float db = dbRel + micDbOffset;

      micDbSmoothed = (MIC_EMA_ALPHA * db) + ((1.0f - MIC_EMA_ALPHA) * micDbSmoothed);
      if (micDbSmoothed < 0) micDbSmoothed = 0;
      if (micDbSmoothed > 120) micDbSmoothed = 120;

      micDbLast = micDbSmoothed;
    }
    micSum = 0; micSumSq = 0; micN = 0;
    winStart = now;
  }
}

void sendSecureData() {
  float dhtTemp = dht.readTemperature();
  float dhtHum  = dht.readHumidity();
  int lightVal  = analogRead(lightPin);
  float pres = 0;
  if (bmeOK) pres = bme.readPressure() / 100.0F;

  if (isnan(dhtTemp)) dhtTemp = 0.0;
  if (isnan(dhtHum))  dhtHum  = 0.0;
  float db = micDbLast;

  String payload;
  payload.reserve(160);
  payload  = "{\"r\":" + String(ROOM_ID);
  payload += ",\"l\":" + String(lightVal);
  payload += ",\"t\":" + String(dhtTemp, 1);
  payload += ",\"h\":" + String(dhtHum, 0);
  payload += ",\"pc\":" + String(number_of_people);
  payload += ",\"db\":" + String(db, 1);
  if (bmeOK) payload += ",\"p\":" + String(pres, 0);
  payload += "}";

  Serial.print("Payload original: ");
  Serial.println(payload);

  size_t plainLen = payload.length();
  size_t pad = 16 - (plainLen % 16);
  if (pad == 0) pad = 16;
  size_t totalLen = plainLen + pad;

  if (totalLen > 256) {
    Serial.println("[ERREUR] Payload trop long");
    return;
  }

  uint8_t buffer[256];
  memcpy(buffer, payload.c_str(), plainLen);
  for (size_t i = 0; i < pad; i++) buffer[plainLen + i] = (uint8_t)pad;

  uint8_t encrypted[256];
  for (size_t i = 0; i < totalLen; i += 16) {
    aes256.encryptBlock(encrypted + i, buffer + i);
  }

  uint8_t mac[32];
  sha256.resetHMAC(keyBytes, 32);
  sha256.update(encrypted, totalLen);
  sha256.finalizeHMAC(keyBytes, 32, mac, 32);

  Serial1.print('<');
  
  for (size_t i = 0; i < totalLen; i++) {
    printHex2(encrypted[i]);
    if (i > 0 && i % 16 == 0) {
        delay(2); 
        Serial1.flush(); 
    }
  }
  
  Serial1.print(':');
  
  for (size_t i = 0; i < 32; i++) {
    printHex2(mac[i]);
    if (i > 0 && i % 8 == 0) delay(1);
  }
  
  Serial1.print('>');
  Serial1.println();

  Serial.println("[OK] Paquet sécurisé envoyé avec tempo.");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println(F("--- EMETTEUR SECURISE (V2 FIXED) ---"));

  Serial1.begin(9600);
  delay(500);
  Serial1.print("+++"); delay(1200);
  Serial1.print("ATBD 7\r");    delay(200);
  Serial1.print("ATWR\r");      delay(200);
  Serial1.print("ATCN\r");      delay(200);
  Serial1.end();

  delay(500);
  Serial1.begin(115200);

  Serial1.print("+++"); delay(1200);
  Serial1.print("ATID 1234\r"); delay(200);
  // Serial1.print("ATCE 0\r"); delay(200); // Si routeur
  Serial1.print("ATCN\r");      delay(200);
  Serial.println(F("XBee initialise 115200."));

  memset(keyBytes, 0, 32);
  size_t kLen = strlen(SECRET_KEY);
  if (kLen > 32) kLen = 32;
  memcpy(keyBytes, SECRET_KEY, kLen);
  aes256.setKey(keyBytes, 32);

  dht.begin();
  if (bme.begin(0x76) || bme.begin(0x77)) {
    bmeOK = true;
    Serial.println(F("BME280 OK"));
  }

  pinMode(MIC_AO_PIN, INPUT);
  initVL53();

  micCalibStart = millis();
  Serial.println(F("[MIC] Calibration 3s..."));
}

void loop() {
  updateMicDb();
  updatePeopleCount();

  if (millis() - lastSend >= SEND_EVERY_MS) {
    lastSend = millis();
    sendSecureData();
  }
}