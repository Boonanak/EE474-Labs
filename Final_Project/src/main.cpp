#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID        "bfcad23b-c6d5-4dc4-bf77-bb07f3f76e14"
#define CHARACTERISTIC_UUID "624dde7b-a518-45c1-bb2f-ba69ccf1e9ff"

// Buttons
#define PAIR_BUTTON 3
#define SHOOT_BUTTON 4
#define REQ_BUTTON 5

// LEDs
#define PAIR_STATUS_LED 6 //flashing if pairing, solid if paired, off if not paired
#define GAME_STATUS_LED 7 //flashing if requested, solid if active, off if not active
#define ALIVE_STATUS_LED 8
#define DEAD_STATUS_LED 9

// IR reciever
#define IR_RECV 10

volatile bool BLE_tripped = false; //communicate between esps
volatile bool pair_pressed = false;
volatile bool shoot_pressed = false;
volatile bool req_pressed = false;


class MyCallbacks: public BLECharacteristicCallbacks {
   void onWrite(BLECharacteristic *pCharacteristic) {
     // =========> TODO: This callback function will be invoked when signal is
     // 		     received over BLE. Implement the necessary functionality that
     //		     will trigger the message to the LCD.
    BLE_tripped = true;    
   }
};

void pairISR() {
  pair_pressed = true;
}

void shootISR() {
  shoot_pressed = true;
}

void reqISR() {
  req_pressed = true;
}

void setup() {
 BLEDevice::init("BestESP32");
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

  // Set button pins as inputs and attach interrupts
  pinMode(PAIR_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PAIR_BUTTON), pairISR, FALLING);

  pinMode(SHOOT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SHOOT_BUTTON), shootISR, FALLING);

  pinMode(REQ_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);
}

void loop() {
  // put your main code here, to run repeatedly:


}