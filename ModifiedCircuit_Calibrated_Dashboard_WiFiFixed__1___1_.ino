#include <OneWire.h>
#include <DallasTemperature.h>
#include <TinyGPSPlus.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <math.h>


// DASHBOARD CONNECTIVITY

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID     = "Tharmika";
const char* WIFI_PASSWORD = "Thar1234";
const char* MQTT_SERVER   = "192.168.184.135";
const int   MQTT_PORT     = 1883;
const char* MQTT_TOPIC    = "Temperature";

WiFiClient espClient;
PubSubClient mqttClient(espClient);


unsigned long lastWiFiRetry = 0;
unsigned long lastMQTTRetry = 0;

const unsigned long WIFI_RETRY_INTERVAL = 10000UL; // 10 s
const unsigned long MQTT_RETRY_INTERVAL = 5000UL;  // 5 s



// EE2120 SMART DISTRIBUTED TEMPERATURE NODE

// NODE ID


const char* NODE_ID = "NODE01";



// PIN DEFINITIONS


#define DS18B20_PIN 4
#define NTC_PIN     34

// GPS UART
#define GPS_RX_PIN  16     // ESP32 RX <- GPS TX
#define GPS_TX_PIN  17     // ESP32 TX -> GPS RX

// OLED I2C
#define OLED_SDA    21
#define OLED_SCL    22

// NEW POWER CONTROL
#define GPS_POWER_PIN   25
#define OLED_POWER_PIN  26
#define BUTTON_PIN      27



// OLED


#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_ADDRESS  0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

bool oledPowered = false;
bool oledAvailable = false;



// OLED DISPLAY STATES


enum OLEDMode
{
  OLED_NORMAL,
  OLED_GPS_UPDATING,
  OLED_GPS_UPDATED,
  OLED_GPS_TIMEOUT
};

OLEDMode oledMode = OLED_NORMAL;



// DS18B20


OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);



// GPS


TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

bool gpsPowered = false;
bool gpsUpdating = false;

unsigned long gpsUpdateStart = 0;
unsigned long gpsFixCountAtStart = 0;

const unsigned long GPS_TIMEOUT = 120000UL; // 2 minutes



// GPS LOCATION
// Saved in ESP32 non-volatile memory

Preferences preferences;

double latitude = 0.0;
double longitude = 0.0;

uint32_t satellites = 0;

bool gpsLocationValid = false;



// NTC PARAMETERS


const float R_FIXED = 10000.0;
const float R_REF = 10000.0;
const float T_REF_K = 298.15;
const float BETA = 3950.0;

const int ADC_MAX = 4095;
const int ADC_SAMPLES = 20;



// CALIBRATION


// DS18B20

const float DS_CAL_A = 1.0000;
const float DS_CAL_B = 0.0000;

// NTC MF5A-3 10K B3950
// Calibration from the measured reference points:
// 28.0 C, 29.0 C, 29.5 C and 31.0 C
// T_calibrated = 1.008076 * T_raw - 2.507173
const float NTC_CAL_A = 1.008076;
const float NTC_CAL_B = -2.507173;



// SENSOR NOISE


const float DS_SIGMA = 0.0251;
const float NTC_SIGMA = 0.0540;

const float DS_VARIANCE =
  DS_SIGMA * DS_SIGMA;

const float NTC_VARIANCE =
  NTC_SIGMA * NTC_SIGMA;



// VALID TEMPERATURE LIMITS

const float MIN_VALID_TEMP = -20.0;
const float MAX_VALID_TEMP = 100.0;

const float MAX_STEP_CHANGE = 8.0;



// PREVIOUS VALUES FOR OUTLIER DETECTION


float previousDS = 0.0;
float previousNTC = 0.0;

bool previousDSAvailable = false;
bool previousNTCAvailable = false;



// KALMAN FILTER


float kalmanTemperature = 0.0;
float kalmanP = 1.0;

bool kalmanInitialized = false;

const float PROCESS_SIGMA = 0.10;

const float PROCESS_VARIANCE =
  PROCESS_SIGMA * PROCESS_SIGMA;




float currentFinalTemperature = 0.0;

bool currentFusionValid = false;
bool currentDSValid = false;
bool currentNTCValid = false;



// MEASUREMENT TIMING


unsigned long lastMeasurementTime = 0;

