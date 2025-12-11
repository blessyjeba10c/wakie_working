#include "Utils.h"
#include "Globals.h"
#include "Config.h"
#include "DisplayManager.h"
#include <LoRa.h>

const char* RECEIVER_PHONES[NUM_RECEIVERS] = {
  "+918667399071",
  "+919944127336"
};

void logToBoth(const String &message) {
  Serial.println(message);
  if (BT.hasClient()) {
    BT.println("[LOG] " + message);
  }
  if (displayState.initialized && !displayState.inMenu && !displayState.inputMode) {
    if (message.indexOf("LoRa") != -1 || message.indexOf("SMS") != -1 || message.indexOf("GPS") != -1) {
      addMessage(message);
    }
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
  s += "\"id\":\"" + String(SOLDIER_ID) + "\",";
  s += "\"t\":" + String(lat, 4) + ",";
  s += "\"g\":" + String(lon, 4) + ",";
  s += "\"ts\":\"" + timestamp + "\",";
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
      logToBoth("[GSM] SMS sent to " + String(RECEIVER_PHONES[i]));
      overallSuccess = true;
    } else {
      logToBoth("[GSM] SMS failed to " + String(RECEIVER_PHONES[i]));
    }
    delay(1000);
  }
  return overallSuccess;
}

void processKeyboardCommand(String command) {
  command.trim();
  command.toLowerCase();
  
  if (command == "tracker") {
    currentMode = MODE_TRACKER;
    displaySuccess("Tracker mode");
  }
  else if (command == "ground") {
    currentMode = MODE_GROUND_STATION;
    displaySuccess("Ground mode");
  }
  else if (command.startsWith("sms ")) {
    String message = command.substring(4);
    displaySuccess("Sending...");
    
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
    
    if (loraOk || smsOk) {
      displaySuccess("Sent " + String(loraOk && smsOk ? "both" : (loraOk ? "LoRa" : "GSM")));
    } else {
      displayError("Send failed!");
    }
  }
}
