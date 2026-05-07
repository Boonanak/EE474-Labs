#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define MAX_TASKS 5

// Task states
#define RUNNING 0
#define READY 1
#define SLEEPING 2
#define HALTED 3

// Periods for each task
#define LED_BLINK_INTERVAL 125
#define LCD_UPDATE_INTERVAL 2000
#define MUSIC_PLAY_INTERVAL 600 // delay between notes
#define ALPHABET_PRINT_INTERVAL 500
#define PRIORITY_UPDATE_INTERVAL 30000

#define MUSIC_BASE_FREQUENCY 150 // A4 note frequency in Hz

// LCD Stuff
#define LCD_ADDRESS 0x27
#define CHANNEL 0
#define FREQ 1000
#define RES 12

// Pins
#define LED_PIN 1
#define BUZZER_PIN 2

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

void sleep_me(unsigned int ms);
void halt_me();

// 12-bit PWM resolution: 0–4095
// Channel frequency: 1000 Hz
// 10-note sequence for buzzer output

int buzzer_notes[10] = {
    512,   // C
    1024,  // D
    1536,  // E
    2048,  // F
    2560,  // G
    3072,  // A
    3584,  // B
    4095,  // High C
    2048,  // F
    1024   // D
};

// Task information
typedef struct TCB {
  void (*function)(); // pointer to the task function
  unsigned short int state; // current state of the task
  unsigned int lastRunTime; // last time the task was run
  unsigned short int priority; // task run priority (lower number = higher priority)
  unsigned int period; // How often the task should be run
  bool reset; // if the task is supposed to reset itself
} TCB;

// Each task priority scheme
int schemeTable[3][4] = {
                  {2, 3, 4, 5},
                  {5, 4, 2, 3},
                  {2, 2, 2, 2}
                  };

// List of tasks and the current task
TCB taskList[MAX_TASKS];
TCB *currentTask;

// Write a line of text to one of the LCD rows
void writeRow(int row, String line) {
  lcd.setCursor(0,row);
  lcd.print("                ");
  lcd.setCursor(0,row);
  lcd.print(line);
}

// Blink an LED at a specific frequency
void taskA() {
    static bool currentState = 0;
    if (currentTask->reset) currentState = 0;

    currentState = !currentState;
    digitalWrite(LED_PIN, currentState);
    sleep_me(LED_BLINK_INTERVAL);
}

// Increment a number on the LCD screen
void taskB() {
  static unsigned int timesRun = 0;
  static unsigned int loopsRun = 0;
  if (currentTask->reset) {
    timesRun = 0;
    loopsRun = 0;
  }
  
  if (timesRun < 10) {
    writeRow(0, String(timesRun + 1)); // to be made
    timesRun++;
  }
  if (timesRun == 10) {
    loopsRun++;
    timesRun = 0;
  }
  
  if (loopsRun == 2) {
    halt_me();
  } else {
    sleep_me(LCD_UPDATE_INTERVAL);
  }
}

// Play a note to the buzzer
void taskC() {
  static unsigned int timesRun = 0;
  static unsigned int loopsRun = 0;
  if (currentTask->reset) {
    timesRun = 0;
    loopsRun = 0;
  }
  unsigned int note = 0;

  if (timesRun < 10) {
    note = buzzer_notes[timesRun];
    writeRow(1, String(note));
    ledcWrite(0, note);
    timesRun++;
  } 
  if (timesRun == 9) {
    loopsRun++;
    timesRun = 0;
  }
  
  if (loopsRun == 2) {
    halt_me();
    ledcWrite(0, 0); // turn off buzzer
    writeRow(1, String(0));
  } else {
    sleep_me(MUSIC_PLAY_INTERVAL);
  }
}

// Print the alphabet to the screen one char at a time
void taskD() {
  static unsigned int currentChar = 65; // increment at the end of the function
  static unsigned int loopsRun = 0;
  if (currentTask->reset) {
    currentChar = 65;
    loopsRun = 0;
  }
  if (currentChar < 91) {
    Serial.println(char(currentChar));
    currentChar++;
  }  else if (currentChar == 91) {
    loopsRun++;
    currentChar = 65;
  }
  
  if (loopsRun == 2) {
    halt_me();
  } else {
    sleep_me(ALPHABET_PRINT_INTERVAL);
  }
}

// Update the priority of the tasks
void taskE() {
  // Task E code here
  static unsigned int currentScheme = 1; // wrap around from 1 to 3
  for (int i = 0; i < MAX_TASKS - 1; i++) {
    taskList[i].priority = schemeTable[currentScheme - 1][i];
    taskList[i].state = READY; // Restart all tasks
    taskList[i].reset = true; 
  }
  Serial.println("Updated Priority Scheme to Scheme " + String(currentScheme));
  currentScheme = (currentScheme % 3) + 1;
  sleep_me(PRIORITY_UPDATE_INTERVAL);
}

// Sleep the current task
void sleep_me(unsigned int ms) {
  currentTask->state = SLEEPING;
  currentTask->lastRunTime = millis();
  currentTask->period = ms;
  currentTask->reset = false;
}

// Halt the current task
void halt_me() {
  currentTask->state = HALTED;
  currentTask->reset = false;
}

// Setup the task list
void setup() {
  Serial.begin(9600);

  lcd.begin();
  pinMode(LED_PIN, OUTPUT);
  ledcSetup(CHANNEL, FREQ, RES);
  ledcAttachPin(BUZZER_PIN, CHANNEL);

  taskList[0] = {taskA, READY, 0, 2, LED_BLINK_INTERVAL};           // TASK A: Blinking LED 
  taskList[1] = {taskB, READY, 0, 3, LCD_UPDATE_INTERVAL};          // TASK B: LCD Counter
  taskList[2] = {taskC, READY, 0, 4, MUSIC_PLAY_INTERVAL};          // TASK C: Music Player
  taskList[3] = {taskD, READY, 0, 5, ALPHABET_PRINT_INTERVAL};      // TASK D: Alphabet Printer
  taskList[4] = {taskE, READY, 0, 1, PRIORITY_UPDATE_INTERVAL};  // TASK E: Priority Updater

}

// Priority non-preemptive scheduler
void loop() {
  bool ranTask = false; // whether or not we have actually run a task yet

  // loop through every priority
  for (int currentPriority = 1; currentPriority <= 5; currentPriority++) {

    // loop through every task in the list
    for (int i = 0; i < MAX_TASKS; i++) { 
      currentTask = &taskList[i];

      // if the task matches the current priority level
      if (taskList[i].priority == currentPriority) {

        // Behavior based on the task's state
        if (taskList[i].state == HALTED) { // skip if halted
          continue;
        // If sleeping, check if it should wake up, and continue search for more tasks to run
        } else if (taskList[i].state == SLEEPING) {
          if (millis() - taskList[i].lastRunTime >= taskList[i].period) {
            taskList[i].state = READY;
          }
          continue;
        // run the task if it is ready, then break out (search complete)
        } else if (taskList[i].state == READY) {
          taskList[i].state = RUNNING;
          taskList[i].function(); 
          // return from task here, tasks are responsible for calling sleep_me and halt_me
          ranTask = true;
          break;
        } else { // skip if running or unknown state
          continue;
        }
      }
    }
    if (ranTask) break; // restart search from highest priority level if we ran a task
  }
}
