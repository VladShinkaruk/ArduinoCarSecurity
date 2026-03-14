#include <SoftwareSerial.h>

SoftwareSerial sim900(8, 9);

void setup() {
  Serial.begin(9600);
  sim900.begin(9600);
  
  Serial.println("=== SIM900 READY ===");
  delay(2000);
  
  sendCommand("AT", "OK", 1000);
  sendCommand("AT+CGMM", "OK", 1000);
  sendCommand("AT+CSQ", "OK", 1000);
  
  Serial.println("=== Вводите команды вручную ===");
}

void loop() {
  if (sim900.available()) {
    Serial.write(sim900.read());
  }
  if (Serial.available()) {
    sim900.write(Serial.read());
  }
}

void sendCommand(String cmd, String expected, int timeout) {
  Serial.print(">>> ");
  Serial.println(cmd);
  sim900.println(cmd);
  delay(timeout);
  
  while(sim900.available()) {
    String response = sim900.readString();
    Serial.print("<<< ");
    Serial.println(response);
  }
}