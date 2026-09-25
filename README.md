# Distributed Smart Temperature Sensing and Sensor Fusion Network

## Overview

The **Distributed Smart Temperature Sensing and Sensor Fusion Network** is an embedded IoT-based temperature monitoring system designed to obtain reliable temperature measurements from multiple sensing nodes and visualize them through a centralized SCADA dashboard.

Each sensing node uses two different temperature sensors:

- **DS18B20 digital temperature sensor**
- **10 kΩ NTC thermistor**

The sensor measurements are acquired by an **ESP32**, calibrated, validated, processed, and combined using a sensor fusion algorithm. The fused temperature is then filtered to obtain a stable and reliable final temperature estimate.

Each node also includes **GPS positioning**, allowing temperature measurements to be associated with geographical coordinates. Data from the distributed nodes is transmitted wirelessly through **Wi-Fi and MQTT** to a **Node-RED-based SCADA dashboard** for real-time monitoring.

The overall system demonstrates the integration of:

- Embedded systems
- Sensor calibration
- Signal processing
- Sensor fusion
- IoT communication
- GPS-based distributed sensing
- Real-time SCADA visualization

---

## Project Objectives

The main objectives of this project are to:

- Design a distributed temperature sensing node.
- Measure temperature using two independent sensors.
- Calibrate the sensors against a reference temperature.
- Reduce measurement noise and fluctuations.
- Detect and reject unrealistic temperature measurements.
- Combine multiple sensor readings using sensor fusion.
- Improve the reliability of the final temperature estimate.
- Obtain geographical coordinates for each sensing node.
- Transmit measurements wirelessly to a central monitoring system.
- Visualize sensor measurements and fused temperature in real time.
- Build a scalable network capable of supporting multiple distributed sensing nodes.

---

## System Architecture

The overall data-processing sequence of each sensing node is:

```text
        NTC Thermistor          DS18B20
              │                    │
              └─────────┬──────────┘
                        │
                        ▼
               Sensor Acquisition
                     ESP32
                        │
                        ▼
                Sensor Calibration
                        │
                        ▼
             Measurement Validation
                        │
                        ▼
              Weighted Sensor Fusion
                        │
                        ▼
                  Kalman Filter
                        │
                        ▼
               Final Temperature
                        │
               ┌────────┴────────┐
               │                 │
               ▼                 ▼
             OLED              Wi-Fi
                                   │
                                   ▼
                                  MQTT
                                   │
                                   ▼
                               Node-RED
                                   │
                                   ▼
                           SCADA Dashboard
```

GPS information is also acquired by the ESP32 and transmitted together with the sensing-node information so that measurements can be associated with their geographical locations.

---

## Hardware Used

The main hardware used in the sensing node includes:

| Component | Purpose |
|---|---|
| ESP32 | Main controller, signal processing, and wireless communication |
| DS18B20 | Digital temperature measurement |
| MF5A-3 10K B3950 NTC Thermistor | Analog temperature measurement |
| NEO-6M GPS Module | Obtaining geographical coordinates |
| SSD1306 OLED Display | Local display of system information |
| IRLZ44N MOSFET | Power switching/control |
| Resistors and supporting components | NTC voltage divider and circuit implementation |

---

## Temperature Sensors

### DS18B20

The **DS18B20** is a digital temperature sensor communicating with the ESP32 through the **1-Wire protocol**.

The digital interface provides a direct temperature reading and avoids the ADC conversion required by an analog sensor.

The DS18B20 was used as one of the two independent temperature measurements for the sensor fusion process.

---

### NTC Thermistor

A **MF5A-3 10K B3950 NTC thermistor** is used as the second temperature sensor.

Because the NTC is a resistive sensor, it is connected as part of a voltage-divider circuit and measured using the ESP32 ADC.

Multiple ADC measurements are taken before temperature calculation in order to reduce random ADC noise.

For each temperature measurement:

```text
20 ADC samples
      │
      ▼
Average ADC value
      │
      ▼
Thermistor resistance
      │
      ▼
Beta equation
      │
      ▼
Raw NTC temperature
      │
      ▼
Calibration equation
      │
      ▼
Corrected NTC temperature
```

---

