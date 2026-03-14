#include <SoftwareSerial.h>

SoftwareSerial gps(10, 11);

void setup() {
  Serial.begin(9600);
  gps.begin(9600);
  
  Serial.println("=== Диагностика GPS ===");
  Serial.println("Ожидание сырых данных с модуля...");
  Serial.println("Подсказка: если видите '$GP...' — связь работает!\n");
}

void loop() {
  if (gps.available()) {
    char c = gps.read();
    Serial.print(c);
  }
}