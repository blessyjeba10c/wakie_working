// Simple SIM800L SMS Test - Send "hello" to two numbers
// Hardware: ESP32 + SIM800L module

#include "SIM800L.h"

// SIM800L pins
#define SIM_RX_PIN 26  // ESP32 RX1 -> SIM800L TX
#define SIM_TX_PIN 25  // ESP32 TX1 -> SIM800L RX

// Phone numbers to send SMS to
const char* PHONE1 = "+918667399071";
const char* PHONE2 = "+919944127336";

// Create SIM800L object and Serial
HardwareSerial SerialSIM(1);
SIM800L sim800l;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== SIM800L SMS Test ===");
  
  // Initialize SIM800L serial
  SerialSIM.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
  delay(2000);
  
  // Initialize SIM800L module
  if (sim800l.begin(SerialSIM)) {
    Serial.println("SIM800L initialized successfully!");
    
    // Check network registration
    if (sim800l.checkNetwork()) {
      Serial.println("Network connected!");
      
      // Get signal strength
      int signal = sim800l.signalStrength();
      Serial.println("Signal strength: " + String(signal) + "/31");
      
      // Send SMS to first number
      Serial.println("\nSending SMS to: " + String(PHONE1));
      if (sendSMS(PHONE1, "hello")) {
        Serial.println("✓ SMS sent to " + String(PHONE1));
      } else {
        Serial.println("✗ Failed to send to " + String(PHONE1));
      }
      delay(2000);
      
      // Send SMS to second number
      Serial.println("\nSending SMS to: " + String(PHONE2));
      if (sendSMS(PHONE2, "hello")) {
        Serial.println("✓ SMS sent to " + String(PHONE2));
      } else {
        Serial.println("✗ Failed to send to " + String(PHONE2));
      }
      
      Serial.println("\n=== Done ===");
      
    } else {
      Serial.println("✗ No network connection!");
    }
  } else {
    Serial.println("✗ SIM800L initialization failed!");
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
