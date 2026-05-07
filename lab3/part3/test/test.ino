#include <BLEDevice.h>
#include <BLEServer.h>
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-123456789abc"


void setup() {
 BLEDevice::init("AwesomeESP32"); // Change this to an unique name
 BLEServer *pServer = BLEDevice::createServer();
 BLEService *pService = pServer->createService(SERVICE_UUID);
 BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                       CHARACTERISTIC_UUID,
                                       BLECharacteristic::PROPERTY_READ |
                                       BLECharacteristic::PROPERTY_WRITE
                                     );
 pService->start();
 // the initial value for the characteristic we defined
 pCharacteristic->setValue("1");
 BLEAdvertising *pAdvertising = pServer->getAdvertising();
 pAdvertising->start();
}
void loop() {}