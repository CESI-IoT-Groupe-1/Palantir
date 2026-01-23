#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT11
const int lightPin = A0;

DHT dht(DHTPIN, DHTTYPE);
Adafruit_BME280 bme;
bool bmeOK = false;

unsigned long lastSend = 0;
const unsigned long SEND_EVERY_MS = 3000;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  delay(2000);

  Serial.println("Capteurs -> XBee");

  dht.begin();

  if (bme.begin(0x76)) { bmeOK = true; Serial.println("BME280 OK (0x76)"); }
  else if (bme.begin(0x77)) { bmeOK = true; Serial.println("BME280 OK (0x77)"); }
  else { bmeOK = false; Serial.println("BME280 introuvable"); }

  Serial1.print("+++"); delay(1200);
  Serial1.print("ATID 1234\r"); delay(500);
  Serial1.print("ATCE 1\r");    delay(500);
  Serial1.print("ATDL FFFF\r"); delay(500);
  Serial1.print("ATWR\r");      delay(500);
  Serial1.print("ATCN\r");      delay(500);

  Serial.println("Emetteur pret : j'envoie...");
}

void loop() {
  if (millis() - lastSend < SEND_EVERY_MS) return;
  lastSend = millis();

  // Lecture capteurs
  float dhtHum = dht.readHumidity();
  float dhtTemp = dht.readTemperature();
  int lightValue = analogRead(lightPin);

  float bmeTemp = NAN, bmeHum = NAN, bmePres = NAN;
  if (bmeOK) {
    bmeTemp = bme.readTemperature();
    bmeHum  = bme.readHumidity();
    bmePres = bme.readPressure() / 100.0F;
  }
#bite
  String payload = "{";
  payload += "\"light_raw\":" + String(lightValue);

  if (!isnan(dhtTemp)) payload += ",\"dht_t\":" + String(dhtTemp, 1);
  if (!isnan(dhtHum))  payload += ",\"dht_h\":" + String(dhtHum, 1);

  if (bmeOK) {
    payload += ",\"bme_t\":" + String(bmeTemp, 1);
    payload += ",\"bme_h\":" + String(bmeHum, 1);
    payload += ",\"bme_p\":" + String(bmePres, 2);
  }

  payload += "}";

  Serial1.println(payload);

  Serial.print("ENVOYE: ");
  Serial.println(payload);
}
