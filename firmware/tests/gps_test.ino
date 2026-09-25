#include <Arduino.h>

#define GPS_RX 16
#define GPS_TX 17

HardwareSerial GPS(2);

void setup()
{
  Serial.begin(115200);

  GPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  Serial.println();
  Serial.println("================================");
  Serial.println("       NEO-6M GPS TEST");
  Serial.println("================================");
  Serial.println("ESP32 RX = GPIO16");
  Serial.println("ESP32 TX = GPIO17");
  Serial.println("GPS Baud = 9600");
  Serial.println();
  Serial.println("Waiting for GPS data...");
}

void loop()
{
  while (GPS.available())
  {
    char c = GPS.read();
    Serial.write(c);
  }
}
