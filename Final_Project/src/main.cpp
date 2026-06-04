/**
 * @file main.cpp
 * @author Vance Borus and Sean Bubernak
 * @brief Implementation of a laser tag game using IR Leds and two ESP32s communicating over BLE
 * @version 1.0
 * @date 2026-06-04
 * 
 * @copyright Copyright (c) 2026
 * 
 */

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
volatile bool pair_pressed = false;
volatile bool req_pressed = false;
volatile bool hitDetected = false;
volatile bool isRequestor = false;

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
  GAME_OVER_MSG,
  NOT_A_MSG
} BLEMessage;

// Connection handles
BLEServer* pServer = nullptr;
BLEClient* pClient = nullptr;
BLECharacteristic* pLocalCharacteristic = nullptr;       // Our server characteristic
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr; // Peer’s characteristic

// Status flags
BLEAddress* peerAddress = nullptr;
bool clientConnected = false;
bool serverConnected = false;
bool remoteCharacteristicReady = false;

// Message buffer
volatile BLEMessage lastReceivedMessage;
volatile bool messageAvailable = false;

// Tie breaking for connection
volatile bool clientChosen = false;
volatile bool iAmClient = false;

/**
 * @brief Server callback to track when a Central client connects to us
 * 
 */
class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
        serverConnected = true;
        Serial.println("Peer connected to our server");
    }

    void onDisconnect(BLEServer* pServer) override {
        serverConnected = false;
        Serial.println("Peer disconnected from our server");
        BLEDevice::startAdvertising();
    }
};

/**
 * @brief Client callback to track when we connect to a server and when notifications are received
 * 
 */
class AdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice device) override {
        if (device.haveServiceUUID() &&
            device.isAdvertisingService(BLEUUID(SERVICE_UUID))) {

            Serial.println("Found peer device!");
            peerAddress = new BLEAddress(device.getAddress());
            BLEDevice::getScan()->stop();
        }
    }
};

/**
 * @brief Server characteristic callback to track when we receive messages from the client
 * 
 */
class ServerCharacteristicCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        std::string value = characteristic->getValue();
        if (value.length() >= 1) {
            lastReceivedMessage = (BLEMessage)value[0];
            messageAvailable = true;
            Serial.printf("Server received message: %d\n", lastReceivedMessage);
        }
    }
};

/**
 * @brief Client callback tracking when we find our target peripheral device
 * 
 * @param characteristic The characteristic that is the source of the event
 * @param data pointer to the data received
 * @param length number of bytes received
 * @param isNotify unused
 */
static void notifyCallback(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify) {
    if (length >= 1) {
        lastReceivedMessage = (BLEMessage)data[0];
        messageAvailable = true;
        Serial.printf("Received message via notify: %d\n", lastReceivedMessage);
    }
}

/**
 * @brief ISR for the pair button, sets the pair_pressed flag
 * 
 */
void pairISR() {
  pair_pressed = true;
}

/**
 * @brief ISR for the game/request button, sets the req_pressed flag
 * 
 */
void reqISR() {
  req_pressed = true;
}

/**
 * @brief ISR for when the IR receiver detects a hit. Detaches itself to prevent multiple triggers
 * 
 */
void IRAM_ATTR ir_isr() {
  hitDetected = true;
  detachInterrupt(digitalPinToInterrupt(IR_RECV));
}

// 
/**
 * @brief Sends a message to the connected device, either as the client or the server
 * 
 * @param msg The message to be sent
 */
void sendMessage(BLEMessage msg) {
    uint8_t data = (uint8_t)msg;

    // CLIENT → write to server characteristic
    if (iAmClient && remoteCharacteristicReady && pRemoteCharacteristic != nullptr) {
        pRemoteCharacteristic->writeValue(&data, 1, true);
        Serial.printf("Client sent message: %d\n", msg);
        return;
    }

    // SERVER → notify client
    if (!iAmClient && pLocalCharacteristic != nullptr) {
        pLocalCharacteristic->setValue(&data, 1);
        pLocalCharacteristic->notify();
        Serial.printf("Server sent message: %d\n", msg);
        return;
    }

    Serial.println("sendMessage() failed: no valid BLE path");
}


