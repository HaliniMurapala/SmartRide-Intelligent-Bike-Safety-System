// SmartRide — Helmet Safety Node
#include "BluetoothSerial.h"
BluetoothSerial SerialBT;
// Replace with your server MAC
uint8_t serverAddress[] = {0xC0,0xCD,0xD6,0x84,0x9D,0x4A};
#define IR_PIN 4
#define MQ3_PIN 34
void setup() {
  pinMode(IR_PIN, INPUT);
  Serial.begin(115200);
  SerialBT.begin("ESP32_Client", true);
  delay(10000);
  Serial.println("Connecting...");
  if (SerialBT.connect(serverAddress)) {
    Serial.println("Connected!");
  } else {
    Serial.println("Failed!");
  }
}
void loop() {
  int irValue = digitalRead(IR_PIN);
  int alcoholValue = analogRead(MQ3_PIN);
  // Format:
  // IR:0,MQ3:1234
  String Data = "IR:" + String(irValue) +
                ",MQ3:" + String(alcoholValue);
  if (SerialBT.connected()) {
    SerialBT.println("Hello Server");
    SerialBT.println(Data);
    Serial.println(Data);
    Serial.println("Sent");
  } else {
    Serial.println("Disconnected");
  }
  delay(2000);
}
