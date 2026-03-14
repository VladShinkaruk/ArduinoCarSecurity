#include <SoftwareSerial.h>

#define SIM_RX 8
#define SIM_TX 9

SoftwareSerial sim900(SIM_RX, SIM_TX);

const String APN = "internet";

const String URL = "http://fedrum.atwebpages.com/status.txt";
const String SERVER = "fedrum.atwebpages.com";

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  Serial.println(F("\n=== ТЕСТ SIM900 HTTP ==="));
  
  sim900.begin(9600);
  delay(2000);
  
  Serial.println(F("\n[1] Проверка модуля..."));
  sim900.println(F("AT"));
  if (!waitForResponse("OK", 3000)) {
    Serial.println(F("❌ ОШИБКА: Модуль не отвечает! Проверьте wiring (8, 9)."));
    while(true);
  }
  Serial.println(F("✅ Модуль отвечает"));
  
  Serial.println(F("\n[2] Проверка SIM..."));
  sim900.println(F("AT+CPIN?"));
  if (!waitForResponse("+CPIN: READY", 3000)) {
    Serial.println(F("❌ ОШИБКА: SIM не найдена или заблокирована!"));
    while(true);
  }
  Serial.println(F("✅ SIM готова"));
  
  Serial.println(F("\n[3] Регистрация в сети..."));
  sim900.println(F("AT+CREG?"));
  delay(1000);
  readSIMResponse();
  
  Serial.println(F("\n[4] Настройка GPRS..."));
  sim900.println(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));
  waitForResponse("OK", 2000);
  
  sim900.print(F("AT+SAPBR=3,1,\"APN\",\""));
  sim900.print(APN);
  sim900.println(F("\""));
  waitForResponse("OK", 2000);
  
  Serial.println(F("    Включаем GPRS..."));
  sim900.println(F("AT+SAPBR=1,1"));
  delay(5000);
  
  sim900.println(F("AT+SAPBR=2,1"));
  delay(2000);
  Serial.println(F("    Статус GPRS:"));
  readSIMResponse();
  
  Serial.println(F("\n[5] HTTP запрос к серверу..."));
  Serial.print("    URL: ");
  Serial.println(URL);
  
  sim900.println(F("AT+HTTPINIT"));
  if (!waitForResponse("OK", 2000)) {
    Serial.println(F("❌ ОШИБКА: HTTPINIT failed"));
    return;
  }
  
  sim900.print(F("AT+HTTPPARA=\"URL\",\""));
  sim900.print(URL);
  sim900.println(F("\""));
  if (!waitForResponse("OK", 2000)) {
    Serial.println(F("❌ ОШИБКА: HTTPPARA failed"));
    return;
  }
  
  Serial.println(F("    Отправка запроса..."));
  sim900.println(F("AT+HTTPACTION=0"));
  
  Serial.println(F("    Ожидание ответа (до 15 сек)..."));
  String actionResp = waitForResponseDetailed("+HTTPACTION:", 15000);
  
  if (actionResp.length() > 0) {
    Serial.print(F("    Ответ сервера: "));
    Serial.println(actionResp);
    
    if (actionResp.indexOf("0,200,") >= 0) {
      Serial.println(F("✅ УСПЕХ! Сервер ответил 200 OK"));
      
      sim900.println(F("AT+HTTPREAD"));
      delay(1000);
      Serial.println(F("    Содержимое файла:"));
      readSIMResponse();
    }
    else if (actionResp.indexOf("0,601,") >= 0) {
      Serial.println(F("❌ ОШИБКА 601: Не удалось подключиться к серверу"));
      Serial.println(F("    Возможные причины:"));
      Serial.println(F("    - Неверный APN"));
      Serial.println(F("    - Нет денег/трафика на SIM"));
      Serial.println(F("    - Сервер блокирует подключение"));
      Serial.println(F("    - Плохой сигнал GSM"));
    }
    else if (actionResp.indexOf("0,603,") >= 0) {
      Serial.println(F("❌ ОШИБКА 603: DNS не смогresolve домен"));
      Serial.println(F("    Попробуйте использовать IP вместо домена"));
    }
    else {
      Serial.println(F("❌ ДРУГАЯ ОШИБКА HTTP"));
    }
  }
  else {
    Serial.println(F("❌ ТАЙМАУТ: Нет ответа от сервера"));
  }
  
  sim900.println(F("AT+HTTPTERM"));
  
  Serial.println(F("\n=== ТЕСТ ЗАВЕРШЁН ==="));
  Serial.println(F("Перезагрузите для повторения...\n"));
}

void loop() {
}

bool waitForResponse(const String& expected, unsigned long timeout) {
  unsigned long t = millis();
  while (millis() - t < timeout) {
    if (sim900.available()) {
      String line = sim900.readStringUntil('\n');
      Serial.print("    <- ");
      Serial.println(line);
      if (line.indexOf(expected) >= 0) return true;
    }
  }
  return false;
}

String waitForResponseDetailed(const String& expected, unsigned long timeout) {
  String resp = "";
  unsigned long t = millis();
  while (millis() - t < timeout) {
    if (sim900.available()) {
      char c = sim900.read();
      resp += c;
      Serial.print(c);
      if (resp.indexOf(expected) >= 0) {
        delay(500);
        while (sim900.available()) {
          c = sim900.read();
          resp += c;
          Serial.print(c);
        }
        return resp;
      }
    }
  }
  return resp;
}

void readSIMResponse() {
  delay(500);
  while (sim900.available()) {
    Serial.write(sim900.read());
  }
  Serial.println();
}