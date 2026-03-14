#include <SoftwareSerial.h>
#include <Wire.h>

#define SIM_RX 8
#define SIM_TX 9
#define GPS_RX 10
#define GPS_TX 11
#define MIC_PIN 5
#define LED_PIN 13
#define ENC_CLK 2
#define ENC_DT  3  
#define ENC_SW  4
#define MPU_ADDR 0x68

SoftwareSerial sim900(SIM_RX, SIM_TX);
SoftwareSerial gps(GPS_RX, GPS_TX);

const int16_t GYRO_THRESHOLD = 900;
const unsigned long CALIBRATION_TIME = 5000;
const unsigned long CONFIRMATION_WINDOW = 3000;
const unsigned long TRIGGER_COOLDOWN = 15000;
const unsigned long LONG_PRESS_TIME = 1000;
const unsigned long DEBOUNCE_DELAY = 100;
const unsigned long SETTLING_TIME = 500;
unsigned long lastMicTrigger = 0;
const unsigned long MIC_COOLDOWN = 1000;

enum SysState { IDLE, SUSPICIOUS, TRIGGERED, COOLDOWN };
volatile SysState alertState = IDLE;
bool systemEnabled = false;
int micBaseline = 0;
int micThreshold = 0;
int16_t gyroBaseX = 0, gyroBaseY = 0, gyroBaseZ = 0;
unsigned long suspiciousStart = 0;
unsigned long lastTriggerTime = 0;
unsigned long lastDebugTime = 0;
bool hasFix = false;
String lastGPSLine = "";
String savedCoords = "";
String savedTime = "";
int visibleSats = 0;
int8_t firstTrigger = 0; 
bool secondTriggered = false;
bool btnPressed = false;
bool longPressDone = false;
unsigned long btnPressStart = 0;
unsigned long lastBtnCheck = 0;
int lastBtnState = HIGH;
unsigned long settlingEnd = 0;
bool settlingActive = false;
float alpha = 0.25;
float filteredGx = 0, filteredGy = 0, filteredGz = 0;
unsigned long lastGpsRead = 0;

void calibrateAll();
void calibrateMic();
void calibrateGyro();
int readMicPeak();
void readMPU(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz);
void checkSensors();
void enterSuspicious(const char *src);
void onTrigger();
void updateLED();
void switchToGPS();
void switchToSIM();
bool mpu6050_check();
void parseGPS();
void parseGGA(String line);
void parseGSV(String line);
float convertToDecimal(String nmeaCoord);
void handleButton();
void toggleSystem();
void printStatus();
void blinkLED(int times, int ms);
void checkIncomingSMS();
void processSMS(String text);
void sendStatusSMS();

void setup() {
  Serial.begin(9600);
  while (!Serial);
  sim900.println("AT+CREG?");
  pinMode(MIC_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT,  INPUT_PULLUP);
  pinMode(ENC_SW,  INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);
  
  Wire.begin();
  sim900.begin(9600);
  gps.begin(9600);
  
  delay(1500);
  
  if (mpu6050_check()) {    
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0x00); 
    Wire.endTransmission(true);
    delay(100);
    
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1B);
    Wire.write(0x18);
    Wire.endTransmission(true);
    delay(100);
    
  } else {
    Serial.println(F("Ошибка MPU-6050!"));
    blinkLED(5, 100);
    while(true);
  }
  
  calibrateAll();
  
  systemEnabled = false;
  alertState = IDLE;
  
  Serial.println(F("\nОхрана выключена"));
  Serial.println(F("Зажмите кнопку для включения охраны"));  
  updateLED();

  switchToSIM();
  delay(500);
  sim900.println(F("AT+CMGF=1"));
  delay(500);

  sim900.println(F("AT+CNMI=2,2,0,0,0"));
  delay(500);
  Serial.println(F("SIM900 готов к приёму SMS"));
}


