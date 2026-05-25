// ─── Blynk Credentials ────────────────────────────────────────────
#define BLYNK_TEMPLATE_ID "TMPL3M2HqoaP3"
#define BLYNK_TEMPLATE_NAME "project"
#define BLYNK_AUTH_TOKEN "uQ6fJvDu0ZnFa37G2Oim_fZL9NQfppJL"
#define BLYNK_PRINT Serial

#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "BluetoothSerial.h"
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

char ssid[] = "ACT";
char pass[] = "86970200";

BluetoothSerial SerialBT;
LiquidCrystal_I2C lcd(0x26, 20, 4);
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

#define RELAY_PIN       33
#define BUTTONACCIDENT  13
#define BTN_SCHOOL      12
#define BTN_HOSPITAL    14
#define BTN_RESIDENTIAL 27
#define BTN_HIGHWAY     18
#define PWM_PIN         19
#define POT_PIN         32
#define BTN1            2

float currentSpeed    = 0;
float Speed           = 0;
float speedBeforeZone = 0;
String url;
float Latitude  = 0;
float Longitude = 0;

bool schoolActive      = false;
bool hospitalActive    = false;
bool residentialActive = false;
bool highwayActive     = false;

bool prevSchool      = false;
bool prevHospital    = false;
bool prevResidential = false;
bool prevHighway     = false;

// ─── State flags ──────────────────────────────────────────────────
bool helmetOn        = false;
bool alcoholDetected = false;
bool helmetConfirmed = false;   // true after first successful helmet ON
bool wifiStarted     = false;
bool btActive        = false;
int  irValue         = 1;
int  mq3Value        = 0;

// ─── Timing ───────────────────────────────────────────────────────
unsigned long lastBlynkSend    = 0;
unsigned long lastAlcoholCheck = 0;
unsigned long btStartTime      = 0;

#define BLYNK_INTERVAL_MS      200
#define ALCOHOL_CHECK_INTERVAL 60000   // recheck every 60 seconds
#define BT_LISTEN_TIMEOUT      10000   // wait max 10s for BT data

// ─── Blynk Virtual Pins ───────────────────────────────────────────
// V0 → Speed (Gauge)
// V1 → Speed Limit / Zone (Label)
// V2 → GPS Latitude  (Label)
// V3 → GPS Longitude (Label)
// V4 → Helmet Status (Label)
// V5 → Alcohol Value (Gauge)
// V6 → Accident Alert (LED)
// V7 → Zone Status (Label)
// V8 → Google Maps URL (Label)

// ─── Forward declarations ─────────────────────────────────────────
void InAccident();
void InGPS();
void HandleZones();
void EnterZone(const char* name, int limit);
void ExitZone(const char* name);
void SpeedLimit(int limit);
void RestoreSpeed();
void OverSpeed();
int  speedToPWM(float speedKmh);
bool isAnyZoneActive();
void UpdateLCD();
void SendToBlynk();
void SwitchToBT();
void SwitchToWiFi();
void WaitForHelmet();
void CheckAlcoholViaBT();

// ─────────────────────────────────────────────────────────────────
void setup() {
  pinMode(RELAY_PIN,       OUTPUT);
  pinMode(BUTTONACCIDENT,  INPUT);
  pinMode(BTN_SCHOOL,      INPUT);
  pinMode(BTN_HOSPITAL,    INPUT);
  pinMode(BTN_RESIDENTIAL, INPUT);
  pinMode(BTN_HIGHWAY,     INPUT);
  pinMode(PWM_PIN,         OUTPUT);
  pinMode(BTN1,            INPUT);

  // Engine OFF at boot
  digitalWrite(RELAY_PIN, LOW);
  analogWrite(PWM_PIN, 0);

  Serial.begin(9600);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("System Ready");
  Serial.println("System Ready");

  // Start BT first — wait for helmet
  SwitchToBT();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Wear Helmet to");
  lcd.setCursor(0, 1); lcd.print("Start Bike");
}

