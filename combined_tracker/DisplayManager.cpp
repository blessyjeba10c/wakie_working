#include "DisplayManager.h"

// Forward declarations of external functions
extern void processKeyboardCommand(String command);

// External variables
extern struct GPSData {
  double latitude;
  double longitude;
  bool isValid;
  uint32_t satellites;
  String timestamp;
} currentGPS;

extern struct SystemStatus {
  bool loraConnected;
  bool networkConnected;
  int signalStrength;
  unsigned long messageCounter;
  String lastSMS;
  unsigned long lastSMSTime;
  String lastLoRa;
  unsigned long lastLoRaTime;
} systemStatus;

extern enum OperatingMode {
  MODE_TRACKER,
  MODE_GROUND_STATION
} currentMode;

void initializeDisplay() {
  u8g2.begin();
  u8g2.enableUTF8Print();
  displayState.initialized = true;
  
  // Show startup screen
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Combined Tracker");
  u8g2.drawHLine(0, 12, 128);
  u8g2.drawStr(0, 25, "GPS: Init...");
  u8g2.drawStr(0, 35, "GSM: Init...");
  u8g2.drawStr(0, 45, "LoRa: Init...");
  u8g2.drawStr(0, 60, "Press * for menu");
  u8g2.sendBuffer();
  delay(2000);
  
  initializeMenus();
  showMainScreen();
}

void updateDisplay() {
  if (!displayState.initialized) return;
  
  if (millis() - displayState.lastUpdate > 500) {
    displayState.lastUpdate = millis();
    
    if (displayState.showingNotification) {
      showNextNotification();
    } else if (displayState.inputMode) {
      showInputScreen();
    } else if (displayState.inMenu) {
      showMenu();
    } else if (displayState.currentScreen == "main") {
      showMainScreen();
    } else if (displayState.currentScreen == "status") {
      showStatusScreen();
    } else if (displayState.currentScreen == "gps") {
      showGPSScreen();
    } else if (displayState.currentScreen == "gsm") {
      showGSMScreen();
    }
  }
}

void showMainScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(0, 10, "Tracker System");
  u8g2.drawHLine(0, 12, 128);
  
  // Mode
  String mode = (currentMode == MODE_TRACKER) ? "TRACKER" : "GROUND";
  char modeLine[32];
  snprintf(modeLine, sizeof(modeLine), "Mode: %s", mode.c_str());
  u8g2.drawStr(0, 25, modeLine);
  
  // GPS Status
  if (currentGPS.isValid) {
    char gpsLine[32];
    snprintf(gpsLine, sizeof(gpsLine), "GPS: LOCK (%d sat)", currentGPS.satellites);
    u8g2.drawStr(0, 35, gpsLine);
  } else {
    u8g2.drawStr(0, 35, "GPS: NO FIX");
  }
  
  // GSM Status
  char gsmLine[32];
  if (systemStatus.networkConnected) {
    snprintf(gsmLine, sizeof(gsmLine), "GSM: OK (%d)", systemStatus.signalStrength);
  } else {
    snprintf(gsmLine, sizeof(gsmLine), "GSM: NO NET");
  }
  u8g2.drawStr(0, 45, gsmLine);
  
  // LoRa Status
  u8g2.drawStr(0, 55, systemStatus.loraConnected ? "LoRa: OK" : "LoRa: FAIL");
  
  // Menu hint
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(0, 62, "*=Menu #=Back");
  
  u8g2.sendBuffer();
}

void showStatusScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(0, 10, "System Status");
  u8g2.drawHLine(0, 12, 128);
  
  char line[32];
  String mode = (currentMode == MODE_TRACKER) ? "TRACKER" : "GROUND";
  snprintf(line, sizeof(line), "Mode: %s", mode.c_str());
  u8g2.drawStr(0, 25, line);
  
  snprintf(line, sizeof(line), "GPS Fix: %s", currentGPS.isValid ? "YES" : "NO");
  u8g2.drawStr(0, 35, line);
  
  snprintf(line, sizeof(line), "GSM: %s", systemStatus.networkConnected ? "YES" : "NO");
  u8g2.drawStr(0, 45, line);
  
  snprintf(line, sizeof(line), "LoRa: %s", systemStatus.loraConnected ? "YES" : "NO");
  u8g2.drawStr(0, 55, line);
  
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(0, 62, "#=Back");
  
  u8g2.sendBuffer();
}

void showGPSScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(0, 10, "GPS Info");
  u8g2.drawHLine(0, 12, 128);
  
  if (currentGPS.isValid) {
    u8g2.drawStr(0, 25, "Status: LOCKED");
    
    char latLine[32];
    snprintf(latLine, sizeof(latLine), "Lat: %.6f", currentGPS.latitude);
    u8g2.drawStr(0, 35, latLine);
    
    char lonLine[32];
    snprintf(lonLine, sizeof(lonLine), "Lon: %.6f", currentGPS.longitude);
    u8g2.drawStr(0, 45, lonLine);
    
    char satLine[32];
    snprintf(satLine, sizeof(satLine), "Satellites: %d", currentGPS.satellites);
    u8g2.drawStr(0, 55, satLine);
  } else {
    u8g2.drawStr(0, 25, "Status: NO FIX");
    u8g2.drawStr(0, 35, "Searching...");
  }
  
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(0, 62, "#=Back");
  
  u8g2.sendBuffer();
}

void showGSMScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(0, 10, "GSM Info");
  u8g2.drawHLine(0, 12, 128);
  
  char line[32];
  snprintf(line, sizeof(line), "Status: %s", systemStatus.networkConnected ? "REG" : "NO REG");
  u8g2.drawStr(0, 25, line);
  
  snprintf(line, sizeof(line), "Signal: %d/31", systemStatus.signalStrength);
  u8g2.drawStr(0, 35, line);
  
  if (systemStatus.lastSMSTime > 0) {
    unsigned long ago = (millis() - systemStatus.lastSMSTime) / 1000;
    snprintf(line, sizeof(line), "Last SMS: %lus ago", ago);
    u8g2.drawStr(0, 45, line);
  } else {
    u8g2.drawStr(0, 45, "Last SMS: Never");
  }
  
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(0, 62, "#=Back");
  
  u8g2.sendBuffer();
}

void showMessage(String message, int duration) {
  if (!displayState.initialized) return;
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);
  
  // Clean emojis
  message.replace("📤", ">"); message.replace("📞", ">");
  message.replace("📻", ">"); message.replace("🔊", ">");
  message.replace("✅", "OK"); message.replace("❌", "X");
  message.replace("📡", ""); message.replace("📱", "");
  
  // Word wrap
  int maxChars = 21;
  int maxLines = 8;
  String lines[8];
  int lineCount = 0;
  
  int start = 0;
  while (start < message.length() && lineCount < maxLines) {
    int end = start + maxChars;
    if (end >= message.length()) {
      end = message.length();
    } else {
      int lastSpace = message.lastIndexOf(' ', end);
      int newline = message.indexOf('\n', start);
      
      if (newline != -1 && newline < end) {
        end = newline;
      } else if (lastSpace > start && lastSpace < end) {
        end = lastSpace;
      }
    }
    
    String line = message.substring(start, end);
    line.trim();
    if (line.length() > 0) {
      lines[lineCount++] = line;
    }
    start = end + 1;
  }
  
  for (int i = 0; i < lineCount; i++) {
    u8g2.drawStr(0, 8 + (i * 8), lines[i].c_str());
  }
  
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(0, 62, "#=Back");
  
  u8g2.sendBuffer();
  delay(duration);
  
  if (!displayState.inMenu) {
    showMainScreen();
  }
}

void displayError(String error) {
  showMessage("ERROR: " + error, 3000);
}

void displaySuccess(String success) {
  showMessage("OK: " + success, 2000);
}

void displayReceivedMessage(String type, String from, String message) {
  // Queue the notification
  if (displayState.notificationCount < 10) {
    displayState.notificationQueue[displayState.notificationCount].type = type;
    displayState.notificationQueue[displayState.notificationCount].from = from;
    displayState.notificationQueue[displayState.notificationCount].message = message;
    displayState.notificationCount++;
  }
  
  // Show immediately if not already showing a notification
  if (!displayState.showingNotification) {
    showNextNotification();
  }
}

