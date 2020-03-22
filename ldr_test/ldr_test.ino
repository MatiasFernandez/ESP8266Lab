#define LDR_PIN A0

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(2000);

  // Wait for serial to initialize.
  while(!Serial) { }

  Serial.println("Device Started");
}

void loop() {
  Serial.println(analogRead(LDR_PIN));
  delay(100);
}