void loop() {
  switchToSIM();
  checkIncomingSMS();

  handleButton();

  if (millis() - lastGpsRead > 200) {
    switchToGPS();
    unsigned long t = millis() + 20;
    while (millis() < t) {
      parseGPS();
    }
    switchToSIM();
    lastGpsRead = millis();
  }

  if (systemEnabled) {

    if (alertState == COOLDOWN && millis() - lastTriggerTime >= TRIGGER_COOLDOWN) {
      alertState = IDLE;
      firstTrigger = 0;
      secondTriggered = false;
      updateLED();
    }

    if (alertState == IDLE || alertState == SUSPICIOUS) {
      checkSensors();
    }

    if (alertState == SUSPICIOUS && millis() - suspiciousStart >= CONFIRMATION_WINDOW) {
      alertState = IDLE;
      firstTrigger = 0;
      secondTriggered = false;
      updateLED();
    }
  }

  updateLED();
}

void handleButton() {
  if (millis() - lastBtnCheck < DEBOUNCE_DELAY) return;
  lastBtnCheck = millis();
  
  int btnState = digitalRead(ENC_SW);
  
  if (btnState == LOW && lastBtnState == HIGH) {
    btnPressed = true;
    longPressDone = false;
    btnPressStart = millis();
  }
  
  if (btnPressed && btnState == LOW) {
    unsigned long holdTime = millis() - btnPressStart;
    if (!longPressDone && holdTime >= LONG_PRESS_TIME) {
      longPressDone = true;
      toggleSystem();
    }
    if (holdTime >= LONG_PRESS_TIME && holdTime % 200 < 100) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
  
  if (btnState == HIGH && lastBtnState == LOW) {
    if (btnPressed) {
      btnPressed = false;
      if (!longPressDone) Serial.println(F("Короткое нажатие"));
      else Serial.println(F("Длинное нажатие"));
    }
  }
  lastBtnState = btnState;
}

void toggleSystem() {
  systemEnabled = !systemEnabled;
  if (systemEnabled) {
    Serial.println(F("\nОхрана включена"));
    Serial.println(F("Зажмите кнопку для выключения охраны\n"));
    alertState = IDLE;
    firstTrigger = 0;
    secondTriggered = false;
    blinkLED(2, 150);
    updateLED();
  } else {
    Serial.println(F("\nОхрана выключена"));
    Serial.println(F("Зажмите кнопку для включения охраны\n"));
    alertState = IDLE;
    digitalWrite(LED_PIN, LOW);
  }
}

void calibrateAll() {
  Serial.println(F("\n КАЛИБРОВКА (5 сек)..."));
  Serial.print(F("   [1/2] МИКРОФОН... "));
  calibrateMic();
  Serial.print(F("   [2/2] ГИРОСКОП... "));
  calibrateGyro();
  Serial.println(F("   Калибровка завершена!\n"));
}

void calibrateMic() {
  Serial.println(F("ОК (цифровой)"));
  
  int testVal = digitalRead(MIC_PIN);
  Serial.print(F("   Текущее состояние: "));
  Serial.println(testVal == HIGH ? F("ШУМ") : F("ТИШИНА"));
  
  micBaseline = 0;
  micThreshold = 1;
}

void calibrateGyro() {
  int32_t sumX = 0, sumY = 0, sumZ = 0, samples = 0;
  unsigned long start = millis();
  while (millis() - start < CALIBRATION_TIME / 2) {
    int16_t ax, ay, az, gx, gy, gz;
    readMPU(&ax, &ay, &az, &gx, &gy, &gz);
    sumX += gx; sumY += gy; sumZ += gz;
    samples++; delay(10);
  }
  gyroBaseX = sumX / samples;
  gyroBaseY = sumY / samples;
  gyroBaseZ = sumZ / samples;
  Serial.print(F("в статике=")); 
  Serial.print(gyroBaseX); Serial.print(F(","));
  Serial.print(gyroBaseY); Serial.print(F(","));
  Serial.println(gyroBaseZ);
}

bool readMicEvent(unsigned long windowMs = 30) {
  unsigned long end = millis() + windowMs;
  int highCount = 0;
  const int MIN_HIGH_SAMPLES = 5;
  
  while (millis() < end) {
    if (digitalRead(MIC_PIN) == HIGH) {
      highCount++;
      if (highCount >= MIN_HIGH_SAMPLES) {
        return true;
      }
    }
    delayMicroseconds(800);
  }
  return false;
}

void readMPU(int16_t *ax, int16_t *ay, int16_t *az, 
             int16_t *gx, int16_t *gy, int16_t *gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 14, true);
  
  if (Wire.available() >= 14) {
    *ax = Wire.read() << 8 | Wire.read();
    *ay = Wire.read() << 8 | Wire.read();
    *az = Wire.read() << 8 | Wire.read();
    Wire.read(); Wire.read();
    *gx = Wire.read() << 8 | Wire.read();
    *gy = Wire.read() << 8 | Wire.read();
    *gz = Wire.read() << 8 | Wire.read();
  }
}

