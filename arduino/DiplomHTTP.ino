#include <SoftwareSerial.h>
#include <Wire.h>

#define SIM_RX 8
#define SIM_TX 9
#define GPS_RX 10
#define GPS_TX 11
#define MIC_PIN A0
#define LED_PIN 13
#define ENC_CLK 2
#define ENC_DT  3  
#define ENC_SW  4
#define MPU_ADDR 0x68
#define SMS_PASS "0809"
#include <TinyGPS++.h>
TinyGPSPlus gpsParser;
SoftwareSerial sim900(SIM_RX, SIM_TX);
SoftwareSerial gps(GPS_RX, GPS_TX);
bool gpsReady = false;
bool httpEnabled = false;
unsigned long lastGPSprint = 0;
const int16_t GYRO_THRESHOLD = 1500;
const unsigned long CALIBRATION_TIME = 5000;
const unsigned long CONFIRMATION_WINDOW = 3000;
const unsigned long TRIGGER_COOLDOWN = 15000;
const unsigned long LONG_PRESS_TIME = 1000;
const unsigned long DEBOUNCE_DELAY = 100;
const unsigned long SETTLING_TIME = 500;
unsigned long lastMicTrigger = 0;
const unsigned long MIC_COOLDOWN = 1000;
bool gpsActive = true;
bool httpActive = false;
bool serverSyncPending = false;
const unsigned long GPS_UPDATE_INTERVAL = 60000;
enum SysState { IDLE, SUSPICIOUS, TRIGGERED, COOLDOWN };
volatile SysState alertState = IDLE;
bool systemEnabled = false;
int micBaseline = 512;
int micNoiseAmplitude = 0;
int micThreshold = 0;
const int MIC_SENSITIVITY_OFFSET = 250;
const int MIC_SAMPLE_WINDOW = 50;
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
unsigned long lastHttpCheck = 0;
const unsigned long HTTP_CHECK_INTERVAL = 10000;
enum Mode {
  MODE_GPS,
  MODE_HTTP
};

Mode currentMode = MODE_GPS;

unsigned long lastGpsUpdate = 0;
const unsigned long GPS_INTERVAL = 300000;
const unsigned long HTTP_INTERVAL = 20000;
bool firstFixDone = false;
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
  sim900.println(F("ATE0"));
  delay(500);
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
  initGPRS(); 

  sim900.println(F("AT+CNMI=2,2,0,0,0"));
  delay(500);
}