void showNextNotification() {
  if (displayState.notificationCount == 0) {
    displayState.showingNotification = false;
    showMainScreen();
    return;
  }
  
  displayState.showingNotification = true;
  
  // Get the first message in queue
  DisplayState::MessageNotification msg = displayState.notificationQueue[0];
  
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  String title = msg.type + " Received";
  u8g2.drawStr(0, 10, title.c_str());
  u8g2.drawHLine(0, 12, 128);
  
  u8g2.setFont(u8g2_font_5x7_tf);
  String fromLine = "From: " + msg.from;
  u8g2.drawStr(0, 22, fromLine.c_str());
  
  // Wrap message
  String shortMsg = msg.message;
  if (shortMsg.length() > 42) {
    shortMsg = shortMsg.substring(0, 39) + "...";
  }
  
  u8g2.drawStr(0, 32, "Msg:");
  u8g2.drawStr(0, 42, shortMsg.substring(0, 21).c_str());
  if (shortMsg.length() > 21) {
    u8g2.drawStr(0, 52, shortMsg.substring(21).c_str());
  }
  
  u8g2.setFont(u8g2_font_4x6_tf);
  char footer[32];
  snprintf(footer, sizeof(footer), "5/*:OK (%d more)", displayState.notificationCount - 1);
  u8g2.drawStr(0, 62, footer);
  
  u8g2.sendBuffer();
}

void dismissCurrentMessage() {
  if (displayState.notificationCount == 0) return;
  
  // Shift queue forward
  for (int i = 0; i < displayState.notificationCount - 1; i++) {
    displayState.notificationQueue[i] = displayState.notificationQueue[i + 1];
  }
  displayState.notificationCount--;
  
  // Show next or return to main screen
  showNextNotification();
}

void addMessage(String message) {
  displayState.messages[displayState.messageIndex] = message;
  displayState.messageIndex = (displayState.messageIndex + 1) % 6;
}

// ============ MENU SYSTEM ============

void initializeMenus() {
  createMainMenu();
}

void createMainMenu() {
  extern bool acknowledgmentEnabled;
  displayState.currentMenu.title = "Main Menu";
  displayState.currentMenu.selectedItem = 0;
  displayState.currentMenu.itemCount = 7;
  
  displayState.currentMenu.items[0] = {"Tracker Mode", "tracker", false};
  displayState.currentMenu.items[1] = {"Ground Mode", "ground", false};
  displayState.currentMenu.items[2] = {"Send Message", "sms_menu", true};
  String ackLabel = acknowledgmentEnabled ? "ACK: ON" : "ACK: OFF";
  displayState.currentMenu.items[3] = {ackLabel, "toggle_ack", false};
  displayState.currentMenu.items[4] = {"GPS Info", "gps", false};
  displayState.currentMenu.items[5] = {"GSM Info", "gsm", false};
  displayState.currentMenu.items[6] = {"System Status", "status", false};
}

void createSMSMenu() {
  displayState.currentMenu.title = "Send Message";
  displayState.currentMenu.selectedItem = 0;
  displayState.currentMenu.itemCount = 4;
  
  displayState.currentMenu.items[0] = {"Msg: Return", "sms Return to base", false};
  displayState.currentMenu.items[1] = {"Msg: Location", "sms Send location", false};
  displayState.currentMenu.items[2] = {"Msg: Status", "sms Send status", false};
  displayState.currentMenu.items[3] = {"Back", "back", false};
}

void showMenu() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(0, 10, displayState.currentMenu.title.c_str());
  u8g2.drawHLine(0, 12, 128);
  
  int startItem = max(0, displayState.currentMenu.selectedItem - 2);
  int endItem = min(startItem + 4, displayState.currentMenu.itemCount);
  
  for (int i = startItem; i < endItem; i++) {
    int y = 25 + (i - startItem) * 10;
    
    if (i == displayState.currentMenu.selectedItem) {
      u8g2.drawBox(0, y - 8, 128, 9);
      u8g2.setColorIndex(0);
    }
    
    String itemText = displayState.currentMenu.items[i].title;
    if (displayState.currentMenu.items[i].isSubmenu) {
      itemText += " >";
    }
    u8g2.drawStr(2, y, itemText.c_str());
    
    if (i == displayState.currentMenu.selectedItem) {
      u8g2.setColorIndex(1);
    }
  }
  
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(0, 62, "2/8:Nav 5:OK #:Back");
  
  u8g2.sendBuffer();
}

