# Firmware

This folder contains the firmware developed for the Distributed Smart Temperature Sensing and Sensor Fusion Network.

## Main Firmware

The `main/` folder contains the final integrated ESP32 firmware used in the complete sensing node.

The main firmware handles:

- DS18B20 temperature measurement
- NTC thermistor measurement
- Sensor calibration
- Measurement validation
- Sensor fusion
- Kalman filtering
- GPS data acquisition
- OLED display
- Wi-Fi communication
- MQTT communication
- MOSFET-based power control

## Testing Firmware

The `tests/` folder contains individual test programs used during hardware development and debugging.

### OLED Test

Used to verify communication with the OLED display and confirm that text and sensor information can be displayed correctly.

### GPS Test

Used to verify communication with the GPS module and check latitude, longitude, and GPS data reception.

### MOSFET Test

Used to verify MOSFET switching and confirm that connected peripherals can be controlled correctly.

### Samples Collection

Used to collect sample readings from the NTC and DB18S20.
