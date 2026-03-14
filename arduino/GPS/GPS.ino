#include <SoftwareSerial.h>

#define GPS_RX 10 
#define GPS_TX 11

SoftwareSerial gps(GPS_RX, GPS_TX);

bool hasFix = false;
unsigned long lastPrintTime = 0;
unsigned long lastHeartbeatTime = 0;

const unsigned long printInterval = 2000;
const unsigned long heartbeatInterval = 30000;

int visibleSats = 0;

void setup() {
  Serial.begin(9600);
  gps.begin(9600);
  
  Serial.println("🛰 GPS Монитор запущен...");
  Serial.println("Ожидание спутников (может занять 5-15 мин)\n");
}

void loop() {
  while (gps.available()) {
    char c = gps.read();
    static String line = "";
    
    if (c == '\n') {
      if (line.startsWith("$GPGGA") || line.startsWith("$GNGGA")) {
        parseGGA(line);
      } else if (line.startsWith("$GPGSV") || line.startsWith("$GNGSV")) {
        parseGSV(line);
      }
      line = "";
    } else {
      line += c;
    }
  }
  
  if (millis() - lastHeartbeatTime >= heartbeatInterval) {
    lastHeartbeatTime = millis();
    
    if (!hasFix) {
      Serial.println("\n⏳ Поиск спутников... система работает (" + String(visibleSats) + " видно)");
      Serial.println("💡 Не выключайте питание, ожидайте фиксации\n");
    }
  }
}

void parseGGA(String line) {
  if (line.length() < 50) return;

  int idx = 0;
  String fields[15];
  String field = "";
  
  for (int i = 0; i < line.length() && idx < 15; i++) {
    if (line[i] == ',') {
      fields[idx++] = field;
      field = "";
    } else {
      field += line[i];
    }
  }
  fields[idx] = field;

  String timeStr = fields[1];
  String fixQual = fields[6];
  String satsNum = fields[7];
  String latRaw  = fields[2];
  String latDir  = fields[3];
  String lonRaw  = fields[4];
  String lonDir  = fields[5];

  bool currentFix = (fixQual != "0" && fixQual.length() > 0);
  
  if (currentFix != hasFix || (millis() - lastPrintTime > printInterval)) {
    hasFix = currentFix;
    lastPrintTime = millis();

    Serial.println("------------------------------");
    Serial.print("📡 Статус: ");
    if (hasFix) {
      Serial.println("✅ ФИКСАЦИЯ ЕСТЬ");
      Serial.print("⏱ Время UTC: "); Serial.println(timeStr);
      Serial.print("🛰 Спутников: "); Serial.println(satsNum);
      
      if (latRaw.length() > 4 && lonRaw.length() > 5) {
        float lat = convertToDecimal(latRaw);
        float lon = convertToDecimal(lonRaw);
        
        if (latDir == "S") lat *= -1;
        if (lonDir == "W") lon *= -1;

        Serial.print("🌍 Широта: "); Serial.println(lat, 6);
        Serial.print("🌍 Долгота: "); Serial.println(lon, 6);
        Serial.print("📍 POS:");
        Serial.print(lat, 6); Serial.print(","); Serial.println(lon, 6);
        Serial.print("🔗 Карта: https://www.google.com/maps?q=");
        Serial.print(lat, 6);
        Serial.print(",");
        Serial.println(lon, 6);
      }
    } else {
      Serial.println("❌ НЕТ ФИКСАЦИИ");
      Serial.print("🛰 Видно спутников: "); Serial.println(visibleSats);
      Serial.println("💡 Совет: Выйдите на улицу или к окну");
    }
    Serial.println("------------------------------\n");
  }
}

void parseGSV(String line) {  
  int parts[20];
  int idx = 0;
  int start = 0;
  
  for (int i = 0; i < line.length() && idx < 18; i++) {
    if (line[i] == ',' || i == line.length() - 1) {
      if (i == line.length() - 1) i++;
      String field = line.substring(start, i);
      parts[idx++] = field.toInt();
      start = i + 1;
    }
  }
  
  if (idx >= 4) {
    visibleSats = parts[3];
    
    Serial.print("🛰 SAT:");
    for (int i = 4; i + 3 < idx; i += 4) {
      int id = parts[i], elev = parts[i+1], az = parts[i+2], snr = parts[i+3];
      if (id > 0 && snr > 0) {
        Serial.print(String(id) + "," + String(elev) + "," + String(az) + "," + String(snr) + ";");
      }
    }
    Serial.println();
  }
}

float convertToDecimal(String nmeaCoord) {
  int dotIndex = nmeaCoord.indexOf('.');
  if (dotIndex < 2) return 0.0;
  
  float degrees = nmeaCoord.substring(0, dotIndex - 2).toFloat();
  float minutes = nmeaCoord.substring(dotIndex - 2).toFloat();
  
  return degrees + (minutes / 60.0);
}