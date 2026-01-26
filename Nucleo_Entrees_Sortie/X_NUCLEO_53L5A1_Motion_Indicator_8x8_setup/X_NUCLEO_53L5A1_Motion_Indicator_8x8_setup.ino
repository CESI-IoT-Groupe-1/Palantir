#include <Wire.h>
#include <Arduino.h>
#include <WiFiS3.h>
#include <PubSubClient.h>

#include <vl53l5cx_class.h>

// ===== WIFI + MQTT =====
char ssid[] = "S25 de Jeremy";
char pass[] = "Yeehaaw1";
const char* mqtt_server = "172.24.201.249";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// ===== SENSOR PINS =====
#define LPN_PIN 5
#define I2C_RST_PIN 3
#define PWREN_PIN A3

#define DEV_I2C Wire

VL53L5CX sensor_vl53l5cx_top(&DEV_I2C, LPN_PIN, I2C_RST_PIN);
VL53L5CX_Motion_Configuration motion_config;

// ===== COUNTING =====
int number_of_people = 0;
String state = "nothing";

void setup()
{
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Starting VL53L5CX + MQTT");

  // ===== POWER SENSOR =====
  pinMode(PWREN_PIN, OUTPUT);
  digitalWrite(PWREN_PIN, HIGH);
  delay(10);

  // ===== I2C =====
  DEV_I2C.begin();
  DEV_I2C.setClock(400000);

  // ===== INIT SENSOR =====
  sensor_vl53l5cx_top.begin();
  sensor_vl53l5cx_top.init_sensor();

  sensor_vl53l5cx_top.vl53l5cx_set_resolution(VL53L5CX_RESOLUTION_8X8);
  sensor_vl53l5cx_top.vl53l5cx_motion_indicator_init(&motion_config, VL53L5CX_RESOLUTION_8X8);
  sensor_vl53l5cx_top.vl53l5cx_motion_indicator_set_resolution(&motion_config, VL53L5CX_RESOLUTION_8X8);
  sensor_vl53l5cx_top.vl53l5cx_motion_indicator_set_distance_motion(&motion_config, 400, 1500);
  sensor_vl53l5cx_top.vl53l5cx_set_ranging_frequency_hz(10);

  sensor_vl53l5cx_top.vl53l5cx_start_ranging();

  Serial.println("VL53L5CX ready!");

  // ===== WIFI =====
  WiFi.begin(ssid, pass);

  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");

  IPAddress ip = WiFi.localIP();
  while (ip == IPAddress(0,0,0,0)) {
    delay(200);
    ip = WiFi.localIP();
  }

  Serial.print("IP: ");
  Serial.println(ip);

  // ===== MQTT =====
  client.setServer(mqtt_server, 1883);

  Serial.print("MQTT connecting...");
  if (client.connect("UNO_R4_VL53")) {
    Serial.println("OK");
  } else {
    Serial.print("FAILED rc=");
    Serial.println(client.state());
  }
}

void loop()
{
  VL53L5CX_ResultsData Results;
  uint8_t NewDataReady;

  client.loop();

  sensor_vl53l5cx_top.vl53l5cx_check_data_ready(&NewDataReady);

  if (NewDataReady)
  {
    sensor_vl53l5cx_top.vl53l5cx_get_ranging_data(&Results);

    int a_zone_count = 0;
    int b_zone_count = 0;

    for (int i = 0; i < 64; i++)
    {
      if (Results.distance_mm[i] > 200 && Results.distance_mm[i] < 1200)
      {
        if (i < 32) a_zone_count++;
        else b_zone_count++;
      }
    }

    uint8_t a_result = (a_zone_count > 5) ? 1 : 0;
    uint8_t b_result = (b_zone_count > 5) ? 1 : 0;

    String event = "";

    // ===== LOGIC ENTRY / EXIT =====
    if (a_result == 1 && b_result == 0) {
      if (state == "nothing") state = "A First";
      else if (state == "B First") {
        event = "SORTIE";
        number_of_people--;
        state = "nothing";
      }
    }

    if (a_result == 0 && b_result == 1) {
      if (state == "nothing") state = "B First";
      else if (state == "A First") {
        event = "ENTREE";
        number_of_people++;
        state = "nothing";
      }
    }

    if (a_result == 0 && b_result == 0) {
      state = "nothing";
    }

    // ===== MQTT SEND =====
    if (event != "") {
      Serial.println("🚶 " + event);
      Serial.print("👥 Count: ");
      Serial.println(number_of_people);

      client.publish("CESI/passage", event.c_str());

      char buf[10];
      sprintf(buf, "%d", number_of_people);
      client.publish("CESI/compteur", buf);
    }
  }

  delay(50);
}