void checkSensors() {
  bool micActive = readMicEvent(30);
  
  int16_t ax, ay, az, gx, gy, gz;
  readMPU(&ax, &ay, &az, &gx, &gy, &gz);
  
  filteredGx = alpha * gx + (1 - alpha) * filteredGx;
  filteredGy = alpha * gy + (1 - alpha) * filteredGy;
  filteredGz = alpha * gz + (1 - alpha) * filteredGz;
  
  bool gyroActive = false;
  int16_t dx = abs(filteredGx - gyroBaseX);
  int16_t dy = abs(filteredGy - gyroBaseY);
  int16_t dz = abs(filteredGz - gyroBaseZ);
  if (dx > GYRO_THRESHOLD || dy > GYRO_THRESHOLD || dz > GYRO_THRESHOLD) 
    gyroActive = true;
  
  if (millis() - lastDebugTime > 500 && alertState != TRIGGERED && alertState != COOLDOWN) {
    lastDebugTime = millis();
    Serial.print(F("[DBG] "));
    Serial.print(alertState == IDLE ? "ОЖИДАНИЕ" : (settlingActive ? "СТАБИЛИЗАЦИЯ" : "ПОДОЗРЕНИЕ"));
    Serial.print(F(" | Микрофон: ")); 
    Serial.print(micActive ? "АКТИВЕН" : "тишина");
    Serial.print(F(" | Гироскоп: ")); 
    Serial.print(dx); Serial.print(F(",")); 
    Serial.print(dy); Serial.print(F(",")); 
    Serial.println(dz);
  }
  
  if (alertState == IDLE) {
    if (micActive) {
        Serial.println(F("ЗВУК! → ПОДОЗРИТЕЛЬНОСТЬ"));
        enterSuspicious("MIC");
    }
    else if (gyroActive) {
      Serial.println(F("ДВИЖЕНИЕ! → ПОДОЗРИТЕЛЬНОСТЬ"));
      enterSuspicious("GYRO");
    }
  }
  else if (alertState == SUSPICIOUS) {
    if (settlingActive) {
      if (millis() >= settlingEnd) {
        settlingActive = false;
        Serial.println(F("Ждем подтверждения..."));
      }
      return;
    }
    
    if (micActive || gyroActive) {
      if (micActive && firstTrigger == 2) secondTriggered = true;
      if (gyroActive && firstTrigger == 1) secondTriggered = true;
      
      Serial.println(F("СОБЫТИЕ ПОДТВЕРЖДЕНО!"));
      onTrigger();
      return;
    }
    
    if (millis() - suspiciousStart >= CONFIRMATION_WINDOW) {
      Serial.println(F("Нет подтверждения → система в режиме охраны"));
      alertState = IDLE;
      firstTrigger = 0;
      secondTriggered = false;
      updateLED();
    }
  }
  updateLED();
}

