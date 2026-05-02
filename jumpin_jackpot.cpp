// MAKE SURE YOU PULL BEFORE YOU DO ANY WORK AND MAKE SURE YOU DON'T OVERWRITE ANYTHING

#include <Arduino.h>
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
#define BTN_LEFT 32        

Adafruit_NeoPixel ring = Adafruit_NeoPixel(16, LED_PIN, NEO_GRB + NEO_KHZ800);
SSD1306Wire lcd(0x3C, SDA, SCL);

// The different screens the game can show
enum GameState {
  MENU,
  WAITING_FOR_START,
  PLAYING_NORMAL,
  GAME_OVER,
  HIGHSCORES,
  INVISIBLE_MODE,
  WAITING_FOR_INVISIBLE
};

GameState currentState = MENU;

// Menu setup
int menuSelection = 0; 
const int NUM_MENU_ITEMS = 4;
String menuItems[NUM_MENU_ITEMS] = {"Classic Game", "Endless Mode", "Highscores", "Invisible Mode"};

// Active game variables
int threshold = 2000;   
int currentScore = 0;
int curr_pixel = 0;
unsigned long lastLedUpdate = 0;
int gameSpeed = 150;    
bool jumpedThisRotation = false;
bool needsReset = false; 

// Mode and win tracking
bool isEndless = false;
bool hasWon = false;

// Audio timing
unsigned long lastBgmTime = 0;
bool bgmHighNote = false;

// Highscore tracking
const int MAX_SCORES = 10; 
int highscores[MAX_SCORES] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int lastAchievedRank = -1; 
int highscoreScrollIndex = 0; // Tracks where we are looking in the list

// Button reading
bool selectPressed = false;
bool rightPressed = false;
bool leftPressed = false;

// Checks the buttons and adds a tiny delay so the menu doesn't scroll too fast
void updateButtons() {
  selectPressed = (digitalRead(BTN_PLAY_SELECT) == LOW);
  rightPressed = (digitalRead(BTN_RIGHT) == LOW);
  leftPressed = (digitalRead(BTN_LEFT) == LOW);
  if (currentState != PLAYING_NORMAL && (selectPressed || rightPressed || leftPressed)) {
      delay(150); 
  }
}

