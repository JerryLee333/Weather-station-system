//  Send sensor data from the onboard temperature, humudity, barometer sensors 
//  as a SIGFOX message, using the UnaBiz UnaShield V2S Arduino Shield.
//  The Arduino Uno onboard LED will flash every few seconds when the sketch is running properly.
//  The data is sent in the Sigfox Custom Payload Format, which will be decoded by the Sigfox cloud.

////////////////////////////////////////////////////////////
//  Begin Sensor Declaration
//  Don't use ports D0, D1: Reserved for viewing debug output through Arduino Serial Monitor
//  Don't use ports D4, D5: Reserved for serial comms with the SIGFOX module.

/***************************************************************************
  This is a library for the BME280 humidity, temperature & pressure sensor
Mike Nguyen (Vinova)  <mike@vinova.sg>Mike Nguyen (Vinova)  <mike@vinova.sg>
  Designed specifically to work with the Adafruit BME280 Breakout
  ----> http://www.adafruit.com/products/2650

  These sensors use I2C or SPI to communicate, 2 or 4 pins are required
  to interface.

  Adafruit invests time and resources providing this open source code,
  please support Adafruit andopen-source hardware by purchasing products
  from Adafruit!

  Written by Limor Fried & Kevin Townsend for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ***************************************************************************/
//created by IoT_Group_2

#include <Wire.h>
#include <SPI.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_BME280.h"
#include "Adafruit_AM2315.h"
#include "LowPower.h"
#include "SIGFOX.h"
#include "cactus_io_AM2315.h"

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
#define SEALEVELPRESSURE_HPA (1013.25)
#define SensorPin A1 
AM2315 am2315;
Adafruit_BME280 bme; // I2C

const int capteur_A = A2;
float sensorValue = 0; 
int val_analogique;
int val_analogique_1;
unsigned int sleepCounter;

//  End Sensor Declaration
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
//  Begin SIGFOX Module Declaration

#include "SIGFOX.h"

//  IMPORTANT: Check these settings with UnaBiz to use the SIGFOX library correctly.
static const String device = "417D73";        //  Set this to your device name if you're using SIGFOX Emulator.
static const bool useEmulator = false;  //  Set to true if using SIGFOX Emulator.
static const bool echo = true;          //  Set to true if the SIGFOX library should display the executed commands.
static const Country country = COUNTRY_SG;  //  Set this to your country to configure the SIGFOX transmission frequencies.
static UnaShieldV2S transceiver(country, useEmulator, device, echo);  //  Assumes you are using UnaBiz UnaShield V2S Dev Kit

//  End SIGFOX Module Declaration
////////////////////////////////////////////////////////////

void setup() {  //  Will be called only once.
  ////////////////////////////////////////////////////////////
  //  Begin General Setup

  //  Initialize console so we can see debug messages (9600 bits per second).
  Serial.begin(9600);  Serial.println(F("Running setup..."));

  pinMode(capteur_A, INPUT);
  pinMode(10, OUTPUT);
  pinMode(9, OUTPUT);

  pinMode(8, OUTPUT);
  pinMode(7, OUTPUT);

  //  End General Setup
  ////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////
  //  Begin Sensor Setup
    
  if (!bme.begin(0x76)) stop("Bosch BME280 sensor missing");  //  Will never return.

  //  End Sensor Setup
  ////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////
  //  Begin SIGFOX Module Setup

  //  Check whether the SIGFOX module is functioning.
  if (!transceiver.begin()) stop("Unable to init SIGFOX module, may be missing");  //  Will never return.

  //  End SIGFOX Module Setup
  ////////////////////////////////////////////////////////////
}

