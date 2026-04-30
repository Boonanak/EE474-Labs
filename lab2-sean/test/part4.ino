#include <Arduino.h>
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_reg.h"

#define BUZZER_PIN    5   
#define PHOTO_PIN     1   

#define LEDC_CHAN     0    
#define LEDC_RES      8    
#define DUTY_CYCLE    128  
#define DARK_THRESHOLD 1500 

void setup() {

  Serial.begin(115200); // AI outputted 9600 here first which worked despite .ini specifying 115200
  
  // Wait up to 5 seconds for Serial to connect, but move on if it doesn't
  uint32_t startWait = millis();
  while (!Serial && (millis() - startWait < 5000));

  // --- Manual Register Setup ---
  // GPIO mode
  PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[BUZZER_PIN], PIN_FUNC_GPIO);
  // Enable pin 5 in the gpio reg
  *((volatile uint32_t *)GPIO_ENABLE_REG) |= (1UL << BUZZER_PIN);

  // --- LEDC Configuration ---
  // Parameters: The channel we are picking, frequency, resolution (8-bit)
  ledcSetup(LEDC_CHAN, 1000, LEDC_RES);
  // Parameters: The physical gpio we are connecting to, the channel we are connecting to it
  ledcAttachPin(BUZZER_PIN, LEDC_CHAN);
  
  Serial.println("\n--- BOOT CHECK ---");
  
  // BUZZER TEST: Force a beep on startup to prove wiring works
  Serial.println("Testing Buzzer...");
  ledcWrite(LEDC_CHAN, DUTY_CYCLE);
  delay(500);
  ledcWrite(LEDC_CHAN, 0);
  Serial.println("Buzzer test complete.");
}

void loop() {
  
  int lightLevel = analogRead(PHOTO_PIN);
  Serial.print("Raw Light Value: ");
  Serial.println(lightLevel);

  if (lightLevel < DARK_THRESHOLD) {
    Serial.println("ALARM: DARKNESS DETECTED");
    
    int tones[] = {500, 1000, 2000};
    for(int i = 0; i < 3; i++) {
      ledcWriteTone(LEDC_CHAN, tones[i]);
      ledcWrite(LEDC_CHAN, DUTY_CYCLE);
      delay(250);
    }
    
    ledcWrite(LEDC_CHAN, 0);
    
    // Wait for light to return
    while(analogRead(PHOTO_PIN) < DARK_THRESHOLD + 100) {
      delay(100);
    }
  }

  delay(500); 
}