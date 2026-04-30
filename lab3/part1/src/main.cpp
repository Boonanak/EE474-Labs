#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define LCD_ADDRESS 0x27
#define SEND_CHAR_CONTROL 0b1101
#define SEND_LATCH_PULSE 0b1001

LiquidCrystal_I2C lcd(LCD_ADDRESS, 16, 2); // Initialize the LCD

void writeNibbleToLCD(uint8_t nibble) {
  Wire.write((nibble << 4) | SEND_CHAR_CONTROL); // Send the nibble with control bits
  Wire.write((nibble << 4) | SEND_LATCH_PULSE);
}

void writeCharacterToLCD(char character) {
  uint8_t topNibble = (character >> 4);
  uint8_t bottomNibble = (character & 0x0F);
  Wire.beginTransmission(LCD_ADDRESS);
  writeNibbleToLCD(topNibble); // Send top nibble
  writeNibbleToLCD(bottomNibble); // Send bottom nibble
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  lcd.begin(); // Initialize the LCD
  delay(2);
  Serial.setTimeout(10000000); 
}


void loop() {
  String inputString = Serial.readStringUntil('\n'); // Read input from Serial until newline
  for (size_t i = 0; i < inputString.length(); i++) {
    if (inputString.charAt(i) == '\r' || inputString.charAt(i) == '\n') {
      continue; // Skip newline characters
    }
    writeCharacterToLCD(inputString.charAt(i)); // Write each character to the LCD (except the newline)
  }
}
