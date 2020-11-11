/*
Classify Gestures
*/

#include <ArduinoBLE.h>
#include <Arduino_LSM9DS1.h>
#include <Arduino_APDS9960.h>
#include <string>
#include <stdlib.h>
#include <TensorFlowLite.h>
#include <tensorflow/lite/micro/all_ops_resolver.h>
#include <tensorflow/lite/micro/micro_error_reporter.h>
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include <tensorflow/lite/version.h>
#include "pruned_quantized_model.h"

#define redPin 22
#define greenPin 23
#define bluePin 24

const float accelerationThreshold = 1.8; // threshold of significant in G's
const int numSamples = 119;
int samplesRead = numSamples;
// global variables used for TensorFlow Lite (Micro)
tflite::MicroErrorReporter tflErrorReporter;

tflite::AllOpsResolver tflOpsResolver;

const tflite::Model* tflModel = nullptr;
tflite::MicroInterpreter* tflInterpreter = nullptr;
TfLiteTensor* tflInputTensor = nullptr;
TfLiteTensor* tflOutputTensor = nullptr;

// static memory buffer for TFLM
constexpr int tensorArenaSize = 40 * 1024;
alignas(16) uint8_t tensorArena[tensorArenaSize];
  
// array to map gesture index to a name
const char* GESTURES[] = {
    "squat",
    "jump",
    "walk",
    "run",
    "other"
};

#define NUM_GESTURES (sizeof(GESTURES) / sizeof(GESTURES[0]))

BLEService sendService("180F");  // BLE Service

BLEStringCharacteristic sendDataChar("2A17",  // standard 16-bit characteristic UUID
    BLERead | BLENotify, 1000); // remote clients will be able to get notifications if this characteristic changes    

void setup() {
  Serial.begin(9600);    // initialize serial communication

  pinMode(LED_BUILTIN, OUTPUT); // initialize the built-in LED pin to indicate when a central is connected
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  digitalWrite(redPin, HIGH); // red
  digitalWrite(greenPin, HIGH); // green
  digitalWrite(bluePin, HIGH); // blue

  // begin initialization
  if (!BLE.begin()) {
//    Serial.println("starting BLE failed!");
    while (1);
  }

  if (!IMU.begin()) {
//    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  // get the TFL representation of the model byte array
  tflModel = tflite::GetModel(model);
  if (tflModel->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while (1);
  }

  // Interpreter to run the model
  tflInterpreter = new tflite::MicroInterpreter(tflModel, tflOpsResolver, tensorArena, tensorArenaSize, &tflErrorReporter);

  // Allocate memory for the model's input and output tensors
  TfLiteStatus allocate_status = tflInterpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    Serial.println("AllocateTensors() failed");
    while(1);
  }

  // Get pointers for the model's input and output tensors
  tflInputTensor = tflInterpreter->input(0);
  tflOutputTensor = tflInterpreter->output(0);

  /* Set a local name for the BLE device
     This name will appear in advertising packets
     and can be used by remote devices to identify this BLE device
  */
  BLE.setLocalName("SendClassifiedData");
  BLE.setAdvertisedService(sendService); // add the service UUID
  sendService.addCharacteristic(sendDataChar); // add the battery level characteristic
  BLE.addService(sendService); // Add the battery service
  
  sendDataChar.writeValue(""); // set initial value for this characteristic

  // start advertising
  BLE.advertise();
  

  Serial.println("Bluetooth device active, waiting for connections...");
}

void loop() {
  // wait for a BLE central
  BLEDevice central = BLE.central();

  // if a central is connected to the peripheral:
  if (central) {
    BLE.poll();
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
    float aX, aY, aZ, gX, gY, gZ, max_num;
    String gesture, output;
    unsigned long StartTime, CurrentTime, ElapsedTime;
  // wait for significant motion
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
    // check if new acceleration AND gyroscope data is available
    if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
      // read the acceleration and gyroscope data
      IMU.readAcceleration(aX, aY, aZ);
      IMU.readGyroscope(gX, gY, gZ);
      // normalize the IMU data between 0 to 1 and store in the model's
      // input tensor
      tflInputTensor->data.f[samplesRead * 6 + 0] = (aX + 4.0) / 8.0;
      tflInputTensor->data.f[samplesRead * 6 + 1] = (aY + 4.0) / 8.0;
      tflInputTensor->data.f[samplesRead * 6 + 2] = (aZ + 4.0) / 8.0;
      tflInputTensor->data.f[samplesRead * 6 + 3] = (gX + 2000.0) / 4000.0;
      tflInputTensor->data.f[samplesRead * 6 + 4] = (gY + 2000.0) / 4000.0;
      tflInputTensor->data.f[samplesRead * 6 + 5] = (gZ + 2000.0) / 4000.0;

      samplesRead++;

      if (samplesRead == numSamples) {
        // Run inferencing
        StartTime = millis();
        TfLiteStatus invokeStatus = tflInterpreter->Invoke();
        if (invokeStatus != kTfLiteOk) {
          Serial.println("Invoke failed!");
          while (1);
          
          return;
        } else {
          CurrentTime = millis();
          ElapsedTime = CurrentTime - StartTime;
        }
        // Loop through the output tensor values from the model
        for (int i = 0; i < NUM_GESTURES; i++) {
          if(!max_num || max_num < tflOutputTensor->data.f[i]){
            max_num = tflOutputTensor->data.f[i];
            gesture = GESTURES[i];
          }
      }

      output = String("gesture") + ":" + String(gesture) + "," + String("probability") + ":" + String(max_num,3) + "," + String("time") + ":" + String(ElapsedTime);
      sendDataChar.setValue(output);
      
        if(gesture == "squat"){
            digitalWrite(redPin, LOW); // red
            delay(2000);
            digitalWrite(redPin, HIGH);
        } else if(gesture == "run") {
            digitalWrite(greenPin, LOW); // green
            delay(2000);
            digitalWrite(greenPin, HIGH);
        } else if(gesture == "walk") {
            digitalWrite(bluePin, LOW); // blue
            delay(2000);
            digitalWrite(bluePin, HIGH);
        } else if(gesture == "jump") {
           digitalWrite(redPin, LOW); //magenta
           digitalWrite(bluePin, LOW);
           delay(2000);
           digitalWrite(redPin, HIGH);
           digitalWrite(bluePin, HIGH);
        } else {
           digitalWrite(redPin, LOW); //white
           digitalWrite(greenPin, LOW); 
            digitalWrite(bluePin, LOW);
           delay(2000);
           digitalWrite(redPin, HIGH);
           digitalWrite(greenPin, HIGH);
           digitalWrite(bluePin, HIGH);
        }
    }
  }  
}
}