void enterSuspicious(const char *src) {
  alertState = SUSPICIOUS;
  suspiciousStart = millis();
  
  if (strcmp(src, "MIC") == 0) {
    firstTrigger = 1;
  } else {
    firstTrigger = 2;
  }
  secondTriggered = false;
  
  settlingActive = true;
  settlingEnd = millis() + SETTLING_TIME;
  
  updateLED();
}

void onTrigger() {
  if (millis() - lastTriggerTime < TRIGGER_COOLDOWN) {
    return;
  }
  
  alertState = TRIGGERED;
  lastTriggerTime = millis();
  
  Serial.println(F("\n🚨🚨🚨 ТРЕВОГА! 🚨🚨🚨"));  

  Serial.println(F("Обновление GPS..."));
  switchToGPS();
  delay(20);
  unsigned long gpsEnd = millis() + 2000;
  while (millis() < gpsEnd) { 
    parseGPS(); 
    delay(10); 
  }
  
  Serial.println(F("Информация:"));
  String localTime = formatLocalTime(savedTime);
  Serial.print(F("   Время: ")); Serial.println(localTime);
  Serial.print(F("   Координаты: ")); Serial.println(hasFix ? savedCoords : "NO_FIX");
  Serial.print(F("   Спутники: ")); Serial.println(visibleSats);
  
  String triggerName;
  if (firstTrigger == 1 && secondTriggered) {
    triggerName = F("МИКРОФОН+ГИРОСКОП");
  } else if (firstTrigger == 2 && secondTriggered) {
    triggerName = F("ГИРОСКОП+МИКРОФОН");
  } else if (firstTrigger == 1) {
    triggerName = F("МИКРОФОН");
  } else {
    triggerName = F("ГИРОСКОП");
  }
  
  Serial.print(F("   Сработал: "));
  Serial.println(triggerName);
  
  String triggerSrc;
  if (firstTrigger == 1 && secondTriggered) {
    triggerSrc = "MIC+GYRO";
  } else if (firstTrigger == 2 && secondTriggered) {
    triggerSrc = "GYRO+MIC";
  } else if (firstTrigger == 1) {
    triggerSrc = "MICROPHONE";
  } else {
    triggerSrc = "GYROSCOPE";
  }
  
  String smsText = "TREVOGA! Sensor: " + triggerSrc;
  if (hasFix) {
    smsText += ", coords: " + savedCoords;
  } else {
    smsText += ", NO_GPS_FIX";
  }
  
  Serial.println(F("\n[Содержимое СМС]"));
  Serial.println(F("   +--"));
  Serial.println("   | " + smsText);
  if (hasFix) {
    Serial.println("   | https://maps.google.com/?q=" + savedCoords);
  }
  Serial.println(F("   +--"));
  
  switchToSIM();
  delay(500);
  
  sim900.println(F("AT+CMGF=1"));
  delay(300);
  sim900.println(F("AT+CSCS=\"GSM\""));
  delay(300);
  
  sim900.print(F("AT+CMGS=\"+375XX445566077\"\r"));
  delay(500);
  
  sim900.print(smsText);
  if (hasFix) {
    sim900.print(F("\nMap: https://maps.google.com/?q="));
    sim900.print(savedCoords);
  }
  
  sim900.print((char)26);
  delay(1500);
  
  if (sim900.find("OK")) {
  } else {
    Serial.println(F("Ошибка отправки СМС"));
  }
  
  Serial.println(F("\nСообщение готово. Система выключена"));
  Serial.println(F("Зажмите кнопку для повторного включения охраны"));
  
  systemEnabled = false;
  alertState = IDLE;
  firstTrigger = 0;
  secondTriggered = false;
  
  updateLED();
  
  return;
}

void updateLED() {
  if (!systemEnabled) { digitalWrite(LED_PIN, LOW); return; }
  switch(alertState) {
    case IDLE: digitalWrite(LED_PIN, LOW); break;
    case SUSPICIOUS: digitalWrite(LED_PIN, (millis() / 200) % 2); break;
    case TRIGGERED:
    case COOLDOWN: digitalWrite(LED_PIN, HIGH); break;
  }
}