// Runs once when the board turns on
void setup() {
  Serial.begin(115200);

  // Start the LED ring and turn off any stray lights
  ring.begin();
  ring.setBrightness(32);
  ring.clear();
  ring.show();

  // Start the screen and make it right-side up
  lcd.init();
  lcd.flipScreenVertically();
  lcd.setFont(ArialMT_Plain_16);

  // Setup hardware pins
  pinMode(BTN_PLAY_SELECT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
}

// Drops a new score into the leaderboard and shifts the rest down
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

// Plays sounds and figures out which screen to show when a game ends
void triggerGameOver(bool playerWon) {
  ring.clear();
  ring.show();

  if (playerWon) {
    hasWon = true;
    
    // Happy winning tune
    tone(BUZZER_PIN, 523, 100); delay(100);
    tone(BUZZER_PIN, 659, 100); delay(100);
    tone(BUZZER_PIN, 784, 100); delay(100);
    tone(BUZZER_PIN, 1046, 300); delay(300);
    noTone(BUZZER_PIN);
    
    currentState = GAME_OVER;
  } else {
    hasWon = false;
    
    // Sad losing tune
    tone(BUZZER_PIN, 400, 200); delay(200);
    tone(BUZZER_PIN, 300, 200); delay(200);
    tone(BUZZER_PIN, 200, 400); delay(400);
    noTone(BUZZER_PIN); 

    // Only save scores and show the leaderboard in Endless mode
    if (isEndless) {
      saveScore(currentScore);
      
      // Auto-scroll the list so your new score is centered
      highscoreScrollIndex = lastAchievedRank - 2;
      if (highscoreScrollIndex < 0) highscoreScrollIndex = 0;
      if (highscoreScrollIndex > MAX_SCORES - 5) highscoreScrollIndex = MAX_SCORES - 5;
      
      currentState = HIGHSCORES;
    } else {
      currentState = GAME_OVER;
    }
  }
}

// The main brain of the game that runs continuously
void loop() {
  updateButtons();

  switch (currentState) {
    
    // --- MAIN MENU ---
    case MENU:
      if (rightPressed) {
        menuSelection = (menuSelection + 1) % NUM_MENU_ITEMS;
      }
      if (leftPressed) {
        menuSelection = (menuSelection - 1 + NUM_MENU_ITEMS) % NUM_MENU_ITEMS;
      }
      
      if (selectPressed) {
        if (menuSelection == 0) {
          isEndless = false;
          currentState = WAITING_FOR_START;
        } else if (menuSelection == 1) {
          isEndless = true;
          currentState = WAITING_FOR_START;
        } else if (menuSelection == 2) {
          lastAchievedRank = -1; 
          currentState = HIGHSCORES;
        }
        else if(menuSelection == 3){
          currentState = WAITING_FOR_INVISIBLE;
        }
      }

      // Draw the text for the menu
      lcd.clear();
      for (int i = 0; i < NUM_MENU_ITEMS; i++) {
        if (i == menuSelection) {
          lcd.drawString(0, (i * 16), ">> " + menuItems[i]); // Updated spacing
        } else {
          lcd.drawString(0, (i * 16), "   " + menuItems[i]); // Updated spacing
        }
      }
      lcd.display();
      
      ring.clear();
      ring.show();
      break;

    // --- WAITING FOR SENSOR START ---
    case WAITING_FOR_START:
      lcd.clear();
      lcd.setTextAlignment(TEXT_ALIGN_CENTER);
      lcd.drawString(64, 15, "Press sensor");
      lcd.drawString(64, 35, "to start!");
      lcd.display();
      lcd.setTextAlignment(TEXT_ALIGN_LEFT);

      // Start the game when the IR sensor reads a finger press
      if (analogRead(IR_PIN) <= threshold) {
        currentScore = 0;
        curr_pixel = 0;
        gameSpeed = 150;
        jumpedThisRotation = false;
        needsReset = false; 
        currentState = PLAYING_NORMAL;
      }
      break;

    case WAITING_FOR_INVISIBLE:
      lcd.clear();
      lcd.setTextAlignment(TEXT_ALIGN_CENTER);
      lcd.drawString(64, 15, "Press sensor");
      lcd.drawString(64, 35, "to start!");
      lcd.display();
      lcd.setTextAlignment(TEXT_ALIGN_LEFT);

      // Start the game when the IR sensor reads a finger press
      if (analogRead(IR_PIN) <= threshold) {
        currentScore = 0;
        curr_pixel = 0;
        gameSpeed = 150;
        jumpedThisRotation = false;
        needsReset = false; 
        currentState = INVISIBLE_MODE;
      }
      break;

    // --- ACTUAL GAMEPLAY ---
    case PLAYING_NORMAL: {
      lcd.clear();
      if (isEndless) lcd.drawString(0, 0, "ENDLESS");
      else lcd.drawString(0, 0, "CLASSIC");
      lcd.drawString(0, 20, "Score: " + String(currentScore));
      lcd.display();

      // YOUR EXACT JUMP LOGIC
      bool isFingerUp = (analogRead(IR_PIN) > threshold);

      if (!isFingerUp) {
        needsReset = false; 
      } else if (!needsReset) {
        jumpedThisRotation = true; 
        needsReset = true; 
        tone(BUZZER_PIN, 1200, 50); 
      }

      // YOUR EXACT ARCADE BEAT LOGIC
      if (millis() - lastBgmTime > (gameSpeed * 2.5)) {
        lastBgmTime = millis();
        if (!needsReset) { 
          if (bgmHighNote) tone(BUZZER_PIN, 180, 40);
          else tone(BUZZER_PIN, 130, 40);
          bgmHighNote = !bgmHighNote;
        }
      }

      // YOUR EXACT MOVEMENT LOGIC
        if (millis() - lastLedUpdate > gameSpeed) {
          lastLedUpdate = millis();
          
          ring.setPixelColor(curr_pixel % 16, 0); // Turn off the old pixel
          curr_pixel++;
          int pos = curr_pixel % 16;
          
          // Only turn ON the new pixel if score is under 3
          if(currentScore < 3){
            ring.setPixelColor(pos, 255);
          }
          
          ring.show();

        // YOUR EXACT ANTI-CHEAT LOGIC
        if (pos > 4 && pos < 12) {
          if (needsReset) {
            triggerGameOver(false);
          } else {
            jumpedThisRotation = false; 
          }
        }

        // YOUR EXACT SCORING LOGIC
        if (pos == 0) { 
          if (jumpedThisRotation) {
            currentScore++;
            if (gameSpeed > 30) {
              gameSpeed -= 5; 
            }
            
            // Check for the Classic mode win condition
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
    case INVISIBLE_MODE: {
      gameSpeed = 80;
      lcd.clear();
      if (isEndless) lcd.drawString(0, 0, "ENDLESS");
      else lcd.drawString(0, 0, "INVISIBLE");
      lcd.drawString(0, 20, "Score: " + String(currentScore));
      lcd.display();

      // YOUR EXACT JUMP LOGIC
      bool isFingerUp = (analogRead(IR_PIN) > threshold);

      if (!isFingerUp) {
        needsReset = false; 
      } else if (!needsReset) {
        jumpedThisRotation = true; 
        needsReset = true; 
        tone(BUZZER_PIN, 1200, 50); 
      }

      // YOUR EXACT ARCADE BEAT LOGIC
      if (millis() - lastBgmTime > (gameSpeed * 2.5)) {
        lastBgmTime = millis();
        if (!needsReset) { 
          if (bgmHighNote) tone(BUZZER_PIN, 180, 40);
          else tone(BUZZER_PIN, 130, 40);
          bgmHighNote = !bgmHighNote;
        }
      }

      // YOUR EXACT MOVEMENT LOGIC
      if (millis() - lastLedUpdate > gameSpeed) {
        lastLedUpdate = millis();
        
        ring.setPixelColor(curr_pixel % 16, 0);
        curr_pixel++;
        int pos = curr_pixel % 16;
        if(currentScore < 3){
          ring.setPixelColor(pos, 255);
          ring.show();
        }

        // YOUR EXACT ANTI-CHEAT LOGIC
        if (pos > 4 && pos < 12) {
          if (needsReset) {
            triggerGameOver(false);
          } else {
            jumpedThisRotation = false; 
          }
        }

        // YOUR EXACT SCORING LOGIC
        if (pos == 0) { 
          if (jumpedThisRotation) {
            currentScore++;
            
            // Check for the Classic mode win condition
            if (!isEndless && currentScore >= 10) {
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
        
        // Spin an orange and yellow light pattern on the ring
        static unsigned long winAnimTimer = 0;
        static int winAnimOffset = 0;
        
        if (millis() - winAnimTimer > 100) {
          winAnimTimer = millis();
          winAnimOffset++;
          
          for (int i = 0; i < 16; i++) {
            if ((i + winAnimOffset) % 2 == 0) {
              ring.setPixelColor(i, 255, 100, 0); // Orange
            } else {
              ring.setPixelColor(i, 255, 255, 0); // Yellow
            }
          }
          ring.show();
        }
      } else {
        lcd.drawString(64, 10, "GAME OVER");
      }
      
      lcd.drawString(64, 35, "Score: " + String(currentScore));
      lcd.display();
      lcd.setTextAlignment(TEXT_ALIGN_LEFT);

      // Let the user click any button to go back to the menu
      if (selectPressed || leftPressed || rightPressed) {
        currentState = MENU;
        delay(200);
      }
      break;
    }

    // --- LEADERBOARD SCREEN ---
    case HIGHSCORES: {
      lcd.clear();
      lcd.setTextAlignment(TEXT_ALIGN_CENTER);
      
      // Draw 5 scores on the screen based on where you are scrolled
      for (int i = 0; i < 5; i++) {
        int scoreIdx = highscoreScrollIndex + i;
        if (scoreIdx >= MAX_SCORES) break;

        String scoreLine = String(scoreIdx + 1) + ". " + String(highscores[scoreIdx]);
        
        // Add arrows pointing to the score you just got
        if (scoreIdx == lastAchievedRank) {
          lcd.drawString(64, i * 12, ">> " + scoreLine + " <<");
        } else {
          lcd.drawString(64, i * 12, scoreLine);
        }
      }
      
      lcd.display();
      lcd.setTextAlignment(TEXT_ALIGN_LEFT);
      
      // Scroll up and down the list using the left and right buttons
      if (leftPressed && highscoreScrollIndex > 0) {
        highscoreScrollIndex--;
      }
      if (rightPressed && highscoreScrollIndex < MAX_SCORES - 5) {
        highscoreScrollIndex++;
      }
      
      // Select button goes back to menu
      if (selectPressed) {
        currentState = MENU;
        delay(200); 
      }
      break;
    }
  }
}