// ─────────────────────────────────────────────────────────────────
void loop() {

  // ── PHASE 1: Helmet not yet confirmed — listen over BT ───────
  if (!helmetConfirmed) {
    WaitForHelmet();
    return;
  }

  // ── PHASE 2: Periodic alcohol recheck over BT ────────────────
  if (btActive) {
    CheckAlcoholViaBT();
    return;
  }

  // ── PHASE 3: Normal operation — WiFi + Blynk ─────────────────
  if (wifiStarted) Blynk.run();

  // GPS feed
  if (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  // Speed
  int potValue = analogRead(POT_PIN);
  currentSpeed = map(potValue, 0, 4095, 0, 100);

  // ── Helmet / alcohol gate ─────────────────────────────────────
  if (!helmetOn || alcoholDetected) {
    digitalWrite(RELAY_PIN, LOW);
    analogWrite(PWM_PIN, 0);
    schoolActive = hospitalActive = residentialActive = highwayActive = false;
    prevSchool   = prevHospital  = prevResidential    = prevHighway   = false;
    Speed = 0;
    delay(150);
    return;
  }

  // ── Normal ride ───────────────────────────────────────────────
  digitalWrite(RELAY_PIN, HIGH);

  Serial.print("Speed: "); Serial.print(currentSpeed); Serial.println(" kmph");

  if (isAnyZoneActive()) speedBeforeZone = currentSpeed;

  if (digitalRead(BUTTONACCIDENT) == HIGH) {
    InAccident();
    delay(150);
    return;
  }
  else{
    Blynk.virtualWrite(V6, "ACCIDENT NOT DETECTED!");
  }

  HandleZones();
  InGPS();
  OverSpeed();

  // Blynk update every 2 seconds
  if (millis() - lastBlynkSend >= BLYNK_INTERVAL_MS) {
    SendToBlynk();
    lastBlynkSend = millis();
  }

  // Trigger alcohol recheck every 60 seconds
  if (millis() - lastAlcoholCheck >= ALCOHOL_CHECK_INTERVAL) {
    Serial.println("Alcohol recheck — switching to BT...");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Alcohol Recheck");
    lcd.setCursor(0, 1); lcd.print("Pausing WiFi...");

    if (wifiStarted) {
      Blynk.disconnect();
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      wifiStarted = false;
    }
    delay(500);
    SwitchToBT();
  }

  delay(150);
}

// ─────────────────────────────────────────────────────────────────
// Switch radio → Bluetooth
// ─────────────────────────────────────────────────────────────────
void SwitchToBT() {
  Serial.println("Starting Bluetooth...");
  SerialBT.begin("ESP32_Server");
  btActive    = true;
  btStartTime = millis();
  Serial.print("BT Address: ");
  Serial.println(SerialBT.getBtAddressString());
}

// ─────────────────────────────────────────────────────────────────
// Switch radio → WiFi + Blynk
// ─────────────────────────────────────────────────────────────────
void SwitchToWiFi() {
  Serial.println("Connecting to WiFi...");
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Connecting WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP());

    Blynk.config(BLYNK_AUTH_TOKEN);
    Blynk.connect();
    wifiStarted = true;

    // Push initial values
    Blynk.virtualWrite(V4, "Helmet ON");
    Blynk.virtualWrite(V5, mq3Value);

    delay(1000);
  } else {
    Serial.println("\nWiFi Failed — running offline.");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1); lcd.print("Running offline...");
    delay(1000);
  }

  // Engine ON — helmet confirmed, alcohol clear
  digitalWrite(RELAY_PIN, HIGH);
  analogWrite(PWM_PIN, 255);

  lcd.clear();
  UpdateLCD();
}