## NTC Temperature Calculation

The resistance of the NTC thermistor is calculated from the measured voltage-divider value.

The thermistor resistance is then converted into temperature using the **Beta equation**.

A general form of the Beta equation is:

```math
\frac{1}{T}
=
\frac{1}{T_0}
+
\frac{1}{B}
\ln\left(\frac{R}{R_0}\right)
```

where:

- `T` = calculated temperature in Kelvin
- `T₀` = nominal temperature in Kelvin
- `R` = measured thermistor resistance
- `R₀` = nominal thermistor resistance
- `B` = thermistor Beta constant

The result is then converted from Kelvin to degrees Celsius.

---

## Sensor Calibration

Sensor calibration was carried out by comparing sensor readings with reference temperature measurements.

The calibration process was used to identify measurement bias and improve the agreement between the sensors.

### NTC Calibration

The experimentally obtained calibration relationship for the NTC thermistor is:

```math
T_{NTC,cal}
=
1.008076T_{NTC,raw}
-
2.507173
```

where:

- `T_NTC,raw` = temperature obtained directly from the thermistor calculation
- `T_NTC,cal` = calibrated NTC temperature

This correction compensates for the systematic error observed during calibration.

### DS18B20 Calibration

The DS18B20 measurements showed sufficiently good agreement with the reference measurements during the calibration process.

Therefore, no additional correction equation was applied to the DS18B20 reading.

---

## Measurement Validation

Before sensor fusion, the temperature measurements are checked for validity.

The accepted operating temperature range is:

```text
-20 °C to 100 °C
```

Measurements outside this range are rejected.

The system also checks for unrealistic sudden changes.

A temperature variation greater than approximately:

```text
8 °C
```

between consecutive measurements is treated as a possible abnormal measurement and rejected.

This prevents faulty readings from strongly affecting the fused temperature.

---

## Noise Reduction

### ADC Averaging

Since the thermistor is an analog sensor, its reading is affected by ADC noise.

To reduce this effect, **20 ADC samples** are collected and averaged before the NTC temperature is calculated.

```text
ADC Sample 1
ADC Sample 2
ADC Sample 3
     .
     .
ADC Sample 20
      │
      ▼
    Average
      │
      ▼
Temperature Calculation
```

This significantly improves the stability of the analog temperature measurement.

---

## EMA Filtering Investigation

An **Exponential Moving Average (EMA)** filter was also investigated during development.

The EMA filter is defined as:

```math
T_k^{EMA}
=
\alpha T_k
+
(1-\alpha)T_{k-1}^{EMA}
```

where:

- `T_k` = current temperature measurement
- `T_(k-1)^EMA` = previous filtered value
- `α` = smoothing factor

A lower value of `α` provides stronger smoothing but causes a slower response to temperature changes.

EMA provided a simple method of reducing short-term fluctuations and was useful during the development and evaluation stage.

For the final processing architecture, sensor fusion followed by a **Kalman filter** was used to obtain the final temperature estimate.

---

## Sensor Fusion

Using only one temperature sensor can make the system more vulnerable to:

- Sensor noise
- Calibration error
- Individual sensor failure
- Temporary abnormal measurements

Therefore, measurements from the DS18B20 and NTC thermistor are combined using **Weighted Least Squares / inverse-variance weighted sensor fusion**.

Sensors with lower measurement variance are given higher importance.

The fused temperature is calculated using:

```math
T_{fused}
=
w_{DS}T_{DS}
+
w_{NTC}T_{NTC}
```

where:

- `T_DS` = DS18B20 temperature
- `T_NTC` = calibrated NTC temperature
- `w_DS` = DS18B20 weighting factor
- `w_NTC` = NTC weighting factor

The experimentally determined sensor standard deviations were approximately:

| Sensor | Standard Deviation |
|---|---:|
| DS18B20 | 0.0251 °C |
| NTC Thermistor | 0.0540 °C |

Using inverse-variance weighting, the resulting weights are approximately:

```text
DS18B20 weight = 0.822
NTC weight     = 0.178
```

Therefore:

```math
T_{fused}
=
0.822T_{DS}
+
0.178T_{NTC}
```