const unsigned long MEASUREMENT_INTERVAL = 1000;



// BUTTON


const unsigned long BUTTON_DEBOUNCE = 40;
const unsigned long LONG_PRESS_TIME = 2000;

bool lastButtonReading = HIGH;
bool stableButtonState = HIGH;

unsigned long lastButtonChange = 0;
unsigned long buttonPressStart = 0;

bool buttonPressActive = false;
bool longPressTriggered = false;



// OLED TIMING


const unsigned long OLED_SHORT_TIME = 10000;
const unsigned long OLED_AFTER_GPS_TIME = 5000;

unsigned long oledOffTime = 0;
unsigned long lastOLEDRefresh = 0;

const unsigned long OLED_REFRESH_INTERVAL = 250;



// FUNCTION PROTOTYPES


void refreshOLED();
void serviceButton();
void serviceGPS();
void serviceOLED();
void serviceBackground();
void connectWiFiAndMQTT(); // ADDED FOR DASHBOARD



// DASHBOARD WIFI/MQTT HELPER


void connectWiFiAndMQTT()
{
  unsigned long now = millis();

  // ----------------------------------------------------------
  // WI-FI
  // ----------------------------------------------------------

  if (WiFi.status() != WL_CONNECTED)
  {
    if (mqttClient.connected())
    {
      mqttClient.disconnect();
    }

    if (now - lastWiFiRetry >= WIFI_RETRY_INTERVAL)
    {
      lastWiFiRetry = now;

      Serial.print("Wi-Fi status: ");
      Serial.println((int)WiFi.status());

      Serial.println("Wi-Fi reconnect requested...");
      WiFi.reconnect();
    }

    return;
  }

  
  // MQTT
 

  if (!mqttClient.connected())
  {
    if (now - lastMQTTRetry >= MQTT_RETRY_INTERVAL)
    {
      lastMQTTRetry = now;

      String clientId = "Node_";
      clientId += NODE_ID;

      Serial.print("Connecting MQTT as ");
      Serial.println(clientId);

      if (mqttClient.connect(clientId.c_str()))
      {
        Serial.println("MQTT CONNECTED");
      }
      else
      {
        Serial.print("MQTT connection failed, state = ");
        Serial.println(mqttClient.state());
      }
    }

    return;
  }

  mqttClient.loop();
}




// CALIBRATION


float calibrateDS18B20(float rawTemperature)
{
  return
    DS_CAL_A * rawTemperature
    +
    DS_CAL_B;
}


float calibrateNTC(float rawTemperature)
{
  return
    NTC_CAL_A * rawTemperature
    +
    NTC_CAL_B;
}



// LOAD SAVED GPS LOCATION


void loadSavedGPS()
{
  preferences.begin("gpsloc", true);

  gpsLocationValid =
    preferences.getBool(
      "valid",
      false
    );

  if (gpsLocationValid)
  {
    latitude =
      preferences.getDouble(
        "lat",
        0.0
      );

    longitude =
      preferences.getDouble(
        "lon",
        0.0
      );
  }

  preferences.end();
}



// SAVE GPS LOCATION


void saveGPS()
{
  preferences.begin("gpsloc", false);

  preferences.putDouble(
    "lat",
    latitude
  );

  preferences.putDouble(
    "lon",
    longitude
  );

  preferences.putBool(
    "valid",
    true
  );

  preferences.end();

  Serial.println(
    "GPS location saved to ESP32 memory."
  );
}



// GPS POWER ON


void gpsPowerOn()
{
  if (gpsPowered)
    return;

  Serial.println("GPS POWER ON");

  // MOSFET ON
  digitalWrite(
    GPS_POWER_PIN,
    HIGH
  );

  delay(200);

  // Restart UART
  gpsSerial.begin(
    9600,
    SERIAL_8N1,
    GPS_RX_PIN,
    GPS_TX_PIN
  );

  // Remove anything currently in UART buffer
  while (gpsSerial.available())
  {
    gpsSerial.read();
  }

  gpsPowered = true;
}



// GPS POWER OFF


void gpsPowerOff()
{
  if (!gpsPowered)
    return;

  Serial.println("GPS POWER OFF");

  // Stop UART first
  gpsSerial.end();

  // Do not drive the GPS signal pins while GPS is OFF
  pinMode(
    GPS_RX_PIN,
    INPUT
  );

  pinMode(
    GPS_TX_PIN,
    INPUT
  );

  // MOSFET OFF
  digitalWrite(
    GPS_POWER_PIN,
    LOW
  );

  gpsPowered = false;
}



