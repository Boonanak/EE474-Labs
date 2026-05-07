#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Generate random Service and Characteristic UUIDs: https://www.uuidgenerator.net/

#define SERVICE_UUID        "bfcad23b-c6d5-4dc4-bf77-bb07f3f76e14"
#define CHARACTERISTIC_UUID "624dde7b-a518-45c1-bb2f-ba69ccf1e9ff"

// Pins
#define BUTTON_PIN 1 //make sure to input pullup

volatile bool BLE_tripped = false;
volatile bool but_pressed = false;

class MyCallbacks: public BLECharacteristicCallbacks {
   void onWrite(BLECharacteristic *pCharacteristic) {
     // =========> TODO: This callback function will be invoked when signal is
     // 		     received over BLE. Implement the necessary functionality that
     //		     will trigger the message to the LCD.
    BLE_tripped = true;    
   }
};


// ==============> TODO: Write your timer ISR here.


// ==============> TODO: Create an ISR function to handle button press here.


void setup() {
 BLEDevice::init("MyESP32");
 BLEServer *pServer = BLEDevice::createServer();
 BLEService *pService = pServer->createService(SERVICE_UUID);
 BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                        CHARACTERISTIC_UUID,
                                        BLECharacteristic::PROPERTY_READ |
                                        BLECharacteristic::PROPERTY_WRITE
                                      );


 pCharacteristic->setCallbacks(new MyCallbacks());
 pService->start();
 BLEAdvertising *pAdvertising = pServer->getAdvertising();
 pAdvertising->start();


   // =========> TODO: Initialize LCD display
   //
   // =========> TODO: create a timer, attach an interrupt, set an alarm which will
   //                  update the counter every second.
   //
    // ========> TODO: Set button pin as input and attach an interrupt
}


void loop() {
 // =========> TODO: Print out an incrementing counter to the LCD.
 //                  If a signal has been received over BLE, print out “New
 //                  Message!” on the LCD.
 //                  If the button has been pressed, print out "Button Pressed"
 //                  on the LCD.

  writeRow(0, counter);

  if (but_pressed) {
    but_pressed = false;
    writeRow(1, "Button Pressed");
    delay(2000);
  }

  if (BLE_tripped) {
    BLE_tripped = false;
    writeRow(1, "New Message!");
    delay(2000);
  }
}
