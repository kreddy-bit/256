// MAKE SURE YOU PULL BEFORE YOU DO ANY WORK AND MAKE SURE YOU DON'T OVERWRITE
// ANYTHING

// RING LED SECTION 
#include <Adafruit_NeoPixel.h>

#define PIN 25


Adafruit_NeoPixel ring = Adafruit_NeoPixel(16, PIN, NEO_GRB + NEO_KHZ800); // 16 LEDs

void setup() {
  ring.begin();
  ring.setBrightness(32); // set a low brightness
  ring.clear(); // clear all pixels
  ring.show();
}

int curr_pixel = 1;
int t = 150;
int count = 0;
void loop() {
  if(t > 50){
    ring.setPixelColor((curr_pixel-1)%16,0);
    ring.setPixelColor((curr_pixel)%16,255);
    ring.show();
    curr_pixel++;
    delay(t);
    count++;
    if(count % 16 == 0){
      t -= 10;
    }
  }
  else{
    for(int i = 0; i < 16; i++){
      ring.setPixelColor(i,255);
    }
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return ring.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return ring.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return ring.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}



// SENSOR SECTION 



// DISPLAY SECTION