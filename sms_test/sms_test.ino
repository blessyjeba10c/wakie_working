// SIM800L SMS Test with Bluetooth Bridge
// Hardware: ESP32 + SIM800L module
// Features: Bluetooth-GSM bridge + periodic "hello" SMS

#include "SIM800L.h"
#include "BluetoothSerial.h"

// SIM800L pins
#define SIM_RX_PIN 26  // ESP32 RX1 -> SIM800L TX
#define SIM_TX_PIN 25  // ESP32 TX1 -> SIM800L RX

// Phone numbers to send SMS to
const char* PHONE1 = "+918667399071";
const char* PHONE2 = "+919944127336";

// SMS send interval (milliseconds)
#define SMS_SEND_INTERVAL 30000  // 30 seconds

// Create objects
HardwareSerial SerialSIM(1);
SIM800L sim800l;
BluetoothSerial BT;

unsigned long lastSMSTime = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== SIM800L + Bluetooth Bridge ===");
  
  // Initialize Bluetooth
  BT.begin("SIM800L_Bridge");
  Serial.println("Bluetooth started: SIM800L_Bridge");
  BT.println("=== SIM800L Bridge ===");
  BT.println("Type AT commands or watch SMS sends");
  
  // Initialize SIM800L serial
  SerialSIM.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
  delay(2000);
  
  // Initialize SIM800L module
  if (sim800l.begin(SerialSIM)) {
    Serial.println("SIM800L initialized successfully!");
    BT.println("SIM800L: OK");
    
    // Check network registration
    if (sim800l.checkNetwork()) {
      Serial.println("Network connected!");
      BT.println("Network: Connected");
      
      // Get signal strength
      int signal = sim800l.signalStrength();
      Serial.println("Signal strength: " + String(signal) + "/31");
      BT.println("Signal: " + String(signal) + "/31");
      
    } else {
      Serial.println("No network connection!");
      BT.println("Network: Disconnected");
    }
  } else {
    Serial.println("SIM800L initialization failed!");
    BT.println("SIM800L: FAIL");
  }
  
  BT.println("\n--- Bridge Active ---");
  BT.println("Auto-sending 'hello' every 30s");
  Serial.println("\n=== Ready ===");
}

void loop() {
  // Bluetooth to GSM bridge
  if (BT.available()) {
    char c = BT.read();
    SerialSIM.write(c);
  }
  
  // GSM to Bluetooth bridge
  if (SerialSIM.available()) {
    char c = SerialSIM.read();
    BT.write(c);
  }
  
  // Periodic SMS sending
  if (millis() - lastSMSTime >= SMS_SEND_INTERVAL) {
    lastSMSTime = millis();
    
    Serial.println("\n--- Sending periodic SMS ---");
    BT.println("\n--- Sending 'hello' SMS ---");
    
    // Send to first number
    Serial.println("Sending to: " + String(PHONE1));
    BT.println("To: " + String(PHONE1));
    if (sendSMS(PHONE1, "hello")) {
      Serial.println("✓ Sent to " + String(PHONE1));
      BT.println("✓ Sent");
    } else {
      Serial.println("✗ Failed to " + String(PHONE1));
      BT.println("✗ Failed");
    }
    delay(2000);
    
    // Send to second number
    Serial.println("Sending to: " + String(PHONE2));
    BT.println("To: " + String(PHONE2));
    if (sendSMS(PHONE2, "hello")) {
      Serial.println("✓ Sent to " + String(PHONE2));
      BT.println("✓ Sent");
    } else {
      Serial.println("✗ Failed to " + String(PHONE2));
      BT.println("✗ Failed");
    }
    
    BT.println("--- Done, next in 30s ---\n");
  }
}

// Helper function to send SMS
bool sendSMS(const char* phoneNumber, const char* message) {
  char* phone = const_cast<char*>(phoneNumber);
  char* msg = const_cast<char*>(message);
  return sim800l.sendSMS(phone, msg);
}