// OLED POWER ON


void oledPowerOn()
{
  if (oledPowered)
    return;

  Serial.println("OLED POWER ON");

  // MOSFET ON
  digitalWrite(
    OLED_POWER_PIN,
    HIGH
  );

  oledPowered = true;

  delay(50);

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  oledAvailable =
    display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDRESS
    );

  if (!oledAvailable)
  {
    Serial.println(
      "WARNING: OLED not detected."
    );

    return;
  }

  display.clearDisplay();

  refreshOLED();
}



// OLED POWER OFF


void oledPowerOff()
{
  if (!oledPowered)
    return;

  Serial.println("OLED POWER OFF");

  if (oledAvailable)
  {
    display.clearDisplay();
    display.display();
  }

  // Stop I2C first
  Wire.end();

  // Prevent back-power through SDA/SCL
  pinMode(
    OLED_SDA,
    INPUT
  );

  pinMode(
    OLED_SCL,
    INPUT
  );

  // MOSFET OFF
  digitalWrite(
    OLED_POWER_PIN,
    LOW
  );

  oledPowered = false;
  oledAvailable = false;
}



// UPDATE OLED

void refreshOLED()
{
  if (!oledPowered ||
      !oledAvailable)
  {
    return;
  }

  lastOLEDRefresh = millis();

  display.clearDisplay();

  display.setTextColor(
    SSD1306_WHITE
  );

  
  // NODE
  

  display.setTextSize(1);

  display.setCursor(
    0,
    0
  );

  display.print("NODE: ");
  display.println(NODE_ID);


  
  // TEMPERATURE
  

  display.setCursor(
    0,
    14
  );

  display.println(
    "FUSED TEMPERATURE"
  );

  display.setTextSize(2);

  display.setCursor(
    0,
    25
  );

  if (currentFusionValid)
  {
    display.print(
      currentFinalTemperature,
      1
    );

    display.print(" C");
  }
  else
  {
    display.print("--.- C");
  }


  
  // GPS STATUS
  

  display.setTextSize(1);

  display.setCursor(
    0,
    48
  );

  if (oledMode == OLED_GPS_UPDATING)
  {
    display.print("GPS: UPDATING");
  }

  else if (oledMode == OLED_GPS_UPDATED)
  {
    display.print("GPS: UPDATED");
  }

  else if (oledMode == OLED_GPS_TIMEOUT)
  {
    display.print("GPS: TIMEOUT");
  }

  else
  {
    if (gpsLocationValid)
    {
      display.print("GPS: SAVED");
    }
    else
    {
      display.print("GPS: NONE");
    }
  }


  
  // SENSOR STATUS
  

  display.setCursor(
    0,
    57
  );

  display.print("DS:");

  if (currentDSValid)
    display.print("OK");
  else
    display.print("ERR");


  display.print(" NTC:");

  if (currentNTCValid)
    display.print("OK");
  else
    display.print("ERR");


  display.display();
}



// START GPS LOCATION UPDATE


void startGPSUpdate()
{
  if (gpsUpdating)
    return;

  Serial.println();
  Serial.println(
    "LONG PRESS -> GPS UPDATE STARTED"
  );

  oledPowerOn();

  oledMode =
    OLED_GPS_UPDATING;

  refreshOLED();

  gpsPowerOn();

  gpsUpdating = true;

  gpsUpdateStart =
    millis();

  // Used so an OLD GPS fix is not accepted
  gpsFixCountAtStart =
    gps.sentencesWithFix();
}



// SERVICE GPS