void blinkLED(int times, int ms) {
  for(int i=0; i<times; i++) {
    digitalWrite(LED_PIN, HIGH); delay(ms);
    digitalWrite(LED_PIN, LOW); delay(ms);
  }
}

void printStatus() {
  Serial.println(F("\nСтатус:"));
  Serial.print(F("   Энергия: ")); Serial.println(systemEnabled ? F("Включено") : F("Выключено"));
  Serial.print(F("   Состояние оповещения: "));
  switch(alertState) {
    case IDLE: Serial.println(F("IDLE")); break;
    case SUSPICIOUS: Serial.println(F("SUSPICIOUS")); break;
    case TRIGGERED: Serial.println(F("TRIGGERED")); break;
    case COOLDOWN: Serial.println(F("COOLDOWN")); break;
  }
  if (systemEnabled) {
    Serial.print(F("   Микрофон: ")); Serial.print(readMicPeak()); Serial.print(F("/")); Serial.println(micThreshold);
    int16_t ax,ay,az,gx,gy,gz; readMPU(&ax,&ay,&az,&gx,&gy,&gz);
    Serial.print(F("   Гироскоп: ")); Serial.print(abs(gx-gyroBaseX)); Serial.print(F(","));
    Serial.print(abs(gy-gyroBaseY)); Serial.print(F(",")); Serial.println(abs(gz-gyroBaseZ));
    Serial.print(F("   GPS: ")); Serial.println(hasFix ? savedCoords : "NO_FIX");
  }
  Serial.println(F("   Cmds: [r]=recal [t]=test [s]=status [p]=power\n"));
}

bool mpu6050_check() {
  Wire.beginTransmission(MPU_ADDR);
  byte error = Wire.endTransmission();
  if (error == 0) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x75);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 1);
    if (Wire.available()) {
      byte id = Wire.read();
      if (id == 0x68 || id == 0x70) return true;
    }
  }
  if (MPU_ADDR == 0x68) {
    Wire.beginTransmission(0x69);
    if (Wire.endTransmission() == 0) return true;
  }
  return false;
}

void switchToGPS() { gps.listen(); delay(10); }
void switchToSIM() { sim900.listen(); delay(10); }

void parseGPS() {
  while (gps.available()) {
    char c = gps.read();
    if (c == '\n') {
      if (lastGPSLine.startsWith("$GPGGA") || lastGPSLine.startsWith("$GNGGA")) parseGGA(lastGPSLine);
      else if (lastGPSLine.startsWith("$GPGSV") || lastGPSLine.startsWith("$GNGSV")) parseGSV(lastGPSLine);
      lastGPSLine = "";
    } else lastGPSLine += c;
  }
}

void parseGGA(String line) {
  if (line.length() < 50) return;
  int idx = 0; String fields[15], field = "";
  for (int i = 0; i < line.length() && idx < 15; i++) {
    if (line[i] == ',') { fields[idx++] = field; field = ""; } 
    else field += line[i];
  }
  fields[idx] = field;
  String fixQual = fields[6];
  if (fixQual != "0" && fixQual.length() > 0) {
    String latRaw = fields[2], latDir = fields[3];
    String lonRaw = fields[4], lonDir = fields[5];
    String timeUTC = fields[1];
    if (latRaw.length() > 4 && lonRaw.length() > 5) {
      float lat = convertToDecimal(latRaw);
      float lon = convertToDecimal(lonRaw);
      if (latDir == "S") lat *= -1;
      if (lonDir == "W") lon *= -1;
      savedCoords = String(lat, 6) + "," + String(lon, 6);
      savedTime = timeUTC;
      hasFix = true;
    }
  }
  if (fields[7].length() > 0) visibleSats = fields[7].toInt();
}

