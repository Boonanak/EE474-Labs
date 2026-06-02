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
#define PAIR_STATUS_LED 6 // flashing if pairing, solid if paired, off if not paired
#define GAME_STATUS_LED 7 // flashing if requested, solid if active, off if not active
#define ALIVE_STATUS_LED 8 // solid if alive, off if hit
#define DEAD_STATUS_LED 9 // off if alive, on if hit and game is over

// IR reciever
#define IR_RECV 10

// System state
volatile bool BLE_tripped = false; //communicate between esps
volatile bool pair_pressed = false;
volatile bool req_pressed = false;
volatile bool hitDetected = false;

// Current state of the game
// Unpaired: not currently paired to another ESP32, waiting for a connection
// Game Over: Paired but game is not active, awaiting start request
// Request Game: Sent a request/received a request to start the game, awaiting confirmation
// Game Active: Game is ongoing, waiting to be hit or receive a game over signal
typedef enum GameState {
  UNPAIRED,
  GAME_OVER,
  REQUEST_GAME,
  GAME_ACTIVE
} GameState;

volatile GameState currentState = UNPAIRED;

// Messages sent by the esp 32 to it's opponent
// Request Game: Message saying a game has been requested
// Game Accepted: Message saying a requested game has been accepted
// Game Over: message saying someone has been hit and the game is over
typedef enum BLEMessage {
  REQUEST_GAME_MSG,
  GAME_ACCEPTED_MSG,
  GAME_OVER_MSG
} BLEMessage;

// Client connection variables
bool isClient = false;
bool connectedToServer = false;
bool isRequestor = false; // Whether or not THIS esp32 requested the game, or if the opponent did
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

void reqISR() {
  req_pressed = true;
}

// ISR for when the IR receiver detects a hit
// Detaches itself to prevent multiple triggers
void IRAM_ATTR ir_isr() {
  hitDetected = true;
  detachInterrupt(digitalPinToInterrupt(IR_RECV));
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

// TASK FUNCTIONS
void pairDevices(void* p) {
  bool isScanning = false;
  BLEScan* pBLEScan = nullptr;

  while (true) {
    // Check if we are currently UNPAIRED and the pairing process was initiated
    if (currentState == UNPAIRED && pair_pressed) {
      
      // If we haven't found a target server yet, but we aren't scanning yet so start scanning
      if (!isClient && !connectedToServer && !isScanning) {
        Serial.println("[Pairing] Button pressed. Starting BLE scan for opponent...");
        
        pBLEScan = BLEDevice::getScan();
        pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
        pBLEScan->setInterval(1349); // How often we restart a scan 
        pBLEScan->setWindow(449); // How long we are listening in a given cycle
        pBLEScan->setActiveScan(true);
        
        pBLEScan->start(4, false); // Start an asynchronous scan for 4 seconds
        isScanning = true;
      }

      // The MyAdvertisedDeviceCallbacks will flip 'isClient' to true if it discovers the service
      if (isClient && !connectedToServer) {
        Serial.println("[Pairing] Opponent spotted! Attempting connection...");
        isScanning = false; // Scan automatically stops in your callback

        if (connectToServer()) {
          Serial.println("[Pairing] Client successfully connected to Opponent's Server!");
          connectedToServer = true;
          
          // Stop advertising server since we successfully paired as a client
          BLEDevice::getAdvertising()->stop(); 

          // Transition state out of UNPAIRED
          detachInterrupt(digitalPinToInterrupt(PAIR_BUTTON));
          pair_pressed = false; 
          currentState = GAME_OVER; 
        } else {
          Serial.println("[Pairing] Connection failed. Retrying scan...");
          isClient = false; // Reset to try scanning again
        }
      }
      
      // If the scan timed out and didn't find anything, allow it to retry
      // if (isScanning && !isClient) {
      //    // Tiny delay to check if background scan finished
      //    vTaskDelay(pdMS_TO_TICKS(100)); 
      // }

    } else if (currentState == UNPAIRED && !pair_pressed) {
      // The button hasn't been pressed yet
      vTaskDelay(pdMS_TO_TICKS(100));
    } else {
      // If we are no longer UNPAIRED, we have successfully paired (or the other ESP connected to us)
      if (currentState != UNPAIRED) {
        Serial.println("[Pairing] Pairing successful! Now killing pairing task.");
        
        // Ensure the pairing button interrupt is completely detached
        detachInterrupt(digitalPinToInterrupt(PAIR_BUTTON));
        pair_pressed = false;

        // Pairing only happens once per device game cycle
        vTaskDelete(NULL); 
      }
    }
    
    // Crucial for FreeRTOS: yield control to let lower-priority tasks run
    vTaskDelay(pdMS_TO_TICKS(50)); 
  }
}

void handleGameState(void *p) {
  while (true) {
    switch(currentState) {
      // In unpaired, we need to wait until we establish a connection with
      // an opponent. Once that is complete we move to GAME_OVER 
      // when moving, detatch interrupt for pair button
      case UNPAIRED: {
        if (connectedToServer) {
          currentState = GAME_OVER;
          pair_pressed = false;
          req_pressed = false;
          detachInterrupt(digitalPinToInterrupt(PAIR_BUTTON));
          attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);
        } else {
          currentState = UNPAIRED;
        }
        break;        
      }
      // In game over, the game is not active. Detatch interrupts for the shoot button, listen for game requests
      // and game request button interrupts.
      // If a game request is sent OR recieved, move to request game
      // if this ESP32 is the requestor, set the isRequestor flag
      case GAME_OVER: {
        break;
      }
      // If in request game, waiting for both ESP32s to accept the game
      // If isRequestor, wait for a GAME_ACCEPTED_MSG from the opponent, then wait 3 seconds before moving to game active
      // if not isRequestor, wait until the accept button isr flag has been set, then send GAME_ACCEPTED_MSG and move to game
      // active in 3.2 seconds
      // When transitioning, detach interrupts for game button, attach interrupt for hit detection
      case REQUEST_GAME: {
        break;
      }
      // If in game active, check hit detection flag constantly
      // if a hit is detected send a GAME_OVER_MSG and move to GAME_OVER
      // if a GAME_OVER_MSG is detected, move to GAME_OVER
      // when transitioning, detatch interrupt for hit detection, and attach game button interrupt
      case GAME_ACTIVE: {
        break;
      }
    }
  }
}