void serviceGPS()
{
  if (!gpsPowered)
    return;

  while (gpsSerial.available() > 0)
  {
    gps.encode(
      gpsSerial.read()
    );
  }


  if (gps.satellites.isValid())
  {
    satellites =
      gps.satellites.value();
  }


  if (!gpsUpdating)
    return;


  
  // NEW VALID GPS FIX

  if (
    gps.sentencesWithFix()
      >
    gpsFixCountAtStart
    &&
    gps.location.isValid()
  )
  {
    latitude =
      gps.location.lat();

    longitude =
      gps.location.lng();

    gpsLocationValid = true;


    Serial.println();

    Serial.println(
      "NEW GPS FIX RECEIVED"
    );

    Serial.print(
      "Latitude : "
    );

    Serial.println(
      latitude,
      6
    );

    Serial.print(
      "Longitude: "
    );

    Serial.println(
      longitude,
      6
    );


    // Save location
    saveGPS();


    gpsUpdating = false;

    // GPS no longer required
    gpsPowerOff();


    oledMode =
      OLED_GPS_UPDATED;

    oledOffTime =
      millis()
      +
      OLED_AFTER_GPS_TIME;

    refreshOLED();

    return;
  }


  
  // GPS TIMEOUT
  

  if (
    millis() -
    gpsUpdateStart
    >=
    GPS_TIMEOUT
  )
  {
    Serial.println(
      "GPS UPDATE TIMEOUT"
    );

    gpsUpdating = false;

    gpsPowerOff();

    oledMode =
      OLED_GPS_TIMEOUT;

    oledOffTime =
      millis()
      +
      OLED_AFTER_GPS_TIME;

    refreshOLED();
  }
}



// BUTTON SERVICE


void serviceButton()
{
  bool reading =
    digitalRead(
      BUTTON_PIN
    );


  
  // DEBOUNCE
  

  if (reading != lastButtonReading)
  {
    lastButtonChange =
      millis();

    lastButtonReading =
      reading;
  }


  if (
    millis() -
    lastButtonChange
    >=
    BUTTON_DEBOUNCE
  )
  {
    if (
      reading
      !=
      stableButtonState
    )
    {
      stableButtonState =
        reading;


      
      // BUTTON PRESSED
      

      if (
        stableButtonState
        ==
        LOW
      )
      {
        buttonPressActive = true;

        longPressTriggered = false;

        buttonPressStart =
          millis();


        // OLED turns ON immediately
        oledPowerOn();


        if (!gpsUpdating)
        {
          oledMode =
            OLED_NORMAL;
        }


        oledOffTime =
          millis()
          +
          OLED_SHORT_TIME;


        refreshOLED();
      }


      
      // BUTTON RELEASED
      

      else
      {
        if (
          buttonPressActive
          &&
          !longPressTriggered
          &&
          !gpsUpdating
        )
        {
          // SHORT PRESS
          Serial.println(
            "SHORT PRESS"
          );

          oledMode =
            OLED_NORMAL;

          oledOffTime =
            millis()
            +
            OLED_SHORT_TIME;

          refreshOLED();
        }

        buttonPressActive =
          false;
      }
    }
  }


  
  // LONG PRESS DETECTION
  

  if (
    buttonPressActive
    &&
    stableButtonState == LOW
    &&
    !longPressTriggered
    &&
    !gpsUpdating
    &&
    millis() -
    buttonPressStart
    >=
    LONG_PRESS_TIME
  )
  {
    longPressTriggered =
      true;

    startGPSUpdate();
  }
}



// OLED SERVICE


void serviceOLED()
{
  if (!oledPowered)
    return;


  // Periodically refresh display
  if (
    oledAvailable
    &&
    millis() -
    lastOLEDRefresh
    >=
    OLED_REFRESH_INTERVAL
  )
  {
    refreshOLED();
  }


  // Do not turn OLED off while:
  // - button held
  // - GPS update running

  if (
    buttonPressActive
    ||
    gpsUpdating
  )
  {
    return;
  }


  if (
    oledOffTime != 0
    &&
    (int32_t)(
      millis() -
      oledOffTime
    )
    >= 0
  )
  {
    oledOffTime = 0;

    oledPowerOff();
  }
}


// ============================================================
// BACKGROUND TASKS
// ============================================================

void serviceBackground()
{
  serviceButton();
  serviceGPS();
  serviceOLED();
  connectWiFiAndMQTT(); // ADDED FOR DASHBOARD CONNECTIVITY
}



// READ DS18B20


bool readDS18B20(
  float &temperature
)
{
  ds18b20.requestTemperatures();

  unsigned long conversionStart =
    millis();


  // DS18B20 12-bit conversion ~750 ms
  while (
    millis() -
    conversionStart
    <
    760
  )
  {
    // Keep button/OLED/GPS responsive
    serviceBackground();

    delay(2);
  }


  temperature =
    ds18b20.getTempCByIndex(0);


  if (
    temperature
    ==
    DEVICE_DISCONNECTED_C
  )
  {
    return false;
  }


  if (
    temperature < -55.0
    ||
    temperature > 125.0
  )
  {
    return false;
  }


  return true;
}