// ─────────────────────────────────────────────────────────────────
// PHASE 1 — Wait for helmet confirmation over BT
// ─────────────────────────────────────────────────────────────────
void WaitForHelmet() {
  if (!SerialBT.available()) return;

  String data = SerialBT.readStringUntil('\n');
  Serial.print("BT: "); Serial.println(data);

  int irIndex = data.indexOf("IR:");
  int mqIndex = data.indexOf("MQ3:");
  if (irIndex < 0 || mqIndex < 0) return;

  irValue  = data.substring(irIndex + 3, mqIndex - 1).toInt();
  mq3Value = data.substring(mqIndex + 4).toInt();

  helmetOn        = (irValue  == 0);
  alcoholDetected = (mq3Value > 1500);

  Serial.print("IR="); Serial.print(irValue);
  Serial.print(" MQ3="); Serial.println(mq3Value);

  // Update LCD during wait
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(helmetOn ? "Helmet: ON      " : "Helmet: OFF     ");
  lcd.setCursor(0, 1); lcd.print(alcoholDetected ? "Alcohol:DETECTED" : "Alcohol: Clear  ");

  if (!helmetOn) {
    lcd.setCursor(0, 2); lcd.print("Wear helmet first!");
    return;
  }

  if (alcoholDetected) {
    lcd.setCursor(0, 2); lcd.print("Alcohol detected!");
    lcd.setCursor(0, 3); lcd.print("Cannot start bike ");
    return;
  }

  // ── Helmet ON + no alcohol → proceed ─────────────────────────
  helmetConfirmed  = true;
  lastAlcoholCheck = millis();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Helmet Confirmed!");
  lcd.setCursor(0, 1); lcd.print("Starting WiFi...");
  Serial.println("Helmet confirmed — stopping BT...");

  SerialBT.end();
  btStop();
  btActive = false;
  delay(500);

  SwitchToWiFi();
}

// ─────────────────────────────────────────────────────────────────
// PHASE 2 — Periodic alcohol recheck over BT
// ─────────────────────────────────────────────────────────────────
void CheckAlcoholViaBT() {

  // Timeout — no data received, go back to WiFi
  if (millis() - btStartTime >= BT_LISTEN_TIMEOUT) {
    Serial.println("BT timeout — resuming WiFi.");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("BT Timeout");
    lcd.setCursor(0, 1); lcd.print("Back to WiFi...");
    SerialBT.end();
    btStop();
    btActive         = false;
    lastAlcoholCheck = millis();
    delay(500);
    SwitchToWiFi();
    return;
  }

  if (!SerialBT.available()) return;

  String data = SerialBT.readStringUntil('\n');
  Serial.print("BT Recheck: "); Serial.println(data);

  int irIndex = data.indexOf("IR:");
  int mqIndex = data.indexOf("MQ3:");
  if (irIndex < 0 || mqIndex < 0) return;

  irValue  = data.substring(irIndex + 3, mqIndex - 1).toInt();
  mq3Value = data.substring(mqIndex + 4).toInt();

  helmetOn        = (irValue  == 0);
  alcoholDetected = (mq3Value > 1500);

  Serial.print("IR="); Serial.print(irValue);
  Serial.print(" MQ3="); Serial.println(mq3Value);

  // Stop BT radio before switching
  SerialBT.end();
  btStop();
  btActive = false;
  delay(500);

  // Switch back to WiFi to send Blynk update
  SwitchToWiFi();

  if (wifiStarted) {
    Blynk.virtualWrite(V4, helmetOn ? "Helmet ON" : "Helmet OFF");
    Blynk.virtualWrite(V5, mq3Value);
  }

  if (alcoholDetected) {
    // ── Drunk detected — kill engine permanently ──────────────
    digitalWrite(RELAY_PIN, LOW);
    analogWrite(PWM_PIN, 0);
    currentSpeed = 0;

    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("ALCOHOL DETECTED!");
    lcd.setCursor(0, 1); lcd.print("Engine OFF");
    lcd.setCursor(0, 2); lcd.print("Value: "); lcd.print(mq3Value);

    Serial.println("ALCOHOL — Engine locked OFF");

    if (wifiStarted) {
      Blynk.logEvent("alcohol",
        String("Alcohol detected! Value: ") + mq3Value);
    }

    // Lock — only Blynk keeps running
    while (true) {
      if (wifiStarted) Blynk.run();
      delay(1000);
    }
  }

  if (!helmetOn) {
    // Helmet removed mid-ride — engine off, wait for recheck
    digitalWrite(RELAY_PIN, LOW);
    analogWrite(PWM_PIN, 0);
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Helmet Removed!");
    lcd.setCursor(0, 1); lcd.print("Engine OFF");
    helmetConfirmed = false;   // force full helmet re-confirm
    return;
  }

  // ── All clear ─────────────────────────────────────────────────
  lastAlcoholCheck = millis();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Check OK");
  lcd.setCursor(0, 1); lcd.print("Resuming ride...");
  delay(1000);
  UpdateLCD();
}

