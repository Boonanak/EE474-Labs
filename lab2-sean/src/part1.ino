#include <Arduino.h>
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_periph.h"

#define GPIO_PIN 5 // GPIO5
#define REPETITIONS 1000 // For part 2

void setup() {
  Serial.begin(115200);

 // This predefined macro sets the functionality of a specified pin (to UART, SPI, 
 // GPIO, etc). Here we are setting pin 5 to be a general purpose input/output 
 // pin.
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[GPIO_PIN], PIN_FUNC_GPIO);
 
 // Write code here (1-3 lines) to mark pin 5 as output
 // using the GPIO_ENABLE_REG macro
  *((volatile uint32_t *)GPIO_ENABLE_REG) |= (uint32_t)(1 << GPIO_PIN);
}

// Step 1
// void loop() {
//  // ===============> TODO:
//  // Turn LED on (set corresponding bit in GPIO output register)
//  // Write code here (1-3 lines) to mark pin 5 as HIGH output
//  // using the GPIO_OUT_REG macro
//   *((volatile uint32_t *)GPIO_OUT_REG) |= (uint32_t)(1 << GPIO_PIN);

//  // Wait for 1 second
//   delay(1000);


//  // ===============> TODO:
//  // Turn LED off (clear corresponding bit in GPIO output register)
//  // Write code here (1-3 lines) to mark pin 5 as LOW output
//  // using the GPIO_OUT_REG macro
//   *((volatile uint32_t *)GPIO_OUT_REG) &= ~((uint32_t)(1 << GPIO_PIN));


//  // Wait for 1 second
//   delay(1000);

// }

void loop() {
  uint64_t startTime, endTime;

  // 1. Benchmark Standard Arduino digitalWrite
  startTime = esp_timer_get_time();
  for (int i = 0; i < REPETITIONS; i++) {
    digitalWrite(GPIO_PIN, HIGH);
    digitalWrite(GPIO_PIN, LOW);
  }
  endTime = esp_timer_get_time();
  int digitalTime = (int)(endTime - startTime);

  // 2. Benchmark Direct Memory Access (GPIO_OUT_REG)
  startTime = esp_timer_get_time();
  for (int i = 0; i < REPETITIONS; i++) {
    // Set HIGH
    *((volatile uint32_t *)GPIO_OUT_REG) |= (uint32_t)(1 << GPIO_PIN);
    // Set LOW
    *((volatile uint32_t *)GPIO_OUT_REG) &= ~((uint32_t)(1 << GPIO_PIN));
  }
  endTime = esp_timer_get_time();
  int manualTime = (int)(endTime - startTime);

  // Output Results
  Serial.println("Results for 1000 repetitions:");
  Serial.printf("  digitalWrite: %d us\n", digitalTime);
  Serial.printf("  Manual Access: %d us\n", manualTime);
  
  if (manualTime > 0) {
    Serial.printf("  Speedup Factor: %fx faster\n", (float)digitalTime / manualTime);
  }
  
  Serial.println("---------------------------------------");

  // Wait 5 seconds before running the test again
  delay(5000);
}