// READ NTC


bool readNTC(
  float &temperature,
  float &resistance,
  float &adcAverage
)
{
  long adcTotal = 0;


  for (
    int i = 0;
    i < ADC_SAMPLES;
    i++
  )
  {
    adcTotal +=
      analogRead(
        NTC_PIN
      );

    serviceBackground();

    delay(5);
  }


  adcAverage =
    (float)adcTotal
    /
    ADC_SAMPLES;


  if (
    adcAverage <= 0
    ||
    adcAverage >= ADC_MAX
  )
  {
    return false;
  }




  resistance =
    R_FIXED
    *
    adcAverage
    /
    (
      ADC_MAX -
      adcAverage
    );


  float inverseTemperature =
    (1.0 / T_REF_K)
    +
    (1.0 / BETA)
    *
    log(
      resistance /
      R_REF
    );


  float tempKelvin =
    1.0 /
    inverseTemperature;


  temperature =
    tempKelvin -
    273.15;


  if (isnan(temperature))
    return false;


  if (
    temperature < -55.0
    ||
    temperature > 150.0
  )
  {
    return false;
  }


  return true;
}



// TEMPERATURE RANGE


bool temperatureInRange(
  float temperature
)
{
  return
    temperature >= MIN_VALID_TEMP
    &&
    temperature <= MAX_VALID_TEMP;
}



// DS OUTLIER


bool checkDSOutlier(
  float temperature
)
{
  if (!previousDSAvailable)
  {
    previousDS =
      temperature;

    previousDSAvailable =
      true;

    return false;
  }


  float difference =
    fabs(
      temperature -
      previousDS
    );


  if (
    difference >
    MAX_STEP_CHANGE
  )
  {
    return true;
  }


  previousDS =
    temperature;

  return false;
}



// NTC OUTLIER

bool checkNTCOutlier(
  float temperature
)
{
  if (!previousNTCAvailable)
  {
    previousNTC =
      temperature;

    previousNTCAvailable =
      true;

    return false;
  }


  float difference =
    fabs(
      temperature -
      previousNTC
    );


  if (
    difference >
    MAX_STEP_CHANGE
  )
  {
    return true;
  }


  previousNTC =
    temperature;

  return false;
}


// WLS SENSOR FUSION

bool calculateWLS(
  bool dsValid,
  float dsTemperature,

  bool ntcValid,
  float ntcTemperature,

  float &fusedTemperature,
  float &measurementVariance,

  float &weightDS,
  float &weightNTC
)
{

  // ==========================================================
  // BOTH AVAILABLE
  // ==========================================================

  if (
    dsValid
    &&
    ntcValid
  )
  {
    float informationDS =
      1.0 /
      DS_VARIANCE;


    float informationNTC =
      1.0 /
      NTC_VARIANCE;


    float totalInformation =
      informationDS
      +
      informationNTC;


    weightDS =
      informationDS
      /
      totalInformation;


    weightNTC =
      informationNTC
      /
      totalInformation;


    fusedTemperature =
      weightDS *
      dsTemperature
      +
      weightNTC *
      ntcTemperature;


    measurementVariance =
      1.0 /
      totalInformation;


    return true;
  }



  // DS ONLY


  if (dsValid)
  {
    fusedTemperature =
      dsTemperature;

    measurementVariance =
      DS_VARIANCE;

    weightDS = 1.0;
    weightNTC = 0.0;

    return true;
  }


  // NTC ONLY


  if (ntcValid)
  {
    fusedTemperature =
      ntcTemperature;

    measurementVariance =
      NTC_VARIANCE;

    weightDS = 0.0;
    weightNTC = 1.0;

    return true;
  }


  return false;
}



// KALMAN FILTER


float updateKalman(
  float measurement,
  float measurementVariance
)
{
  if (!kalmanInitialized)
  {
    kalmanTemperature =
      measurement;

    kalmanP =
      measurementVariance;

    kalmanInitialized =
      true;

    return kalmanTemperature;
  }


  // Prediction
  float predictedTemperature =
    kalmanTemperature;


  float predictedP =
    kalmanP
    +
    PROCESS_VARIANCE;


  // Kalman gain
  float kalmanGain =
    predictedP
    /
    (
      predictedP
      +
      measurementVariance
    );


  // Update
  kalmanTemperature =
    predictedTemperature
    +
    kalmanGain
    *
    (
      measurement -
      predictedTemperature
    );


  kalmanP =
    (1.0 - kalmanGain)
    *
    predictedP;


  return kalmanTemperature;
}



