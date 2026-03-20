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

SoftwareSerial sim900(SIM_RX, SIM_TX);
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);

const int16_t GYRO_THRESHOLD = 900;
const unsigned long CALIBRATION_TIME = 5000;
const unsigned long CONFIRMATION_WINDOW = 3000;
const unsigned long TRIGGER_COOLDOWN = 15000;
const unsigned long LONG_PRESS_TIME = 1000;
const unsigned long DEBOUNCE_DELAY = 100;
const unsigned long SETTLING_TIME = 500;
const unsigned long MIC_COOLDOWN = 1000;
const unsigned long MIC_SAMPLE_WINDOW = 50;
const int MIC_SENSITIVITY_OFFSET = 250;

enum SysState { STATE_IDLE, STATE_SUSPICIOUS, STATE_TRIGGERED, STATE_COOLDOWN };
volatile SysState alertState = STATE_IDLE;

bool systemEnabled = false;
bool hasFix = false;
String savedCoords = "";
String savedTime = "";
int visibleSats = 0;
int micBaseline = 512;
int micNoiseAmplitude = 0;
int micThreshold = 0;
unsigned long lastMicTrigger = 0;
int16_t gyroBaseX = 0, gyroBaseY = 0, gyroBaseZ = 0;
float alpha = 0.25;
float filteredGx = 0, filteredGy = 0, filteredGz = 0;
unsigned long suspiciousStart = 0;
unsigned long lastTriggerTime = 0;
unsigned long lastDebugTime = 0;
int8_t firstTrigger = 0; 
bool secondTriggered = false;
bool settlingActive = false;
unsigned long settlingEnd = 0;
bool btnPressed = false;
bool longPressDone = false;
unsigned long btnPressStart = 0;
unsigned long lastBtnCheck = 0;
int lastBtnState = HIGH;

void calibrateAll();
void calibrateMic();
void calibrateGyro();
int readMicPeak();
bool readMicEvent();
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
String formatLocalTime(String utcTime);
void handleButton();
void toggleSystem();
void blinkLED(int times, int ms);
void sendAlarmSMS(const String &triggerSrc);
bool waitSIM(const char* expected, unsigned long timeout);
const char* getStateName(SysState state);

void setup() {
  Serial.begin(9600);
  while (!Serial);
  
  pinMode(MIC_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT,  INPUT_PULLUP);
  pinMode(ENC_SW,  INPUT_PULLUP);
  digitalWrite(LED_PIN, LOW);
  
  Wire.begin();
  sim900.begin(9600);
  gpsSerial.begin(9600);
  
  delay(1500);
  
  sim900.println(F("ATE0"));
  delay(300);
  sim900.println(F("AT+CMGF=1"));
  delay(300);
  sim900.println(F("AT+CSCS=\"GSM\""));
  delay(300);
  
  if (mpu6050_check()) {    
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B); Wire.write(0x00);
    Wire.endTransmission(true);
    delay(100);
    
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x1B); Wire.write(0x18);
    Wire.endTransmission(true);
    delay(100);
  } else {
    Serial.println(F("Ошибка MPU-6050!"));
    blinkLED(5, 100);
    while(true);
  }
  calibrateAll();
  
  systemEnabled = false;
  alertState = STATE_IDLE;
  
  Serial.println(F("\n=== ОХРАНА ГОТОВА ==="));
  Serial.println(F("Охрана выключена"));
  Serial.println(F("Зажмите кнопку 1 сек для ВКЛЮЧЕНИЯ"));
  updateLED();
}

void loop() {
  handleButton();
  
  if (!systemEnabled) {
    switchToGPS();
    parseGPS();
    updateLED();
    delay(100);
    return;
  }
  
  if (alertState == STATE_COOLDOWN && millis() - lastTriggerTime >= TRIGGER_COOLDOWN) {
    alertState = STATE_IDLE;
    firstTrigger = 0;
    secondTriggered = false;
    Serial.println(F("Система снова в режиме охраны"));
    updateLED();
  }
  
  switchToGPS();
  parseGPS();
  
  if (alertState == STATE_IDLE || alertState == STATE_SUSPICIOUS) {
    checkSensors();
  }
  
  if (alertState == STATE_SUSPICIOUS && millis() - suspiciousStart >= CONFIRMATION_WINDOW) {
    Serial.println(F("Нет подтверждения -> возврат в охрану"));
    alertState = STATE_IDLE;
    firstTrigger = 0;
    secondTriggered = false;
    updateLED();
  }
  
  delay(50);
}

const char* getStateName(SysState state) {
  switch(state) {
    case STATE_IDLE: return "ОЖИДАНИЕ";
    case STATE_SUSPICIOUS: return "ПОДОЗРЕНИЕ";
    case STATE_TRIGGERED: return "ТРЕВОГА";
    case STATE_COOLDOWN: return "ПАУЗА";
    default: return "???";
  }
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
      else Serial.println(F("Длинное нажатие - переключение"));
    }
  }
  lastBtnState = btnState;
}