void loop() {  //  Will be called repeatedly.
  ////////////////////////////////////////////////////////////
  
  //// sleeping mode to safe the battery, still need to calculate the efficiency 
    for (sleepCounter = 1; sleepCounter > 0; sleepCounter--)
    {
      LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
      //LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, 
       //          SPI_OFF, USART0_OFF, TWI_OFF);
    }



  //  Begin Sensor Loop
  //  Read onborad the ambient temperature, humidity, air pressure.
    digitalWrite(10, HIGH); 
    digitalWrite(9, LOW);
    digitalWrite(8, HIGH); 
    digitalWrite(7, LOW);
    float filteredTemp = bme.readTemperature();
    float filteredHumidity = bme.readHumidity();
    float filteredPressure = bme.readPressure() / 100.0F;
    float filteredAltitude = bme.readAltitude(SEALEVELPRESSURE_HPA);

   /////temp+humi
    am2315.readSensor();
    
    ////soil_sensor
     sensorValue = sensorValue + analogRead(SensorPin); 
     delay(1); 
     sensorValue = sensorValue/10.0; 
     
    /////rain_sensor
     val_analogique=analogRead(capteur_A); 
     val_analogique_1 = -(val_analogique/10.0)+100; 
     delay(100); 
     
  //  Get temperature of the SIGFOX module.
  float moduleTemp; transceiver.getTemperature(moduleTemp);
  //  Optional: We may scale the values by 100 so we can have 2 decimal places of precision.
  //  For now we don't scale the values.
  unsigned int scaledonboardTemperature = filteredTemp * 1.0;
  unsigned int scaledonboardHumidity =filteredHumidity * 1.0;
  unsigned int scaledAltitude = filteredAltitude * 1.0;
  unsigned int scaledPressure = filteredPressure / 10.0;

  Serial.print("onboardTemp = ");  Serial.print(scaledonboardTemperature);          Serial.println(" degrees C");
  Serial.print("air_Temp = ");     Serial.print(am2315.getTemperature_C());                   Serial.println(" degrees C");
  Serial.print("air_Hum = ");      Serial.print(am2315.getHumidity());                      Serial.println("persendtage");
  Serial.print("air_pre = ");      Serial.print(scaledPressure);                  Serial.println(" ppm");
  Serial.print("soil_moi = ");     Serial.print(sensorValue);                     Serial.println(" persendtage");
  Serial.print("rain = "); Serial.print(val_analogique_1);                           Serial.println("(high means dry ,low means wet)");


  //  End Sensor Loop
  ////////////////////////////////////////////////////////////

  ////////////////////////////////////////////////////////////
  //  Begin SIGFOX Module Loop (Sanding the message to sigfox cloud)

  //  Send 1.onboard_temperature,2.air_temperature,3.air_humidity,4.soil_moisture,5.air_pressure, 6.empty
  static int counter = 0, successCount = 0, failCount = 0;  //  Count messages sent and failed.
  Serial.print(F("\nRunning loop #")); Serial.println(counter);

  //  Compose the message, total 4 bytes. (char) forces the value to be 8 bit.
  //  Note this doesn't use the Structured Message Format like in previous examples.
  //  It uses the Sigfox Custom Payload format: temp::int:8 humid::int:8 alt::int:8 modtemp::int:8
  //  without structure message we can send 6 kind of varible data maximum 

  String msg = transceiver.toHex((char) scaledonboardTemperature) //  int 1 byte: temperature * 1
    + transceiver.toHex((char) am2315.getTemperature_C())    //  int 1 byte: humidity * 1
    + transceiver.toHex((char) am2315.getHumidity())    //  int 1 byte: altitude * 1
    + transceiver.toHex((char) scaledPressure)//  int 1 byte: moisture * 1
    + transceiver.toHex((char) sensorValue)//  int 1 byte: onboardtemp * 1
    + transceiver.toHex((char) val_analogique_1); //  int 1 byte: module battery * 1 I left it incaase we can need to add some other sensor 


  //  Send the message to  
  if (transceiver.sendMessage(msg)) {
    successCount++;  //  If successful, count the message sent successfully.
  } else {
    failCount++;  //  If failed, count the message that could not be sent.
  }
  counter++;

  //  Flash the LED on and off at every iteration so we know the sketch is still running.
  if (counter % 2 == 0) {
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED on (HIGH is the voltage level).
  } else {
    digitalWrite(LED_BUILTIN, LOW);   // Turn the LED off (LOW is the voltage level).
  }
  
  //  Show updates every 10 messages.
  if (counter % 10 == 0) {
    Serial.print(F("Messages sent successfully: "));   Serial.print(successCount);
    Serial.print(F(", failed: "));  Serial.println(failCount);
  }
}
