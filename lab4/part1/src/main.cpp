// Total times for tasks
#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

const TickType_t ledTaskExecutionTime = 48000 / portTICK_PERIOD_MS;      // 48 seconds
const TickType_t counterTaskExecutionTime = 20000 / portTICK_PERIOD_MS;  // 20 seconds
const TickType_t alphabetTaskExecutionTime = 26000 / portTICK_PERIOD_MS; // 26 seconds
// Remaining Execution Times
volatile TickType_t remainingLedTime = ledTaskExecutionTime;
volatile TickType_t remainingCounterTime = counterTaskExecutionTime;
volatile TickType_t remainingAlphabetTime = alphabetTaskExecutionTime;


void ledTask(void *arg) {
   // TODO: Blink an LED and update remaining time for this task
}


void counterTask(void *arg) {
 // TODO: Print out an incrementing counter to your LCD, and 
 //       update remaining time for this task
}


void alphabetTask(void *arg) {
 // TODO: Print out the alphabet to Serial, and update remaining
 //       time for this task
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