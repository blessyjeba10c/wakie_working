// Combined GPS Tracker + SMS Receiver with FreeRTOS
// Integrates GPS tracking, LoRa communication, and SMS receiving/sending
// Uses FreeRTOS for concurrent task management

#include <Arduino.h>
#include <TinyGPS++.h>
#include <LoRa.h>
#include "BluetoothSerial.h"
#include "SIM800L.h"

// --- Configuration ---
const char* SOLDIER_ID = "BSF1875";
const char* DEVICE_TYPE = "VHF";

// LoRa frequency
const long LORA_FREQ = 433E6;

// Hardware serial ports (ESP32)
HardwareSerial SerialGPS(2);  // GPS on Serial2
HardwareSerial SerialSIM(1);  // SIM800L on Serial1

// Pin definitions
const int GPS_RX_PIN = 16;    // RX2 pin (to TX of GPS)
const int GPS_TX_PIN = 17;    // TX2 pin (to RX of GPS)
const int SIM_RX_PIN = 26;    // RX1 pin (to TX of SIM800L)
const int SIM_TX_PIN = 25;    // TX1 pin (to RX of SIM800L)

// LoRa pins
const int LORA_SS = 5;
const int LORA_RST = 14;
const int LORA_DIO0 = 27;

// Receiver phone numbers for SMS
const char* RECEIVER_PHONES[] = {
  "+918667399071",
  "+919944127336"
};
const int NUM_RECEIVERS = 2;

// Global objects
TinyGPSPlus gps;
BluetoothSerial BT;
SIM800L sim800l;

// FreeRTOS Task Handles
TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t loraTaskHandle = NULL;
TaskHandle_t smsTaskHandle = NULL;
TaskHandle_t bluetoothTaskHandle = NULL;

// Shared data structures (protected by mutexes)
SemaphoreHandle_t gpsMutex;
SemaphoreHandle_t loraMutex;
SemaphoreHandle_t smsMutex;

struct GPSData {
  double latitude;
  double longitude;
  bool isValid;
  uint32_t satellites;
  String timestamp;
} currentGPS;

struct SystemStatus {
  bool loraConnected;
  bool networkConnected;
  int signalStrength;
  unsigned long messageCounter;
  String lastSMS;
  unsigned long lastSMSTime;
  String lastLoRa;
  unsigned long lastLoRaTime;
} systemStatus;

// Operating mode
enum OperatingMode {
  MODE_TRACKER,      // Sends GPS data
  MODE_GROUND_STATION // Receives data and sends commands
};

OperatingMode currentMode = MODE_TRACKER;

// --- Helper Functions ---

void logToBoth(const String &message) {
  Serial.println(message);
  if (BT.hasClient()) {
    BT.println("[LOG] " + message);
  }
}

String formatGpsTimestamp(TinyGPSDate &d, TinyGPSTime &t) {
  if (!d.isValid() || !t.isValid()) return String("1970-01-01T00:00:00Z");
  char buf[32];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
    d.year(), d.month(), d.day(), t.hour(), t.minute(), t.second());
  return String(buf);
}

String createPayload(double lat, double lon, const String &timestamp, const String &msgId) {
  String s = "{";
  s += "\"soldier_id\":\"" + String(SOLDIER_ID) + "\",";
  s += "\"device_type\":\"" + String(DEVICE_TYPE) + "\",";
  s += "\"latitude\":" + String(lat, 6) + ",";
  s += "\"longitude\":" + String(lon, 6) + ",";
  s += "\"timestamp\":\"" + timestamp + "\",";
  s += "\"message_id\":\"" + msgId + "\"";
  s += "}";
  return s;
}

bool sendSMSToNumber(const char *toNumber, const String &message) {
  char msgBuffer[message.length() + 1];
  message.toCharArray(msgBuffer, message.length() + 1);
  char *phoneBuffer = const_cast<char*>(toNumber);
  return sim800l.sendSMS(phoneBuffer, msgBuffer);
}

bool sendSMSToAll(const String &message) {
  bool overallSuccess = false;
  for (int i = 0; i < NUM_RECEIVERS; i++) {
    logToBoth("[GSM] Sending to " + String(RECEIVER_PHONES[i]));
    if (sendSMSToNumber(RECEIVER_PHONES[i], message)) {
      logToBoth("[GSM] SMS sent successfully to " + String(RECEIVER_PHONES[i]));
      overallSuccess = true;
    } else {
      logToBoth("[GSM] SMS send failed to " + String(RECEIVER_PHONES[i]));
    }
    delay(1000);
  }
  return overallSuccess;
}

