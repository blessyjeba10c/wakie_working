// GPS + LoRa + SIM800L tracker for ESP32
// - Uses TinyGPS++ for NEO-6M GPS
// - Uses LoRa library for SX127x
// - Uses SIM800L via HardwareSerial (AT commands) for GSM (SMS fallback)
// - Uses BluetoothSerial to print received locations as JSON
// Configure ROLE as SENDER or RECEIVER

#include <Arduino.h>
#include <TinyGPS++.h>
#include <LoRa.h>
#include "BluetoothSerial.h"
#include "SIM800L.h"

// --- Configuration ---
// ROLE: set to true for sender, false for receiver
bool ROLE_IS_SENDER = true; // can be toggled at runtime

// Toggle button pin (optional - connect button between this pin and GND)
const int TOGGLE_BUTTON_PIN = 2;

// Identity
const char* SOLDIER_ID = "BSF1875";
const char* DEVICE_TYPE = "VHF";

// LoRa frequency
const long LORA_FREQ = 433E6;

// Hardware serial ports (ESP32)
// GPS (NEO-6M) connected to Serial 0 (USB Serial)
// SIM800L connected to Serial1 (RX1, TX1)
HardwareSerial SerialSIM(1);

// Serial pins - change to match your wiring
const int GPS_RX_PIN = 6; // RX2 pin (to TX of GPS)
const int GPS_TX_PIN = 17; // TX2 pin (to RX of GPS)
const int SIM_RX_PIN = 26; // RX1 pin (to TX of SIM800L)
const int SIM_TX_PIN = 25; // TX1 pin (to RX of SIM800L)

// LoRa pins - adjust if needed for your board
const int LORA_SS = 5;
const int LORA_RST = 14;
const int LORA_DIO0 = 27;

// Receiver phone numbers for GSM fallback (E.164) - only used by sender
const char* RECEIVER_PHONES[] = {
  "+918667399071", // Primary receiver
  "+919944127336"  // Secondary receiver - change to actual second number
};
const int NUM_RECEIVERS = 2;

// Timing
// No ACK system - send on both LoRa and GSM

TinyGPSPlus gps;
BluetoothSerial BT;
SIM800L sim800l;

unsigned long messageCounter = 0;
bool loraConnected = false;

// Helper: log to both Serial and Bluetooth
void logToBoth(const String &message) {
  Serial.println(message);
  BT.println("[LOG] " + message);
}

// Handle Bluetooth serial commands
void handleBluetoothCommands() {
  if (BT.available()) {
    String command = BT.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    logToBoth("BT Command: " + command);
    
    if (command == "sender") {
      ROLE_IS_SENDER = true;
      logToBoth("Mode switched to: SENDER");
      BT.println(">>> MODE: SENDER - Will send GPS data every 10 seconds");
    }
    else if (command == "receiver") {
      ROLE_IS_SENDER = false;
      logToBoth("Mode switched to: RECEIVER (Ground Station)");
      BT.println(">>> MODE: GROUND STATION");
      BT.println(">>> - Listening for tracker data");
      BT.println(">>> - Type 'sms <message>' to send instructions");
      BT.println(">>> Example: sms Move to coordinates 28.6139, 77.2090");
    }
    else if (command == "status") {
      BT.println("=== TRACKER STATUS ===");
      String mode = ROLE_IS_SENDER ? "TRACKER (Sender)" : "GROUND STATION (Receiver)";
      BT.println("Mode: " + mode);
      BT.println("LoRa: " + String(loraConnected ? "Connected" : "Disconnected"));
      BT.println("GPS Fix: " + String(gps.location.isValid() ? "Valid" : "No Fix"));
      if (gps.location.isValid()) {
        BT.println("Location: " + String(gps.location.lat(), 6) + ", " + String(gps.location.lng(), 6));
      }
      BT.println("Satellites: " + String(gps.satellites.value()));
      BT.println("Network: " + String(sim800l.checkNetwork() ? "Connected" : "Disconnected"));
      if (!ROLE_IS_SENDER) {
        BT.println("Ground Station Functions: SMS sending enabled");
      }
      BT.println("===================");
    }
    else if (command.startsWith("sms ")) {
      // Command format: "sms your message here"
      if (command.length() > 4) {
        String message = command.substring(4); // Remove "sms " prefix
        message.trim();
        if (message.length() > 0) {
          logToBoth("Sending guiding SMS: " + message);
          BT.println(">>> Sending SMS to tracker...");
          bool smsOk = sendSMS(RECEIVER_PHONES[0], message); // Send guiding SMS to primary receiver
          if (smsOk) {
            BT.println(">>> SMS sent successfully!");
            logToBoth("Guiding SMS sent successfully");
          } else {
            BT.println(">>> SMS send failed!");
            logToBoth("Guiding SMS send failed");
          }
        } else {
          BT.println("Error: Empty message. Use: sms your message here");
        }
      } else {
        BT.println("Error: No message provided. Use: sms your message here");
      }
    }
    else if (command == "help") {
      BT.println("=== AVAILABLE COMMANDS ===");
      BT.println("sender      - Switch to sender mode");
      BT.println("receiver    - Switch to receiver mode (ground station)");
      BT.println("status      - Show current status");
      BT.println("sms <msg>   - Send guiding SMS to tracker");
      BT.println("help        - Show this help");
      BT.println("\nExample: sms Return to base immediately");
      BT.println("=========================");
    }
    else {
      BT.println("Unknown command. Type 'help' for available commands.");
    }
  }
}

