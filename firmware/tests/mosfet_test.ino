#define MOSFET_PIN 25

void setup()
{
  pinMode(MOSFET_PIN, OUTPUT);
}

void loop()
{
  digitalWrite(MOSFET_PIN, HIGH);
  Serial.println("MOSFET ON");
  delay(3000);

  digitalWrite(MOSFET_PIN, LOW);
  Serial.println("MOSFET OFF");
  delay(3000);
}
