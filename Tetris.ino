#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// Button pins
#define BTN_LEFT   32
#define BTN_RIGHT  33
#define BTN_DOWN   25
#define BTN_ROTATE 26
#define BTN_START  27

void setup() {
  // TFT setup
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);

  tft.setCursor(10, 10);
  tft.println("Button Test");

  // Button setup (INPUT_PULLUP)
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_ROTATE, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);
}

void loop() {
  if (digitalRead(BTN_LEFT) == LOW) {
    showPress("LEFT");
  }

  if (digitalRead(BTN_RIGHT) == LOW) {
    showPress("RIGHT");
  }

  if (digitalRead(BTN_DOWN) == LOW) {
    showPress("DOWN");
  }

  if (digitalRead(BTN_ROTATE) == LOW) {
    showPress("ROTATE");
  }

  if (digitalRead(BTN_START) == LOW) {
    showPress("START");
  }
}

void showPress(const char* label) {
  tft.fillRect(0, 50, tft.width(), 30, TFT_BLACK);
  tft.setCursor(10, 60);
  tft.print("Pressed: ");
  tft.print(label);

  delay(200); // crude debounce for now
}
