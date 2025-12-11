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
  
  static unsigned long lastDebugTime = 0;
  
  while (true) {
    // Only parse GPS in TRACKER mode
    if (currentMode == MODE_TRACKER) {
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
        
        // Debug GPS status every 10 seconds
        if (millis() - lastDebugTime > 10000) {
          lastDebugTime = millis();
          if (BT.hasClient()) {
            BT.println("[GPS] Sats: " + String(currentGPS.satellites) + 
                       ", Fix: " + String(currentGPS.isValid ? "YES" : "NO"));
            if (currentGPS.isValid) {
              BT.println("[GPS] Lat: " + String(currentGPS.latitude, 6) + 
                         ", Lng: " + String(currentGPS.longitude, 6));
            }
          }
        }
      }
    } else {
      // In GROUND mode, just clear the serial buffer to prevent overflow
      while (SerialGPS.available()) {
        SerialGPS.read();
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(GPS_UPDATE_INTERVAL));
  }
}

void loraTask(void *parameter) {
  logToBoth("[LoRa Task] Started");
  
  static bool waitingForAck = false;
  static unsigned long ackWaitStart = 0;
  static String lastSentPayload = "";
  
  // Keep LoRa in receive mode
  LoRa.idle();
  LoRa.receive();
  
  while (true) {
    // Use timeout instead of blocking forever to ensure frequent checks
    if (xSemaphoreTake(loraMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      // Receive LoRa packets
      int packetSize = LoRa.parsePacket();
      if (packetSize) {
        if (BT.hasClient()) {
          BT.println("[LoRa RX] Packet detected, size: " + String(packetSize));
        }
        
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
            
            // Only show on display in TRACKER mode
            if (displayState.initialized && currentMode == MODE_TRACKER) {
              displayReceivedMessage("LoRa", String(rssi) + "dBm", incoming);
            }
            
            // Send ACK if acknowledgment is enabled
            if (acknowledgmentEnabled) {
              delay(100); // Small delay before ACK
              LoRa.beginPacket();
              LoRa.print("ACK:" + incoming.substring(0, 20));
              LoRa.endPacket();
              LoRa.receive(); // Return to RX mode
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
            if (BT.hasClient()) {
              BT.println("[LoRa TX] Payload: " + payload);
              BT.println("[LoRa TX] Size: " + String(payload.length()) + " bytes");
            }
            
            LoRa.beginPacket();
            LoRa.print(payload);
            LoRa.endPacket();
            LoRa.receive(); // Return to RX mode immediately
            
            if (BT.hasClient()) {
              BT.println("[LoRa TX] Transmission complete");
            }
            
            if (acknowledgmentEnabled) {
              // Wait for ACK
              waitingForAck = true;
              ackWaitStart = millis();
              lastSentPayload = payload;
              logToBoth("[LoRa] Waiting for ACK (timeout: 5s)...");
            } else {
              // Send via GSM simultaneously (no ACK mode)
              if (BT.hasClient()) {
                BT.println("[Mode] No ACK - sending GSM simultaneously");
              }
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
  logToBoth("[SMS Task] Started - Queue mode");
  
  static unsigned long checkCount = 0;
  
  while (true) {
    if (xSemaphoreTake(smsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
      // Check for stored messages in SIM memory
      checkCount++;
      if (BT.hasClient() && (checkCount % 10 == 0)) {
        BT.println("[SMS] Checking queue (check #" + String(checkCount) + ")...");
      }
      
      SerialSIM.println("AT+CMGL=\"REC UNREAD\""); // List unread messages
      delay(500); // Wait for response
      
      String response = "";
      unsigned long startTime = millis();
      while (millis() - startTime < 1000 && SerialSIM.available()) {
        response += SerialSIM.readString();
        delay(10);
      }
      
      if (response.indexOf("+CMGL:") != -1) {
        // Parse multiple messages
        int index = 0;
        while (true) {
          int msgStart = response.indexOf("+CMGL:", index);
          if (msgStart == -1) break;
          
          // Extract message index for deletion
          int indexStart = msgStart + 7;
          int indexEnd = response.indexOf(',', indexStart);
          String msgIndex = response.substring(indexStart, indexEnd);
          msgIndex.trim();
          
          // Extract sender number
          int phoneStart = response.indexOf('"', indexEnd) + 1;
          int phoneEnd = response.indexOf('"', phoneStart);
          String senderNumber = response.substring(phoneStart, phoneEnd);
          
          // Extract message body (next line after +CMGL)
          int bodyStart = response.indexOf('\n', msgStart) + 1;
          int bodyEnd = response.indexOf('\n', bodyStart);
          if (bodyEnd == -1) bodyEnd = response.length();
          String messageBody = response.substring(bodyStart, bodyEnd);
          messageBody.trim();
          
          if (messageBody.length() > 0 && !messageBody.startsWith("OK") && !messageBody.startsWith("+CMGL")) {
            systemStatus.lastSMS = messageBody;
            systemStatus.lastSMSTime = millis();
            
            logToBoth("[SMS RX] From: " + senderNumber);
            logToBoth("[SMS RX] Msg: " + messageBody);
            
            if (BT.hasClient()) {
              BT.println("\n📱 SMS RECEIVED");
              BT.println("From: " + senderNumber);
              BT.println("Msg: " + messageBody + "\n");
            }
            
            // Only show on display in TRACKER mode
            if (displayState.initialized && currentMode == MODE_TRACKER) {
              displayReceivedMessage("SMS", senderNumber, messageBody);
            }
            
            // Delete message after reading
            delay(100);
            SerialSIM.println("AT+CMGD=" + msgIndex);
            delay(200);
          }
          
          index = bodyEnd + 1;
          if (index >= response.length()) break;
        }
      }
      
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
        BT.println(">>> MODE CHANGED: TRACKER");
        BT.println(">>> Will send GPS every " + String(GPS_SEND_INTERVAL/1000) + "s");
        
        // Resume display and keyboard tasks
        if (displayTaskHandle != NULL) {
          vTaskResume(displayTaskHandle);
          BT.println(">>> Display task RESUMED");
        }
        if (keyboardTaskHandle != NULL) {
          vTaskResume(keyboardTaskHandle);
          BT.println(">>> Keyboard task RESUMED");
        }
      }
      else if (command == "ground") {
        currentMode = MODE_GROUND_STATION;
        BT.println(">>> MODE CHANGED: GROUND STATION");
        BT.println(">>> Will receive data only");
        BT.println(">>> Suspending Display & Keyboard for max reception");
        
        // Suspend display and keyboard tasks to save CPU for LoRa/SMS
        if (displayTaskHandle != NULL) {
          vTaskSuspend(displayTaskHandle);
          BT.println(">>> Display task SUSPENDED");
        }
        if (keyboardTaskHandle != NULL) {
          vTaskSuspend(keyboardTaskHandle);
          BT.println(">>> Keyboard task SUSPENDED");
        }
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
      else if (command == "gpsraw" || command == "nmea") {
        BT.println("=== GPS RAW DATA (2 sec) ===");
        unsigned long startTime = millis();
        while (millis() - startTime < 2000) {
          if (SerialGPS.available()) {
            char c = SerialGPS.read();
            BT.write(c);
          }
          yield();
        }
        BT.println("\n=== END GPS RAW ===");
      }
      else if (command == "checksms" || command == "smscheck") {
        BT.println(">>> Checking SMS queue...");
        if (xSemaphoreTake(smsMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          SerialSIM.println("AT+CMGL=\"ALL\"");
          delay(1000);
          while (SerialSIM.available()) {
            BT.write(SerialSIM.read());
          }
          xSemaphoreGive(smsMutex);
        }
        BT.println(">>> Done");
      }
      else if (command == "help") {
        BT.println("=== COMMANDS ===");
        BT.println("tracker - Tracker mode");
        BT.println("ground - Ground mode");
        BT.println("status - Show status");
        BT.println("sms <msg> - Send message");
        BT.println("gpsraw/nmea - Show GPS raw");
        BT.println("checksms - Check SMS queue");
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
    // Only update display in TRACKER mode
    if (currentMode == MODE_TRACKER) {
      updateDisplay();
    }
    vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_INTERVAL));
  }
}

void keyboardTask(void *parameter) {
  logToBoth("[Keyboard Task] Started");
  
  while (true) {
    // Only scan keyboard in TRACKER mode
    if (currentMode == MODE_TRACKER) {
      scanKeyboard();
    }
    vTaskDelay(pdMS_TO_TICKS(KEYBOARD_SCAN_INTERVAL));
  }
}
