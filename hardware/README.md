# Hardware

This folder contains the hardware details of the **Distributed Smart Temperature Sensing and Sensor Fusion Network**.

The sensing node is built around an **ESP32 Dev Module**. Two temperature sensors are used: a **DS18B20 digital temperature sensor** and an **MF5A-3 10 kΩ B3950 NTC thermistor**. The ESP32 reads both sensors, processes the measurements, performs sensor fusion, and obtains the final temperature estimate.

A **NEO-6M GPS module** provides the geographical location of the sensing node, while an **SSD1306 OLED display** is used for local visualization. Two **IRLZ44N N-channel MOSFETs** are used as low-side switches to control the ground paths of the GPS and OLED so that these peripherals can be switched off when they are not required. A push button is used to activate the display and request a GPS location update.

The ESP32 also uses Wi-Fi and MQTT to transmit the processed temperature and location information to the Node-RED monitoring dashboard.

## Hardware Logic

During normal operation, the DS18B20 and NTC thermistor continuously measure temperature.

The ESP32:
1. Reads both temperature sensors.
2. Calibrates and validates the measurements.
3. Performs WLS sensor fusion.
4. Applies Kalman filtering to obtain the final temperature.
5. Sends the processed data to the dashboard through Wi-Fi and MQTT.

A short button press turns on the OLED to display the current system information. A long button press turns on both the OLED and GPS to obtain and save an updated GPS location.

The GPS and OLED are normally switched off when they are not required in order to reduce unnecessary power consumption.

## Complete Pin Connections

| Component / Signal | Component Pin | ESP32 / Circuit Connection | Purpose |
|---|---|---|---|
| DS18B20 | VCC | 3.3 V | Sensor supply |
| DS18B20 | GND | GND | Ground |
| DS18B20 | DATA | GPIO4 | 1-Wire temperature data |
| DS18B20 Pull-up | 4.7 kΩ resistor | Between GPIO4 and 3.3 V | DATA pull-up |
| NTC Divider | Fixed resistor upper terminal | 3.3 V | Divider supply |
| NTC Divider | 10 kΩ fixed resistor lower terminal | GPIO34 | Divider output |
| NTC Thermistor | Upper terminal | GPIO34 | ADC measurement point |
| NTC Thermistor | Lower terminal | GND | Ground |
| NEO-6M GPS | TX | GPIO16 | GPS TX → ESP32 RX2 |
| NEO-6M GPS | RX | GPIO17 | GPS RX ← ESP32 TX2 |
| NEO-6M GPS | VCC | GPS supply used in prototype | GPS supply |
| NEO-6M GPS | GND | GPS MOSFET Drain | Switched ground |
| GPS IRLZ44N | Gate | GPIO25 through 220 Ω | GPS power control |
| GPS IRLZ44N | Drain | GPS GND | Low-side switched connection |
| GPS IRLZ44N | Source | Common GND | Ground |
| GPS MOSFET Pull-down | 100 kΩ resistor | Gate to GND | Prevents floating gate |
| SSD1306 OLED | VCC | 3.3 V | OLED supply |
| SSD1306 OLED | SDA | GPIO21 | I2C data |
| SSD1306 OLED | SCL | GPIO22 | I2C clock |
| SSD1306 OLED | GND | OLED MOSFET Drain | Switched ground |
| OLED IRLZ44N | Gate | GPIO26 through 220 Ω | OLED power control |
| OLED IRLZ44N | Drain | OLED GND | Low-side switched connection |
| OLED IRLZ44N | Source | Common GND | Ground |
| OLED MOSFET Pull-down | 100 kΩ resistor | Gate to GND | Prevents floating gate |
| Push Button | Terminal 1 | GPIO27 | User input |
| Push Button | Terminal 2 | GND | Active-LOW button |
| ESP32 | Power | USB / 5 V | Main controller supply |
| ESP32 | GND | Common GND | Common circuit reference |

## ESP32 GPIO Summary

| GPIO | Function |
|---:|---|
| GPIO4 | DS18B20 DATA |
| GPIO16 | GPS RX2 |
| GPIO17 | GPS TX2 |
| GPIO21 | OLED SDA |
| GPIO22 | OLED SCL |
| GPIO25 | GPS MOSFET control |
| GPIO26 | OLED MOSFET control |
| GPIO27 | Push button |
| GPIO34 | NTC ADC input |