// Handle setting the LED states
void ledHandler(void *p) {
  static bool pair_led_state = LOW;
  static bool game_led_state = LOW;
  static bool alive_led_state = HIGH;
  // dont need to track dead led since it is the opposite of alive
  while(true) {
    switch(currentState) {
      // Blue: off normally, blinking if pairing flag set
      // Yellow: off
      // Green: off 
      // Red: off
      case UNPAIRED: {
        game_led_state = LOW;
        digitalWrite(GAME_STATUS_LED, game_led_state);
        alive_led_state = LOW;
        digitalWrite(ALIVE_STATUS_LED, alive_led_state);
        digitalWrite(DEAD_STATUS_LED, LOW);
        if (pair_pressed) {
          pair_led_state = !pair_led_state;
          digitalWrite(PAIR_STATUS_LED, pair_led_state);
        } else {
          pair_led_state = LOW;
          digitalWrite(PAIR_STATUS_LED, pair_led_state);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        break;
      }
      // Blue: solid on
      // Yellow: off
      // Green: retain last state
      // Red: opposite of Green
      case GAME_OVER: {
        pair_led_state = HIGH;
        digitalWrite(PAIR_STATUS_LED, pair_led_state);
        game_led_state = LOW;
        digitalWrite(GAME_STATUS_LED, game_led_state);
        digitalWrite(ALIVE_STATUS_LED, alive_led_state);
        digitalWrite(DEAD_STATUS_LED, !alive_led_state);
        vTaskDelay(pdMS_TO_TICKS(500));
        break;
      }
      // Blue: solid on
      // Yellow: blinking
      // Green: retain last state
      // Red: opposite of green
      case REQUEST_GAME: {
        pair_led_state = HIGH;
        digitalWrite(PAIR_STATUS_LED, pair_led_state);
        digitalWrite(ALIVE_STATUS_LED, alive_led_state);
        digitalWrite(DEAD_STATUS_LED, !alive_led_state);
        if (game_led_state) {
          game_led_state = !game_led_state;
          digitalWrite(GAME_STATUS_LED, game_led_state);
        } else {
          game_led_state = LOW;
          digitalWrite(GAME_STATUS_LED, game_led_state);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        break;
      }
      // Blue: solid on
      // Yellow: solid on
      // Green: solid on if no hit is detected, off if hit detected
      // Red: opposite of green
      case GAME_ACTIVE: {
        game_led_state = HIGH;
        digitalWrite(GAME_STATUS_LED, game_led_state);
        pair_led_state = HIGH;
        digitalWrite(PAIR_STATUS_LED,pair_led_state);
        alive_led_state = hitDetected;
        digitalWrite(ALIVE_STATUS_LED, alive_led_state);
        digitalWrite(DEAD_STATUS_LED, !alive_led_state);
        vTaskDelay(pdMS_TO_TICKS(250));
        break;
      }
    }
  }
}


void setup() {
  Serial.begin(9600);

  // Initialize LEDs
  pinMode(PAIR_STATUS_LED, OUTPUT);
  pinMode(GAME_STATUS_LED, OUTPUT);
  pinMode(ALIVE_STATUS_LED, OUTPUT);
  pinMode(DEAD_STATUS_LED, OUTPUT);
  digitalWrite(ALIVE_STATUS_LED, HIGH); // Default to alive

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

  // Set button pins as inputs and attach interrupts
  pinMode(PAIR_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PAIR_BUTTON), pairISR, FALLING);

  pinMode(REQ_BUTTON, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);\

  // Start advertising so other ESPs can find us
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->start();
  //attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);

  pinMode(IR_RECV, INPUT);
  //attachInterrupt(digitalPinToInterrupt(IR_RECV), ir_isr, FALLING);
  xTaskCreatePinnedToCore(pairDevices, "Pairing Task", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ledHandler, "LED Task", 2048, NULL, 1, NULL, 1);
}

// Empty in FreeRTOS
void loop() {
  /*
  Serial.println("Hit Status: " + String(hitDetected));
  Serial.println("Pair Pressed: " + String(pair_pressed));
  Serial.println("Request Pressed: " + String(req_pressed));
  delay(1000);
  */
}