// --- FreeRTOS Task: GPS Processing ---
void gpsTask(void *parameter) {
  logToBoth("[GPS Task] Started");
  
  while (true) {
    // Read GPS data
    while (SerialGPS.available()) {
      char c = SerialGPS.read();
      gps.encode(c);
    }
    
    // Update shared GPS data
    if (xSemaphoreTake(gpsMutex, portMAX_DELAY) == pdTRUE) {
      currentGPS.latitude = gps.location.lat();
      currentGPS.longitude = gps.location.lng();
      currentGPS.isValid = gps.location.isValid();
      currentGPS.satellites = gps.satellites.value();
      currentGPS.timestamp = formatGpsTimestamp(gps.date, gps.time);
      xSemaphoreGive(gpsMutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
  }
}

// --- FreeRTOS Task: LoRa Communication ---
void loraTask(void *parameter) {
  logToBoth("[LoRa Task] Started");
  
  while (true) {
    if (xSemaphoreTake(loraMutex, portMAX_DELAY) == pdTRUE) {
      // Check for incoming LoRa packets
      int packetSize = LoRa.parsePacket();
      if (packetSize) {
        String incoming = "";
        while (LoRa.available()) {
          incoming += (char)LoRa.read();
        }
        incoming.trim();
        
        if (incoming.length() > 0) {
          systemStatus.lastLoRa = incoming;
          systemStatus.lastLoRaTime = millis();
          
          int rssi = LoRa.packetRssi();
          float snr = LoRa.packetSnr();
          
          logToBoth("[LoRa RX] " + incoming);
          if (BT.hasClient()) {
            BT.println("");
            BT.println("📡━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            BT.println("📻 NEW LORA MESSAGE");
            BT.println("📡━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            BT.println("RSSI: " + String(rssi) + " dBm");
            BT.println("SNR: " + String(snr) + " dB");
            BT.println("Data: " + incoming);
            BT.println("📡━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            BT.println("");
          }
        }
      }
      
      // Send GPS data if in tracker mode
      if (currentMode == MODE_TRACKER) {
        static unsigned long lastSendTime = 0;
        if (millis() - lastSendTime >= 10000) { // Send every 10 seconds
          lastSendTime = millis();
          
          // Get GPS data
          GPSData localGPS;
          if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            localGPS = currentGPS;
            xSemaphoreGive(gpsMutex);
          }
          
          if (localGPS.isValid) {
            String msgId = String(millis()) + "-" + String(systemStatus.messageCounter++);
            String payload = createPayload(localGPS.latitude, localGPS.longitude, 
                                          localGPS.timestamp, msgId);
            
            logToBoth("[LoRa TX] Sending GPS data");
            LoRa.beginPacket();
            LoRa.print(payload);
            LoRa.endPacket();
            logToBoth("[LoRa TX] Data sent");
          } else {
            logToBoth("[LoRa TX] No GPS fix, skipping send");
          }
        }
      }
      
      xSemaphoreGive(loraMutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
  }
}

// --- FreeRTOS Task: SMS Communication ---
void smsTask(void *parameter) {
  logToBoth("[SMS Task] Started");
  
  static bool waitingForBody = false;
  static String smsHeader = "";
  static String senderNumber = "";
  
  while (true) {
    if (xSemaphoreTake(smsMutex, portMAX_DELAY) == pdTRUE) {
      // Handle incoming SMS
      while (SerialSIM.available()) {
        String line = SerialSIM.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        
        if (line.startsWith("+CMT:")) {
          smsHeader = line;
          waitingForBody = true;
          
          // Extract sender number
          int firstQuote = line.indexOf('"');
          int secondQuote = line.indexOf('"', firstQuote + 1);
          if (firstQuote != -1 && secondQuote != -1) {
            senderNumber = line.substring(firstQuote + 1, secondQuote);
          }
          logToBoth("SMS incoming from: " + senderNumber);
          continue;
        }
        
        if (waitingForBody && line.length() > 0) {
          String messageBody = line;
          messageBody.trim();
          
          systemStatus.lastSMS = messageBody;
          systemStatus.lastSMSTime = millis();
          
          if (BT.hasClient()) {
            BT.println("");
            BT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            BT.println("📱 NEW SMS RECEIVED");
            BT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            BT.println("From: " + senderNumber);
            BT.println("Message: \"" + messageBody + "\"");
            BT.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            BT.println("");
          }
          
          logToBoth("SMS from " + senderNumber + ": " + messageBody);
          
          waitingForBody = false;
          smsHeader = "";
          senderNumber = "";
        }
      }
      
      // Send GPS data via SMS if in tracker mode
      if (currentMode == MODE_TRACKER) {
        static unsigned long lastSMSSendTime = 0;
        if (millis() - lastSMSSendTime >= 10000) { // Send every 10 seconds
          lastSMSSendTime = millis();
          
          // Get GPS data
          GPSData localGPS;
          if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            localGPS = currentGPS;
            xSemaphoreGive(gpsMutex);
          }
          
          if (localGPS.isValid) {
            String msgId = String(millis()) + "-" + String(systemStatus.messageCounter);
            String payload = createPayload(localGPS.latitude, localGPS.longitude, 
                                          localGPS.timestamp, msgId);
            
            logToBoth("[GSM] Sending GPS data via SMS");
            bool smsOk = sendSMSToAll(payload);
            logToBoth(smsOk ? "[GSM] SMS sent to receivers" : "[GSM] SMS send failed");
          }
        }
      }
      
      xSemaphoreGive(smsMutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(500)); // 500ms delay
  }
}

// --- FreeRTOS Task: Bluetooth Command Handler ---
void bluetoothTask(void *parameter) {
  logToBoth("[Bluetooth Task] Started");
  
  while (true) {
    if (BT.available()) {
      String command = BT.readStringUntil('\n');
      command.trim();
      command.toLowerCase();
      
      logToBoth("BT Command: " + command);
      
      if (command == "tracker") {
        currentMode = MODE_TRACKER;
        logToBoth("Mode switched to: TRACKER");
        BT.println(">>> MODE: TRACKER - Sending GPS data");
      }
      else if (command == "ground") {
        currentMode = MODE_GROUND_STATION;
        logToBoth("Mode switched to: GROUND STATION");
        BT.println(">>> MODE: GROUND STATION - Receiving data");
        BT.println(">>> Type 'sms <message>' to send commands");
      }
      else if (command == "status") {
        BT.println("=== SYSTEM STATUS ===");
        String mode = (currentMode == MODE_TRACKER) ? "TRACKER" : "GROUND STATION";
        BT.println("Mode: " + mode);
        BT.println("LoRa: " + String(systemStatus.loraConnected ? "Connected" : "Disconnected"));
        
        // Get GPS data safely
        GPSData localGPS;
        if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          localGPS = currentGPS;
          xSemaphoreGive(gpsMutex);
        }
        
        BT.println("GPS Fix: " + String(localGPS.isValid ? "Valid" : "No Fix"));
        if (localGPS.isValid) {
          BT.println("Location: " + String(localGPS.latitude, 6) + ", " + String(localGPS.longitude, 6));
        }
        BT.println("Satellites: " + String(localGPS.satellites));
        BT.println("Network: " + String(systemStatus.networkConnected ? "Connected" : "Disconnected"));
        BT.println("Signal: " + String(systemStatus.signalStrength) + " dBm");
        BT.println("Last SMS: " + (systemStatus.lastSMSTime > 0 ? String((millis() - systemStatus.lastSMSTime) / 1000) + "s ago" : "Never"));
        BT.println("Last LoRa: " + (systemStatus.lastLoRaTime > 0 ? String((millis() - systemStatus.lastLoRaTime) / 1000) + "s ago" : "Never"));
        BT.println("Uptime: " + String(millis() / 1000) + "s");
        BT.println("===================");
      }
      else if (command.startsWith("sms ")) {
        if (command.length() > 4) {
          String message = command.substring(4);
          message.trim();
          if (message.length() > 0) {
            logToBoth("Sending message via LoRa and GSM: " + message);
            BT.println(">>> Sending message on both channels...");
            
            bool loraOk = false;
            bool smsOk = false;
            
            // Send via LoRa
            if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
              logToBoth("[LoRa] Sending command message");
              LoRa.beginPacket();
              LoRa.print(message);
              LoRa.endPacket();
              loraOk = true;
              logToBoth("[LoRa] Message sent");
              xSemaphoreGive(loraMutex);
            }
            
            // Send via GSM
            if (xSemaphoreTake(smsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
              smsOk = sendSMSToNumber(RECEIVER_PHONES[0], message);
              xSemaphoreGive(smsMutex);
              if (smsOk) {
                logToBoth("[GSM] SMS sent successfully");
              } else {
                logToBoth("[GSM] SMS send failed");
              }
            }
            
            // Report results
            BT.println(">>> LoRa: " + String(loraOk ? "✓ Sent" : "✗ Failed"));
            BT.println(">>> GSM: " + String(smsOk ? "✓ Sent" : "✗ Failed"));
            if (loraOk || smsOk) {
              BT.println(">>> Message sent on " + String(loraOk && smsOk ? "both" : (loraOk ? "LoRa only" : "GSM only")));
            } else {
              BT.println(">>> Failed to send on both channels!");
            }
          }
        }
      }
      else if (command == "help") {
        BT.println("=== AVAILABLE COMMANDS ===");
        BT.println("tracker     - Switch to tracker mode");
        BT.println("ground      - Switch to ground station mode");
        BT.println("status      - Show system status");
        BT.println("sms <msg>   - Send SMS message");
        BT.println("help        - Show this help");
        BT.println("=========================");
      }
      else {
        BT.println("Unknown command. Type 'help' for commands.");
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(100)); // 100ms delay
  }
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Initialize Bluetooth
  BT.begin("Combined_Tracker");
  logToBoth("=== Combined GPS Tracker + SMS Receiver ===");
  logToBoth("Bluetooth: Combined_Tracker");
  
  // Initialize GPS Serial
  SerialGPS.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  logToBoth("GPS Serial initialized");
  
  // Initialize SIM800L Serial
  SerialSIM.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);
  delay(2000);
  
  // Initialize SIM800L driver
  if (sim800l.begin(SerialSIM)) {
    logToBoth("SIM800L initialized successfully");
    systemStatus.networkConnected = sim800l.checkNetwork();
    systemStatus.signalStrength = sim800l.signalStrength();
    logToBoth("Network: " + String(systemStatus.networkConnected ? "Connected" : "Disconnected"));
    logToBoth("Signal: " + String(systemStatus.signalStrength) + " dBm");
  } else {
    logToBoth("SIM800L initialization failed!");
  }
  
  // Configure SMS notifications
  SerialSIM.println("AT+CMGF=1"); // Text mode
  delay(200);
  SerialSIM.println("AT+CNMI=2,1,0,0,0"); // Enable SMS notifications
  delay(200);
  
  // Initialize LoRa
  SPI.begin();
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    logToBoth("LoRa initialization failed!");
    systemStatus.loraConnected = false;
  } else {
    logToBoth("LoRa initialized successfully");
    systemStatus.loraConnected = true;
  }
  
  // Initialize system status
  systemStatus.messageCounter = 0;
  systemStatus.lastSMSTime = 0;
  systemStatus.lastLoRaTime = 0;
  
  // Initialize GPS data
  currentGPS.isValid = false;
  currentGPS.latitude = 0.0;
  currentGPS.longitude = 0.0;
  currentGPS.satellites = 0;
  
  // Create mutexes
  gpsMutex = xSemaphoreCreateMutex();
  loraMutex = xSemaphoreCreateMutex();
  smsMutex = xSemaphoreCreateMutex();
  
  // Create FreeRTOS tasks
  xTaskCreatePinnedToCore(
    gpsTask,           // Task function
    "GPS Task",        // Task name
    4096,             // Stack size
    NULL,             // Parameters
    2,                // Priority
    &gpsTaskHandle,   // Task handle
    0                 // Core 0
  );
  
  xTaskCreatePinnedToCore(
    loraTask,
    "LoRa Task",
    4096,
    NULL,
    2,
    &loraTaskHandle,
    1                 // Core 1
  );
  
  xTaskCreatePinnedToCore(
    smsTask,
    "SMS Task",
    4096,
    NULL,
    1,
    &smsTaskHandle,
    0                 // Core 0
  );
  
  xTaskCreatePinnedToCore(
    bluetoothTask,
    "Bluetooth Task",
    4096,
    NULL,
    1,
    &bluetoothTaskHandle,
    1                 // Core 1
  );
  
  logToBoth("All FreeRTOS tasks started");
  
  String mode = (currentMode == MODE_TRACKER) ? "TRACKER" : "GROUND STATION";
  logToBoth("Current mode: " + mode);
  
  BT.println("\n=== SYSTEM READY ===");
  BT.println("Type 'help' for commands");
  BT.println("====================");
}

void loop() {
  // Main loop is not used - all work done in FreeRTOS tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}