// Check if LoRa is connected by trying to reinitialize
bool checkLoRaConnection() {
  // Try to reinitialize LoRa - if it fails, module is disconnected
  return LoRa.begin(LORA_FREQ);
}

// Helper: format GPS timestamp as ISO8601 Z (UTC)
String formatGpsTimestamp(TinyGPSDate &d, TinyGPSTime &t) {
  if (!d.isValid() || !t.isValid()) return String("1970-01-01T00:00:00Z");
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
    d.year(), d.month(), d.day(), t.hour(), t.minute(), t.second());
  return String(buf);
}

// Minimal helper to extract simple string values from a JSON-like payload
String extractJsonValue(const String &payload, const String &key) {
  String pattern = String("\"") + key + "\":\"";
  int idx = payload.indexOf(pattern);
  if (idx == -1) return String();
  idx += pattern.length();
  int end = payload.indexOf('"', idx);
  if (end == -1) return String();
  return payload.substring(idx, end);
}

// Send SMS via SIM800L driver to single number
bool sendSMSToNumber(const char *toNumber, const String &message) {
  // Convert String to char array for SIM800L driver
  char msgBuffer[message.length() + 1];
  message.toCharArray(msgBuffer, message.length() + 1);
  
  // Cast const char* to char* (SIM800L driver expects non-const)
  char *phoneBuffer = const_cast<char*>(toNumber);
  
  bool result = sim800l.sendSMS(phoneBuffer, msgBuffer);
  return result;
}

// Send SMS to all configured receiver numbers
bool sendSMS(const String &message) {
  logToBoth("[GSM] Sending SMS to " + String(NUM_RECEIVERS) + " numbers...");
  
  bool overallSuccess = false;
  
  for (int i = 0; i < NUM_RECEIVERS; i++) {
    logToBoth("[GSM] Sending to " + String(RECEIVER_PHONES[i]));
    
    bool result = sendSMSToNumber(RECEIVER_PHONES[i], message);
    
    if (result) {
      logToBoth("[GSM] SMS sent successfully to " + String(RECEIVER_PHONES[i]));
      overallSuccess = true; // At least one succeeded
    } else {
      logToBoth("[GSM] SMS send failed to " + String(RECEIVER_PHONES[i]));
    }
    
    delay(1000); // Small delay between sends
  }
  
  return overallSuccess;
}

// Send SMS to specific number (for ground station guiding messages)
bool sendSMS(const char *toNumber, const String &message) {
  logToBoth("[GSM] Sending SMS to specific number...");
  return sendSMSToNumber(toNumber, message);
}



// Send payload via LoRa (no ACK needed)
void sendViaLoRa(const String &payload) {
  if (loraConnected && checkLoRaConnection()) {
    logToBoth("[LoRa] Sending data");
    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();
    logToBoth("[LoRa] Data sent");
  } else {
    logToBoth("[LoRa] Module not connected, skipping LoRa send");
  }
}