void navigateUp() {
  if (displayState.currentMenu.selectedItem > 0) {
    displayState.currentMenu.selectedItem--;
    showMenu();
  }
}

void navigateDown() {
  if (displayState.currentMenu.selectedItem < displayState.currentMenu.itemCount - 1) {
    displayState.currentMenu.selectedItem++;
    showMenu();
  }
}

void selectMenuItem() {
  DisplayState::MenuItem selectedItem = displayState.currentMenu.items[displayState.currentMenu.selectedItem];
  
  if (selectedItem.isSubmenu) {
    if (displayState.menuStackDepth < 4) {
      displayState.menuStack[displayState.menuStackDepth] = displayState.currentMenu;
      displayState.menuStackDepth++;
    }
    
    if (selectedItem.action == "sms_menu") {
      createSMSMenu();
    }
    showMenu();
  } else {
    executeMenuAction(selectedItem.action);
  }
}

void goBack() {
  if (displayState.menuStackDepth > 0) {
    displayState.menuStackDepth--;
    displayState.currentMenu = displayState.menuStack[displayState.menuStackDepth];
    showMenu();
  } else {
    displayState.inMenu = false;
    showMainScreen();
  }
}

void executeMenuAction(String action);  // Forward declaration

void startInput(String prompt, String action) {
  displayState.inputMode = true;
  displayState.inputPrompt = prompt;
  displayState.inputValue = "";
  displayState.pendingAction = action;
  showInputScreen();
}

void handleInput(char c) {
  if (!displayState.inputMode) return;
  
  if (c == '#') {
    cancelInput();
  } else if (c == '*') {
    confirmInput();
  } else if (c == 'C') {
    if (displayState.inputValue.length() > 0) {
      displayState.inputValue.remove(displayState.inputValue.length() - 1);
      showInputScreen();
    }
  } else if (c != 0) {
    displayState.inputValue += c;
    showInputScreen();
  }
}

void cancelInput() {
  displayState.inputMode = false;
  showMenu();
}

void confirmInput();  // Forward declaration

void confirmInput() {
  if (displayState.inputValue.length() > 0) {
    String fullCommand = displayState.pendingAction + " " + displayState.inputValue;
    processKeyboardCommand(fullCommand);
    displayState.inputMode = false;
    displaySuccess("Command sent");
  } else {
    displayState.inputMode = false;
    showMenu();
  }
}

void executeMenuAction(String action) {
  extern bool acknowledgmentEnabled;
  
  // Handle quick messages
  if (action.startsWith("sms ")) {
    processKeyboardCommand(action);
    displayState.inMenu = false;
    displaySuccess("Sending...");
    return;
  }
  
  // Handle mode switches
  if (action == "tracker") {
    processKeyboardCommand("tracker");
    displayState.inMenu = false;
  } else if (action == "ground") {
    processKeyboardCommand("ground");
    displayState.inMenu = false;
  } else if (action == "toggle_ack") {
    acknowledgmentEnabled = !acknowledgmentEnabled;
    String msg = acknowledgmentEnabled ? "ACK Enabled" : "ACK Disabled";
    displaySuccess(msg);
    createMainMenu(); // Refresh menu to update label
  } else if (action == "gps") {
    displayState.currentScreen = "gps";
    displayState.inMenu = false;
  } else if (action == "gsm") {
    displayState.currentScreen = "gsm";
    displayState.inMenu = false;
  } else if (action == "status") {
    displayState.currentScreen = "status";
    displayState.inMenu = false;
  } else if (action == "back") {
    goBack();
  }
}

void showInputScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  
  u8g2.drawStr(0, 10, "Input");
  u8g2.drawHLine(0, 12, 128);
  
  u8g2.drawStr(0, 25, displayState.inputPrompt.c_str());
  
  String displayValue = displayState.inputValue + "_";
  u8g2.drawStr(0, 40, displayValue.c_str());
  
  u8g2.setFont(u8g2_font_4x6_tf);
  u8g2.drawStr(0, 55, "* to confirm");
  u8g2.drawStr(0, 62, "# cancel C backspace");
  
  u8g2.sendBuffer();
}
