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

// System state
volatile bool BLE_tripped = false; //communicate between esps
volatile bool pair_pressed = false;
volatile bool shoot_pressed = false;
volatile bool req_pressed = false;

// Client connection variables
bool isClient = false;
bool connectedToServer = false;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
BLEAdvertisedDevice* targetDevice = nullptr;

class MyServerCallbacks: public BLECharacteristicCallbacks {
   void onWrite(BLECharacteristic *pCharacteristic) {
     // This fires on the SERVER esp when the CLIENT esp sends a message
     BLE_tripped = true;    
   }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Look for a device advertising our exact game Service UUID
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      BLEDevice::getScan()->stop(); // Found it! Stop looking.
      targetDevice = new BLEAdvertisedDevice(advertisedDevice);
      isClient = true; // Flag to execute connection in the loop
    }
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

// Helper function for Client Connection
bool connectToServer() {
  BLEClient* pClient = BLEDevice::createClient();
  if (!pClient->connect(targetDevice)) return false;

  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pRemoteService == nullptr) return false;

  pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
  if (pRemoteCharacteristic == nullptr) return false;

  return true;
}

void setup() {
  Serial.begin(9600);

  // Initialize LEDs
  pinMode(PAIR_STATUS_LED, OUTPUT);
  pinMode(GAME_STATUS_LED, OUTPUT);
  pinMode(ALIVE_STATUS_LED, OUTPUT);
  pinMode(DEAD_STATUS_LED, OUTPUT);
  digitalWrite(ALIVE_STATUS_LED, HIGH); // Default to alive

  // Set button pins as inputs and attach interrupts
  pinMode(PAIR_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PAIR_BUTTON), pairISR, FALLING);

  pinMode(SHOOT_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SHOOT_BUTTON), shootISR, FALLING);

  pinMode(REQ_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);\

  BLEDevice::init("GameESP32");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyServerCallbacks());
  pService->start();
  
  // Start advertising so other ESPs can find us
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
}

void loop() {
  // put your main code here, to run repeatedly:

  
}