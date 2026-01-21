void setup() {
  Serial.begin(9600);
  Serial1.begin(9600);
  delay(2000);

  Serial1.print("+++"); delay(1200);
  Serial1.print("ATID 1234\r"); delay(500);
  Serial1.print("ATCE 0\r");    delay(500);
  Serial1.print("ATJV 1\r");    delay(500);
  Serial1.print("ATWR\r");      delay(500);
  Serial1.print("ATCN\r");      delay(500);
  Serial.println("Recepteur pret : j'ecoute...");
}

void loop() {
  if (Serial1.available()) {
    while(Serial1.available()) {
      Serial.write(Serial1.read());
    }
  }
}