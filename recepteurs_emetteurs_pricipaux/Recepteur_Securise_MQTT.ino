#include <WiFiS3.h>
#include <PubSubClient.h>
#include <AES.h>
#include <SHA256.h>

char ssid[] = "S25 de Jeremy";
char pass[] = "Yeehaaw1";
const char* mqtt_server = "10.23.159.146";
const int   mqtt_port   = 1883;
const char* mqtt_topic  = "capteurs/room212";

IPAddress ip(10, 23, 159, 50);
IPAddress gateway(10, 23, 159, 56);
IPAddress subnet(255, 255, 255, 0);

WiFiClient wifiClient;
PubSubClient client(wifiClient);

const char* SECRET_KEY = "ZIGBEE_SECRET_KEY_2026_PRO_SAFE!";
AES256 aes256;
SHA256 sha256;
uint8_t keyBytes[32];

String rxBuffer = "";
unsigned long lastMqttCheck = 0;
bool receptionEnCours = false;

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

bool hexToBytes(String hex, uint8_t* out, int len) {
  for (int i = 0; i < len; i++) {
    int hi = hexVal(hex[2 * i]);
    int lo = hexVal(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (hi << 4) | lo;
  }
  return true;
}

void processPacket(String packet) {
  packet.trim();

  int sep = packet.indexOf(':');
  if (sep == -1) {
    Serial.print("[ERR] Pas de séparateur ':' trouvé. Packet: ");
    Serial.println(packet);
    return;
  }

  String cipherHex = packet.substring(0, sep);
  String hmacHex = packet.substring(sep + 1);

  if (hmacHex.length() != 64) {
    Serial.print("[ERR] Taille HMAC invalide: ");
    Serial.println(hmacHex.length());
    return;
  }
  
  uint8_t rxMac[32];
  if (!hexToBytes(hmacHex, rxMac, 32)) {
    Serial.println("[ERR] HMAC contient des caractères non-hex");
    return;
  }

  int cipherLen = cipherHex.length() / 2;
  if (cipherLen > 256) {
    Serial.println("[ERR] Cipher trop long");
    return;
  }
  
  uint8_t cipher[256];
  if (!hexToBytes(cipherHex, cipher, cipherLen)) {
    Serial.println("[ERR] Cipher contient des caractères non-hex");
    return;
  }

  uint8_t calcMac[32];
  sha256.resetHMAC(keyBytes, 32);
  sha256.update(cipher, cipherLen);
  sha256.finalizeHMAC(keyBytes, 32, calcMac, 32);

  if (memcmp(calcMac, rxMac, 32) != 0) {
    Serial.println("[SEC] ALERTE: Signature HMAC invalide ! Paquet corrompu ou attaque.");
    return;
  }

  uint8_t plain[256];
  for (int i = 0; i < cipherLen; i += 16) {
    aes256.decryptBlock(plain + i, cipher + i);
  }

  uint8_t pad = plain[cipherLen - 1];
  if (pad < 1 || pad > 16) pad = 0;
  plain[cipherLen - pad] = 0;

  String json = (char*)plain;
  json.replace("nan", "null");
  
  Serial.print("[SUCCES] Donnée Reçue : ");
  Serial.println(json);

  if (client.connected()) {
    if (client.publish(mqtt_topic, json.c_str())) {
      Serial.println("[MQTT] Publié OK");
    } else {
      Serial.println("[MQTT] Echec publication");
    }
  } else {
    Serial.println("[MQTT] Non connecté - donnée perdue");
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- RECEPTEUR MQTT ---");

  Serial1.begin(9600); 
  delay(500);
  Serial1.print("+++"); delay(1200);
  Serial1.print("ATBD 7\r");    delay(200);
  Serial1.print("ATWR\r");      delay(200);
  Serial1.print("ATCN\r");      delay(200);
  Serial1.end(); 
  
  delay(500);
  Serial1.begin(115200);
  Serial.println("XBee port ouvert à 115200.");

  rxBuffer.reserve(512);
  memset(keyBytes, 0, 32);
  strncpy((char*)keyBytes, SECRET_KEY, 32);
  aes256.setKey(keyBytes, 32);

  WiFi.config(ip, gateway, subnet);
  WiFi.begin(ssid, pass);
  Serial.print("Connexion WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi OK!");

  client.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      unsigned long now = millis();
      if (now - lastMqttCheck > 5000) { 
        lastMqttCheck = now;
        Serial.print("Reconnexion MQTT...");
        if (client.connect("UNO_R4_Gateway")) {
          Serial.println("OK");
        }
      }
    } else {
      client.loop();
    }
  }

  while (Serial1.available()) {
    char c = (char)Serial1.read();

    if (c == '<') {
      rxBuffer = "";
      receptionEnCours = true;
    }
    else if (c == '>') {
      if (receptionEnCours) {
        processPacket(rxBuffer);
        receptionEnCours = false;
        rxBuffer = "";
      }
    }
    else if (receptionEnCours) {
      if ((c >= '0' && c <= '9') || 
          (c >= 'A' && c <= 'F') || 
          (c >= 'a' && c <= 'f') || 
          c == ':') {
        rxBuffer += c;
      }
      
      if (rxBuffer.length() > 600) {
        Serial.println("[ERR] Buffer overflow, packet rejeté");
        receptionEnCours = false;
        rxBuffer = "";
      }
    }
  }
}