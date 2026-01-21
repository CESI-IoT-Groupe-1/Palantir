void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  delay(2000);

  Serial1.print("+++"); delay(1200);
  Serial1.print("ATID 1234\r"); delay(500);
  Serial1.print("ATCE 1\r");    delay(500);
  Serial1.print("ATDL FFFF\r");  delay(500);
  Serial1.print("ATWR\r");      delay(500);
  Serial1.print("ATCN\r");      delay(500);
  Serial.println("Emetteur pret : j'envoie...");
}

void loop() {
  Serial1.println("MESSAGE_FROM_A");
  Serial.println("Envoi en cours...");
  delay(2000);
}