/**
 * @brief Check if there is a message to receive
 * 
 * @param msgOut reference to a variable to store the received message in
 * @return true if message was detected, stores message in msgOut
 * @return false if no message was detected
 */
bool getReceivedMessage(BLEMessage &msgOut) {
    if (!messageAvailable)
        return false;

    msgOut = lastReceivedMessage;
    messageAvailable = false;
    return true;
}

/**
 * @brief Pair both devices together, pair buttons must be pressed by both players in order to begin pairing
 * 
 * @param p unused
 */
void pairDevices(void* p) {

    Serial.println("Waiting for pair button...");

    // Wait for THIS device to press the pair button
    while (!pair_pressed) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    Serial.println("Pair button pressed. Starting BLE...");

    BLEDevice::init("LaserTag_Node");

    // --------------------------
    // 1. Start BLE Server
    // --------------------------
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    pLocalCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pLocalCharacteristic->setCallbacks(new ServerCharacteristicCallbacks());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    BLEDevice::startAdvertising();

    Serial.println("Server started, advertising...");

    // --------------------------
    // 2. Scan for peer
    // --------------------------
    BLEScan* scan = BLEDevice::getScan();
    scan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    scan->setActiveScan(true);

    Serial.println("Scanning for peer...");

    while (peerAddress == nullptr) {
        scan->start(3, false);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    Serial.print("Found peer: ");
    Serial.println(peerAddress->toString().c_str());

    // --------------------------
    // 3. MAC ADDRESS TIE-BREAKER
    // --------------------------
    String myMac = BLEDevice::getAddress().toString().c_str();
    String peerMac = peerAddress->toString().c_str();

    // Lower MAC becomes the client
    iAmClient = (myMac < peerMac);

    Serial.printf("My MAC:   %s\n", myMac.c_str());
    Serial.printf("Peer MAC: %s\n", peerMac.c_str());
    Serial.printf("I am the %s\n", iAmClient ? "CLIENT" : "SERVER");

    // --------------------------
    // 4. SERVER-ONLY DEVICE
    // --------------------------
    if (!iAmClient) {
        Serial.println("Waiting for client to connect...");
        while (!serverConnected) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        Serial.println("Client connected!");
        remoteCharacteristicReady = false;
        vTaskSuspend(NULL);
    }

    // --------------------------
    // 5. CLIENT DEVICE
    // --------------------------
    Serial.println("Connecting as client...");

    pClient = BLEDevice::createClient();
    if (!pClient->connect(*peerAddress)) {
        Serial.println("Client connection failed");
        vTaskDelete(NULL);
    }

    clientConnected = true;

    BLERemoteService* remoteService = pClient->getService(SERVICE_UUID);
    pRemoteCharacteristic = remoteService->getCharacteristic(CHARACTERISTIC_UUID);

    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    remoteCharacteristicReady = true;

    Serial.println("Pairing complete!");
    vTaskDelay(pdMS_TO_TICKS(100));
    vTaskSuspend(NULL);
}

/**
 * @brief Check if pairing between the devices is complete. Different for client vs server
 * 
 * @return true if paired
 * @return false if not paired
 */
bool pairingComplete() {
    if (iAmClient) {
        // Client needs full connection
        return serverConnected && clientConnected && remoteCharacteristicReady;
    } else {
        // Server only needs the client to connect
        return serverConnected;
    }
}

/**
 * @brief Task FSM to move through the states of the game
 * 
 * @param p unused
 */
void handleGameState(void *p) {
  while (true) {
    switch(currentState) {
      // In unpaired, we need to wait until we establish a connection with
      // an opponent. Once that is complete we move to GAME_OVER 
      // when moving, detatch interrupt for pair button
      case UNPAIRED: {
        if (pairingComplete()) {
          pair_pressed = false;
          req_pressed = false;
          detachInterrupt(digitalPinToInterrupt(PAIR_BUTTON));
          attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);
          vTaskDelay(pdMS_TO_TICKS(500));
          currentState = GAME_OVER;
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
        static BLEMessage msg;
        if (req_pressed) {
          sendMessage(REQUEST_GAME_MSG);
          isRequestor = true;
          detachInterrupt(digitalPinToInterrupt(REQ_BUTTON));
          vTaskDelay(pdMS_TO_TICKS(500));
          currentState = REQUEST_GAME;
        } else if (getReceivedMessage(msg) && msg == REQUEST_GAME_MSG) {
          msg = NOT_A_MSG;
          isRequestor = false;
          vTaskDelay(pdMS_TO_TICKS(500));
          currentState = REQUEST_GAME;
        } else {
          currentState = GAME_OVER;
        }
        break;
      }
      // If in request game, waiting for both ESP32s to accept the game
      // If isRequestor, wait for a GAME_ACCEPTED_MSG from the opponent, then wait 3 seconds before moving to game active
      // if not isRequestor, wait until the accept button isr flag has been set, then send GAME_ACCEPTED_MSG and move to game
      // active in 3.2 seconds
      // When transitioning, detach interrupts for game button, attach interrupt for hit detection
      case REQUEST_GAME: {
        static BLEMessage msg;
        if (isRequestor) {
          if (getReceivedMessage(msg) && msg == GAME_ACCEPTED_MSG) {
            msg = NOT_A_MSG;
            vTaskDelay(pdMS_TO_TICKS(3000));
            attachInterrupt(digitalPinToInterrupt(IR_RECV), ir_isr, FALLING);
            vTaskDelay(pdMS_TO_TICKS(500));
            currentState = GAME_ACTIVE;
          }
        } else {
          if (req_pressed) {
            sendMessage(GAME_ACCEPTED_MSG);
            vTaskDelay(pdMS_TO_TICKS(3200));
            req_pressed = false;
            detachInterrupt(digitalPinToInterrupt(REQ_BUTTON));
            attachInterrupt(digitalPinToInterrupt(IR_RECV), ir_isr, FALLING);
            vTaskDelay(pdMS_TO_TICKS(500));
            currentState = GAME_ACTIVE;
          }
        }
        break;
      }
      // If in game active, check hit detection flag constantly
      // if a hit is detected send a GAME_OVER_MSG and move to GAME_OVER
      // if a GAME_OVER_MSG is detected, move to GAME_OVER
      // when transitioning, detatch interrupt for hit detection, and attach game button interrupt
      case GAME_ACTIVE: {
        static BLEMessage msg;
        req_pressed = false;
        if (hitDetected) {
          sendMessage(GAME_OVER_MSG);
          detachInterrupt(digitalPinToInterrupt(IR_RECV));
          attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);
          vTaskDelay(pdMS_TO_TICKS(500));
          hitDetected = false;
          currentState = GAME_OVER;
        } else if (getReceivedMessage(msg) && msg == GAME_OVER_MSG) {
          msg = NOT_A_MSG;
          detachInterrupt(digitalPinToInterrupt(IR_RECV));
          attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);
          vTaskDelay(pdMS_TO_TICKS(500));
          currentState = GAME_OVER;
        } else {
          currentState = GAME_ACTIVE;
        }
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

/**
 * @brief FSM to control the state of the game LEDs based on current game state
 * 
 * @param p unused
 */
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
        if (req_pressed || (!isRequestor && lastReceivedMessage == REQUEST_GAME_MSG)) {
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
        alive_led_state = !hitDetected;
        digitalWrite(ALIVE_STATUS_LED, alive_led_state);
        digitalWrite(DEAD_STATUS_LED, !alive_led_state);
        vTaskDelay(pdMS_TO_TICKS(50));
        break;
      }
    }
  }
}

/**
 * @brief Initilizes devices, sets pin modes, starts FreeRTOS tasks
 * 
 */
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

  pinMode(REQ_BUTTON, INPUT_PULLUP);

  pinMode(IR_RECV, INPUT);
  //attachInterrupt(digitalPinToInterrupt(IR_RECV), ir_isr, FALLING);
  xTaskCreatePinnedToCore(pairDevices, "Pairing Task", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ledHandler, "LED Task", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(handleGameState, "Game Task", 2048, NULL, 1, NULL, 1);
}

/**
 * @brief Unused in FreeRTOS
 * 
 */
void loop() {}