// Create JSON payload string for sending; message_id included for ACKs
String createPayload(double lat, double lon, const String &timestamp, const String &msgId) {
  String s = "{";
  s += "\"soldier_id\":\"" + String(SOLDIER_ID) + "\",";
  s += "\"device_type\":\"" + String(DEVICE_TYPE) + "\",";
  s += "\"latitude\":" + String(lat, 6) + ",";
  s += "\"longitude\":" + String(lon, 6) + ",";
  s += "\"communication_mode\":\"\","; // will be set by receiver
  s += "\"timestamp\":\"" + timestamp + "\",";
  s += "\"message_id\":\"" + msgId + "\"";
  s += "}";
  return s;
}

// Print to Bluetooth Serial in required format (ensures communication_mode set)
void publishToBluetooth(const String &jsonPayload, const char *commMode) {
  // Build output by replacing or inserting communication_mode
  String out = jsonPayload;
  int idx = out.indexOf("\"communication_mode\":");
  if (idx != -1) {
    int start = out.indexOf('"', idx + 1);
    int firstColon = out.indexOf(':', idx);
    int quote1 = out.indexOf('"', firstColon);
    int quote2 = out.indexOf('"', quote1 + 1);
    if (quote1 != -1 && quote2 != -1) {
      // find bounds of existing value
      int valStart = quote1 + 1;
      int valEnd = quote2 - 1;
      // replace value
      out = out.substring(0, valStart) + String(commMode) + out.substring(valEnd + 2);
    } else {
      // fallback: do a simple insert after key
      out.replace("\"communication_mode\":\"\"", String("\"communication_mode\":\"") + commMode + "\"");
    }
  }
  BT.println(out);
  logToBoth("[BT] " + out);
}

// Receiver: process an incoming LoRa packet and reply ACK
void handleLoRaReceive() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();
    incoming.trim();
    logToBoth("[LoRa RX] " + incoming);
    publishToBluetooth(incoming, "Lora");
  }
}