// PRINT GPS


void printGPS()
{
  Serial.println("GPS");


  if (gpsLocationValid)
  {
    Serial.print(
      "Latitude   : "
    );

    Serial.println(
      latitude,
      6
    );


    Serial.print(
      "Longitude  : "
    );

    Serial.println(
      longitude,
      6
    );


    if (gpsPowered)
    {
      Serial.println(
        "Source     : LIVE GPS"
      );
    }
    else
    {
      Serial.println(
        "Source     : SAVED LOCATION"
      );
    }
  }
  else
  {
    Serial.println(
      "Location   : NO SAVED GPS FIX"
    );
  }


  Serial.print(
    "GPS Power  : "
  );

  Serial.println(
    gpsPowered ?
    "ON" :
    "OFF"
  );


  if (gpsPowered)
  {
    Serial.print(
      "Satellites : "
    );

    Serial.println(
      satellites
    );
  }
}



// PROCESS TEMPERATURE


void processTemperature()
{

  // 1. DS18B20


  float dsRaw = 0.0;

  bool dsValid =
    readDS18B20(
      dsRaw
    );



  // 2. NTC


  float ntcRaw = 0.0;
  float ntcResistance = 0.0;
  float ntcADC = 0.0;


  bool ntcValid =
    readNTC(
      ntcRaw,
      ntcResistance,
      ntcADC
    );



  // 3. CALIBRATION


  float dsCalibrated =
    dsRaw;

  float ntcCalibrated =
    ntcRaw;


  if (dsValid)
  {
    dsCalibrated =
      calibrateDS18B20(
        dsRaw
      );
  }


  if (ntcValid)
  {
    ntcCalibrated =
      calibrateNTC(
        ntcRaw
      );
  }



  // 4. RANGE CHECK

  if (
    dsValid
    &&
    !temperatureInRange(
      dsCalibrated
    )
  )
  {
    dsValid = false;
  }


  if (
    ntcValid
    &&
    !temperatureInRange(
      ntcCalibrated
    )
  )
  {
    ntcValid = false;
  }



  // 5. OUTLIER CHECK


  bool dsOutlier = false;
  bool ntcOutlier = false;


  if (dsValid)
  {
    dsOutlier =
      checkDSOutlier(
        dsCalibrated
      );

    if (dsOutlier)
      dsValid = false;
  }


  if (ntcValid)
  {
    ntcOutlier =
      checkNTCOutlier(
        ntcCalibrated
      );

    if (ntcOutlier)
      ntcValid = false;
  }



  // 6. WLS


  float wlsTemperature = 0.0;
  float wlsVariance = 0.0;

  float weightDS = 0.0;
  float weightNTC = 0.0;


  bool fusionValid =
    calculateWLS(
      dsValid,
      dsCalibrated,

      ntcValid,
      ntcCalibrated,

      wlsTemperature,
      wlsVariance,

      weightDS,
      weightNTC
    );



  // 7. KALMAN


  float finalTemperature = 0.0;


  if (fusionValid)
  {
    finalTemperature =
      updateKalman(
        wlsTemperature,
        wlsVariance
      );
  }


  // Save latest values for OLED
  currentFusionValid =
    fusionValid;

  currentDSValid =
    dsValid;

  currentNTCValid =
    ntcValid;

  if (fusionValid)
  {
    currentFinalTemperature =
      finalTemperature;
  }


  if (oledPowered)
  {
    refreshOLED();
  }


  // PUBLISH DASHBOARD MQTT JSON TELEMETRY

  if (mqttClient.connected())
  {
    StaticJsonDocument<512> doc;

    doc["node"] = NODE_ID;

    if (dsValid)
      doc["sensor1"] = dsCalibrated;
    else
      doc["sensor1"] = nullptr;

    if (ntcValid)
      doc["sensor2"] = ntcCalibrated;
    else
      doc["sensor2"] = nullptr;

    if (fusionValid)
    {
      doc["wls"] = wlsTemperature;
      doc["fused"] = finalTemperature;
    }
    else
    {
      doc["wls"] = nullptr;
      doc["fused"] = nullptr;
    }

    if (gpsLocationValid)
    {
      doc["lat"] = latitude;
      doc["lon"] = longitude;
    }
    else
    {
      doc["lat"] = 0.0;
      doc["lon"] = 0.0;
    }

    doc["satellites"] = satellites;

    doc["dsValid"] = dsValid;
    doc["ntcValid"] = ntcValid;
    doc["gpsValid"] = gpsLocationValid;

    doc["wifi"] = (WiFi.status() == WL_CONNECTED);
    doc["mqtt"] = true;

    char jsonBuffer[512];

    serializeJson(
      doc,
      jsonBuffer,
      sizeof(jsonBuffer)
    );

    mqttClient.publish(
      MQTT_TOPIC,
      jsonBuffer
    );

    Serial.print("MQTT JSON: ");
    Serial.println(jsonBuffer);
  }



  // SERIAL DISPLAY


  Serial.println();
  Serial.println(
    "-------------------------------------------------"
  );

  Serial.print("Node: ");
  Serial.println(NODE_ID);



  // DS18B20


  Serial.println();
  Serial.println("DS18B20");


  if (dsValid)
  {
    Serial.print("Raw        : ");
    Serial.print(dsRaw, 2);
    Serial.println(" C");

    Serial.print("Calibrated : ");
    Serial.print(dsCalibrated, 2);
    Serial.println(" C");

    Serial.println("Status     : OK");
  }
  else
  {
    if (dsOutlier)
      Serial.println(
        "Status     : OUTLIER REJECTED"
      );
    else
      Serial.println(
        "Status     : SENSOR ERROR"
      );
  }



  // NTC


  Serial.println();
  Serial.println("NTC");


  if (ntcValid)
  {
    Serial.print("ADC        : ");
    Serial.println(ntcADC, 0);

    Serial.print("Resistance : ");
    Serial.print(
      ntcResistance / 1000.0,
      2
    );
    Serial.println(" kOhm");

    Serial.print("Raw        : ");
    Serial.print(ntcRaw, 2);
    Serial.println(" C");

    Serial.print("Calibrated : ");
    Serial.print(ntcCalibrated, 2);
    Serial.println(" C");

    Serial.println("Status     : OK");
  }
  else
  {
    if (ntcOutlier)
      Serial.println(
        "Status     : OUTLIER REJECTED"
      );
    else
      Serial.println(
        "Status     : SENSOR ERROR"
      );
  }



  // FUSION
 

  Serial.println();


  if (fusionValid)
  {
    Serial.println(
      "SENSOR FUSION"
    );

    Serial.print(
      "DS Weight       : "
    );
    Serial.println(
      weightDS,
      4
    );

    Serial.print(
      "NTC Weight      : "
    );
    Serial.println(
      weightNTC,
      4
    );

    Serial.print(
      "WLS Temperature : "
    );
    Serial.print(
      wlsTemperature,
      2
    );
    Serial.println(" C");

    Serial.print(
      "Kalman Final    : "
    );
    Serial.print(
      finalTemperature,
      2
    );
    Serial.println(" C");
  }
  else
  {
    Serial.println(
      "SENSOR FUSION FAILED"
    );
  }


  Serial.println();

  printGPS();




  if (
    dsRaw != DEVICE_DISCONNECTED_C
    &&
    ntcRaw > -55.0
    &&
    ntcRaw < 150.0
  )
  {
    Serial.print("DATA,");
    Serial.print(dsRaw, 2);
    Serial.print(",");
    Serial.println(ntcRaw, 2);
  }


  // TELEMETRY


  if (fusionValid)
  {
    Serial.print("TELEMETRY,");

    Serial.print(NODE_ID);
    Serial.print(",");


    if (dsValid)
      Serial.print(
        dsCalibrated,
        3
      );
    else
      Serial.print("nan");


    Serial.print(",");


    if (ntcValid)
      Serial.print(
        ntcCalibrated,
        3
      );
    else
      Serial.print("nan");


    Serial.print(",");

    Serial.print(
      wlsTemperature,
      3
    );

    Serial.print(",");

    Serial.print(
      finalTemperature,
      3
    );

    Serial.print(",");


    // GPS saved coordinates
    if (gpsLocationValid)
    {
      Serial.print(
        latitude,
        6
      );

      Serial.print(",");

      Serial.print(
        longitude,
        6
      );
    }
    else
    {
      Serial.print(
        "0.000000"
      );

      Serial.print(",");

      Serial.print(
        "0.000000"
      );
    }


    Serial.print(",");

    Serial.print(
      satellites
    );


    Serial.print(",");

    Serial.print(
      gpsLocationValid ?
      1 :
      0
    );


    Serial.print(",");

    Serial.print(
      dsValid ?
      1 :
      0
    );


    Serial.print(",");

    Serial.println(
      ntcValid ?
      1 :
      0
    );
  }
}


