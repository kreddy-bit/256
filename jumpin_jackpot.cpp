#include <Arduino.h>
#include <random>
#include <Wire.h>
#include <SSD1306Wire.h>
#include <Adafruit_NeoPixel.h>

// Hardware connections
#define LED_PIN 25
#define IR_PIN A3
#define BUZZER_PIN 17

// Button connections
#define BTN_PLAY_SELECT 0  
#define BTN_RIGHT 35       
#define BTN_LEFT 34        

Adafruit_NeoPixel ring = Adafruit_NeoPixel(16, LED_PIN, NEO_GRB + NEO_KHZ800);
SSD1306Wire lcd(0x3C, SDA, SCL);

// The different screens the game can show
enum GameState {
  MENU,
  WAITING_FOR_START,
  PLAYING_NORMAL,
  PLAYING_INVISIBLE, 
  PLAYING_RANDOM, 
  GAME_OVER,
  HIGHSCORES,
  WAITING_RANDOM
};

GameState currentState = MENU;

// Menu setup 
int menuSelection = 0; 
const int NUM_MENU_ITEMS = 5;
String menuItems[NUM_MENU_ITEMS] = {
  "Classic Game", 
  "Endless Mode", 
  "Invisible Mode", 
  "Random", 
  "Highscores"
};

// Active game variables
int threshold = 2000;   
int currentScore = 0;
int curr_pixel = 0;
unsigned long lastLedUpdate = 0;
int gameSpeed = 150;    
bool jumpedThisRotation = false;
bool needsReset = false; 

// Mode and win tracking
int activeMode = 0; // 0 = Classic, 1 = Endless, 2 = Invisible
bool isEndless = false;
bool hasWon = false;
bool isNewHighscore = false; 

// Audio timing
unsigned long lastBgmTime = 0;
bool bgmHighNote = false;

// Highscore tracking
const int MAX_SCORES = 25; 
int highscores[MAX_SCORES] = {0}; 
String highscoreNames[MAX_SCORES]; 
int lastAchievedRank = -1; 
int highscoreScrollIndex = 0; 

// Pool of random retro gamer tags
const int NUM_RANDOM_NAMES = 15;
String randomNames[NUM_RANDOM_NAMES] = {
  "Viper", "Ghost", "Ninja", "Nova", "Pulse", 
  "Echo", "Hawk", "Wolf", "Neon", "Jinx", 
  "Zane", "Axel", "Kael", "Nyx", "Rex"
};

// Button state variables
bool selectPressed = false;
bool rightPressed = false;
bool leftPressed = false;

// Memory variables to track button states
bool lastSelectState = HIGH;
bool lastRightState = HIGH;
bool lastLeftState = HIGH;

// Checks the buttons using State Change Detection
void updateButtons() {
  bool currentSelectRaw = digitalRead(BTN_PLAY_SELECT);
  bool currentRightRaw = digitalRead(BTN_RIGHT);
  bool currentLeftRaw = digitalRead(BTN_LEFT);

  selectPressed = (currentSelectRaw == LOW && lastSelectState == HIGH);
  rightPressed = (currentRightRaw == LOW && lastRightState == HIGH);
  leftPressed = (currentLeftRaw == LOW && lastLeftState == HIGH);

  lastSelectState = currentSelectRaw;
  lastRightState = currentRightRaw;
  lastLeftState = currentLeftRaw;
  
  // Don't delay the game logic if we are actively playing
  if (currentState != PLAYING_NORMAL && currentState != PLAYING_INVISIBLE && (selectPressed || rightPressed || leftPressed)) {
      delay(50); 
  }
}

// Helper function to draw the idle spinning lights on menus
void drawDefaultLights() {
  static unsigned long defaultTimer = 0;
  static int offset = 0;
  
  if (millis() - defaultTimer > 150) { 
    defaultTimer = millis();
    offset++;
    
    for (int i = 0; i < 16; i++) {
      int colorType = (i + offset) % 3;
      if (colorType == 0) ring.setPixelColor(i, 255, 0, 0);         // Red
      else if (colorType == 1) ring.setPixelColor(i, 255, 100, 0);  // Orange
      else ring.setPixelColor(i, 255, 255, 0);                      // Yellow
    }
    ring.show();
  }
}