void loop() {
  if (systemEnabled) {
    checkSensors();
  }

  if (currentMode == MODE_GPS) {
    switchToGPS();
    parseGPS();

    if (hasFix) {
      Serial.println("GPS зафиксирован");
      Serial.println(savedCoords);
      lastGpsUpdate = millis();
      currentMode = MODE_HTTP;
      Serial.println("Переключение на HTTP");
    }
  }
  else if (currentMode == MODE_HTTP) {
      switchToSIM();

      if (serverSyncPending) {
          sendServerState();
          serverSyncPending = false;
      }
      checkIncomingSMS();

      if (alertState == IDLE && millis() - lastHttpCheck > HTTP_INTERVAL) {
          lastHttpCheck = millis();
          checkHTTP();
      }

      if (millis() - lastGpsUpdate > GPS_INTERVAL) {
        Serial.println("Update GPS");
        hasFix = false;
        currentMode = MODE_GPS;
      }
  }
  handleButton(); 
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
  serverSyncPending = true;

  if (systemEnabled) {
    Serial.println(F("\nОхрана включена"));
    alertState = IDLE;
    firstTrigger = 0;
    secondTriggered = false;
    blinkLED(2, 150);
    updateLED();
  } 
  else {
    Serial.println(F("\nОхрана выключена"));
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
  unsigned long startTime = millis();
  int minVal = 1023;
  int maxVal = 0;

  while (millis() - startTime < CALIBRATION_TIME) {
    int val = analogRead(MIC_PIN);
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;

    delay(2);
  }

  micBaseline = (minVal + maxVal) / 2;
  micNoiseAmplitude = (maxVal - minVal) / 2;
  micThreshold = micNoiseAmplitude + MIC_SENSITIVITY_OFFSET;

  Serial.print(F("База: "));
  Serial.print(micBaseline);
  Serial.print(F(" | Амплитуда: "));
  Serial.print(micNoiseAmplitude);
  Serial.print(F(" | Порог: "));
  Serial.println(micThreshold);
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

bool readMicEvent() {
  int minVal = 1023;
  int maxVal = 0;
  unsigned long windowStart = millis();

  while (millis() - windowStart < MIC_SAMPLE_WINDOW) {
    int val = analogRead(MIC_PIN);
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;

    delay(1);
  }

  int amplitude = (maxVal - minVal) / 2;

  if (amplitude > micThreshold) {
    if (millis() - lastMicTrigger > MIC_COOLDOWN) {
      lastMicTrigger = millis();
      return true;
    }
  }
  return false;
}

int readMicPeak() {
  int minVal = 1023;
  int maxVal = 0;

  for (int i = 0; i < 30; i++) {
    int val = analogRead(MIC_PIN);
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;

    delayMicroseconds(400);
  }
  return (maxVal - minVal) / 2;
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
  bool micActive = readMicEvent();
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
    int micLevel = readMicPeak();

    Serial.print(F(" | MIC="));
    Serial.print(micLevel);
    Serial.print(F("/"));
    Serial.print(micThreshold);
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
    triggerSrc = "MICGYRO";
  } else if (firstTrigger == 2 && secondTriggered) {
    triggerSrc = "GYROMIC";
  } else if (firstTrigger == 1) {
    triggerSrc = "MIC";
  } else {
    triggerSrc = "GYRO";
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
  sendAlarmLog(triggerSrc);

  systemEnabled = false;
  serverSyncPending = true;
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

void initGPRS() {
  sim900.println(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""));
  waitSIM("OK",2000);

  sim900.println(F("AT+SAPBR=3,1,\"APN\",\"internet\""));
  waitSIM("OK",2000);

  sim900.println(F("AT+SAPBR=1,1"));
  delay(5000);

  sim900.println(F("AT+SAPBR=2,1"));
  waitSIM("OK",2000);
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
    Serial.print(F("   GPS: ")); Serial.println(savedCoords.length() ? savedCoords : "NO_FIX");
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

void switchToGPS() {
  gps.listen();
  while (gps.available()) gps.read();
  delay(50);
}

void switchToSIM() { 
  sim900.listen(); 
  delay(10); 
}

void parseGPS() {
  while (gps.available()) {
    char c = gps.read();
    if (gpsParser.encode(c)) {
      if (gpsParser.location.isValid()) {
        savedCoords = String(gpsParser.location.lat(), 6) + "," + String(gpsParser.location.lng(), 6);
        savedTime = String(gpsParser.time.hour()) + ":" + String(gpsParser.time.minute()) + ":" + String(gpsParser.time.second());
        hasFix = true;
      } else {
        hasFix = false;
      }
      visibleSats = gpsParser.satellites.value();
    }
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
  text.trim();
  text.toUpperCase();

  if (!text.endsWith(SMS_PASS)) {
    return;
  }

  if (text.startsWith("ON")) {
    if (!systemEnabled) {
      Serial.println(F("SMS: Включение охраны"));
      toggleSystem();
      sendServerState();
    }
  }
  else if (text.startsWith("OFF")) {
    if (systemEnabled) {
      Serial.println(F("SMS: Выключение охраны"));
      toggleSystem();
      sendServerState();
    }
  }
  else if (text.startsWith("STATUS")) {
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

  bool micActive = readMicPeak() > micThreshold;
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

void checkHTTP() {
  Serial.println(F("Проверка сервера"));
  sim900.println(F("AT+HTTPINIT"));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPPARA=\"URL\",\"http://fedrum.atwebpages.com/status.txt\""));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPACTION=0"));

  if(!waitSIM("+HTTPACTION:",7000)) {
    Serial.println(F("HTTP fail"));
    return;
  }

  sim900.println(F("AT+HTTPREAD"));

  String response = "";
  unsigned long t = millis();

  while (millis() - t < 2000) {
    while (sim900.available()) {
      response += (char)sim900.read();
    }
  }

  Serial.println(response);

  bool serverState = systemEnabled;

  if (response.indexOf("ON") >= 0) serverState = true;
  if (response.indexOf("OFF") >= 0) serverState = false;

  if (serverState != systemEnabled) {
    Serial.println(F("Ответ сервера:"));
    systemEnabled = serverState;
    updateLED();
  }

  sim900.println(F("AT+HTTPTERM"));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPINIT"));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPPARA=\"URL\",\"http://fedrum.atwebpages.com/request_status.txt\""));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPACTION=0"));

  if(!waitSIM("+HTTPACTION:",7000)) {
    Serial.println(F("Ошибка сервера"));
    return;
  }

  sim900.println(F("AT+HTTPREAD"));

  String req = "";
  t = millis();

  while (millis() - t < 2000) {
    while (sim900.available()) {
      req += (char)sim900.read();
    }
  }

  Serial.println(req);

  if (req.indexOf("STATUS") >= 0) {


    sendHttpStatus();
  }

  sim900.println(F("AT+HTTPTERM"));
  waitSIM("OK",2000);
}

bool waitSIM(const char* expected, unsigned long timeout)
{
  String resp = "";
  unsigned long t = millis();

  while (millis() - t < timeout) {

    while (sim900.available()) {

      char c = sim900.read();
      resp += c;

      if (resp.indexOf(expected) >= 0) {
        return true;
      }
    }
  }

  return false;
}

void sendAlarmLog(String sensor) {

  Serial.println(F("Отправка тревоги"));

  switchToSIM();

  sim900.println(F("AT+HTTPINIT"));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPPARA=\"UA\",\"Mozilla/5.0\""));
  waitSIM("OK",2000);

  String url = "http://fedrum.atwebpages.com/log.php?event=alarm";

  url += "&sensor=" + sensor;

  if (hasFix) {
    url += "&coords=" + savedCoords;
  }

  Serial.println(url);

  sim900.print(F("AT+HTTPPARA=\"URL\",\""));
  sim900.print(url);
  sim900.println(F("\""));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPACTION=0"));
  waitSIM("+HTTPACTION:",7000);

  sim900.println(F("AT+HTTPTERM"));
  waitSIM("OK",2000);

  Serial.println(F("Тревога отправлена"));
}

