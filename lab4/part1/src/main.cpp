#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <stdint.h>
#include <LiquidCrystal_I2C.h>

// LCD Stuff
#define LCD_ADDRESS 0x27
#define CHANNEL 0
#define FREQ 1000
#define RES 12

const TickType_t ledTaskExecutionTime = 48000 / portTICK_PERIOD_MS;      // 48 seconds
const TickType_t counterTaskExecutionTime = 20000 / portTICK_PERIOD_MS;  // 20 seconds
const TickType_t alphabetTaskExecutionTime = 26000 / portTICK_PERIOD_MS; // 26 seconds
// Remaining Execution Times
volatile TickType_t remainingLedTime = ledTaskExecutionTime;
volatile TickType_t remainingCounterTime = counterTaskExecutionTime;
volatile TickType_t remainingAlphabetTime = alphabetTaskExecutionTime;

TaskHandle_t ledHandle = NULL;
TaskHandle_t counterHandle = NULL;
TaskHandle_t alphabetHandle = NULL;

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

// Write a line of text to one of the LCD rows
void writeRow(int row, String line) {
  lcd.setCursor(0,row);
  lcd.print("                ");
  lcd.setCursor(0,row);
  lcd.print(line);
}

// Helper function to safely handle delays and track remaining runtime
void executeDelay(TickType_t delayTicks, volatile TickType_t *remainingTime) {
    vTaskDelay(delayTicks);
    if (*remainingTime >= delayTicks) {
        *remainingTime -= delayTicks;
    } else {
        *remainingTime = 0;
    }
}

void ledTask(void *arg) {
    const int LED_PIN = 4; // UPDATE TO ACTUAL LED PIN
    pinMode(LED_PIN, OUTPUT);

    // One complete pattern loop = 2s + 0.95s + 0.1s + 0.95s = 4 seconds
    const TickType_t tOn1  = 2000 / portTICK_PERIOD_MS;
    const TickType_t tOff1 = 950 / portTICK_PERIOD_MS;
    const TickType_t tOn2  = 100 / portTICK_PERIOD_MS;
    const TickType_t tOff2 = 950 / portTICK_PERIOD_MS;

    while (1) {
        // Repeat the pattern 12 times (12 * 4s = 48 seconds total execution)
        for (int i = 0; i < 12; i++) {
            // ON 2sec
            digitalWrite(LED_PIN, HIGH);
            executeDelay(tOn1, &remainingLedTime);

            // OFF 0.95 sec
            digitalWrite(LED_PIN, LOW);
            executeDelay(tOff1, &remainingLedTime);

            // ON 0.10 sec
            digitalWrite(LED_PIN, HIGH);
            executeDelay(tOn2, &remainingLedTime);

            // OFF 0.95 sec
            digitalWrite(LED_PIN, LOW);
            executeDelay(tOff2, &remainingLedTime);
        }
    }
}

void counterTask(void *arg) {
    const TickType_t oneSecond = 1000 / portTICK_PERIOD_MS;

    while (1) {
        for (int count = 1; count <= 20; count++) {
            writeRow(0, String(count));
            
            // Hold each value for 1 second (20 counts * 1s = 20 seconds total)
            executeDelay(oneSecond, &remainingCounterTime);
        }
    }
}

void alphabetTask(void *arg) {
    const TickType_t oneSecond = 1000 / portTICK_PERIOD_MS;

    while (1) {
        for (char letter = 'A'; letter <= 'Z'; letter++) {
            // Print out 1 letter to Serial Monitor
            Serial.printf("[Serial Alphabet]: %c\n", letter);
            
            // Wait 1 second per letter (26 letters * 1s = 26 seconds total)
            executeDelay(oneSecond, &remainingAlphabetTime);
        }
    }
}

void scheduleTasks(void *arg) {
   // TODO: Implement SRTF scheduling logic. This function should select the task with 
   //       the shortest remaining time and run it. Once a task completes it should 
   //       reset its remaining time.
}


void setup() {
   // TODO: Create 4 tasks and pin them to core 0:
   //          1. A scheduler that handles the scheduling of the other three tasks
   //          2. Blink an LED
   //          3. Print a counter to the LCD
   //          4. Print the alphabet to Serial
}
void loop() {}