// ─────────────────────────────────────────────────────────────────
void SendToBlynk() {
  if (!wifiStarted) return;

  Blynk.virtualWrite(V0, (int)currentSpeed);
  Blynk.virtualWrite(V4, helmetOn ? "Helmet ON" : "Helmet OFF");
  Blynk.virtualWrite(V5, mq3Value);

  if (gps.location.isValid()) {
    Blynk.virtualWrite(V2, Latitude);
    Blynk.virtualWrite(V3, Longitude);
    Blynk.virtualWrite(V8, url);
  }

  String zone = "No Zone";
  int    limit = 80;
  if      (schoolActive)      { zone = "School Zone";   limit = 30; }
  else if (hospitalActive)    { zone = "Hospital Zone"; limit = 20; }
  else if (residentialActive) { zone = "Residential";   limit = 40; }
  else if (highwayActive)     { zone = "Highway";       limit = 80; }

  Blynk.virtualWrite(V7, zone);
  Blynk.virtualWrite(V1, String(limit) + " kmph");
}

// ─────────────────────────────────────────────────────────────────
// LCD layout (fixed rows):
//   Row 0 — Helmet status
//   Row 1 — Alcohol status
//   Row 2 — Engine status
//   Row 3 — Zone / Speed (updated live)
// ─────────────────────────────────────────────────────────────────
void UpdateLCD() {
  lcd.setCursor(0, 0);
  lcd.print(helmetOn ? "Helmet: ON      " : "Helmet: OFF     ");

  lcd.setCursor(0, 1);
  lcd.print(alcoholDetected ? "Alcohol:DETECTED" : "Alcohol: Clear  ");

  lcd.setCursor(0, 2);
  if (!helmetOn)
    lcd.print("Engine:OFF(Helmet)  ");
  else if (alcoholDetected)
    lcd.print("Engine:OFF(Alcohol) ");
  else
    lcd.print("Engine: ON          ");
}

// ─────────────────────────────────────────────────────────────────
int speedToPWM(float speedKmh) {
  return constrain(map((long)speedKmh, 0, 100, 0, 255), 0, 255);
}

bool isAnyZoneActive() {
  return schoolActive || hospitalActive || residentialActive || highwayActive;
}

// ─────────────────────────────────────────────────────────────────
void HandleZones() {
  bool curSchool      = digitalRead(BTN_SCHOOL)      == HIGH;
  bool curHospital    = digitalRead(BTN_HOSPITAL)    == HIGH;
  bool curResidential = digitalRead(BTN_RESIDENTIAL) == HIGH;
  bool curHighway     = digitalRead(BTN_HIGHWAY)     == HIGH;

  if (curSchool && !prevSchool) {
    if (!schoolActive) {
      if (!isAnyZoneActive()) speedBeforeZone = currentSpeed;
      schoolActive = true;
      EnterZone("SCHOOL ZONE", 30);
    } else {
      schoolActive = false;
      ExitZone("SCHOOL ZONE");
      if (!isAnyZoneActive()) RestoreSpeed();
    }
  }
  if (curHospital && !prevHospital) {
    if (!hospitalActive) {
      if (!isAnyZoneActive()) speedBeforeZone = currentSpeed;
      hospitalActive = true;
      EnterZone("HOSPITAL ZONE", 20);
    } else {
      hospitalActive = false;
      ExitZone("HOSPITAL ZONE");
      if (!isAnyZoneActive()) RestoreSpeed();
    }
  }
  if (curResidential && !prevResidential) {
    if (!residentialActive) {
      if (!isAnyZoneActive()) speedBeforeZone = currentSpeed;
      residentialActive = true;
      EnterZone("RESIDENTIAL", 40);
    } else {
      residentialActive = false;
      ExitZone("RESIDENTIAL");
      if (!isAnyZoneActive()) RestoreSpeed();
    }
  }
  if (curHighway && !prevHighway) {
    if (!highwayActive) {
      if (!isAnyZoneActive()) speedBeforeZone = currentSpeed;
      highwayActive = true;
      EnterZone("HIGHWAY", 80);
    } else {
      highwayActive = false;
      ExitZone("HIGHWAY");
      if (!isAnyZoneActive()) RestoreSpeed();
    }
  }

  prevSchool      = curSchool;
  prevHospital    = curHospital;
  prevResidential = curResidential;
  prevHighway     = curHighway;

  if (!isAnyZoneActive()) {
    Speed = currentSpeed;
    analogWrite(PWM_PIN, speedToPWM(Speed));
    lcd.setCursor(0, 3);
    lcd.print("Normal  ");
    lcd.print((int)Speed);
    lcd.print("kmph      ");
  }
}