void toggleSystem() {
  systemEnabled = !systemEnabled;
  
  if (systemEnabled) {
    Serial.println(F("\nОХРАНА ВКЛЮЧЕНА"));
    Serial.println(F("Зажмите кнопку 1 сек для ВЫКЛЮЧЕНИЯ"));
    alertState = STATE_IDLE;
    firstTrigger = 0;
    secondTriggered = false;
    blinkLED(2, 150);
  } else {
    Serial.println(F("\nОХРАНА ВЫКЛЮЧЕНА"));
    Serial.println(F("Зажмите кнопку 1 сек для ВКЛЮЧЕНИЯ"));
    alertState = STATE_IDLE;
    digitalWrite(LED_PIN, LOW);
  }
  updateLED();
}

void calibrateAll() {
  Serial.println(F("\nКАЛИБРОВКА (5 сек)..."));
  
  Serial.print(F("  Микрофон... "));
  calibrateMic();
  
  Serial.print(F("  Гироскоп... "));
  calibrateGyro();
  
  Serial.println(F("  Калибровка завершена!\n"));
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

  Serial.print(F("OK | База=")); 
  Serial.print(micBaseline);
  Serial.print(F(" Шум=")); 
  Serial.print(micNoiseAmplitude);
  Serial.print(F(" Порог=")); 
  Serial.println(micThreshold);
}

