#ifndef CONFIG_H
#define CONFIG_H

// Device Configuration
#define SOLDIER_ID "BSF1875"
#define DEVICE_TYPE "VHF"

// LoRa Configuration
#define LORA_FREQ 433E6

// GPS Pins (Serial 0 - USB Serial)
#define GPS_RX_PIN 3   // RX0
#define GPS_TX_PIN 1   // TX0

// SIM800L Pins
#define SIM_RX_PIN 25
#define SIM_TX_PIN 26

// LoRa Pins
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 27

// I2C Pins (Display & Keyboard)
#define I2C_SDA 21
#define I2C_SCL 22

// Receiver Phone Numbers
#define NUM_RECEIVERS 2
extern const char* RECEIVER_PHONES[];

// Timing Configuration
#define GPS_UPDATE_INTERVAL 100      // ms
#define LORA_UPDATE_INTERVAL 50      // ms (faster for better reception)
#define SMS_UPDATE_INTERVAL 3000     // ms (reduced priority, check every 3s)
#define DISPLAY_UPDATE_INTERVAL 200  // ms
#define KEYBOARD_SCAN_INTERVAL 50    // ms
#define GPS_SEND_INTERVAL 10000      // ms (10 seconds)
#define LORA_ACK_TIMEOUT 5000        // ms (5 seconds)

#endif