// Read lines from SIM and handle incoming SMS notifications (CMT)
void handleSimIncoming() {
  static bool waitingForBody = false;
  static String smsHeader = "";
  
  while (SerialSIM.available()) {
    String line = SerialSIM.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;
    
    logToBoth("[SIM] " + line);
    
    if (line.startsWith("+CMT:")) {
      // SMS notification header - extract sender info
      smsHeader = line;
      waitingForBody = true;
      logToBoth("Incoming SMS detected");
      continue;
    }
    
    if (waitingForBody) {
      String body = line;
      body.trim();
      
      // Print SMS details to Serial and Bluetooth
      logToBoth("=== INCOMING SMS ===");
      logToBoth("Header: " + smsHeader);
      logToBoth("Message: " + body);
      logToBoth("==================");
      
      // Check if it's a JSON payload (tracker message)
      if (body.indexOf("soldier_id") != -1 || body.indexOf("latitude") != -1) {
        publishToBluetooth(body, "GSM");
        logToBoth("SMS contains tracker data");
        BT.println(">>> TRACKER DATA RECEIVED VIA SMS");
      } else {
        // Regular SMS - could be guiding message or other
        logToBoth("Regular SMS received");
        BT.println(">>> GUIDING MESSAGE: " + body);
        
        // If in sender mode, this is a guiding message for this tracker
        if (ROLE_IS_SENDER) {
          BT.println(">>> INSTRUCTION RECEIVED: " + body);
          logToBoth("Received instruction: " + body);
        }
      }
      
      waitingForBody = false;
      smsHeader = "";
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Bluetooth serial
  BT.begin("Tracker_BT2");
  delay(1000); // Wait for BT to initialize
  
  logToBoth("GPS-LoRa-GSM Tracker starting...");
  logToBoth("Bluetooth started as 'Tracker_BT'");
  logToBoth("GPS using Serial 0 (USB Serial)");

  // SIM serial and SIM800L driver initialization
  SerialSIM.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
  delay(1000);
  
  // Initialize SIM800L driver
  if (sim800l.begin(SerialSIM)) {
    logToBoth("SIM800L initialized successfully");
  } else {
    logToBoth("SIM800L initialization failed!");
  }

  // init LoRa
  SPI.begin();
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    logToBoth("Starting LoRa failed!");
    loraConnected = false;
  } else {
    logToBoth("LoRa initialized");
    loraConnected = checkLoRaConnection();
    logToBoth(loraConnected ? "LoRa module connected" : "LoRa module not detected");
  }

  // Configure SMS notifications for incoming messages
  SerialSIM.println("AT+CNMI=2,1,0,0,0"); // Enable SMS notifications
  delay(200);
  
  // Check network status
  if (sim800l.checkNetwork()) {
    logToBoth("Network connected");
    logToBoth("Signal strength: " + String(sim800l.signalStrength()));
  } else {
    logToBoth("No network connection");
  }

  logToBoth(ROLE_IS_SENDER ? "Initial Role: SENDER" : "Initial Role: RECEIVER");
  
  // Show available commands on Bluetooth
  BT.println("\n=== GPS TRACKER & GROUND STATION ===");
  BT.println("Type 'help' for commands");
  String mode = ROLE_IS_SENDER ? "TRACKER (Sender)" : "GROUND STATION (Receiver)";
  BT.println("Current mode: " + mode);
  if (!ROLE_IS_SENDER) {
    BT.println("Ground Station: Use 'sms <message>' to send instructions");
  }
  BT.println("=====================================");
}

void loop() {
  // Handle Bluetooth commands for live mode switching
  handleBluetoothCommands();
  
  // feed GPS parser from Serial 0
  while (Serial.available()) {
    char c = Serial.read();
    gps.encode(c);
  }

  // Always handle incoming LoRa and SIM messages (receiver path)
  handleLoRaReceive();
  handleSimIncoming();

  if (ROLE_IS_SENDER) {
    // Show live GPS status every 10 seconds
    static unsigned long lastStatusTime = 0;
    if (millis() - lastStatusTime > 5000) { // Status every 5 seconds
      BT.println("[GPS] Satellites: " + String(gps.satellites.value()) + ", Fix: " + String(gps.location.isValid() ? "YES" : "NO"));
      if (gps.location.isValid()) {
        BT.println("[GPS] Lat: " + String(gps.location.lat(), 6) + ", Lng: " + String(gps.location.lng(), 6));
      }
      lastStatusTime = millis();
    }
    
    // Check GPS fix status and send data every 10 seconds
    static unsigned long lastSendTime = 0;
    if (millis() - lastSendTime >= 10000) { // Send every 10 seconds
      lastSendTime = millis();
      
      if (gps.location.isValid() && gps.date.isValid() && gps.time.isValid()) {
      double lat = gps.location.lat();
      double lon = gps.location.lng();
      String ts = formatGpsTimestamp(gps.date, gps.time);
      String msgId = String(millis()) + "-" + String(messageCounter++);
      String payload = createPayload(lat, lon, ts, msgId);

      logToBoth("[Payload] " + payload);

      // Send on both LoRa and GSM simultaneously
      logToBoth("[Send] Transmitting on both LoRa and GSM");
      
      // Send via LoRa
      sendViaLoRa(payload);
      
      // Send via GSM
      bool smsOk = sendSMS(payload);
      logToBoth(smsOk ? "[GSM] SMS sent to receivers" : "[GSM] All SMS sends failed");
      
      // Publish locally for confirmation
      publishToBluetooth(payload, "Both");
    } else {
      // No GPS fix - send "no fix" message
      String ts = "1970-01-01T00:00:00Z";
      String msgId = String(millis()) + "-" + String(messageCounter++);
      String payload = createPayload(0.0, 0.0, ts, msgId);
      payload.replace("\"latitude\":0.000000,\"longitude\":0.000000", "\"status\":\"no_fix\"");
      
      logToBoth("[No GPS Fix] " + payload);
      
      // Send on both LoRa and GSM simultaneously
      logToBoth("[Send] Transmitting no fix on both LoRa and GSM");
      
      // Send via LoRa
      sendViaLoRa(payload);
      
      // Send via GSM
      bool smsOk = sendSMS(payload);
      logToBoth(smsOk ? "[GSM] No fix SMS sent to receivers" : "[GSM] All no fix SMS sends failed");
      
      // Publish locally for confirmation
      publishToBluetooth(payload, "Both");
      }
    }
  }
  
  // Small delay to prevent overwhelming the loop
  delay(10);
}
