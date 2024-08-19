#include <Arduino.h>

/*
 * This program is located at /Volumes/SamsungSSD500GB/Dropbox/reserve/DevDoc/Arduino/ESP8266_InfluxDB/BME280_to_InfluxDB/BME280_to_InfluxDB.ino
  Rui Santos
  Complete project details at our blog: https://RandomNerdTutorials.com/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
// Based on this library example: https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino/blob/master/examples/SecureBatchWrite/SecureBatchWrite.ino

#include <Arduino.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
WiFiManager wm;
#include <Wire.h>
// #include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#if defined(ESP32)
  #include <WiFiMulti.h>
  WiFiMulti wifiMulti;
  #define DEVICE "ESP32"
#elif defined(ESP8266)
  #include <ESP8266WiFiMulti.h>
  ESP8266WiFiMulti wifiMulti;
  #define DEVICE "ESP8266"
  #define WIFI_AUTH_OPEN ENC_TYPE_NONE
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include "params.h"

// Set timezone string according to https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
// Examples:
//  Pacific Time:   "PST8PDT"
//  Eastern:        "EST5EDT"
//  Japanesse:      "JST-9"
//  Central Europe: "CET-1CEST,M3.5.0,M10.5.0/3"
#define TZ_INFO "JST-9"

// InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
// InfluxDB client instance without preconfigured InfluxCloud certificate for insecure connection 
//InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

// Data point
Point sensorReadings("measurements");

//BME280
#define SDA D6
#define SCL D7

Adafruit_BME280 bme; // I2C
ADC_MODE(ADC_VCC); //to watch 3.3V pin voltage

float temperature;
float humidity;
float pressure;
float vcc_3v3;

void monitorAlive(){  //just for monitoring live
  digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED on (Note that LOW is the voltage level
                                    // but actually the LED is on; this is because 
                                    // it is acive low on the ESP-01)
  delay(200);                      // Wait for a 1 mili second
  digitalWrite(LED_BUILTIN, LOW);  // Turn the LED off by making the voltage HIGH
}

void goodNightLED(){  //system will go to deepsleep
      digitalWrite(LED_BUILTIN, HIGH);   // Turn the LED on (Note that LOW is the voltage level
                                    // but actually the LED is on; this is because 
                                    // it is acive low on the ESP-01)
      delay(200);                      // Wait for a 1 mili second
      digitalWrite(LED_BUILTIN, LOW);  // Turn the LED off by making the voltage HIGH
      delay(200);                      // Wait for a 1 mili second

  int i = 0;
  while (i<3) {
      digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level
                                    // but actually the LED is on; this is because 
                                    // it is acive low on the ESP-01)
      delay(100);                      // Wait for a 1 mili second
      digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
      ++i;
      delay(100);                      // Wait for a 1 mili second
  }
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);  // initialize onboard LED as output  
  pinMode(D0, WAKEUP_PULLUP); // Connect D0 to RST to wake up, D0 is GPIO16
  monitorAlive();
  Serial.begin(115200);
  Serial.println("This program is located at /Users/haya/Documents/PlatformIO/Projects/BME280_to_InfluxDB_DeepSleep/src"); 

  // Setup wifi
  WiFi.mode(WIFI_STA);
    wm.setConfigPortalTimeout(120);
    //automatically connect using saved credentials if they exist
    //If connection fails it starts an access point with the specified name
    wm.setConfigPortalBlocking(false); //If this is set to true, 
    //the config portal will block until the user exits the portal
    if(wm.autoConnect("AutoConnectAP")){
        Serial.println("connected...yeey :)");
    }
    else {
        Serial.println("Configportal running");
    }
  
  //Init BME280 sensor
  pinMode(D8, OUTPUT);     // Initialize D8 pin as an output。I2CのGNDにするため
  digitalWrite(D8, LOW);    // turn the D8 pin voltage LOW。GNDレベルに落とす

  Serial.println(F("BME280 test"));
  Wire.begin(SDA, SCL);

  // default settings
  // (you can also pass in a Wire library object like &Wire2)
  if (!bme.begin(0x76)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    delay(1000); // Add a delay to prevent watchdog reset
    ESP.restart(); // Optionally restart the ESP instead of hanging
  }
  
  // Add tags
  sensorReadings.addTag("device", DEVICE);
  sensorReadings.addTag("location", "office");
  sensorReadings.addTag("sensor", "bme280");

  // Accurate time is necessary for certificate validation and writing in batches
  // For the fastest time sync find NTP servers in your area: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }
}

void loop() {
  wm.process();

  // Get latest sensor readings
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();
  pressure = bme.readPressure()/100.0F;

  Serial.println();
  //**** read 3.3v pin ****
  Serial.println("Get ADC voltage:");
  vcc_3v3 = float(ESP.getVcc()) / 1000;
  Serial.println( "3.3V:>>" + String(vcc_3v3));


  // Add readings as fields to point
  sensorReadings.addField("temperature", temperature);
  sensorReadings.addField("humidity", humidity);
  sensorReadings.addField("pressure", pressure);
  sensorReadings.addField("battery", vcc_3v3);

  // Print what are we exactly writing
  Serial.print("Writing: ");
  Serial.println(client.pointToLineProtocol(sensorReadings));
  
  // Write point into buffer
  client.writePoint(sensorReadings);

  // Clear fields for next usage. Tags remain the same.
  sensorReadings.clearFields();

  goodNightLED();
  Serial.println("Going to deep Sleep in 60 seconds. Good night!");
  WiFi.disconnect(true); //disconnect and remove wifi config. 
  //This is to prevent the ESP8266 from connecting to the wifi after deep sleep.
  //Sleep 1 minute. ESP.deepSleep(1* 60 * 1000 * 1000, WAKE_RF_DEFAULT);
  ESP.deepSleep(1* 60 * 1000 * 1000, WAKE_RF_DEFAULT); 
  
  delay(30000);
}
