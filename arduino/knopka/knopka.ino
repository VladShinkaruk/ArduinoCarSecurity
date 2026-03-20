const int pinCLK = 2;
const int pinDT = 3;
const int pinSW = 4;

int counter = 0;
int lastCLKState = HIGH;
int currentCLKState;
int lastDTState;

unsigned long pressStartTime = 0;
bool isPressed = false;
bool longPressDetected = false;
const unsigned long longPressThreshold = 1000;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50; 

void setup() {
  Serial.begin(9600);
  
  pinMode(pinCLK, INPUT_PULLUP);
  pinMode(pinDT, INPUT_PULLUP);
  pinMode(pinSW, INPUT_PULLUP);
  
  lastCLKState = digitalRead(pinCLK);
  lastDTState = lastCLKState;
  Serial.println("Система готова. Вращайте или жмите кнопку.");
  Serial.println("Удерживайте кнопку 1 сек для эмуляции включения/выключения.");
}

void loop() {
  currentCLKState = digitalRead(pinCLK);
  
  if (currentCLKState != lastCLKState && currentCLKState == LOW) {
    if (digitalRead(pinDT) != currentCLKState) {
      counter++;
      Serial.print("Вправо | Счет: ");
    } else {
      counter--;
      Serial.print("Влево | Счет: ");
    }
    Serial.println(counter);
  }
  lastCLKState = currentCLKState;


  int reading = digitalRead(pinSW);
    if (reading != lastDebounceTime) { 
  }
  
  if (reading == LOW) {
    if (!isPressed) {
      isPressed = true;
      pressStartTime = millis();
      longPressDetected = false;
      Serial.println("Кнопка нажата (старт таймера)");
    }
    
    if (!longPressDetected && (millis() - pressStartTime > longPressThreshold)) {
      longPressDetected = true;
      Serial.println(">>> УДЕРЖАНИЕ ЗАФИКСИРОВАНО (ВКЛ/ВЫКЛ СИСТЕМЫ) <<<");
    }
  } 
  else {
    if (isPressed) {
      isPressed = false;
      
      if (!longPressDetected) {
        Serial.println("Обычный клик");
      } else {
        Serial.println("Кнопка отпущена после удержания");
      }
    }
  }
  
  delay(10); 
}