// ─────────────────────────────────────────────────────────────────
void OverSpeed() {
  if (currentSpeed > 80) {
    Serial.println("OVER SPEED ALERT");
    lcd.setCursor(0, 3);
    lcd.print("OVERSPEED! >80kmph  ");
    if (wifiStarted) {
      Blynk.virtualWrite(V1, "OVER SPEED!");
      Blynk.logEvent("overspeed",
        String("Speed: ") + (int)currentSpeed + " kmph");
    }
    SpeedLimit(80);
  }
}

// ─────────────────────────────────────────────────────────────────
void EnterZone(const char* name, int limit) {
  Serial.print("ENTERING : "); Serial.println(name);
  char buf[21];
  snprintf(buf, sizeof(buf), "%-12s %3dkmph", name, limit);
  lcd.setCursor(0, 3);
  lcd.print(buf);
  if (wifiStarted)
    Blynk.logEvent("zone_enter",
      String("Entered ") + name + " | Limit: " + limit + " kmph");
  SpeedLimit(limit);
}

// ─────────────────────────────────────────────────────────────────
void ExitZone(const char* name) {
  Serial.print("EXITING : "); Serial.println(name);
  lcd.setCursor(0, 3);
  lcd.print("Exiting zone...     ");
  delay(500);
}

// ─────────────────────────────────────────────────────────────────
void RestoreSpeed() {
  Serial.println("Restoring speed...");
  while (Speed < speedBeforeZone) {
    Speed += 1;
    if (Speed > speedBeforeZone) Speed = speedBeforeZone;
    analogWrite(PWM_PIN, speedToPWM(Speed));
    lcd.setCursor(0, 3);
    lcd.print("Restore ");
    lcd.print((int)Speed);
    lcd.print("kmph    ");
    delay(300);
  }
  Serial.println("Speed Restored.");
}

// ─────────────────────────────────────────────────────────────────
void InAccident() {
  digitalWrite(RELAY_PIN, LOW);
  analogWrite(PWM_PIN, 0);
  currentSpeed = 0;
  Serial.println("ACCIDENT DETECTED — Engine OFF");

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("!! ACCIDENT !!");
  lcd.setCursor(0, 1); lcd.print("Engine OFF");

  if (wifiStarted) {
    Blynk.virtualWrite(V6, "ACCIDENT DETECTED!");
    String alertMsg = "ACCIDENT DETECTED!";
    if (gps.location.isValid())
      alertMsg += " Location: " + url;
    Blynk.logEvent("accident", alertMsg);
    lcd.setCursor(0, 2); lcd.print("Alert Sent!");
  }
}

// ─────────────────────────────────────────────────────────────────
void InGPS() {
  if (gps.location.isValid()) {
    Latitude  = gps.location.lat();
    Longitude = gps.location.lng();
    url = "https://www.google.com/maps/place/"
          + String(Latitude, 6) + "," + String(Longitude, 6);
    Serial.print("Lat: "); Serial.print(Latitude, 6);
    Serial.print(" Lng: "); Serial.println(Longitude, 6);
  }
}

// ─────────────────────────────────────────────────────────────────
void SpeedLimit(int limit) {
  if (Speed > limit) {
    while (Speed > limit) {
      Speed--;
      analogWrite(PWM_PIN, speedToPWM(Speed));
      lcd.setCursor(0, 3);
      lcd.print("Slowing ");
      lcd.print((int)Speed);
      lcd.print("kmph    ");
      delay(200);
    }
    Serial.print("Speed matched: "); Serial.println(limit);
  } else {
    Speed = limit;
    analogWrite(PWM_PIN, speedToPWM(Speed));
  }
}
