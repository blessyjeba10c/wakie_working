#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>
#include <TinyGPS++.h>
#include "BluetoothSerial.h"
#include "SIM800L.h"
#include <U8g2lib.h>

// Operating Mode
enum OperatingMode {
  MODE_TRACKER,
  MODE_GROUND_STATION
};

// GPS Data Structure
struct GPSData {
  double latitude;
  double longitude;
  bool isValid;
  uint32_t satellites;
  String timestamp;
};

// System Status Structure
struct SystemStatus {
  bool loraConnected;
  bool networkConnected;
  int signalStrength;
  unsigned long messageCounter;
  String lastSMS;
  unsigned long lastSMSTime;
  String lastLoRa;
  unsigned long lastLoRaTime;
};

// Global Variables
extern TinyGPSPlus gps;
extern BluetoothSerial BT;
extern SIM800L sim800l;
// SerialGPS is defined as Serial in main file
extern HardwareSerial SerialSIM;

extern GPSData currentGPS;
extern SystemStatus systemStatus;
extern OperatingMode currentMode;
extern bool acknowledgmentEnabled;

// FreeRTOS Mutexes
extern SemaphoreHandle_t gpsMutex;
extern SemaphoreHandle_t loraMutex;
extern SemaphoreHandle_t smsMutex;

// Display and Keyboard
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

#endif
