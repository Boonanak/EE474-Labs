#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD Stuff
#define LCD_ADDRESS 0x27
#define RES 12

// Configuration & Pins 
#define LDR_PIN          5    // ADC pin for the Photoresistor voltage divider
#define ALARM_LED_PIN    4     // LED for Anomaly Alarm
#define SMA_WINDOW_SIZE  10

// Global Variables

// FreeRTOS Handles
SemaphoreHandle_t xLightDataSemaphore = NULL;
TaskHandle_t xLightDetectorHandle = NULL;
TaskHandle_t xLCDHandle = NULL;
TaskHandle_t xAlarmHandle = NULL;
TaskHandle_t xPrimeHandle = NULL;

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2);

// Function Prototypes
void vLightDetectorTask(void *pvParameters);
void vLCDTask(void *pvParameters);
void vAnomalyAlarmTask(void *pvParameters);
void vPrimeCalculationTask(void *pvParameters);
bool isPrime(int n);

void setup() {
    // Init
    Serial.begin(9600);
    pinMode(LDR_PIN, INPUT);
    pinMode(ALARM_LED_PIN, OUTPUT);
    digitalWrite(ALARM_LED_PIN, LOW);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("System Initializing");

    // Create Binary Semaphore

    // TODO

    // Create Tasks and Assign to Cores
    // Core 0 Tasks
    xTaskCreatePinnedToCore(
        vLightDetectorTask,     // Task function
        "Light Detector",       // Task name
        4096,                   // Stack size
        NULL,                   // Parameters
        1,                      // Priority (Higher priority for real-time acquisition)
        &xLightDetectorHandle,  // Task handle
        0                       // Core ID
    );

    xTaskCreatePinnedToCore(
        vLCDTask,
        "LCD Display",
        4096,
        NULL,
        2,                      // Medium priority
        &xLCDHandle,
        0                       // Core ID
    );

    // Core 1 Tasks
    xTaskCreatePinnedToCore(
        vAnomalyAlarmTask,
        "Anomaly Alarm",
        4096,
        NULL,
        2,                      // Medium priority
        &xAlarmHandle,
        1                       // Core ID
    );

    xTaskCreatePinnedToCore(
        vPrimeCalculationTask,
        "Prime Calc",
        4096,
        NULL,
        3,                      // Low priority (background execution)
        &xPrimeHandle,
        1                       // Core ID
    );

    Serial.println("System setup completed. FreeRTOS Scheduler running...");
}

void loop() {}

// CORE 0: Light Detector Task
void vLightDetectorTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay500ms = pdMS_TO_TICKS(500);
    // Initialize light reading buffer with current reading from the pin
    static int lightReadings[SMA_WINDOW_SIZE] = {analogRead(LDR_PIN)};
    while (1) {
        int lightValue = analogRead(LDR_PIN);
        shiftArray(lightReadings, lightValue);
        int averageLight = caculateAverage(lightReadings);
        vTaskDelay(xDelay500ms);
    }
}

// CORE 0: LCD Task
void vLCDTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay250ms = pdMS_TO_TICKS(250);

    // TODO
}

// CORE 1: Anomaly Alarm Task
void vAnomalyAlarmTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay250ms = pdMS_TO_TICKS(250); // Checks every display cycle timing
  
    // TODO
}

// CORE 1: Prime Calculation Task
void vPrimeCalculationTask(void *pvParameters) {
    // Computes math in background loop
    for (int i = 2; i <= 5000; i++) {
        if (isPrime(i)) {
            Serial.printf("[Prime Core 1]: %d\n", i);
        }
        // Yield execution to allow lower/equal priority operations to communicate smoothly
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }

    Serial.println("--- Background Prime Calculations Completed up to 5000 ---");
    
    // Self terminate task once processing ends
    vTaskSuspend(xPrimeHandle);
}

// Helper math function
bool isPrime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

int caculateAverage(int *arr) {
    int sum = 0;
    for (int i = 0; i < SMA_WINDOW_SIZE; i++) {
        sum += arr[i];
    }
    return sum / SMA_WINDOW_SIZE;
}

void shiftArray(int *arr, int newValue) {
    for (int i = 0; i < SMA_WINDOW_SIZE - 1; i++) {
        arr[i] = arr[i + 1];
    }
    arr[SMA_WINDOW_SIZE - 1] = newValue;
}