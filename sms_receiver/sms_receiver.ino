// SMS + LoRa Receiver with Bluetooth Display
// Receives SMS and LoRa messages, displays both via Bluetooth Serial
// Uses SIM800L for GSM and LoRa module for radio communication

#include "BluetoothSerial.h"
#include "SIM800L.h"
#include <LoRa.h>
#include <SPI.h>

// Hardware serial for SIM800L
HardwareSerial SerialSIM(1);

// SIM800L pins - adjust for your wiring
const int SIM_RX_PIN = 26; // ESP32 pin to SIM800L TX
const int SIM_TX_PIN = 25; // ESP32 pin to SIM800L  RX

// LoRa pins - adjust for your board
const int LORA_SS = 5;
const int LORA_RST = 14;
const int LORA_DIO0 = 27;
const long LORA_FREQ = 433E6; // 866 MHz

// Objects
BluetoothSerial BT;
SIM800L sim800l;

// Message storage
String lastSMS = "";
unsigned long lastSMSTime = 0;
String lastLoRa = "";
unsigned long lastLoRaTime = 0;
bool loraConnected = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize Bluetooth
  BT.begin("SMS_Receiver");
  Serial.println("Bluetooth started as 'SMS_Receiver'");
  
  // Initialize SIM800L serial
  SerialSIM.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
  delay(2000);
  
  // Initialize SIM800L driver
  if (sim800l.begin(SerialSIM)) {
    Serial.println("SIM800L initialized successfully");
    BT.println("=== SMS RECEIVER READY ===");
    BT.println("SIM800L initialized successfully");
  } else {
    Serial.println("SIM800L initialization failed!");
    BT.println("ERROR: SIM800L initialization failed!");
    return;
  }
  
  // Configure SMS notifications
  SerialSIM.println("AT+CMGF=1"); // Text mode
  delay(200);
  SerialSIM.println("AT+CNMI=2,1,0,0,0"); // Enable SMS notifications
  delay(200);
  
  // Check network
  if (sim800l.checkNetwork()) {
    Serial.println("Network connected");
    BT.println("Network: Connected");
    BT.println("Signal: " + String(sim800l.signalStrength()) + " dBm");
  } else {
    Serial.println("No network connection");
    BT.println("Network: Disconnected");
  }
  
  // Initialize LoRa
  SPI.begin();
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    Serial.println("Starting LoRa failed!");
    BT.println("WARNING: LoRa initialization failed!");
    loraConnected = false;
  } else {
    Serial.println("LoRa initialized");
    BT.println("LoRa initialized successfully");
    loraConnected = true;
  }
  
  BT.println("========================");
  BT.println("Waiting for SMS and LoRa messages...");
  BT.println("Type 'status' for system info");
  BT.println("");
}

void loop() {
  // Handle Bluetooth commands
  handleBluetoothCommands();
  
  // Handle incoming SMS
  handleIncomingSMS();
  
  // Handle incoming LoRa
  handleIncomingLoRa();
  
  delay(10);
}

// Handle Bluetooth serial commands
void handleBluetoothCommands() {
  if (BT.available()) {
    String command = BT.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    if (command == "status") {
      showStatus();
    }
    else if (command == "help") {
      BT.println("=== COMMANDS ===");
      BT.println("status - Show system status");
      BT.println("help   - Show this help");
      BT.println("===============");
    }
    else if (command == "test") {
      BT.println("System is working!");
      BT.println("Last SMS: " + (lastSMS.length() > 0 ? lastSMS : "None"));
      BT.println("Last LoRa: " + (lastLoRa.length() > 0 ? lastLoRa : "None"));
    }
    else {
      BT.println("Unknown command. Type 'help' for available commands.");
    }
  }
}

// Show system status
void showStatus() {
  BT.println("=== DUAL RECEIVER STATUS ===");
  BT.println("Device: SMS + LoRa Receiver");
  BT.println("GSM Network: " + String(sim800l.checkNetwork() ? "Connected" : "Disconnected"));
  BT.println("GSM Signal: " + String(sim800l.signalStrength()) + " dBm");
  BT.println("LoRa Status: " + String(loraConnected ? "Connected" : "Disconnected"));
  BT.println("Last SMS: " + (lastSMSTime > 0 ? String(millis() - lastSMSTime) + "ms ago" : "Never"));
  BT.println("Last LoRa: " + (lastLoRaTime > 0 ? String(millis() - lastLoRaTime) + "ms ago" : "Never"));
  BT.println("Total Uptime: " + String(millis() / 1000) + " seconds");
  BT.println("============================");
}

// Handle incoming SMS messages
void handleIncomingSMS() {
  static bool waitingForBody = false;
  static String smsHeader = "";
  static String senderNumber = "";
  
  while (SerialSIM.available()) {
    String line = SerialSIM.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    
    // Debug: show raw SIM responses
    Serial.println("[SIM] " + line);
    
    if (line.startsWith("+CMT:")) {
      // SMS header: +CMT: "+1234567890","","21/12/11,14:30:15+00"
      smsHeader = line;
      waitingForBody = true;
      
      // Extract sender number
      int firstQuote = line.indexOf('"');
      int secondQuote = line.indexOf('"', firstQuote + 1);
      if (firstQuote != -1 && secondQuote != -1) {
        senderNumber = line.substring(firstQuote + 1, secondQuote);
      }
      
      Serial.println("SMS incoming from: " + senderNumber);
      continue;
    }
    
    if (waitingForBody && line.length() > 0) {
      String messageBody = line;
      messageBody.trim();
      
      // Store last SMS info
      lastSMS = messageBody;
      lastSMSTime = millis();
      
      // Display SMS on Bluetooth
      BT.println("");
      BT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
      BT.println("📱 NEW SMS RECEIVED");
      BT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
      BT.println("From: " + senderNumber);
      BT.println("Time: " + getTimeStamp());
      BT.println("Message:");
      BT.println("\"" + messageBody + "\"");
      BT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
      BT.println("");
      
      // Also log to Serial
      Serial.println("SMS from " + senderNumber + ": " + messageBody);
      
      waitingForBody = false;
      smsHeader = "";
      senderNumber = "";
    }
  }
}

// Get timestamp string
String getTimeStamp() {
  unsigned long currentTime = millis();
  unsigned long seconds = currentTime / 1000;
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  
  seconds = seconds % 60;
  minutes = minutes % 60;
  hours = hours % 24;
  
  return String(hours) + ":" + 
         (minutes < 10 ? "0" : "") + String(minutes) + ":" + 
         (seconds < 10 ? "0" : "") + String(seconds);
}

// Handle incoming LoRa messages
void handleIncomingLoRa() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    incoming.trim();
    
    if (incoming.length() > 0) {
      // Store last LoRa info
      lastLoRa = incoming;
      lastLoRaTime = millis();
      
      // Get signal info
      int rssi = LoRa.packetRssi();
      float snr = LoRa.packetSnr();
      
      // Display LoRa message on Bluetooth
      BT.println("");
      BT.println("📡━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
      BT.println("📻 NEW LORA MESSAGE RECEIVED");
      BT.println("📡━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
      BT.println("Time: " + getTimeStamp());
      BT.println("RSSI: " + String(rssi) + " dBm");
      BT.println("SNR: " + String(snr) + " dB");
      BT.println("Size: " + String(packetSize) + " bytes");
      BT.println("Data:");
      BT.println(incoming);
      BT.println("📡━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
      BT.println("");
      
      // Also log to Serial
      Serial.println("LoRa received (" + String(rssi) + "dBm): " + incoming);
    }
  }
}