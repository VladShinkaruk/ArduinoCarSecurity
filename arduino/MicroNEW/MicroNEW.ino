const int MIC_PIN = A0;
const int LED_PIN = 13;
const int CALIBRATION_TIME = 5000;
const int SENSITIVITY_OFFSET = 50;
const int COOLDOWN = 300; 
const int SAMPLE_WINDOW = 50; 
int baseline = 512; 
int noiseAmplitude = 0; 
int threshold = 0; 
unsigned long lastTriggerTime = 0;

void setup() {
  Serial.begin(9600);
  pinMode(MIC_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  Serial.println(">>> НАСТРОЙКА МИКРОФОНА (амплитуда) <<<");
  Serial.println("Тишина! Идет калибровка...");
  
  unsigned long startTime = millis();
  int minVal = 1023, maxVal = 0;
  
  while (millis() - startTime < CALIBRATION_TIME) {
    int val = analogRead(MIC_PIN);
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;
    
    if ((millis() - startTime) % 1000 < 20) { 
       Serial.print((CALIBRATION_TIME - (millis() - startTime)) / 1000);
       Serial.print(" ");
    }
    delay(2);
  }
  
  baseline = (minVal + maxVal) / 2;
  noiseAmplitude = (maxVal - minVal) / 2;
  threshold = noiseAmplitude + SENSITIVITY_OFFSET;
  
  Serial.println("\n\n=== КАЛИБРОВКА ЗАВЕРШЕНА ===");
  Serial.print("Центр сигнала (baseline): "); Serial.println(baseline);
  Serial.print("Амплитуда шума: "); Serial.println(noiseAmplitude);
  Serial.print("Порог срабатывания (амплитуда): "); Serial.println(threshold);
  Serial.println("Слушаю звуки... (кричи/хлопай)");
  Serial.println("------------------------------------------------");
}

void loop() {
  int minVal = 1023, maxVal = 0;
  unsigned long windowStart = millis();
  
  while (millis() - windowStart < SAMPLE_WINDOW) {
    int val = analogRead(MIC_PIN);
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;
    delay(1);
  }
  
  int currentAmplitude = (maxVal - minVal) / 2;
  
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 200) {
    Serial.print("Амплитуда: ");
    Serial.print(currentAmplitude);
    Serial.print(" | Порог: ");
    Serial.print(threshold);
    Serial.print(" [");
    for (int i = 0; i < 40; i++) {
      int level = map(i, 0, 40, 0, 300);
      if (level >= threshold) Serial.print("|");
      else if (level <= currentAmplitude) Serial.print("=");
      else Serial.print("-");
    }
    Serial.println("]");
    lastPrint = millis();
  }
  
  if (currentAmplitude > threshold) {
    if (millis() - lastTriggerTime > COOLDOWN) {
      lastTriggerTime = millis();
      
      digitalWrite(LED_PIN, HIGH);
      Serial.print(">>> 🔥 СРАБОТКА! Амплитуда: ");
      Serial.println(currentAmplitude);
      
      delay(100);
      digitalWrite(LED_PIN, LOW);
    }
  }
  
  delay(10);
}