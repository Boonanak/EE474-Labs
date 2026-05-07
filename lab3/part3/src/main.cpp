#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>


//===============> TODO:
// Generate random Service and Characteristic UUIDs: https://www.uuidgenerator.net/


#define SERVICE_UUID        ""
#define CHARACTERISTIC_UUID ""

// Pins
#define BUTTON_PIN 1 //make sure to input pullup


class MyCallbacks: public BLECharacteristicCallbacks {
   void onWrite(BLECharacteristic *pCharacteristic) {
     // =========> TODO: This callback function will be invoked when signal is
     // 		     received over BLE. Implement the necessary functionality that
     //		     will trigger the message to the LCD.
   }
};


// ==============> TODO: Write your timer ISR here.


// ==============> TODO: Create an ISR function to handle button press here.

// put function declarations here:
int myFunction(int, int);

void setup() {
  // put your setup code here, to run once:
  int result = myFunction(2, 3);
}

void loop() {
  // put your main code here, to run repeatedly:
}

// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}