/**
 * @file main.cpp
 * @author Vance Borus & Sean Bubernak
 * @brief Lab 4 Part 1 - Shortest Remaining Time First Scheduler
 * @version 1.0
 * @date 2026-05-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */

// AICODE: CoPilot-300

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
const TickType_t counterTaskExecutionTime = 20000 / portTICK_PERIOD_MS;  // 20 seconds, SHORTEST
const TickType_t alphabetTaskExecutionTime = 26000 / portTICK_PERIOD_MS; // 26 seconds
// Remaining Execution Times
volatile TickType_t remainingLedTime = ledTaskExecutionTime;
volatile TickType_t remainingCounterTime = counterTaskExecutionTime;
volatile TickType_t remainingAlphabetTime = alphabetTaskExecutionTime;

#define LED_PIN 4

TaskHandle_t ledHandle = NULL;
TaskHandle_t counterHandle = NULL;
TaskHandle_t alphabetHandle = NULL;

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

/**
 * @brief Write a line of text to one of the LCD rows
 * 
 * @param row row of the LCD display to write to
 * @param line Text to write to the LCD
 */
void writeRow(int row, String line) {
  lcd.setCursor(0,row);
  lcd.print("                ");
  lcd.setCursor(0,row);
  lcd.print(line);
}

/**
 * @brief Helper function to safely handle delays and track remaining runtime
 * 
 * @param delayTicks How many ticks to delay for 
 * @param remainingTime How much time is remaining for the current task (will be decremented by delayTicks, but not below 0)
 */
void executeDelay(TickType_t delayTicks, volatile TickType_t *remainingTime) {
    vTaskDelay(delayTicks);
    if (*remainingTime >= delayTicks) {
        *remainingTime -= delayTicks;
    } else {
        *remainingTime = 0;
    }
}

/**
 * @brief Blinks an LED in a specific pattern
 * 
 * @param arg Unused
 */
void ledTask(void *arg) {
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

/**
 * @brief Counts from 1 to 20, printing each number to the LCD and holding 
 * it for 1 second before moving to the next number
 * 
 * @param arg unused
 */
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

/**
 * @brief Print letters A-Z to the Serial Monitor, 
 * holding each letter for 1 second before moving to the next
 * 
 * @param arg unused
 */
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

/**
 * @brief Implementation a Shortest Remaining Time First scheduler with priority one which
 * Checks the remaining execution time of each task. It will run any ready task with shortest
 * remaining time, and suspend all other ready tasks. 
 * if a task is suspended, and it has the shortest remaining time, it will be resumed.
 * 
 * @param arg unused
 */
void scheduleTasks(void *arg) {
   TickType_t lastWakeTime = xTaskGetTickCount();
    while (1) {
         // get the current state of all tasks
         eTaskState ledState = eTaskGetState(ledHandle);
         eTaskState counterState = eTaskGetState(counterHandle);
         eTaskState alphabetState = eTaskGetState(alphabetHandle);

         TaskHandle_t shortestTask = NULL;
         TickType_t shortestTime = portMAX_DELAY;

         if (remainingLedTime == 0) {
            remainingLedTime = ledTaskExecutionTime;
         }
         if (remainingCounterTime == 0) {
            remainingCounterTime = counterTaskExecutionTime;
         }
         if (remainingAlphabetTime == 0) {
            remainingAlphabetTime = alphabetTaskExecutionTime;
         }

         // Find the task with the shortest time that it ready or suspended
         if ((ledState == eReady || ledState == eSuspended) && remainingLedTime < shortestTime) {
             shortestTime = remainingLedTime;
             shortestTask = ledHandle;
         }
         if ((counterState == eReady || counterState == eSuspended) && remainingCounterTime < shortestTime) {
             shortestTime = remainingCounterTime;
             shortestTask = counterHandle;
         }
         if ((alphabetState == eReady || alphabetState == eSuspended) && remainingAlphabetTime < shortestTime) {
             shortestTime = remainingAlphabetTime;
             shortestTask = alphabetHandle;
         }

         // Resume the task with the shortest remaining time if it's not already running
         if (shortestTask != NULL) vTaskResume(shortestTask);

         // Suspend all other ready tasks that are not the shortest
         if (ledHandle != shortestTask && ledState == eReady) {
             vTaskSuspend(ledHandle);
         }
         if (counterHandle != shortestTask && counterState == eReady) {
             vTaskSuspend(counterHandle);
         }
         if (alphabetHandle != shortestTask && alphabetState == eReady) {
             vTaskSuspend(alphabetHandle);
         }

         // Yield scheduler
         vTaskDelayUntil(&lastWakeTime, 50 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief Creates the tasks and sets up the LCD and LED
 * 
 */
void setup() {
    Serial.begin(9600);
    delay(1000); 
    Serial.println("SRTF Scheduler Initializing...");
    lcd.init();
    lcd.backlight();
    pinMode(LED_PIN, OUTPUT);

    // Create the tasks in a suspended state
    xTaskCreatePinnedToCore(ledTask, "LED Task", 2048, NULL, 1, &ledHandle, 0);
    vTaskSuspend(ledHandle);
    xTaskCreatePinnedToCore(counterTask, "Counter Task", 2048, NULL, 1, &counterHandle, 0);
    vTaskSuspend(counterHandle);

    xTaskCreatePinnedToCore(alphabetTask, "Alphabet Task", 2048, NULL, 1, &alphabetHandle, 0);
    vTaskSuspend(alphabetHandle);

    // Create the scheduler with a HIGHER priority (2) so it can preempt worker tasks (1)
    xTaskCreatePinnedToCore(scheduleTasks, "SRTF Scheduler", 3072, NULL, 2, NULL, 0);
}

void loop() {}
