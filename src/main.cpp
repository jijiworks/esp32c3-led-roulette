#include <Arduino.h>
#include <esp_random.h>

// -------------------------
// Pin configuration
// -------------------------
const uint8_t LED_PINS[8] = {0, 1, 2, 3, 4, 5, 6, 7};
const uint8_t BUTTON_PIN = 10;
const uint8_t BUZZER_PIN = 20;   // Active buzzer

// -------------------------
// Roulette settings
// -------------------------
const int FAST_DELAY_MS = 80;      // Initial fast lighting speed
const int SLOW_DELAY_MS = 250;     // Slow lighting for pin1, pin2, and pin3
const int FINAL_HOLD_MS = 2000;    // Keep the final LED on after stopping

// Deceleration settings for the final random section
const int STOP_DELAY_START_MS = 280;  // Start of the random section
const int STOP_DELAY_END_MS   = 520;  // End of the random section

// Buzzer settings
const int BEEP_ON_MS_FAST = 30;    // Buzzer duration during fast phase
const int BEEP_ON_MS_SLOW = 80;    // Buzzer duration during slow phase
const int BEEP_ON_MS_STOP = 60;    // Buzzer duration during final movement

bool isRunning = false;
bool prevButtonState = HIGH;

// Mapping between numbers 1-8 and the stop pin
// 1->4, 2->5, 3->6, 4->7, 5->0, 6->1, 7->2, 8->3
const uint8_t RESULT_TO_PIN[8] = {4, 5, 6, 7, 0, 1, 2, 3};

// -------------------------
// Helper functions
// -------------------------
void allOff() {
  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_PINS[i], LOW);
  }
  digitalWrite(BUZZER_PIN, LOW);
}

int indexOfPin(uint8_t gpioPin) {
  for (int i = 0; i < 8; i++) {
    if (LED_PINS[i] == gpioPin) {
      return i;
    }
  }
  return -1;
}

int nextIndex(int currentIndex) {
  return (currentIndex + 1) % 8;
}

int getRouletteResult() {
  return (esp_random() % 8) + 1;
}

int stepsToTarget(int currentIndex, int targetIndex) {
  if (targetIndex >= currentIndex) {
    return targetIndex - currentIndex;
  }
  return (8 - currentIndex) + targetIndex;
}

// Watch for the cancel button while running
bool checkCancelButton() {
  bool state = digitalRead(BUTTON_PIN);

  if (prevButtonState == HIGH && state == LOW) {
    delay(20);
    if (digitalRead(BUTTON_PIN) == LOW) {
      allOff();
      return true;
    }
  }

  prevButtonState = state;
  return false;
}

// Wait for the specified time, but cancel if the button is pressed
bool waitWithCancel(int ms) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    if (checkCancelButton()) {
      return true;
    }
    delay(1);
  }
  return false;
}

// Light only one LED and sound the buzzer at the same time
bool lightAndBeepByIndex(int index, int totalDelayMs, int beepOnMs) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_PINS[i], LOW);
  }

  digitalWrite(LED_PINS[index], HIGH);
  digitalWrite(BUZZER_PIN, HIGH);

  if (waitWithCancel(beepOnMs)) {
    return true;
  }

  digitalWrite(BUZZER_PIN, LOW);

  int remain = totalDelayMs - beepOnMs;
  if (remain > 0) {
    if (waitWithCancel(remain)) {
      return true;
    }
  }

  return false;
}

// Keep only the final LED on without the buzzer
bool holdFinalLedOnly(int index, int holdMs) {
  for (int i = 0; i < 8; i++) {
    digitalWrite(LED_PINS[i], LOW);
  }

  digitalWrite(LED_PINS[index], HIGH);
  digitalWrite(BUZZER_PIN, LOW);

  return waitWithCancel(holdMs);
}

// Calculate the delay for the deceleration phase
int calcStopDelayMs(int stepNo, int totalSteps) {
  // If totalSteps is 1 or less, use the slowest delay
  if (totalSteps <= 1) {
    return STOP_DELAY_END_MS;
  }

  // stepNo: 0 to totalSteps - 1
  long diff = STOP_DELAY_END_MS - STOP_DELAY_START_MS;
  long add  = (diff * stepNo) / (totalSteps - 1);
  return STOP_DELAY_START_MS + (int)add;
}

// -------------------------
// Main roulette function
// -------------------------
void runRoulette() {
  isRunning = true;

  // Wait until the button is released so the start press is not detected as cancel
  while (digitalRead(BUTTON_PIN) == LOW) {
    delay(1);
  }
  prevButtonState = HIGH;

  int result = getRouletteResult(); // 1 to 8
  uint8_t stopPin = RESULT_TO_PIN[result - 1];
  int stopIndex = indexOfPin(stopPin);

  // 1) Start from the top (pin0) and make 2 full laps
  for (int lap = 0; lap < 2; lap++) {
    for (int i = 0; i < 8; i++) {
      if (lightAndBeepByIndex(i, FAST_DELAY_MS, BEEP_ON_MS_FAST)) {
        isRunning = false;
        return;
      }
    }
  }

  // After 2 laps, light pin0 once more
  if (lightAndBeepByIndex(0, FAST_DELAY_MS, BEEP_ON_MS_FAST)) {
    isRunning = false;
    return;
  }

  // 2) Slowly light three LEDs (pin1, pin2, pin3)
  for (int i = 1; i <= 3; i++) {
    if (lightAndBeepByIndex(i, SLOW_DELAY_MS, BEEP_ON_MS_SLOW)) {
      isRunning = false;
      return;
    }
  }

  // Current position is pin3
  int currentIndex = 3;

  // 3) Move toward the stop position based on the random result, slowing down gradually
  int steps = stepsToTarget(currentIndex, stopIndex);

  for (int i = 0; i < steps; i++) {
    currentIndex = nextIndex(currentIndex);
    int currentDelay = calcStopDelayMs(i, steps);
    if (lightAndBeepByIndex(currentIndex, currentDelay, BEEP_ON_MS_STOP)) {
      isRunning = false;
      return;
    }
  }

  // 4) Hold the final LED without buzzer
  if (holdFinalLedOnly(stopIndex, FINAL_HOLD_MS)) {
    isRunning = false;
    return;
  }

  allOff();
  isRunning = false;
}

// -------------------------
// setup / loop
// -------------------------
void setup() {
  delay(500);

  for (int i = 0; i < 8; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  allOff();

  prevButtonState = digitalRead(BUTTON_PIN);
}

void loop() {
  bool currentButtonState = digitalRead(BUTTON_PIN);

  // Start only when stopped and the button is pressed
  if (!isRunning && prevButtonState == HIGH && currentButtonState == LOW) {
    delay(20);
    if (digitalRead(BUTTON_PIN) == LOW) {
      runRoulette();
      currentButtonState = digitalRead(BUTTON_PIN);
    }
  }

  prevButtonState = currentButtonState;
}