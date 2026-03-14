#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>

#define TFT_CS   10
#define TFT_RST  8
#define TFT_DC   9
#define BUTTON_PIN 4

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

int lastButtonState = HIGH;
int currentState;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

int screenState = 0;

void setup() {
  Serial.begin(9600);

  Serial.println("Инициализация дисплея...");
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST77XX_BLACK);
  tft.setRotation(1);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Нажмите кнопку для смены изображения!");
  drawWelcomeScreen();
}

void loop() {
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentState) {
      currentState = reading;
      if (currentState == LOW) {
        screenState++;
        if (screenState > 4) {
          screenState = 0;
        }
        drawImage(screenState);
      }
    }
  }
  lastButtonState = reading;
}

void drawImage(int state) {
  tft.fillScreen(ST77XX_BLACK);

  switch (state) {
    case 0:
      tft.setCursor(20, 60);
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(2);
      tft.println("Hello!");
      break;
    case 1:
      tft.fillCircle(tft.width() / 2, tft.height() / 2, 30, ST77XX_RED);
      break;
    case 2:
      tft.fillRect(40, 40, 50, 50, ST77XX_BLUE);
      break;
    case 3:
      tft.fillTriangle(80, 30, 30, 110, 130, 110, ST77XX_YELLOW);
      break;
    case 4:
      tft.fillRoundRect(30, 40, 100, 50, 10, ST77XX_GREEN);
      break;
  }
}

void drawWelcomeScreen() {
  tft.setCursor(20, 40);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.println("Button Test");
  tft.setCursor(10, 70);
  tft.println("Press to change");
}