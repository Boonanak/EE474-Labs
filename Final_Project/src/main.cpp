#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include "BluetoothSerial.h" // Much simpler library than BLE

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
volatile bool connected = false;

typedef enum GameState {
  UNPAIRED,
  GAME_OVER,
  REQUEST_GAME,
  GAME_ACTIVE
} GameState;

volatile GameState currentState = UNPAIRED;

typedef enum BLEMessage {
  REQUEST_GAME_MSG = 'R',
  GAME_ACCEPTED_MSG = 'A',
  GAME_OVER_MSG = 'O'
} BLEMessage;

BluetoothSerial SerialBT;

void pairISR() { pair_pressed = true; }
void reqISR() { req_pressed = true; }
void IRAM_ATTR ir_isr() {
  hitDetected = true;
  detachInterrupt(digitalPinToInterrupt(IR_RECV));
}

// 1. SIMPLE SEND
void sendMessage(BLEMessage msg) {
  if (connected) {
    SerialBT.write((uint8_t)msg);
    Serial.printf("Sent message: %c\n", (char)msg);
  }
}

// 2. SIMPLE, NON-BLOCKING RECEIVE
bool checkForReceivedMessages(BLEMessage msg) {
  if (SerialBT.available()) {
    char incoming = SerialBT.peek(); // Look at the byte without removing it yet
    if (incoming == (char)msg) {
      SerialBT.read(); // Clear it from buffer
      Serial.printf("Received message: %c\n", incoming);
      return true;
    }
  }
  return false;
}

// 3. SIMPLE PAIRING TASK
void pairDevices(void* p) {
  SerialBT.begin("LaserTag", true); // Boot as master/slave symmetrically
  
  Serial.println("Searching for peer named 'LaserTag'...");
  
  while (currentState == UNPAIRED) {
    // Attempt a connection to any peer broadcasting the name "LaserTag"
    if (!connected) {
      connected = SerialBT.connect("LaserTag");
    }
    
    if (connected) {
      currentState = GAME_OVER;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  Serial.println("Device Paired! Suspending pairing task.");
  vTaskSuspend(NULL);
}

void handleGameState(void *p) {
  while (true) {
    // If we suddenly lose connection, drop back to unpaired status
    if (!connected && currentState != UNPAIRED) {
      currentState = UNPAIRED;
      vTaskResume(NULL); // Re-awaken pairing task
    }

    switch(currentState) {
      case UNPAIRED: {
        if (connected) {
          currentState = GAME_OVER;
          pair_pressed = false;
          req_pressed = false;
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
        // Did our peer accept? 
        if (checkForReceivedMessages(GAME_ACCEPTED_MSG)) {
          vTaskDelay(pdMS_TO_TICKS(3000));
          currentState = GAME_ACTIVE;
          attachInterrupt(digitalPinToInterrupt(IR_RECV), ir_isr, FALLING);
        }
        // Or are we the ones who need to press the button to accept?
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
  Serial.begin(115200);

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