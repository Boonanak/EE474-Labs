/*
Filename: main.cpp
Author: Sean Bubernak
Date: 4/8/26
Description: This program controls two LEDs, it blinks one three times and then shifts to shift
the other three times, this cycle conintues to oscillate
AI Usage: Gemini 0 0 0 (writing testing debugging)
*/

// -------------------------------------------- INCLUDES -----------------------------------------
#include <Arduino.h>

// -------------------------------------------- GLOBAL VARIABLES ---------------------------------
const int ledBPin = 43;
const int ledGPin = 44;

// -------------------------------------------- FUNCTION PROTOTYPES ------------------------------
void blink(int ledPin);

/*
Inputs: NA
Returns: NA
Description: Initializing the pin that controls the blue LED and the pin that controls the green
LED as outputs.
*/
void setup() {
  // The pin that connects to the LED is an output
  pinMode(ledBPin, OUTPUT);
  pinMode(ledGPin, OUTPUT);
}

/*
Inputs: NA
Returns: NA
Description: Looping code that continuously calls the blink function for the blue and then for the
green LED
*/
void loop() {
  // Call once for pin controlling blue pin and once for green
  blink(ledBPin);
  blink(ledGPin);
}

/*
Inputs: 
  int ledPin: The pin that the LED that we wish to blink is connected to 
Returns: NA
Description:Takes in a pin that controls an LED to blink and blinks said LED three times for 0.5
sec at a time 
*/
void blink(int ledPin) {
  // For visibility blink it 3 times
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPin, HIGH);
    delay(500);
    digitalWrite(ledPin, LOW);
    delay(500);
  }
}