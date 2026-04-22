// MAKE SURE YOU PULL BEFORE YOU DO ANY WORK AND MAKE SURE YOU DON'T OVERWRITE
// ANYTHING

// RING LED SECTION 





// SENSOR SECTION 
#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>

SSD1306Wire lcd(0x3C, SDA, SCL);

#define IR_PIN A3

int threshold = 2000;   // adjust this!!
bool touched = false;

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.flipScreenVertically();
  lcd.setFont(ArialMT_Plain_16);
}

void loop() {
  int value = analogRead(IR_PIN);

  // continuous detection
  if (value > threshold) {
    touched = true;
  } else {
    touched = false;
  }

  // OLED display
  lcd.clear();
  lcd.drawString(0, 20, touched ? "NOT TOUCHED" : "TOUCHED");
  lcd.display();

  // debug
  Serial.print("Value: ");
  Serial.print(value);
  Serial.print(" | ");
  Serial.println(touched ? "TOUCHED" : "NOT");

  delay(100);
}


// DISPLAY SECTION
