/*
Filename: main.cpp
Author: Sean Bubernak
Date: 4/8/26
Description: This program is a little more complex than the previous two as it involves
controlling the blinking rate of two different LEDs at the same time. The idea behind this part is
that there are two LEDs, the first blinks at 10HZ and the second at 100HZ in their default
behavior. However, when the button dperessed they flip, so the one previously blinking slow, now
blinks fast and vice-versa for the other.
AI Usage: Gemini 3 0 5
*/

// -------------------------------------------- INCLUDES -----------------------------------------
#include <Arduino.h>

// -------------------------------------------- GLOBAL VARIABLES ---------------------------------
const int LED1_PIN = 43;
const int LED2_PIN = 44;
const int BUTTON_PIN = 1;

const long FREQ_10HZ_MS = 50;
const long FREQ_100HZ_MS = 5;

struct Blinker; // Forward declare the struct so the function knows it exists

// -------------------------------------------- FUNCTION PROTOTYPES ------------------------------
void updateBlinker(Blinker &b);

struct Blinker {
  int pin;
  long interval;
  unsigned long previousMillis;
  bool state;
};

// Initialize with default rates
Blinker led1 = {LED1_PIN, FREQ_10HZ_MS, 0, LOW}; 
Blinker led2 = {LED2_PIN, FREQ_100HZ_MS, 0, LOW};

/*
Inputs: NA
Returns: NA
Description: Initializing the pins that control the LEDs and the pin that reads from the pushbutton
*/
void setup() {
  pinMode(led1.pin, OUTPUT);
  pinMode(led2.pin, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // How we get around backwards functionality of the pushbutton
}

/*
Inputs: NA
Returns: NA
Description:
*/
/*
As described in the spec, if the button is pressed, swap the speeds at which the LEDs blink and if
not, keep the LEDs blinking at their original rates. We achieve this by using an event loop where
we are constantly checking the checking the state of the button and always updating the leds based
on the value from the button.
*/
void loop() {
  // Check button state
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);

  if (buttonPressed) {
    // Swap the intervals when held
    led1.interval = FREQ_100HZ_MS;
    led2.interval = FREQ_10HZ_MS;
  } else {
    // Revert to original rates when released
    led1.interval = FREQ_10HZ_MS;
    led2.interval = FREQ_100HZ_MS;
  }

  // These run constantly, regardless of the button state
  updateBlinker(led1);
  updateBlinker(led2);
}

/*
Inputs: NA
Returns: NA
Description: This function is passed the particular LED's data via reference. This is an important
distinction because we are not making a copy of the LED info, we are reaching out and modify
current time and the last time that the particular LED changed state, if that difference is
greater than the time we want to wait, 10Hz or 100Hz blinking, then we update it.  
*/  
void updateBlinker(Blinker &b) {
  unsigned long currentMillis = millis();
  if (currentMillis - b.previousMillis >= b.interval) {
    b.previousMillis = currentMillis;
    b.state = !b.state;
    digitalWrite(b.pin, b.state);
  }
}