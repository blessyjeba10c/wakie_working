#include "Tasks.h"
#include "Globals.h"
#include "Config.h"
#include "Utils.h"
#include "DisplayManager.h"
#include "KeyboardManager.h"
#include <LoRa.h>

TaskHandle_t gpsTaskHandle = NULL;
TaskHandle_t loraTaskHandle = NULL;
TaskHandle_t smsTaskHandle = NULL;
TaskHandle_t bluetoothTaskHandle = NULL;
TaskHandle_t displayTaskHandle = NULL;
TaskHandle_t keyboardTaskHandle = NULL;

void gpsTask(void *parameter) {
  logToBoth("[GPS Task] Started");
  
  while (true) {
    while (SerialGPS.available()) {
      char c = SerialGPS.read();
      gps.encode(c);
    }
    
    if (xSemaphoreTake(gpsMutex, portMAX_DELAY) == pdTRUE) {
      currentGPS.latitude = gps.location.lat();
      currentGPS.longitude = gps.location.lng();
      currentGPS.isValid = gps.location.isValid();
      currentGPS.satellites = gps.satellites.value();
      currentGPS.timestamp = formatGpsTimestamp(gps.date, gps.time);
      xSemaphoreGive(gpsMutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(GPS_UPDATE_INTERVAL));
  }
}

void loraTask(void *parameter) {
  logToBoth("[LoRa Task] Started");
  
  static bool waitingForAck = false;
  static unsigned long ackWaitStart = 0;
  static String lastSentPayload = "";
  
  while (true) {
    if (xSemaphoreTake(loraMutex, portMAX_DELAY) == pdTRUE) {
      // Receive LoRa packets
      int packetSize = LoRa.parsePacket();
      if (packetSize) {
        String incoming = "";
        while (LoRa.available()) {
          incoming += (char)LoRa.read();
        }
        incoming.trim();
        
        if (incoming.length() > 0) {
          // Check if this is an ACK message
          if (incoming.startsWith("ACK:")) {
            if (waitingForAck) {
              logToBoth("[LoRa] ACK received");
              waitingForAck = false;
              if (displayState.initialized) {
                displaySuccess("LoRa ACK OK");
              }
            }
          } else {
            // Regular message received
            systemStatus.lastLoRa = incoming;
            systemStatus.lastLoRaTime = millis();
            
            int rssi = LoRa.packetRssi();
            float snr = LoRa.packetSnr();
            
            logToBoth("[LoRa RX] " + incoming);
            if (BT.hasClient()) {
              BT.println("\n📡 LORA MESSAGE");
              BT.println("RSSI: " + String(rssi) + " dBm");
              BT.println("SNR: " + String(snr) + " dB");
              BT.println("Data: " + incoming + "\n");
            }
            
            if (displayState.initialized) {
              displayReceivedMessage("LoRa", String(rssi) + "dBm", incoming);
            }
            
            // Send ACK if acknowledgment is enabled
            if (acknowledgmentEnabled) {
              delay(100); // Small delay before ACK
              LoRa.beginPacket();
              LoRa.print("ACK:" + incoming.substring(0, 20));
              LoRa.endPacket();
              logToBoth("[LoRa] ACK sent");
            }
          }
        }
      }
      
      // Check ACK timeout
      if (waitingForAck && (millis() - ackWaitStart > LORA_ACK_TIMEOUT)) {
        logToBoth("[LoRa] ACK timeout - fallback to GSM");
        waitingForAck = false;
        
        if (displayState.initialized) {
          displayError("LoRa fail, GSM send");
        }
        
        // Send via GSM as fallback
        xSemaphoreGive(loraMutex);
        if (xSemaphoreTake(smsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
          logToBoth("[GSM TX] Fallback sending");
          sendSMSToAll(lastSentPayload);
          xSemaphoreGive(smsMutex);
        }
        if (xSemaphoreTake(loraMutex, portMAX_DELAY) != pdTRUE) {
          vTaskDelay(pdMS_TO_TICKS(LORA_UPDATE_INTERVAL));
          continue;
        }
      }
      
      // Send GPS data if in tracker mode
      if (currentMode == MODE_TRACKER && !waitingForAck) {
        static unsigned long lastSendTime = 0;
        if (millis() - lastSendTime >= GPS_SEND_INTERVAL) {
          lastSendTime = millis();
          
          GPSData localGPS;
          if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            localGPS = currentGPS;
            xSemaphoreGive(gpsMutex);
          }
          
          if (localGPS.isValid) {
            String msgId = String(millis()) + "-" + String(systemStatus.messageCounter++);
            String payload = createPayload(localGPS.latitude, localGPS.longitude, 
                                          localGPS.timestamp, msgId);
            
            // Send via LoRa
            logToBoth("[LoRa TX] Sending GPS");
            LoRa.beginPacket();
            LoRa.print(payload);
            LoRa.endPacket();
            
            if (acknowledgmentEnabled) {
              // Wait for ACK
              waitingForAck = true;
              ackWaitStart = millis();
              lastSentPayload = payload;
              logToBoth("[LoRa] Waiting for ACK...");
            } else {
              // Send via GSM simultaneously (no ACK mode)
              xSemaphoreGive(loraMutex);
              if (xSemaphoreTake(smsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
                logToBoth("[GSM TX] Sending GPS");
                sendSMSToAll(payload);
                xSemaphoreGive(smsMutex);
              }
              if (xSemaphoreTake(loraMutex, portMAX_DELAY) != pdTRUE) {
                vTaskDelay(pdMS_TO_TICKS(LORA_UPDATE_INTERVAL));
                continue;
              }
            }
          }
        }
      }
      
      xSemaphoreGive(loraMutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(LORA_UPDATE_INTERVAL));
  }
}

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
          
          int firstQuote = line.indexOf('"');
          int secondQuote = line.indexOf('"', firstQuote + 1);
          if (firstQuote != -1 && secondQuote != -1) {
            senderNumber = line.substring(firstQuote + 1, secondQuote);
          }
          logToBoth("SMS from: " + senderNumber);
          continue;
        }
        
        if (waitingForBody && line.length() > 0) {
          String messageBody = line;
          messageBody.trim();
          
          systemStatus.lastSMS = messageBody;
          systemStatus.lastSMSTime = millis();
          
          if (BT.hasClient()) {
            BT.println("\n📱 SMS RECEIVED");
            BT.println("From: " + senderNumber);
            BT.println("Msg: " + messageBody + "\n");
          }
          
          if (displayState.initialized) {
            displayReceivedMessage("SMS", senderNumber, messageBody);
          }
          
          logToBoth("SMS: " + messageBody);
          
          waitingForBody = false;
          smsHeader = "";
          senderNumber = "";
        }
      }
      
      // GPS sending is now handled in loraTask for synchronization
      // smsTask only handles receiving messages
      
      xSemaphoreGive(smsMutex);
    }
    
    vTaskDelay(pdMS_TO_TICKS(SMS_UPDATE_INTERVAL));
  }
}

