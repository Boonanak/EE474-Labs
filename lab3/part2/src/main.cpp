#include <Arduino.h>

#define MAX_TASKS 5

#define RUNNING 0
#define READY 1
#define SLEEPING 2
#define HALTED 3

#define LED_BLINK_INTERVAL 125
#define LCD_UPDATE_INTERVAL 2000
#define MUSIC_PLAY_INTERVAL 500 // delay between notes
#define ALPHABET_PRINT_INTERVAL 500
#define PRIORITY_UPDATE_INTERVAL 30000

#define MUSIC_BASE_FREQUENCY 440 // A4 note frequency in Hz

typedef struct TCB {
  void (*function)(); // pointer to the task function
  unsigned short int state; // current state of the task
  unsigned int lastRunTime; // last time the task was run
  unsigned short int priority; // task run priority (lower number = higher priority)
  unsigned int period; // period of the task in ms
} TCB;

TCB taskList[MAX_TASKS];
TCB *currentTask;

void taskA() {
  // Task A code here
}

void taskB() {
  // Task B code here
  static unsigned int timesRun = 0; // increment at the end of the function
}

void taskC() {
  // Task C code here
  static unsigned int timesRun = 0; // increment at the end of the function
}

void taskD() {
  // Task D code here
  static unsigned int timesRun = 0; // increment at the end of the function
}

void taskE() {
  // Task E code here
  static unsigned int currentScheme = 1; // wrap around from 1 to 3
}

void sleep_me(unsigned int ms) {
  currentTask->state = SLEEPING;
  currentTask->lastRunTime = millis();
  currentTask->period = ms;
}

void halt_me() {
  currentTask->state = HALTED;
}

void setup() {
  taskList[0] = {taskA, READY, 0, 2, LED_BLINK_INTERVAL};           // TASK A: Blinking LED 
  taskList[1] = {taskB, READY, 0, 3, LCD_UPDATE_INTERVAL};          // TASK B: LCD Counter
  taskList[2] = {taskC, READY, 0, 4, MUSIC_PLAY_INTERVAL};          // TASK C: Music Player
  taskList[3] = {taskD, READY, 0, 5, ALPHABET_PRINT_INTERVAL};      // TASK D: Alphabet Printer
  taskList[4] = {taskE, SLEEPING, 0, 1, PRIORITY_UPDATE_INTERVAL};  // TASK E: Priority Updater

}

void loop() {
  bool taskNotFound = true;
  while (taskNotFound) {
   int currentPriority = 1;
   for (int i = 0; i < MAX_TASKS; i++) {
      if (taskList[i].priority == currentPriority && taskList[i].state != SLEEPING) {

      }
    }
    currentPriority++;
    if (currentPriority > 5) taskNotFound = false; // exit if there were no tasks to run
  }
}