The DS18B20 receives a larger weighting because it demonstrated lower measurement variation during testing.

---

## Why Weighted Sensor Fusion?

Several approaches can be used to combine temperature measurements.

A simple average would give both sensors equal importance:

```math
T_{avg}
=
\frac{T_{DS}+T_{NTC}}{2}
```

However, the two sensors do not have exactly the same measurement uncertainty.

Weighted fusion provides a better approach because the contribution of each sensor is determined according to its measured reliability.

Therefore, the more stable sensor has a larger influence on the final result.

---

## Sensor Failure Handling

The fusion algorithm is designed to handle situations where one sensor measurement becomes invalid.

The normal case is:

```text
DS18B20 valid + NTC valid
            │
            ▼
       Sensor Fusion
```

If only the DS18B20 is valid:

```text
Only DS18B20 valid
        │
        ▼
Use DS18B20 measurement
```

If only the NTC is valid:

```text
Only NTC valid
       │
       ▼
Use NTC measurement
```

This provides a basic level of fault tolerance and allows temperature monitoring to continue even if one sensor measurement becomes temporarily unavailable.

---

## Kalman Filtering

After sensor fusion, a **one-dimensional Kalman filter** is applied to the fused temperature.

The Kalman filter combines the current measurement with the previous estimated temperature and measurement uncertainty.

This produces a temperature output that is:

- Smooth
- Stable
- Less sensitive to measurement noise
- Responsive to actual temperature changes

The process noise parameter used during development was:

```text
Q = 0.01
```

The complete final temperature-processing sequence is therefore:

```text
DS18B20 ────────────────┐
                        │
                        ▼
                    Validation
                        │
                        ├──────────────┐
                        │              │
NTC → ADC Averaging → Calibration     │
                        │              │
                        ▼              │
                    Validation         │
                        │              │
                        └──────┬───────┘
                               │
                               ▼
                    Weighted Sensor Fusion
                               │
                               ▼
                         Kalman Filter
                               │
                               ▼
                        Final Temperature
```

---

## GPS Integration

A **NEO-6M GPS module** is integrated with each sensing node.

The GPS module provides the geographical location of the sensing node.

This allows the monitoring system to associate:

```text
Temperature
    +
Node Identification
    +
Latitude
    +
Longitude
```

with each measurement.

When multiple sensing nodes are deployed at different locations, their measurements can be used to construct a distributed temperature monitoring network.

---

## Wireless Communication

The ESP32 provides built-in Wi-Fi connectivity.

The processed data follows the communication path:

```text
Sensors
   │
   ▼
ESP32
   │
   ▼
Wi-Fi
   │
   ▼
MQTT Broker
   │
   ▼
Node-RED
   │
   ▼
SCADA Dashboard
```

MQTT was selected because it is lightweight and well suited to IoT systems where multiple distributed devices periodically transmit sensor information.

The architecture can be extended by adding additional sensing nodes while maintaining the same central monitoring system.

---

## Node-RED SCADA Dashboard

A real-time monitoring dashboard was developed using **Node-RED**.

The dashboard is used to visualize information received from the sensing nodes.

The dashboard includes information such as:

- DS18B20 temperature
- NTC temperature
- Fused temperature
- Temperature history
- Wi-Fi/network status
- GPS latitude
- GPS longitude
- Node location
- Map-based visualization

This allows the operator to observe both the individual sensor readings and the final processed temperature.

---

## Data Processing Flow

The complete data-processing flow of the system can be summarized as:

```text
Temperature Measurement
          │
          ▼
Sensor Acquisition
          │
          ▼
NTC ADC Averaging
          │
          ▼
Temperature Conversion
          │
          ▼
Sensor Calibration
          │
          ▼
Measurement Validation
          │
          ▼
Inverse-Variance Weighted Fusion
          │
          ▼
Kalman Filtering
          │
          ▼
Final Temperature Estimate
          │
          ▼
GPS / Node Information
          │
          ▼
Wi-Fi
          │
          ▼
MQTT
          │
          ▼
Node-RED SCADA
          │
          ▼
Real-Time Visualization
```

---

## Local Display

An **SSD1306 OLED display** is used to provide local information directly at the sensing node.