void bluetoothTask(void *parameter) {
  logToBoth("[Bluetooth Task] Started");
  
  while (true) {
    if (BT.available()) {
      String command = BT.readStringUntil('\n');
      command.trim();
      command.toLowerCase();
      
      logToBoth("BT: " + command);
      
      if (command == "tracker") {
        currentMode = MODE_TRACKER;
        BT.println(">>> TRACKER MODE");
      }
      else if (command == "ground") {
        currentMode = MODE_GROUND_STATION;
        BT.println(">>> GROUND MODE");
      }
      else if (command == "status") {
        GPSData localGPS;
        if (xSemaphoreTake(gpsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
          localGPS = currentGPS;
          xSemaphoreGive(gpsMutex);
        }
        
        BT.println("=== STATUS ===");
        BT.println("Mode: " + String(currentMode == MODE_TRACKER ? "TRACKER" : "GROUND"));
        BT.println("LoRa: " + String(systemStatus.loraConnected ? "OK" : "FAIL"));
        BT.println("GPS: " + String(localGPS.isValid ? "LOCK" : "NO FIX"));
        if (localGPS.isValid) {
          BT.println("Lat: " + String(localGPS.latitude, 6));
          BT.println("Lng: " + String(localGPS.longitude, 6));
        }
        BT.println("Sats: " + String(localGPS.satellites));
        BT.println("GSM: " + String(systemStatus.networkConnected ? "OK" : "FAIL"));
        BT.println("Signal: " + String(systemStatus.signalStrength));
        BT.println("==============");
      }
      else if (command.startsWith("sms ")) {
        if (command.length() > 4) {
          String message = command.substring(4);
          message.trim();
          if (message.length() > 0) {
            bool loraOk = false;
            bool smsOk = false;
            
            if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
              LoRa.beginPacket();
              LoRa.print(message);
              LoRa.endPacket();
              loraOk = true;
              xSemaphoreGive(loraMutex);
            }
            
            if (xSemaphoreTake(smsMutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
              smsOk = sendSMSToNumber(RECEIVER_PHONES[0], message);
              xSemaphoreGive(smsMutex);
            }
            
            BT.println(">>> " + String(loraOk && smsOk ? "Sent both" : (loraOk ? "LoRa only" : (smsOk ? "GSM only" : "Failed"))));
          }
        }
      }
      else if (command == "help") {
        BT.println("=== COMMANDS ===");
        BT.println("tracker - Tracker mode");
        BT.println("ground - Ground mode");
        BT.println("status - Show status");
        BT.println("sms <msg> - Send message");
        BT.println("================");
      }
      else {
        BT.println("Unknown. Type 'help'");
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void displayTask(void *parameter) {
  logToBoth("[Display Task] Started");
  
  while (true) {
    updateDisplay();
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_INTERVAL));
  }
}

void keyboardTask(void *parameter) {
  logToBoth("[Keyboard Task] Started");
  
  while (true) {
    scanKeyboard();
    vTaskDelay(pdMS_TO_TICKS(KEYBOARD_SCAN_INTERVAL));
  }
}
