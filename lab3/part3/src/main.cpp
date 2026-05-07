#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>


//===============> TODO:
// Generate random Service and Characteristic UUIDs: https://www.uuidgenerator.net/


#define SERVICE_UUID        ""
#define CHARACTERISTIC_UUID ""


class MyCallbacks: public BLECharacteristicCallbacks {
   void onWrite(BLECharacteristic *pCharacteristic) {
     // =========> TODO: This callback function will be invoked when signal is
     // 		     received over BLE. Implement the necessary functionality that
     //		     will trigger the message to the LCD.
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


}