void calibrateGyro() {
  int32_t sumX = 0, sumY = 0, sumZ = 0, samples = 0;
  unsigned long start = millis();
  
  while (millis() - start < CALIBRATION_TIME / 2) {
    int16_t ax, ay, az, gx, gy, gz;
    readMPU(&ax, &ay, &az, &gx, &gy, &gz);
    sumX += gx; sumY += gy; sumZ += gz;
    samples++;
    delay(10);
  }
  
  gyroBaseX = sumX / samples;
  gyroBaseY = sumY / samples;
  gyroBaseZ = sumZ / samples;
  
  Serial.print(F("Статика: ")); 
  Serial.print(gyroBaseX); Serial.print(F(","));
  Serial.print(gyroBaseY); Serial.print(F(","));
  Serial.println(gyroBaseZ);
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

void parseGPS() {
  static String line = "";
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    if (c == '\n') {
      if (line.startsWith("$GPGGA") || line.startsWith("$GNGGA")) 
        parseGGA(line);
      else if (line.startsWith("$GPGSV") || line.startsWith("$GNGSV")) 
        parseGSV(line);
      line = "";
    } else {
      line += c;
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
  if (fields[7].length() > 0) 
    visibleSats = fields[7].toInt();
}

void parseGSV(String line) {
  int firstComma = line.indexOf(',');
  if (firstComma < 0) return;
  int thirdComma = -1, commaCount = 0;
  for (int i = firstComma + 1; i < line.length(); i++) {
    if (line[i] == ',') { 
      commaCount++; 
      if (commaCount == 2) { thirdComma = i; break; } 
    }
  }
  if (thirdComma > 0) {
    String sats = line.substring(firstComma + 1, thirdComma);
    visibleSats = sats.toInt();
  }
}

float convertToDecimal(String nmeaCoord) {
  int dotIndex = nmeaCoord.indexOf('.');
  if (dotIndex < 2) return 0.0;
  float degrees = nmeaCoord.substring(0, dotIndex - 2).toFloat();
  float minutes = nmeaCoord.substring(dotIndex - 2).toFloat();
  return degrees + (minutes / 60.0);
}

String formatLocalTime(String utcTime) {
  if (utcTime.length() < 6) return "N/A";
  int hh = utcTime.substring(0, 2).toInt();
  int mm = utcTime.substring(2, 4).toInt();
  int ss = utcTime.substring(4, 6).toInt();
  hh += 3;
  if (hh >= 24) hh -= 24;
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", hh, mm, ss);
  return String(buf);
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
  
  if (millis() - lastDebugTime > 500 && alertState != STATE_TRIGGERED) {
    lastDebugTime = millis();
    Serial.print(F("[DEBUG] "));
    Serial.print(getStateName(alertState));
    Serial.print(F(" | Микрофон: ")); 
    Serial.print(readMicPeak());
    Serial.print(F("/")); 
    Serial.print(micThreshold);
    Serial.print(F(" | Гироскоп: ")); 
    Serial.print(dx); Serial.print(F(",")); 
    Serial.print(dy); Serial.print(F(",")); 
    Serial.println(dz);
  }
  
  if (alertState == STATE_IDLE) {
    if (micActive) {
      Serial.println(F("ЗВУК! -> ПОДОЗРЕНИЕ"));
      enterSuspicious("MIC");
    }
    else if (gyroActive) {
      Serial.println(F("ДВИЖЕНИЕ! -> ПОДОЗРЕНИЕ"));
      enterSuspicious("GYRO");
    }
  }
  else if (alertState == STATE_SUSPICIOUS) {
    if (settlingActive) {
      if (millis() >= settlingEnd) {
        settlingActive = false;
        Serial.println(F("Ждём подтверждения..."));
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
  }
  
  updateLED();
}

void enterSuspicious(const char *src) {
  alertState = STATE_SUSPICIOUS;
  suspiciousStart = millis();
  
  firstTrigger = (strcmp(src, "MIC") == 0) ? 1 : 2;
  secondTriggered = false;
  settlingActive = true;
  settlingEnd = millis() + SETTLING_TIME;
  
  updateLED();
}

void onTrigger() {
  if (millis() - lastTriggerTime < TRIGGER_COOLDOWN) return;
  
  alertState = STATE_TRIGGERED;
  lastTriggerTime = millis();
  
  Serial.println(F("\n*** ТРЕВОГА! ***"));
  Serial.println(F("Обновление GPS..."));
  switchToGPS();
  unsigned long gpsEnd = millis() + 1500;
  while (millis() < gpsEnd) { 
    parseGPS(); 
    delay(10); 
  }
  
  String localTime = formatLocalTime(savedTime);
  Serial.print(F("   Время: ")); Serial.println(localTime);
  Serial.print(F("   Координаты: ")); 
  Serial.println(hasFix ? savedCoords : F("НЕТ FIX"));
  Serial.print(F("   Спутники: ")); Serial.println(visibleSats);
  
  const char* triggerName;
  String triggerSrc;
  
  if (firstTrigger == 1 && secondTriggered) {
    triggerName = "МИКРОФОН+ГИРОСКОП";
    triggerSrc = "MIC+GYRO";
  } else if (firstTrigger == 2 && secondTriggered) {
    triggerName = "ГИРОСКОП+МИКРОФОН";
    triggerSrc = "GYRO+MIC";
  } else if (firstTrigger == 1) {
    triggerName = "МИКРОФОН";
    triggerSrc = "MICROPHONE";
  } else {
    triggerName = "ГИРОСКОП";
    triggerSrc = "GYROSCOPE";
  }
  
  Serial.print(F("   Сработал: ")); Serial.println(triggerName);
  sendAlarmSMS(triggerSrc);
  
  systemEnabled = false;
  alertState = STATE_COOLDOWN;
  firstTrigger = 0;
  secondTriggered = false;
  
  Serial.println(F("\nСообщение отправлено"));
  Serial.println(F("Система выключена"));
  Serial.println(F("Зажмите кнопку для повторного включения"));
  
  updateLED();
}

void sendAlarmSMS(const String &triggerSrc) {
  Serial.println(F("\nОтправка SMS..."));
  
  switchToSIM();
  delay(300);
  
  sim900.println(F("AT+CMGF=1"));
  delay(200);
  sim900.println(F("AT+CSCS=\"GSM\""));
  delay(200);
  
  String smsText = "TREVOGA! Sensor: " + triggerSrc;
  
  if (hasFix && savedCoords.length() > 5) {
    smsText += " | Coords: " + savedCoords;
    smsText += " | Map: https://maps.google.com/?q=" + savedCoords;
  } else {
    smsText += " | NO_GPS";
  }
  
  sim900.print(F("AT+CMGS=\"+375445566077\"\r"));
  delay(500);
  sim900.print(smsText);
  sim900.write(26);
  delay(2000);
  
  if (waitSIM("OK", 4000)) {
    Serial.println(F("SMS отправлена"));
    Serial.println(F("Текст:"));
    Serial.println(F("   ---"));
    Serial.println("   " + smsText);
    Serial.println(F("   ---"));
  } else {
    Serial.println(F("Ошибка отправки SMS"));
  }
}

bool waitSIM(const char* expected, unsigned long timeout) {
  String resp = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (sim900.available()) {
      char c = sim900.read();
      resp += c;
      if (resp.indexOf(expected) >= 0) return true;
    }
  }
  return false;
}

void updateLED() {
  if (!systemEnabled) {
    digitalWrite(LED_PIN, LOW);
    return;
  }
  
  switch(alertState) {
    case STATE_IDLE:
      digitalWrite(LED_PIN, LOW);
      break;
    case STATE_SUSPICIOUS:
      digitalWrite(LED_PIN, (millis() / 150) % 2);
      break;
    case STATE_TRIGGERED:
    case STATE_COOLDOWN:
      digitalWrite(LED_PIN, HIGH);
      break;
  }
}

void blinkLED(int times, int ms) {
  for(int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(ms);
    digitalWrite(LED_PIN, LOW);
    delay(ms);
  }
}

void switchToGPS() { 
  gpsSerial.listen(); 
  delayMicroseconds(100); 
}

void switchToSIM() { 
  sim900.listen(); 
  delayMicroseconds(100); 
}