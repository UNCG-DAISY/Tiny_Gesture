/*
Capture Data
*/

#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>
#include <string>
#include <iostream>
#include <stdlib.h>

 // BLE Service
BLEService captureDataService("180F");

BLEStringCharacteristic captureDataChar("2A18",  // standard 16-bit characteristic UUID
    BLERead | BLENotify, 512); // remote clients will be able to get notifications if this characteristic changes    

const float accelerationThreshold = 1.5; // threshold of significant in G's
const int numSamples = 119;
int samplesRead = numSamples;

void setup() {
  Serial.begin(9600);    // initialize serial communication

  pinMode(LED_BUILTIN, OUTPUT); // initialize the built-in LED pin to indicate when a central is connected

  // begin initialization
  if (!BLE.begin()) {
    Serial.println("starting BLE failed!");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  /* 
     This name will appear in advertising packets
     and can be used by remote devices to identify this BLE device
  */
  BLE.setLocalName("CaptureGestureData");
  BLE.setAdvertisedService(captureDataService); // add the service UUID
  captureDataService.addCharacteristic(captureDataChar); // add the characteristic
  BLE.addService(captureDataService); // Add the service
  captureDataChar.writeValue(""); // set initial value for this characteristic

  // start advertising
  BLE.advertise();

  Serial.println("Bluetooth device active, waiting for connections...");
}

void loop() {
  // wait for a BLE central
  BLEDevice central = BLE.central();

  // if a central is connected to the peripheral:
  if (central) {
    // turn on the LED to indicate the connection:
    digitalWrite(LED_BUILTIN, HIGH);

    // while the central is connected:
    while (central.connected()) {
      updateData();
    }
    // when the central disconnects, turn off the LED:
    digitalWrite(LED_BUILTIN, LOW);
  }
}

void updateData() {

  float aX, aY, aZ, gX, gY, gZ;
  String output;
  
//   wait for significant motion
  while (samplesRead == numSamples) {
    if (IMU.accelerationAvailable()) {
      // read the acceleration data
      IMU.readAcceleration(aX, aY, aZ);

      // sum up the absolutes
      float aSum = fabs(aX) + fabs(aY) + fabs(aZ);

      // check if it's above the threshold
      if (aSum >= accelerationThreshold) {
        // reset the sample read count
        samplesRead = 0;
        break;
      }
    }
  }

  // check if the all the required samples have been read since
  // the last time the significant motion was detected
  while (samplesRead < numSamples) {
    // check if both new acceleration and gyroscope data is
    // available
    if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
      // read the acceleration and gyroscope data
      IMU.readAcceleration(aX, aY, aZ);
      IMU.readGyroscope(gX, gY, gZ);

      samplesRead++;

      output = "\"aX\":" + String(aX,3) + ", \"aY\":" + String(aY,3) + ", \"aZ\":" + String(aZ,3) + ", \"gX\":" + String(gX,3) + ", \"gY\":" +String(gY,3) + ", \"gZ\":" + String(gZ,3);
      captureDataChar.writeValue(output);
      
      if (samplesRead == numSamples) {
        // add an empty line if it's the last sample
        captureDataChar.writeValue(" ");
      }
    }
  }
  
}
