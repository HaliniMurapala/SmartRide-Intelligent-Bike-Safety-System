
# SmartRide - Intelligent Bike Safety System

A bike safety system built using two ESP32 microcontrollers 
as part of my embedded systems training at Radar Institute, 
Bengaluru. This project enforces real road safety rules and 
sends live alerts to a mobile app.

---

## Why I Built This

In India, most bike accidents happen because of three reasons:
- Rider not wearing helmet
- Drunk driving
- Overspeeding near schools, hospitals and residential areas

I wanted to build something that actually solves these problems 
using embedded systems — not just detect them but actively 
prevent them.

---

## What It Does

- Checks if the rider is wearing helmet before starting the bike
- Detects alcohol in the rider's breath
- Rechecks alcohol every 60 seconds during the ride
- Automatically reduces speed when entering sensitive zones:
  - School Zone - 30 kmph
  - Hospital Zone - 20 kmph
  - Residential Area - 40 kmph
  - Highway - 80 kmph
- Cuts engine and sends SOS alert if accident is detected
- Tracks live GPS location and sends Google Maps link on accident
- Shows all status on a 20x4 LCD display
- Sends real time data to Blynk mobile app

---

## How It Works

The system has two ESP32 nodes:

Node 1 - Helmet Side:
- IR sensor checks if helmet is worn
- MQ3 sensor reads alcohol level
- Sends data wirelessly to bike node via Bluetooth

Node 2 - Bike Side:
- Receives helmet and alcohol data via Bluetooth
- If helmet is on and no alcohol - starts the engine
- GPS module tracks live location
- Speed is controlled automatically using PWM
- Every 60 seconds it switches back to Bluetooth 
  to recheck alcohol level
- If accident button is pressed - engine cuts off 
  immediately and SOS alert is sent to phone via Blynk

---

## Components Used

- 2 x ESP32 DevKit V1
- IR Sensor - helmet detection
- MQ3 Alcohol Sensor
- NEO-6M GPS Module
- Relay Module - ignition control
- 20x4 I2C LCD Display
- Potentiometer - speed simulation
- Push buttons - zone selection and accident trigger

---

## Technologies and Protocols

- Embedded C / Arduino Framework
- Bluetooth Classic - helmet to bike communication
- WiFi + Blynk IoT - mobile dashboard and alerts
- UART - GPS communication
- I2C - LCD display
- ADC - alcohol sensor reading
- PWM - speed control
- GPIO - relay, buttons, sensors

---

## Blynk App Dashboard

| Virtual Pin | What it shows |
|---|---|
| V0 | Live speed in kmph |
| V1 | Current speed limit |
| V2 | GPS Latitude |
| V3 | GPS Longitude |
| V4 | Helmet status |
| V5 | Alcohol sensor value |
| V6 | Accident alert |
| V7 | Current zone |
| V8 | Google Maps link |

---

## Project Structure

SmartRide-Intelligent-Bike-Safety-System/
├── Helmet_Node/
│   └── helmet_node.ino
├── Bike_Node/
│   └── bike_node.ino
└── README.md

---

## What I Learned

Building this project taught me how to:
- Switch between Bluetooth and WiFi on the same ESP32
- Control hardware like relay and motor using PWM
- Parse GPS data using TinyGPS++ library
- Send real time IoT alerts using Blynk
- Handle multiple sensors and inputs simultaneously

---

## About Me

Murapala Halini
Embedded Systems Engineer - Fresher
Bengaluru, India
LinkedIn: linkedin.com/in/halinimurapala
GitHub: github.com/HaliniMurapala
