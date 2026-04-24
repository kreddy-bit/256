// MAKE SURE YOU PULL BEFORE YOU DO ANY WORK AND MAKE SURE YOU DON'T OVERWRITE ANYTHING

#include <Arduino.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <Adafruit_NeoPixel.h>

// --- HARDWARE DEFINITIONS ---
#define LED_PIN 25
#define IR_PIN A3
#define BUZZER_PIN 17

// CHANGE THESE TO YOUR ACTUAL BUTTON PINS
#define BTN_PLAY_SELECT 0  
#define BTN_RIGHT 35       
#define BTN_LEFT 32        

Adafruit_NeoPixel ring = Adafruit_NeoPixel(16, LED_PIN, NEO_GRB + NEO_KHZ800);
SSD1306Wire lcd(0x3C, SDA, SCL);

// --- GAME STATES ---
enum GameState {
  MENU,
  PLAYING_NORMAL,
  PLAYING_PLACEHOLDER,
  HIGHSCORES
};

GameState currentState = MENU;

// --- MENU VARIABLES ---
int menuSelection = 0; 
const int NUM_MENU_ITEMS = 3;
String menuItems[NUM_MENU_ITEMS] = {"Normal Game", "Placeholder", "Highscores"};

// --- GAMEPLAY VARIABLES ---
int threshold = 2000;   
int currentScore = 0;
int curr_pixel = 0;
unsigned long lastLedUpdate = 0;
int gameSpeed = 150;    
bool jumpedThisRotation = false;
bool needsReset = false; 

// --- AUDIO VARIABLES ---
unsigned long lastBgmTime = 0;
bool bgmHighNote = false;

// --- HIGHSCORE VARIABLES ---
const int MAX_SCORES = 5; 
int highscores[MAX_SCORES] = {0, 0, 0, 0, 0};
int lastAchievedRank = -1; 

// --- BUTTON DEBOUNCING ---
bool selectPressed = false;
bool rightPressed = false;
bool leftPressed = false;

void updateButtons() {
  selectPressed = (digitalRead(BTN_PLAY_SELECT) == LOW);
  rightPressed = (digitalRead(BTN_RIGHT) == LOW);
  leftPressed = (digitalRead(BTN_LEFT) == LOW);
  if (selectPressed || rightPressed || leftPressed) delay(150); 
}

