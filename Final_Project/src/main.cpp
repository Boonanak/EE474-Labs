#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <WiFi.h>
#include <esp_now.h>

// Buttons
#define PAIR_BUTTON 3
#define SHOOT_BUTTON 4
#define REQ_BUTTON 5

// LEDs
#define PAIR_STATUS_LED 6 
#define GAME_STATUS_LED 7 
#define ALIVE_STATUS_LED 8 
#define DEAD_STATUS_LED 9 

// IR receiver
#define IR_RECV 10

// System state
volatile bool pair_pressed = false;
volatile bool req_pressed = false;
volatile bool hitDetected = false;
bool peerRegistered = false;

// Broadcast MAC address (updated later in pair task from peerInfo struct)
uint8_t peerMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef enum GameState {
  UNPAIRED,
  GAME_OVER,
  REQUEST_GAME,
  GAME_ACTIVE
} GameState;

volatile GameState currentState = UNPAIRED;

typedef enum BLEMessage {
  REQUEST_GAME_MSG = 1,
  GAME_ACCEPTED_MSG = 2,
  GAME_OVER_MSG = 3
} BLEMessage;

volatile int lastReceivedMessage = -1;

void pairISR() { pair_pressed = true; }
void reqISR() { req_pressed = true; }
void IRAM_ATTR ir_isr() {
  hitDetected = true;
  detachInterrupt(digitalPinToInterrupt(IR_RECV));
}

// Global callback function that runs automatically whenever data arrives over the air
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  if (len > 0) {
    lastReceivedMessage = incomingData[0];
    
    // Automatically dynamic-pair: update the target destination with the sender's real MAC
    if (!peerRegistered) {
      memcpy(peerMac, mac, 6);
      peerRegistered = true;
    }
  }
}

void sendMessage(BLEMessage msg) {
  uint8_t payload = (uint8_t)msg;
  esp_err_t result = esp_now_send(peerMac, &payload, 1);
  if (result == ESP_OK) {
    Serial.printf("Message Sent: %d\n", msg);
  } else {
    Serial.println("Error sending the data");
  }
}

bool checkForReceivedMessages(BLEMessage msg) {
  if (lastReceivedMessage == (int)msg) {
    lastReceivedMessage = -1; // Consume it
    return true;
  }
  return false;
}

void pairDevices(void* p) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register receive callback
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  // Add the broadcast peer configuration initially
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, peerMac, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add initial peer");
  }

  Serial.println("Ready to pair! Press PAIR button or wait for incoming packet...");

  while (currentState == UNPAIRED) {
    // If the pair button is pressed, we send a beacon poke out to anyone listening
    if (pair_pressed) {
      uint8_t broadcastPoke = 0; 
      esp_now_send(peerMac, &broadcastPoke, 1);
      pair_pressed = false;
    }

    // Once we receive a packet from another board, OnDataRecv saves their specific address
    if (peerRegistered) {
      // Re-register peer explicitly to optimize transmission pipeline
      esp_now_del_peer(peerMac);
      esp_now_peer_info_t peerInfo2 = {};
      memcpy(peerInfo2.peer_addr, peerMac, 6);
      peerInfo2.channel = 0;
      peerInfo2.encrypt = false;
      esp_now_add_peer(&peerInfo2);

      currentState = GAME_OVER;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }

  Serial.println("Device Paired! Suspending pairing task.");
  vTaskSuspend(NULL);
}

void handleGameState(void *p) {
  while (true) {
    switch(currentState) {
      case UNPAIRED: {
        if (peerRegistered) {
          currentState = GAME_OVER;
          detachInterrupt(digitalPinToInterrupt(PAIR_BUTTON));
          attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);
        }
        break;        
      }
      
      case GAME_OVER: {
        if (req_pressed) {
          req_pressed = false;
          currentState = REQUEST_GAME;
          sendMessage(REQUEST_GAME_MSG); 
        } else if (checkForReceivedMessages(REQUEST_GAME_MSG)) {
          currentState = REQUEST_GAME;
        }
        break;
      }
      
      case REQUEST_GAME: {
        if (checkForReceivedMessages(GAME_ACCEPTED_MSG)) {
          vTaskDelay(pdMS_TO_TICKS(3000));
          currentState = GAME_ACTIVE;
          attachInterrupt(digitalPinToInterrupt(IR_RECV), ir_isr, FALLING);
        }
        else if (req_pressed) {
          req_pressed = false;
          sendMessage(GAME_ACCEPTED_MSG);
          vTaskDelay(pdMS_TO_TICKS(3200));
          currentState = GAME_ACTIVE;
          attachInterrupt(digitalPinToInterrupt(IR_RECV), ir_isr, FALLING);
        }
        break;
      }
      
      case GAME_ACTIVE: {
        if (hitDetected) {
          sendMessage(GAME_OVER_MSG);
          hitDetected = false;
          currentState = GAME_OVER;
          attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);
        } else if (checkForReceivedMessages(GAME_OVER_MSG)) {
          currentState = GAME_OVER;
          attachInterrupt(digitalPinToInterrupt(REQ_BUTTON), reqISR, FALLING);
        }
        break;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

void ledHandler(void *p) {
  static bool pair_led_state = LOW;
  static bool game_led_state = LOW;
  
  while(true) {
    switch(currentState) {
      case UNPAIRED: {
        digitalWrite(GAME_STATUS_LED, LOW);
        digitalWrite(ALIVE_STATUS_LED, LOW);
        digitalWrite(DEAD_STATUS_LED, LOW);
        pair_led_state = !pair_led_state;
        digitalWrite(PAIR_STATUS_LED, pair_led_state);
        vTaskDelay(pdMS_TO_TICKS(500));
        break;
      }
      case GAME_OVER: {
        digitalWrite(PAIR_STATUS_LED, HIGH);
        digitalWrite(GAME_STATUS_LED, LOW);
        digitalWrite(ALIVE_STATUS_LED, HIGH);
        digitalWrite(DEAD_STATUS_LED, LOW);
        vTaskDelay(pdMS_TO_TICKS(500));
        break;
      }
      case REQUEST_GAME: {
        digitalWrite(PAIR_STATUS_LED, HIGH);
        digitalWrite(ALIVE_STATUS_LED, HIGH);
        digitalWrite(DEAD_STATUS_LED, LOW);
        game_led_state = !game_led_state;
        digitalWrite(GAME_STATUS_LED, game_led_state);
        vTaskDelay(pdMS_TO_TICKS(250));
        break;
      }
      case GAME_ACTIVE: {
        digitalWrite(GAME_STATUS_LED, HIGH);
        digitalWrite(PAIR_STATUS_LED, HIGH);
        digitalWrite(ALIVE_STATUS_LED, !hitDetected);
        digitalWrite(DEAD_STATUS_LED, hitDetected);
        vTaskDelay(pdMS_TO_TICKS(100));
        break;
      }
    }
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(PAIR_STATUS_LED, OUTPUT);
  pinMode(GAME_STATUS_LED, OUTPUT);
  pinMode(ALIVE_STATUS_LED, OUTPUT);
  pinMode(DEAD_STATUS_LED, OUTPUT);

  pinMode(PAIR_BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PAIR_BUTTON), pairISR, FALLING);

  pinMode(REQ_BUTTON, INPUT_PULLUP);
  pinMode(IR_RECV, INPUT);
  
  xTaskCreatePinnedToCore(pairDevices, "Pairing Task", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ledHandler, "LED Task", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(handleGameState, "Game Task", 4096, NULL, 1, NULL, 1);
}

void loop() {}