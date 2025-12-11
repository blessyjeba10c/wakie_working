// Simple SIM800L SMS Test - Send "hello" to two numbers
// Hardware: ESP32 + SIM800L module

#include "SIM800L.h"
#include "BluetoothSerial.h"

// SIM800L pins
#define SIM_RX_PIN 26  // ESP32 RX1 -> SIM800L TX
#define SIM_TX_PIN 25  // ESP32 TX1 -> SIM800L RX

// Phone numbers to send SMS to
const char* PHONE1 = "+918667399071";
const char* PHONE2 = "+919944127336";

// Create SIM800L object and Serial
HardwareSerial SerialSIM(1);
SIM800L sim800l;
BluetoothSerial BT;

// Helper to log to both Serial and Bluetooth
void logBoth(String message) {
  Serial.println(message);
  BT.println(message);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize Bluetooth
  BT.begin("GPS_Tracker_SMS_Test");
  delay(1000);
  
  logBoth("=== SIM800L SMS Test ===");
  logBoth("Bluetooth: GPS_Tracker_SMS_Test");
  
  // Initialize SIM800L serial
  SerialSIM.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
  delay(2000);
  
  // Initialize SIM800L module
  if (sim800l.begin(SerialSIM)) {
    logBoth("SIM800L initialized successfully!");
    
    // Check network registration
    if (sim800l.checkNetwork()) {
      logBoth("Network connected!");
      
      // Get signal strength
      int signal = sim800l.signalStrength();
      logBoth("Signal strength: " + String(signal) + "/31");
      
      // Send SMS to first number
      logBoth("\nSending SMS to: " + String(PHONE1));
      if (sendSMS(PHONE1, "hello")) {
        logBoth("✓ SMS sent to " + String(PHONE1));
      } else {
        logBoth("✗ Failed to send to " + String(PHONE1));
      }
      delay(2000);
      
      // Send SMS to second number
      logBoth("\nSending SMS to: " + String(PHONE2));
      if (sendSMS(PHONE2, "hello")) {
        logBoth("✓ SMS sent to " + String(PHONE2));
      } else {
        logBoth("✗ Failed to send to " + String(PHONE2));
      }
      
      logBoth("\n=== Done ===");
      
    } else {
      logBoth("✗ No network connection!");
    }
  } else {
    logBoth("✗ SIM800L initialization failed!");
  }
}

void loop() {
  // Nothing to do - one-time send
  delay(1000);
}

// Helper function to send SMS
bool sendSMS(const char* phoneNumber, const char* message) {
  char* phone = const_cast<char*>(phoneNumber);
  char* msg = const_cast<char*>(message);
  return sim800l.sendSMS(phone, msg);
}