void setup() {
  Serial.begin(115200);

  // Init LEDs
  ring.begin();
  ring.setBrightness(32);
  ring.clear();
  ring.show();

  // Init OLED
  lcd.init();
  lcd.flipScreenVertically();
  lcd.setFont(ArialMT_Plain_16);

  // Init Buttons & Buzzer
  pinMode(BTN_PLAY_SELECT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
}

void saveScore(int score) {
  lastAchievedRank = -1;
  for (int i = 0; i < MAX_SCORES; i++) {
    if (score > highscores[i]) {
      for (int j = MAX_SCORES - 1; j > i; j--) {
        highscores[j] = highscores[j - 1];
      }
      highscores[i] = score;
      lastAchievedRank = i; 
      break;
    }
  }
}

// Helper function to handle the Game Over sequence
void triggerGameOver() {
  // Turn off LED ring
  ring.clear();
  ring.show();

  // Play losing arcade music (Descending tones)
  tone(BUZZER_PIN, 400, 200);
  delay(200);
  tone(BUZZER_PIN, 300, 200);
  delay(200);
  tone(BUZZER_PIN, 200, 400);
  delay(400);
  noTone(BUZZER_PIN); // Ensure buzzer shuts off

  saveScore(currentScore);
  currentState = HIGHSCORES;
}

void loop() {
  updateButtons();

  switch (currentState) {
    
    // ---------------------------------------------------------
    // MAIN MENU STATE
    // ---------------------------------------------------------
    case MENU:
      if (rightPressed) {
        menuSelection = (menuSelection + 1) % NUM_MENU_ITEMS;
      }
      if (leftPressed) {
        menuSelection = (menuSelection - 1 + NUM_MENU_ITEMS) % NUM_MENU_ITEMS;
      }
      
      if (selectPressed) {
        if (menuSelection == 0) {
          currentScore = 0;
          curr_pixel = 0;
          gameSpeed = 150;
          jumpedThisRotation = false;
          needsReset = false; 
          currentState = PLAYING_NORMAL;
        } else if (menuSelection == 1) {
          currentState = PLAYING_PLACEHOLDER;
        } else if (menuSelection == 2) {
          lastAchievedRank = -1; 
          currentState = HIGHSCORES;
        }
      }

      lcd.clear();
      for (int i = 0; i < NUM_MENU_ITEMS; i++) {
        if (i == menuSelection) {
          lcd.drawString(0, 8 + (i * 18), ">> " + menuItems[i]); 
        } else {
          lcd.drawString(0, 8 + (i * 18), "   " + menuItems[i]);
        }
      }
      lcd.display();
      
      ring.clear();
      ring.show();
      break;

    // ---------------------------------------------------------
    // PLAYING NORMAL STATE
    // ---------------------------------------------------------
    case PLAYING_NORMAL: { // <--- ADDED CURLY BRACE HERE
      lcd.clear();
      lcd.drawString(0, 0, "PLAYING!");
      lcd.drawString(0, 20, "Score: " + String(currentScore));
      lcd.display();

      // --- ANTI-HOLD IR LOGIC & JUMP SOUND ---
      bool isFingerUp = (analogRead(IR_PIN) > threshold);

      if (!isFingerUp) {
        needsReset = false; // Finger removed, ready to jump again
      } else if (!needsReset) {
        jumpedThisRotation = true; 
        needsReset = true; 
        // Play success jump blip
        tone(BUZZER_PIN, 1200, 50); 
      }

      // --- ARCADE BACKGROUND BEAT ---
      // Plays a simple alternating low tone that speeds up as the game speeds up
      if (millis() - lastBgmTime > (gameSpeed * 2.5)) {
        lastBgmTime = millis();
        if (!needsReset) { // Only play if we aren't currently playing the jump sound
          if (bgmHighNote) tone(BUZZER_PIN, 180, 40);
          else tone(BUZZER_PIN, 130, 40);
          bgmHighNote = !bgmHighNote;
        }
      }

      // --- LED RING MOVEMENT & GAME LOGIC ---
      if (millis() - lastLedUpdate > gameSpeed) {
        lastLedUpdate = millis();
        
        ring.setPixelColor(curr_pixel % 16, 0);
        curr_pixel++;
        int pos = curr_pixel % 16;
        
        ring.setPixelColor(pos, 255);
        ring.show();

        // 1. Halfway Check (Anti-Cheat)
        if (pos == 8) {
          if (needsReset) {
            // Finger was NOT removed before the halfway mark!
            triggerGameOver();
          } else {
            jumpedThisRotation = false; // Safe to jump next time
          }
        }

        // 2. Start Line Check (Scoring)
        if (pos == 0) { 
          if (jumpedThisRotation) {
            currentScore++;
            if (currentScore % 5 == 0 && gameSpeed > 30) {
              gameSpeed -= 15; // Speed up the game
            }
          } else {
            // Missed the jump!
            triggerGameOver();
          }
        }
      }
      break;
    } // <--- ADDED CURLY BRACE HERE

    // ---------------------------------------------------------
    // PLAYING PLACEHOLDER STATE
    // ---------------------------------------------------------
    case PLAYING_PLACEHOLDER:
      lcd.clear();
      lcd.drawString(0, 20, "PLACEHOLDER");
      lcd.drawString(0, 40, "Press Select...");
      lcd.display();
      
      if (selectPressed) {
        currentState = MENU;
      }
      break;

    // ---------------------------------------------------------
    // HIGHSCORES STATE
    // ---------------------------------------------------------
    case HIGHSCORES:
      lcd.clear();
      lcd.drawString(0, 0, "- HIGH SCORES -");
      
      for (int i = 0; i < MAX_SCORES; i++) {
        String scoreLine = String(i + 1) + ". " + String(highscores[i]);
        
        if (i == lastAchievedRank) {
          lcd.drawString(0, 15 + (i * 10), ">> " + scoreLine + " <<");
        } else {
          lcd.drawString(0, 15 + (i * 10), "   " + scoreLine);
        }
      }
      
      lcd.display();
      
      if (selectPressed || rightPressed || leftPressed) {
        currentState = MENU;
        delay(200); 
      }
      break;
  }
}
