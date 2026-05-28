/**
 * @file main.cpp
 * @author Vance Borus & Sean Bubernak
 * @brief Lab 4 Part 2: FreeRTOS Multicore Task Management with Anomaly Detection and LCD Display
 * @version 1.0
 * @date 2026-05-26
 * 
 * @copyright Copyright (c) 2026
 * 
 */

// AICODE - CoPilot-300

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

// LCD Stuff
#define LCD_ADDRESS 0x27
#define RES 12

// Configuration & Pins 
#define LDR_PIN          5    // ADC pin for the Photoresistor voltage divider
#define ALARM_LED_PIN    4     // LED for Anomaly Alarm
#define SMA_WINDOW_SIZE  10

// Anomaly Detection Threshold
#define LOW_THRESH 300
#define HIGH_THRESH 3800

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
int calculateAverage(int *arr);
void shiftArray(int *arr, int newValue);



int averageLight = 0; // Shared variable for average light level, updated by Light Detector Task and read by LCD Task
int lightValue = 0;

/**
 * @brief Intializes the LCD screen, creates FreeRTOS tasks for light detection, LCD display, 
 * anomaly alarm, and prime calculation, and starts the scheduler.
 * 
 */
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

    xLightDataSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xLightDataSemaphore); // Start with semaphore available
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

/**
 * @brief Computes an SMA of light readings every 500ms
 * Runs on CORE0, producer for the semaphore
 * 
 */
void vLightDetectorTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay500ms = pdMS_TO_TICKS(500);
    // Initialize light reading buffer with current reading from the pin
    static int lightReadings[SMA_WINDOW_SIZE] = {analogRead(LDR_PIN)};
    while (1) {
        lightValue = analogRead(LDR_PIN);
        shiftArray(lightReadings, lightValue);
        averageLight = calculateAverage(lightReadings);
        xSemaphoreGive(xLightDataSemaphore); // Signal LCD Task that new data is available
        vTaskDelay(xDelay500ms);
    }
}

/**
 * @brief Display the simple moving average and current light level data to the LCD 
 * Tied to core 0, consumer of the semaphore
 */
void vLCDTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay250ms = pdMS_TO_TICKS(250);

    int localLight = 0;
    float localSMA = 0.0;

    lcd.clear();

    while(1) {
        if (xSemaphoreTake(xLightDataSemaphore, portMAX_DELAY) == pdTRUE) {
            localLight = lightValue;
            localSMA = averageLight;

            lcd.setCursor(0, 0);
            lcd.print("Light: ");
            lcd.print(localLight);
            lcd.print("    "); // Clear trailing characters

            lcd.setCursor(0, 1);
            lcd.print("SMA:   ");
            lcd.print(localSMA, 1);
            lcd.print("    ");
        }
            vTaskDelay(xDelay250ms);
        }
}

/**
 * @brief Checks if the average light level is outside the defined thresholds and 
 * flashes an alarm LED if an anomaly is detected. Runs on CORE1
 * Alarm happens if the average light level is above HIGH_THRESH or below LOW_THRESH, 
 * flashing the LED 3 times with a delay in between to make it noticeable, then waits for 
 * 2 seconds before checking again to avoid rapid flashing. If the light level is normal, ensures the alarm LED is off.
 * 
 */
void vAnomalyAlarmTask(void *pvParameters) {
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xDelay250ms = pdMS_TO_TICKS(250); // Checks every display cycle timing
    while (1) {
        if (xSemaphoreTake(xLightDataSemaphore, portMAX_DELAY) == pdTRUE) {
            if (averageLight > HIGH_THRESH || averageLight < LOW_THRESH) {
                for (int i = 0; i < 3; i++) { // Flash alarm LED 3 times
                    digitalWrite(ALARM_LED_PIN, HIGH);
                    vTaskDelay(pdMS_TO_TICKS(166));
                    digitalWrite(ALARM_LED_PIN, LOW);
                    vTaskDelay(pdMS_TO_TICKS(166));
                }
                vTaskDelay(pdMS_TO_TICKS(2000)); // Wait before checking again to avoid rapid flashing
            } else {
                digitalWrite(ALARM_LED_PIN, LOW);
            }
        }
        vTaskDelay(xDelay250ms);
    }
}

/**
 * @brief Computes prime numbers up to 5000 in the background, printing them to Serial. 
 * Runs on CORE1 with lowest priority, yielding frequently to allow other tasks to run smoothly.
 * 
 */
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

/**
 * @brief Checks if a number is prime
 * 
 * @param n Number to check
 * @return true if prime
 * @return false if not prime
 */
bool isPrime(int n) {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

/**
 * @brief computes the SMA of the values in the array
 * 
 * @param arr pointer to array of light readings
 * @return int average of the values in the array
 */
int calculateAverage(int *arr) {
    int sum = 0;
    for (int i = 0; i < SMA_WINDOW_SIZE; i++) {
        sum += arr[i];
    }
    return sum / SMA_WINDOW_SIZE;
}

/**
 * @brief shifts a value into the end of the array, pushing out the top value
 * 
 * @param arr array to have shifted in
 * @param newValue value to shift into the top
 */
void shiftArray(int *arr, int newValue) {
    for (int i = 0; i < SMA_WINDOW_SIZE - 1; i++) {
        arr[i] = arr[i + 1];
    }
    arr[SMA_WINDOW_SIZE - 1] = newValue;
}