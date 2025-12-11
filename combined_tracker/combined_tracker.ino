// Combined GPS Tracker + SMS Receiver with FreeRTOS
// Refactored into modular files

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include "Config.h"
#include "DisplayManager.h"
#include "KeyboardManager.h"
#include "Globals.h"
#include "Utils.h"
#include "Tasks.h"

// Global object definitions
TinyGPSPlus gps;
BluetoothSerial BT;
SIM800L sim800l;
HardwareSerial &SerialGPS = Serial;  // GPS uses Serial 0 (USB Serial)
HardwareSerial SerialSIM(1);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R2, U8X8_PIN_NONE);

// Global data
GPSData currentGPS;
SystemStatus systemStatus;
OperatingMode currentMode = MODE_TRACKER;
bool acknowledgmentEnabled = false;

// FreeRTOS Mutexes
SemaphoreHandle_t gpsMutex;
SemaphoreHandle_t loraMutex;
SemaphoreHandle_t smsMutex;

// Display and Keyboard State definitions (must be defined here)
// These are extern in Globals.h but need actual definitions
DisplayState displayState = {0};  // Initialize with zeros
KeyboardState keyboardState = {0};  // Initialize with zeros

void setup() {
  Serial.begin(9600);  // Match GPS baud rate (GPS on Serial 0)
  delay(1000);
  
  // Initialize Bluetooth
  BT.begin("Combined_Tracker_2");
  logToBoth("=== Combined Tracker ===");
  
  // Initialize GPS (uses Serial 0 - shared with USB)
  // Note: GPS and Serial Monitor cannot be used simultaneously
  logToBoth("GPS on Serial 0");
  
  // Initialize SIM800L
  SerialSIM.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
  delay(2000);
  
  if (sim800l.begin(SerialSIM)) {
    logToBoth("SIM800L OK");
    systemStatus.networkConnected = sim800l.checkNetwork();
    systemStatus.signalStrength = sim800l.signalStrength();
  } else {
    logToBoth("SIM800L FAIL");
  }
  
  SerialSIM.println("AT+CMGF=1");
  delay(200);
  // Store SMS in SIM memory instead of forwarding immediately
  SerialSIM.println("AT+CNMI=2,0,0,0,0"); // Changed from 2,1 to 2,0 (no forward)
  delay(200);
  SerialSIM.println("AT+CPMS=\"SM\",\"SM\",\"SM\""); // Use SIM storage
  delay(200);
  
  // Initialize LoRa
  SPI.begin();
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (LoRa.begin(LORA_FREQ)) {
    logToBoth("LoRa OK");
    systemStatus.loraConnected = true;
  } else {
    logToBoth("LoRa FAIL");
    systemStatus.loraConnected = false;
  }
  
  // Initialize system
  systemStatus.messageCounter = 0;
  systemStatus.lastSMSTime = 0;
  systemStatus.lastLoRaTime = 0;
  
  currentGPS.isValid = false;
  currentGPS.latitude = 0.0;
  currentGPS.longitude = 0.0;
  currentGPS.satellites = 0;
  
  // Initialize Display and Keyboard
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);
  
  initializeDisplay();
  initializeKeyboard();
  logToBoth("Display & Keyboard OK");
  
  // Create mutexes
  gpsMutex = xSemaphoreCreateMutex();
  loraMutex = xSemaphoreCreateMutex();
  smsMutex = xSemaphoreCreateMutex();
  
  // Create FreeRTOS tasks
  xTaskCreatePinnedToCore(gpsTask, "GPS", 4096, NULL, 2, &gpsTaskHandle, 0);
  xTaskCreatePinnedToCore(loraTask, "LoRa", 4096, NULL, 2, &loraTaskHandle, 1);
  xTaskCreatePinnedToCore(smsTask, "SMS", 4096, NULL, 1, &smsTaskHandle, 0);
  xTaskCreatePinnedToCore(bluetoothTask, "BT", 4096, NULL, 1, &bluetoothTaskHandle, 1);
  xTaskCreatePinnedToCore(displayTask, "Display", 4096, NULL, 1, &displayTaskHandle, 1);
  xTaskCreatePinnedToCore(keyboardTask, "Keyboard", 4096, NULL, 1, &keyboardTaskHandle, 0);
  
  logToBoth("All tasks started");
  logToBoth("Mode: TRACKER");
  
  BT.println("\n=== SYSTEM READY ===");
  BT.println("Debug Mode: ENABLED");
  BT.println("All operations will be logged");
  BT.println("\n--- Configuration ---");
  BT.println("GPS: Serial 0 @ 9600");
  BT.println("GSM: Serial 1 @ 9600");
  BT.println("LoRa: " + String(LORA_FREQ / 1E6) + " MHz");
  BT.println("GPS Send Interval: " + String(GPS_SEND_INTERVAL / 1000) + "s");
  BT.println("SMS Check Interval: " + String(SMS_UPDATE_INTERVAL / 1000) + "s");
  BT.println("ACK Mode: " + String(acknowledgmentEnabled ? "ON" : "OFF"));
  BT.println("\nType 'help' for commands");
  BT.println("=====================\n");
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