void sendServerState() {
  Serial.println(F("Синхронизация..."));
  switchToSIM();

  sim900.println(F("AT+HTTPINIT"));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPPARA=\"UA\",\"Mozilla/5.0\""));
  waitSIM("OK",2000);

  String url = "http://fedrum.atwebpages.com/set.php?state=";

  if (systemEnabled)
    url += "ON";
  else
    url += "OFF";

  sim900.print(F("AT+HTTPPARA=\"URL\",\""));
  sim900.print(url);
  sim900.println(F("\""));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPACTION=0"));
  waitSIM("+HTTPACTION:",7000);

  sim900.println(F("AT+HTTPTERM"));
  waitSIM("OK",2000);

  Serial.println(F("Сервер синхронизирован"));
}

void sendHttpStatus() {
  Serial.println(F("Отправка статуса"));
  switchToGPS();
  unsigned long t = millis() + 500;
  while (millis() < t) parseGPS();
  switchToSIM();

  bool micActive = readMicPeak() > micThreshold;
  int16_t ax, ay, az, gx, gy, gz;
  readMPU(&ax,&ay,&az,&gx,&gy,&gz);

  int16_t dx = abs(gx - gyroBaseX);
  int16_t dy = abs(gy - gyroBaseY);
  int16_t dz = abs(gz - gyroBaseZ);

  String status =
        String(systemEnabled ? 1 : 0) + "|" +
        String(alertState) + "|" +
        String(micActive ? 1 : 0) + "|" +
        String(dx) + "," + String(dy) + "," + String(dz) + "|" +
        String(visibleSats) + "|" +
        (hasFix ? savedCoords : "0,0");

  Serial.println(status);

  sim900.println(F("AT+HTTPINIT"));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPPARA=\"CID\",1"));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPPARA=\"URL\",\"http://fedrum.atwebpages.com/update_device_status.php\""));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\""));
  waitSIM("OK",2000);

  String body = "d=" + status;

  sim900.print(F("AT+HTTPDATA="));
  sim900.print(body.length());
  sim900.println(F(",5000"));
  waitSIM("DOWNLOAD",3000);

  sim900.print(body);
  waitSIM("OK",5000);

  sim900.println(F("AT+HTTPACTION=1"));
  waitSIM("+HTTPACTION:",8000);

  sim900.println(F("AT+HTTPTERM"));
  waitSIM("OK",2000);

  Serial.println(F("Статус отправлен"));
  Serial.println(F("Очистка запроса"));

  sim900.println(F("AT+HTTPINIT"));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPPARA=\"CID\",1"));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPPARA=\"URL\",\"http://fedrum.atwebpages.com/clear_request.php\""));
  waitSIM("OK",2000);

  sim900.println(F("AT+HTTPACTION=0"));
  waitSIM("+HTTPACTION:",8000);

  sim900.println(F("AT+HTTPTERM"));
  waitSIM("OK",2000);

  Serial.println(F("Запрос очищен"));
}