// Runs once when the board turns on
void setup() {
  Serial.begin(115200);

  ring.begin();
  ring.setBrightness(32);
  ring.clear();
  ring.show();

  lcd.init();
  lcd.flipScreenVertically();
  lcd.setFont(ArialMT_Plain_16);

  pinMode(BTN_PLAY_SELECT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // Fill the initial empty leaderboard with placeholder names
  for (int i = 0; i < MAX_SCORES; i++) {
    highscoreNames[i] = "---";
  }

  // Seed the random number generator
  randomSeed(analogRead(14)); 
}

// Drops a new score into the leaderboard, grabs a random name, and shifts the rest down
void saveScore(int score) {
  lastAchievedRank = -1;
  for (int i = 0; i < MAX_SCORES; i++) {
    if (score > highscores[i]) {
      for (int j = MAX_SCORES - 1; j > i; j--) {
        highscores[j] = highscores[j - 1];
        highscoreNames[j] = highscoreNames[j - 1]; 
      }
      highscores[i] = score;
      highscoreNames[i] = randomNames[random(0, NUM_RANDOM_NAMES)]; 
      lastAchievedRank = i; 
      break;
    }
  }
}

// Plays sounds and figures out which screen to show when a game ends
void triggerGameOver(bool playerWon) {
  ring.clear();
  ring.show();

  if (!isEndless) { 
    // --- CLASSIC / INVISIBLE MODE LOGIC ---
    if (playerWon) {
      hasWon = true;
      isNewHighscore = false;
      tone(BUZZER_PIN, 523, 100); delay(100);
      tone(BUZZER_PIN, 659, 100); delay(100);
      tone(BUZZER_PIN, 784, 100); delay(100);
      tone(BUZZER_PIN, 1046, 300); delay(300);
      noTone(BUZZER_PIN);
    } else {
      hasWon = false;
      isNewHighscore = false;
      tone(BUZZER_PIN, 400, 200); delay(200);
      tone(BUZZER_PIN, 300, 200); delay(200);
      tone(BUZZER_PIN, 200, 400); delay(400);
      noTone(BUZZER_PIN); 
    }
    currentState = GAME_OVER;

  } else {
    // --- ENDLESS MODE LOGIC ---
    saveScore(currentScore);
    
    highscoreScrollIndex = lastAchievedRank - 2;
    if (highscoreScrollIndex < 0) highscoreScrollIndex = 0;
    if (highscoreScrollIndex > MAX_SCORES - 5) highscoreScrollIndex = MAX_SCORES - 5;

    if (lastAchievedRank == 0 && currentScore > 0) {
      isNewHighscore = true;
      hasWon = false; 
      tone(BUZZER_PIN, 523, 100); delay(100);
      tone(BUZZER_PIN, 659, 100); delay(100);
      tone(BUZZER_PIN, 784, 100); delay(100);
      tone(BUZZER_PIN, 1046, 300); delay(300);
      noTone(BUZZER_PIN);
      currentState = GAME_OVER; 
    } else {
      tone(BUZZER_PIN, 400, 200); delay(200);
      tone(BUZZER_PIN, 300, 200); delay(200);
      tone(BUZZER_PIN, 200, 400); delay(400);
      noTone(BUZZER_PIN); 
      currentState = HIGHSCORES;
    }
  }
}

// The main brain of the game that runs continuously
void loop() {
  updateButtons();

  switch (currentState) {
    
    // --- MAIN MENU ---
    case MENU: { 
      if (rightPressed) {
        menuSelection = (menuSelection + 1) % NUM_MENU_ITEMS;
      }
      if (leftPressed) {
        menuSelection = (menuSelection - 1 + NUM_MENU_ITEMS) % NUM_MENU_ITEMS;
      }
      
      if (selectPressed) {
        if (menuSelection == 0) { // Classic
          activeMode = 0;
          isEndless = false;
          ring.clear(); ring.show();
          currentState = WAITING_FOR_START;
        } else if (menuSelection == 1) { // Endless
          activeMode = 1;
          isEndless = true;
          ring.clear(); ring.show();
          currentState = WAITING_FOR_START;
        } else if (menuSelection == 2) { // Invisible
          activeMode = 2;
          isEndless = false; // Invisible uses a 10-point win condition, not endless
          ring.clear(); ring.show();
          currentState = WAITING_FOR_START;
        } else if (menuSelection == 3) { // random
          currentState = WAITING_RANDOM;
        } else if (menuSelection == 4) { // Highscores
          lastAchievedRank = -1; 
          currentState = HIGHSCORES;
        }
      }

      // Main Menu Scrolling Logic
      lcd.clear();
      
      int menuScrollView = menuSelection - 1; 
      if (menuScrollView < 0) menuScrollView = 0;
      if (menuScrollView > NUM_MENU_ITEMS - 3) menuScrollView = NUM_MENU_ITEMS - 3;

      for (int i = 0; i < 3; i++) {
        int itemIdx = menuScrollView + i;
        if (itemIdx >= NUM_MENU_ITEMS) break;

        if (itemIdx == menuSelection) {
          lcd.drawString(0, 8 + (i * 18), ">> " + menuItems[itemIdx]); 
        } else {
          lcd.drawString(0, 8 + (i * 18), "   " + menuItems[itemIdx]);
        }
      }
      lcd.display();
      drawDefaultLights();
      break;
    } 

    // --- WAITING FOR SENSOR START ---
    case WAITING_FOR_START:
      lcd.clear();
      lcd.setTextAlignment(TEXT_ALIGN_CENTER);
      lcd.drawString(64, 15, "Press sensor");
      lcd.drawString(64, 35, "to start!");
      lcd.display();
      lcd.setTextAlignment(TEXT_ALIGN_LEFT);

      ring.clear();
      ring.setPixelColor(0, 255, 255, 0);
      ring.show();

      if (analogRead(IR_PIN) <= threshold) {
        currentScore = 0;
        curr_pixel = 0;
        
        // Invisible mode starts at a fast, fixed speed of 80ms!
        gameSpeed = (activeMode == 2) ? 80 : 150; 
        
        jumpedThisRotation = false;
        needsReset = false; 
        
        // Route to the correct mode based on menu selection
        if (activeMode == 2) currentState = PLAYING_INVISIBLE;
        else currentState = PLAYING_NORMAL;
      }
      break;
    case WAITING_RANDOM:
      lcd.clear();
      lcd.setTextAlignment(TEXT_ALIGN_CENTER);
      lcd.drawString(64, 15, "Press sensor");
      lcd.drawString(64, 35, "to start!");
      lcd.display();
      lcd.setTextAlignment(TEXT_ALIGN_LEFT);

      ring.clear();
      ring.setPixelColor(0, 255, 255, 0);
      ring.show();

      if (analogRead(IR_PIN) <= threshold) {
        currentScore = 0;
        curr_pixel = 0;
        
        // Invisible mode starts at a fast, fixed speed of 80ms!
        gameSpeed = 150; 
        
        jumpedThisRotation = false;
        needsReset = false; 
        
        // Route to the correct mode based on menu selection
        if (activeMode == 3) currentState = PLAYING_RANDOM;
        else currentState = PLAYING_RANDOM;
      }
      break;
    // --- NORMAL / ENDLESS GAMEPLAY ---
    case PLAYING_NORMAL: {
      lcd.clear();
      if (isEndless) lcd.drawString(0, 0, "ENDLESS");
      else lcd.drawString(0, 0, "CLASSIC");
      lcd.drawString(0, 20, "Score: " + String(currentScore));
      lcd.display();

      bool isFingerUp = (analogRead(IR_PIN) > threshold);

      if (!isFingerUp) {
        needsReset = false; 
      } else if (!needsReset) {
        jumpedThisRotation = true; 
        needsReset = true; 
        tone(BUZZER_PIN, 1200, 50); 
      }

      if (millis() - lastBgmTime > (gameSpeed * 2.5)) {
        lastBgmTime = millis();
        if (!needsReset) { 
          if (bgmHighNote) tone(BUZZER_PIN, 180, 40);
          else tone(BUZZER_PIN, 130, 40);
          bgmHighNote = !bgmHighNote;
        }
      }

      if (millis() - lastLedUpdate > gameSpeed) {
        lastLedUpdate = millis();
        
        ring.setPixelColor(curr_pixel % 16, 0); 
        curr_pixel++;
        int pos = curr_pixel % 16;
        
        ring.setPixelColor(0, 255, 255, 0); 
        ring.setPixelColor(pos, 255); 
        ring.show();

        if (pos > 4 && pos < 12) {
          if (needsReset) {
            triggerGameOver(false);
          } else {
            jumpedThisRotation = false; 
          }
        }

        if (pos == 0) { 
          if (jumpedThisRotation) {
            currentScore++;
            if (gameSpeed > 30) {
              gameSpeed -= 5; 
            }
            
            if (!isEndless && currentScore >= 25) {
              triggerGameOver(true);
            }
          } else {
            triggerGameOver(false);
          }
        }
      }
      break;
    } 

    // --- INVISIBLE GAMEPLAY ---
    case PLAYING_INVISIBLE: {
      lcd.clear();
      lcd.drawString(0, 0, "INVISIBLE");
      lcd.drawString(0, 20, "Score: " + String(currentScore));
      lcd.display();

      bool isFingerUp = (analogRead(IR_PIN) > threshold);

      if (!isFingerUp) {
        needsReset = false; 
      } else if (!needsReset) {
        jumpedThisRotation = true; 
        needsReset = true; 
        tone(BUZZER_PIN, 1200, 50); 
      }

      // The background beat is crucial here so the player can keep rhythm!
      if (millis() - lastBgmTime > (gameSpeed * 2.5)) {
        lastBgmTime = millis();
        if (!needsReset) { 
          if (bgmHighNote) tone(BUZZER_PIN, 180, 40);
          else tone(BUZZER_PIN, 130, 40);
          bgmHighNote = !bgmHighNote;
        }
      }

      if (millis() - lastLedUpdate > gameSpeed) {
        lastLedUpdate = millis();
        
        ring.setPixelColor(curr_pixel % 16, 0); 
        curr_pixel++;
        int pos = curr_pixel % 16;
        
        // Keep the jump marker lit so they know where to aim
        ring.setPixelColor(0, 255, 255, 0); 
        
        // --- NEW LOGIC: Flash the blue light at the top (pixel 8) ---
        if (currentScore < 3 || pos == 8) {
          ring.setPixelColor(pos, 255); 
        }
        
        ring.show();

        if (pos > 4 && pos < 12) {
          if (needsReset) {
            triggerGameOver(false);
          } else {
            jumpedThisRotation = false; 
          }
        }

        if (pos == 0) { 
          if (jumpedThisRotation) {
            currentScore++;
            
            // Win condition for Invisible Mode is 10 points
            if (currentScore >= 10) {
              triggerGameOver(true);
            }
          } else {
            triggerGameOver(false);
          }
        }
      }
      break;
    }

    // --- PLACEHOLDER STATES ---
    case PLAYING_RANDOM: {
      lcd.clear();
      if (isEndless) lcd.drawString(0, 0, "ENDLESS");   
      else lcd.drawString(0, 0, "RANDOM");
      lcd.drawString(0, 20, "Score: " + String(currentScore));
      lcd.display();

      bool isFingerUp = (analogRead(IR_PIN) > threshold);

      if (!isFingerUp) {
        needsReset = false; 
      } else if (!needsReset) {
        jumpedThisRotation = true; 
        needsReset = true; 
        tone(BUZZER_PIN, 1200, 50); 
      }

      if (millis() - lastBgmTime > (gameSpeed * 2.5)) {
        lastBgmTime = millis();
        if (!needsReset) { 
          if (bgmHighNote) tone(BUZZER_PIN, 180, 40);
          else tone(BUZZER_PIN, 130, 40);
          bgmHighNote = !bgmHighNote;
        }
      }

      if (millis() - lastLedUpdate > gameSpeed) {
        lastLedUpdate = millis();
        
        ring.setPixelColor(curr_pixel % 16, 0); 
        curr_pixel++;
        int pos = curr_pixel % 16;
        
        ring.setPixelColor(0, 255, 255, 0); 
        ring.setPixelColor(pos, 255); 
        ring.show();

        if (pos > 4 && pos < 12) {
          if (needsReset) {
            triggerGameOver(false);
          } else {
            jumpedThisRotation = false; 
          }
        }

        if (pos == 0) { 
          if (jumpedThisRotation) {
            currentScore++;
            gameSpeed = random(30,131);
            
            if (!isEndless && currentScore >= 15) {
              triggerGameOver(true);
            }
          } else {
            triggerGameOver(false);
          }
        }
      }
      break;
    } 
    // --- WIN / LOSE SCREEN ---
    case GAME_OVER: {
      lcd.clear();
      lcd.setTextAlignment(TEXT_ALIGN_CENTER);
      
      if (hasWon) {
        lcd.drawString(64, 10, "YOU WIN!");
      } else if (isNewHighscore) {
        lcd.drawString(64, 10, "NEW HIGHSCORE!");
      } else {
        lcd.drawString(64, 10, "GAME OVER");
      }
      
      lcd.drawString(64, 35, "Score: " + String(currentScore));
      lcd.display();
      lcd.setTextAlignment(TEXT_ALIGN_LEFT);

      if (hasWon || isNewHighscore) {
        static unsigned long winAnimTimer = 0;
        static int winAnimOffset = 0;
        
        if (millis() - winAnimTimer > 100) {
          winAnimTimer = millis();
          winAnimOffset++;
          for (int i = 0; i < 16; i++) {
            if ((i + winAnimOffset) % 2 == 0) {
              ring.setPixelColor(i, 255, 100, 0); 
            } else {
              ring.setPixelColor(i, 255, 255, 0); 
            }
          }
          ring.show();
        }
      } else {
        static unsigned long flashTimer = 0;
        static bool flashState = false;
        
        if (millis() - flashTimer > 250) {
          flashTimer = millis();
          flashState = !flashState;
          
          if (flashState) {
            for(int i = 0; i < 16; i++) ring.setPixelColor(i, 255, 0, 0);
          } else {
            ring.clear();
          }
          ring.show();
        }
      }

      if (selectPressed || leftPressed || rightPressed) {
        ring.clear(); 
        ring.show();
        
        if (isEndless) currentState = HIGHSCORES; 
        else currentState = MENU;
        
        delay(200);
      }
      break;
    }

    // --- LEADERBOARD SCREEN ---
    case HIGHSCORES: {
      lcd.clear();
      lcd.setTextAlignment(TEXT_ALIGN_CENTER);
      
      for (int i = 0; i < 5; i++) {
        int scoreIdx = highscoreScrollIndex + i;
        if (scoreIdx >= MAX_SCORES) break;

        String scoreLine = String(scoreIdx + 1) + ". " + highscoreNames[scoreIdx] + ": " + String(highscores[scoreIdx]);
        
        if (scoreIdx == lastAchievedRank) {
          lcd.drawString(64, i * 12, ">> " + scoreLine + " <<");
        } else {
          lcd.drawString(64, i * 12, scoreLine);
        }
      }
      
      lcd.display();
      lcd.setTextAlignment(TEXT_ALIGN_LEFT);
      drawDefaultLights();
      
      if (leftPressed && highscoreScrollIndex > 0) {
        highscoreScrollIndex--;
      }
      if (rightPressed && highscoreScrollIndex < MAX_SCORES - 5) {
        highscoreScrollIndex++;
      }
      
      if (selectPressed) {
        ring.clear(); 
        ring.show();
        currentState = MENU;
        delay(200); 
      }
      break;
    }
  }
}