// SETUP


void setup()
{
  Serial.begin(115200);


  // MOSFETS


  pinMode(
    GPS_POWER_PIN,
    OUTPUT
  );

  pinMode(
    OLED_POWER_PIN,
    OUTPUT
  );


  // Both OFF initially
  digitalWrite(
    GPS_POWER_PIN,
    LOW
  );

  digitalWrite(
    OLED_POWER_PIN,
    LOW
  );



  // BUTTON


  pinMode(
    BUTTON_PIN,
    INPUT_PULLUP
  );

  lastButtonReading =
    digitalRead(
      BUTTON_PIN
    );

  stableButtonState =
    lastButtonReading;



  // SIGNAL PINS HIGH IMPEDANCE WHILE MODULES OFF


  pinMode(
    GPS_RX_PIN,
    INPUT
  );

  pinMode(
    GPS_TX_PIN,
    INPUT
  );

  pinMode(
    OLED_SDA,
    INPUT
  );

  pinMode(
    OLED_SCL,
    INPUT
  );


  // DS18B20


  ds18b20.begin();

  ds18b20.setResolution(12);

  ds18b20.setWaitForConversion(
    false
  );



  // ADC


  analogReadResolution(12);



  // LOAD PREVIOUS GPS POSITION


  loadSavedGPS();


  // DASHBOARD WI-FI + MQTT SETUP


  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  mqttClient.setServer(
    MQTT_SERVER,
    MQTT_PORT
  );

  // Start Wi-Fi connection ONCE.
  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  lastWiFiRetry = millis();
  lastMQTTRetry = millis();

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);



  // SERIAL INFORMATION


  Serial.println();

  Serial.println(
    "================================================="
  );

  Serial.println(
    "EE2120 SMART DISTRIBUTED TEMPERATURE NODE"
  );

  Serial.println(
    "DS18B20 + NTC + NEO-6M + OLED"
  );

  Serial.println(
    "WLS + KALMAN FILTER"
  );

  Serial.println(
    "MOSFET POWER CONTROL + PUSH BUTTON"
  );

  Serial.println(
    "================================================="
  );


  Serial.print(
    "Node ID          : "
  );

  Serial.println(
    NODE_ID
  );


  Serial.print(
    "DS18B20 detected : "
  );

  Serial.println(
    ds18b20.getDeviceCount()
  );


  Serial.println(
    "GPS power        : OFF"
  );

  Serial.println(
    "OLED power       : OFF"
  );


  if (gpsLocationValid)
  {
    Serial.println(
      "Saved GPS        : AVAILABLE"
    );

    Serial.print(
      "Latitude         : "
    );

    Serial.println(
      latitude,
      6
    );

    Serial.print(
      "Longitude        : "
    );

    Serial.println(
      longitude,
      6
    );
  }
  else
  {
    Serial.println(
      "Saved GPS        : NONE"
    );
  }


  Serial.println();

  Serial.println(
    "SHORT PRESS:"
  );

  Serial.println(
    "OLED ON for 10 seconds"
  );

  Serial.println();

  Serial.println(
    "LONG PRESS (2 sec):"
  );

  Serial.println(
    "OLED ON + GPS location update"
  );

  Serial.println();


  // Start first temperature reading quickly
  lastMeasurementTime =
    millis() -
    MEASUREMENT_INTERVAL;
}



// MAIN LOOP


void loop()
{
  // Button / OLED / GPS always serviced
  serviceBackground();


  // Temperature measurement
  if (
    millis() -
    lastMeasurementTime
    >=
    MEASUREMENT_INTERVAL
  )
  {
    lastMeasurementTime =
      millis();

    processTemperature();
  }
}