void parseGSV(String line) {
  int firstComma = line.indexOf(',');
  if (firstComma < 0) return;
  int thirdComma = -1, commaCount = 0;
  for (int i = firstComma + 1; i < line.length(); i++) {
    if (line[i] == ',') { commaCount++; if (commaCount == 2) { thirdComma = i; break; } }
  }
  if (thirdComma > 0) {
    String sats = line.substring(firstComma + 1, thirdComma);
    visibleSats = sats.toInt();
  }
}

String formatLocalTime(String utcTime) {
  if (utcTime.length() < 6) return "N/A";
  
  int hh = utcTime.substring(0, 2).toInt();
  int mm = utcTime.substring(2, 4).toInt();
  int ss = utcTime.substring(4, 6).toInt();
  
  hh += 3;
  if (hh >= 24) hh -= 24;
  
  char buffer[9];
  sprintf(buffer, "%02d:%02d:%02d", hh, mm, ss);
  return String(buffer);
}

float convertToDecimal(String nmeaCoord) {
  int dotIndex = nmeaCoord.indexOf('.');
  if (dotIndex < 2) return 0.0;
  float degrees = nmeaCoord.substring(0, dotIndex - 2).toFloat();
  float minutes = nmeaCoord.substring(dotIndex - 2).toFloat();
  return degrees + (minutes / 60.0);
}

void checkIncomingSMS() {
  static String line = "";
  static bool waitingText = false;

  while (sim900.available()) {

    char c = sim900.read();

    if (c == '\r') continue;

    if (c == '\n') {

      line.trim();

      if (line.startsWith("+CMT:")) {
        waitingText = true;
      }
      else if (waitingText) {
        Serial.print(F("Получено SMS: "));
        Serial.println(line);
        processSMS(line);
        waitingText = false;
      }

      line = "";
    }
    else {
      line += c;
    }
  }
}

void processSMS(String text) {
  text.toUpperCase();

  if (text.indexOf("ON") >= 0) {
    if (!systemEnabled) {
      Serial.println(F("SMS: Включение охраны"));
      toggleSystem();
    }
  }
  else if (text.indexOf("OFF") >= 0) {
    if (systemEnabled) {
      Serial.println(F("SMS: Выключение охраны"));
      toggleSystem();
    }
  }

  else if (text.indexOf("STATUS") >= 0) {
  Serial.println(F("SMS: Запрос статуса"));
  sendStatusSMS();
  }
}

void sendStatusSMS() {
  switchToGPS();
  unsigned long t = millis() + 500;
  while (millis() < t) {
    parseGPS();
  }
  switchToSIM();

  bool micActive = digitalRead(MIC_PIN);

  int16_t ax, ay, az, gx, gy, gz;
  readMPU(&ax, &ay, &az, &gx, &gy, &gz);

  int16_t dx = abs(gx - gyroBaseX);
  int16_t dy = abs(gy - gyroBaseY);
  int16_t dz = abs(gz - gyroBaseZ);

  String smsText = "STATUS\n";

  smsText += "System: ";
  smsText += systemEnabled ? "ON" : "OFF";

  smsText += "\nAlert: ";
  switch(alertState) {
    case IDLE: smsText += "IDLE"; break;
    case SUSPICIOUS: smsText += "SUSPICIOUS"; break;
    case TRIGGERED: smsText += "TRIGGERED"; break;
    case COOLDOWN: smsText += "COOLDOWN"; break;
  }

  smsText += "\nMic: ";
  smsText += micActive ? "NOISE" : "QUIET";

  smsText += "\nGyro: ";
  smsText += String(dx) + "," + String(dy) + "," + String(dz);

  smsText += "\nSats: ";
  smsText += String(visibleSats);

  smsText += "\nGPS: ";
  smsText += hasFix ? savedCoords : "NO_FIX";

  sim900.println(F("AT+CMGF=1"));
  delay(200);

  sim900.print(F("AT+CMGS=\"+375чч445566077\"\r"));
  delay(400);

  sim900.print(smsText);
  sim900.write(26);

  delay(1500);

  Serial.println(F("STATUS отправлен"));
}