This allows important operating information to be observed without requiring access to the remote SCADA dashboard.

---

## Power Control

An **IRLZ44N N-channel MOSFET** is used for controlled switching of selected loads in the sensing node.

This allows peripherals to be powered only when required and helps reduce unnecessary power consumption in portable operation.

---

## Distributed Network Concept

The project is designed around the concept of multiple intelligent sensing nodes.

```text
                ┌───────────────┐
                │ Sensor Node 1 │
                └───────┬───────┘
                        │
                        ▼
                ┌───────────────┐
                │               │
┌───────────────┤ Wi-Fi / MQTT  ├───────────────┐
│               │               │               │
▼               └───────┬───────┘               ▼
Sensor Node 2            │                 Sensor Node 3
                         │
                         ▼
                    MQTT Broker
                         │
                         ▼
                      Node-RED
                         │
                         ▼
                  SCADA Dashboard
```

Each node performs local sensing and processing before transmitting data.

This reduces the amount of raw data that must be transmitted and allows each node to operate as an intelligent edge device.

---

## Testing

The system was evaluated through several stages, including:

- Individual sensor testing
- NTC thermistor characterization
- DS18B20 testing
- Sensor calibration
- Sensor noise analysis
- Comparison against reference temperature
- Sensor fusion testing
- Temperature-change testing
- Wireless communication testing
- GPS testing
- MQTT data transmission
- Node-RED dashboard testing
- Complete system integration testing

Testing confirmed that changes in sensor measurements could be processed by the ESP32 and transmitted to the SCADA dashboard for real-time visualization.

---

## Advantages of the System

The developed system provides several advantages:

- Uses two independent temperature sensing methods
- Reduces sensor noise
- Corrects systematic sensor error through calibration
- Gives more importance to the more reliable sensor
- Detects invalid or abnormal measurements
- Provides fallback operation if one sensor measurement is unavailable
- Provides a filtered final temperature estimate
- Supports wireless data transmission
- Provides geographical information using GPS
- Enables real-time remote monitoring
- Can be expanded into a larger distributed sensing network

---

## Applications

The concept can be extended to applications such as:

- Environmental temperature monitoring
- Smart agriculture
- Industrial temperature monitoring
- Warehouse monitoring
- Cold-chain monitoring
- Distributed IoT sensing
- Laboratory monitoring
- Smart building monitoring
- Remote environmental data collection

---

## Repository Structure

```text
distributed-temperature-sensor-fusion-network/
│
├── README.md
│
├── firmware/
│   └── ESP32 source code
│
├── hardware/
│   ├── circuit diagrams
│   └── schematic
│
├── calibration/
│   ├── calibration data
│   └── calibration results
│
├── sensor-fusion/
│   └── sensor fusion implementation
│
├── dashboard/
│   ├── Node-RED flow
│   └── dashboard screenshots
│
├── results/
│   ├── experimental data
│   └── plots
│
└── docs/
    └── project documentation
```

---

## Software and Tools

The project uses:

- **Arduino IDE** – ESP32 firmware development
- **Embedded C/C++** – Microcontroller programming
- **Node-RED** – SCADA dashboard development
- **MQTT** – IoT communication
- **Wi-Fi** – Wireless network communication

---

## Future Improvements

Possible future developments include:

- Increasing the number of distributed sensing nodes
- Improving automatic sensor fault detection
- Adding long-term cloud data storage
- Improving power-management strategies
- Developing a dedicated PCB
- Improving enclosure design
- Adding more environmental sensors
- Developing temperature-map interpolation between distributed nodes
- Remote configuration of sensing nodes
- Mobile-based monitoring

---

## Authors

Developed as part of an engineering project for the course:

**EE2120 - Electrical Measurements and Instrumentation**

### Team Members

- Juththis.S
- Pooja J.J.K.
- Magishana P.
- Tharmika B.

---

## Project Status

**Prototype developed and tested.**

The system successfully demonstrates:

- Distributed temperature sensing
- Sensor calibration
- Sensor fusion
- Filtering
- GPS-based node positioning
- Wireless communication
- Real-time SCADA visualization

---

## License

This repository is intended for **educational and academic purposes**.
