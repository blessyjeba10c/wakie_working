#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <U8g2lib.h>
#include <Wire.h>

// Display state structure
struct DisplayState {
  bool initialized = false;
  unsigned long lastUpdate = 0;
  String currentScreen = "main";
  bool inMenu = false;
  bool inputMode = false;
  
  // Input state
  String inputPrompt = "";
  String inputValue = "";
  String pendingAction = "";
  
  // Menu state
  struct MenuItem {
    String title;
    String action;
    bool isSubmenu;
  };
  
  struct Menu {
    String title;
    int selectedItem = 0;
    int itemCount = 0;
    MenuItem items[10];
  };
  
  Menu currentMenu;
  Menu menuStack[4];
  int menuStackDepth = 0;
  
  // Message history
  String messages[6];
  int messageIndex = 0;
  String lastCommandOutput = "";
  
  // Notification queue
  struct MessageNotification {
    String type;    // "SMS" or "LoRa"
    String from;    // Sender/source
    String message; // Message content
  };
  
  MessageNotification notificationQueue[10];
  int notificationCount = 0;
  bool showingNotification = false;
};

extern DisplayState displayState;
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Display functions
void initializeDisplay();
void updateDisplay();
void showMainScreen();
void showStatusScreen();
void showGPSScreen();
void showGSMScreen();
void showMessage(String message, int duration = 3000);
void displayError(String error);
void displaySuccess(String success);

// Menu functions
void initializeMenus();
void createMainMenu();
void createSMSMenu();
void showMenu();
void navigateUp();
void navigateDown();
void selectMenuItem();
void goBack();
void executeMenuAction(String action);

// Input functions
void startInput(String prompt, String action);
void handleInput(char c);
void cancelInput();
void confirmInput();
void showInputScreen();

// Message display
void addMessage(String message);
void displayReceivedMessage(String type, String from, String message);
void dismissCurrentMessage();
void showNextNotification();

#endif
