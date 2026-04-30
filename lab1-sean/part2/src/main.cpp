/*
Filename: main.cpp
Author: Sean Bubernak
Date: 4/8/26
Description: This program uses a button to control the blinking rate of a single LED. When the
button is not pressed, the LED blinks slowly, and when it is pressed (and held) it blinks fast. 
AI Usage: Gemini 0 0 0
*/


// -------------------------------------------- INCLUDES -----------------------------------------
#include <Arduino.h>

// -------------------------------------------- GLOBAL VARIABLES ---------------------------------
const int ledOPin = 43;
const int pushIPin = 44;

// -------------------------------------------- FUNCTION PROTOTYPES ------------------------------
void blink(int delayTime);

/*
Inputs: NA
Returns: NA
Description: Initializing the pin that controls LED and the pin that reads from the pushbutton.
*/
void setup() {
  // Set these pins up as outputs and inputs respectively
  pinMode(ledOPin, OUTPUT);
  pinMode(pushIPin, INPUT);
}

/*
Inputs: NA
Returns: NA
Description: Continously loop and check if the button is pressed, if it is, blink 10 times per
second, if not blink once per second.
*/
void loop() {
  // NOTE: PRESSED RESULTS IN A LOW SIGNAL SO THIS IS REVERSED
  if (digitalRead(pushIPin)) {
    blink(1000);
  } else {
    blink(100);
  }
}

/*
Inputs: NA
Returns: NA
Description:A function that simply blinks the LED with the delay passed in
*/
void blink(int delayTime) {
    digitalWrite(ledOPin, HIGH);
    delay(delayTime);
    digitalWrite(ledOPin, LOW);
    delay(delayTime);
}