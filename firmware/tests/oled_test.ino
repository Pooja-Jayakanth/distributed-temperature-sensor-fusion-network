#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup()
{
  Serial.begin(115200);

  Wire.begin(OLED_SDA, OLED_SCL);

  // Most 0.96" OLEDs use 0x3C
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED NOT FOUND!");
    while (1);
  }

  Serial.println("OLED FOUND!");

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(10, 10);
  display.println("OLED TEST");

  display.setTextSize(1);
  display.setCursor(20, 40);
  display.println("ESP32 WORKING");

  display.display();
}

void loop()
{
}
