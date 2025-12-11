#include "KeyboardManager.h"
#include "DisplayManager.h"

extern void processKeyboardCommand(String command);
extern void displaySuccess(String message);
extern void displayError(String message);

const KeyAction keyMatrix[ROWS][COLS] = {
    {KEY_1, KEY_2, KEY_3, KEY_A},
    {KEY_4, KEY_5, KEY_6, KEY_B},
    {KEY_7, KEY_8, KEY_9, KEY_C},
    {KEY_STAR, KEY_0, KEY_HASH, KEY_D}
};

void initializeKeyboard() {
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(0xFF);
  if (Wire.endTransmission() == 0) {
    keyboardState.initialized = true;
    Serial.println("Keyboard initialized!");
  } else {
    keyboardState.initialized = false;
    Serial.println("Keyboard init failed!");
  }
}

void scanKeyboard() {
  if (!keyboardState.initialized) return;
  
  static unsigned long lastScan = 0;
  if (millis() - lastScan < 50) return;
  lastScan = millis();
  
  for (byte col = 0; col < COLS; col++) {
    byte col_mask = 0x0F;
    col_mask &= ~(1 << col);
    byte output = (0xF0) | col_mask;

    Wire.beginTransmission(PCF8574_ADDR);
    Wire.write(output);
    Wire.endTransmission();
    delayMicroseconds(50);

    Wire.requestFrom(PCF8574_ADDR, 1);
    if (Wire.available()) {
      byte data = Wire.read();

      for (byte row = 0; row < ROWS; row++) {
        if (!(data & (1 << (row + 4)))) {
          int keyIndex = row * COLS + col;
          
          if (!keyboardState.keyPressed[keyIndex]) {
            keyboardState.keyPressed[keyIndex] = true;
            keyboardState.lastKey = keyMatrix[row][col];
            keyboardState.lastKeyTime = millis();
            keyboardState.keyHoldTime = 0;
          } else {
            keyboardState.keyHoldTime = millis() - keyboardState.lastKeyTime;
            
            if (keyboardState.keyHoldTime > 1000) {
              KeyAction longPressKey = KEY_NONE;
              
              switch (keyMatrix[row][col]) {
                case KEY_STAR: longPressKey = KEY_MENU; break;
                case KEY_HASH: longPressKey = KEY_BACK; break;
                case KEY_5: longPressKey = KEY_SELECT; break;
                case KEY_2: longPressKey = KEY_UP; break;
                case KEY_8: longPressKey = KEY_DOWN; break;
                case KEY_4: longPressKey = KEY_LEFT; break;
                case KEY_6: longPressKey = KEY_RIGHT; break;
              }
              
              if (longPressKey != KEY_NONE) {
                handleKeyPress(longPressKey);
                keyboardState.keyPressed[keyIndex] = false;
                keyboardState.lastKey = KEY_NONE;
              }
            }
          }
        } else {
          int keyIndex = row * COLS + col;
          if (keyboardState.keyPressed[keyIndex]) {
            keyboardState.keyPressed[keyIndex] = false;
            
            if (keyboardState.lastKey == keyMatrix[row][col]) {
              handleKeyPress(keyboardState.lastKey);
              keyboardState.lastKey = KEY_NONE;
            }
          }
        }
      }
    }
  }
  
  Wire.beginTransmission(PCF8574_ADDR);
  Wire.write(0xFF);
  Wire.endTransmission();
}

void handleKeyPress(KeyAction key) {
  // Handle notification dismissal first
  if (displayState.showingNotification) {
    if (key == KEY_5 || key == KEY_STAR || key == KEY_SELECT) {
      dismissCurrentMessage();
    }
    return; // Block all other inputs when showing notification
  }
  
  if (displayState.inputMode) {
    char c = keyToChar(key);
    if (c != 0) {
      handleInput(c);
    } else if (key == KEY_HASH || key == KEY_BACK) {
      handleInput('#');
    } else if (key == KEY_STAR || key == KEY_SELECT) {
      handleInput('*');
    } else if (key == KEY_C) {
      handleInput('C');
    }
    return;
  }
  
  if (displayState.inMenu) {
    switch (key) {
      case KEY_2:
      case KEY_UP:
        navigateUp();
        break;
        
      case KEY_8:
      case KEY_DOWN:
        navigateDown();
        break;
        
      case KEY_5:
      case KEY_SELECT:
        selectMenuItem();
        break;
        
      case KEY_HASH:
      case KEY_BACK:
        goBack();
        break;
        
      case KEY_STAR:
      case KEY_MENU:
        displayState.inMenu = false;
        showMainScreen();
        break;
        
      default:
        if (key >= KEY_1 && key <= KEY_9) {
          int itemIndex = key - KEY_1;
          if (itemIndex < displayState.currentMenu.itemCount) {
            displayState.currentMenu.selectedItem = itemIndex;
            selectMenuItem();
          }
        }
        break;
    }
    return;
  }
  
  switch (key) {
    case KEY_STAR:
    case KEY_MENU:
      displayState.inMenu = true;
      createMainMenu();
      showMenu();
      break;
      
    case KEY_BACK:
      displayState.currentScreen = "main";
      showMainScreen();
      break;
      
    case KEY_A:
      displayState.currentScreen = "status";
      break;
      
    case KEY_B:
      displayState.currentScreen = "gsm";
      break;
      
    case KEY_C:
      displayState.currentScreen = "gps";
      break;
      
    case KEY_D:
      displayState.inMenu = true;
      createMainMenu();
      showMenu();
      break;
      
    default:
      char c = keyToChar(key);
      if (c != 0) {
        addToInput(c);
      }
      break;
  }
}

char keyToChar(KeyAction key) {
  switch (key) {
    case KEY_0: return '0';
    case KEY_1: return '1';
    case KEY_2: return '2';
    case KEY_3: return '3';
    case KEY_4: return '4';
    case KEY_5: return '5';
    case KEY_6: return '6';
    case KEY_7: return '7';
    case KEY_8: return '8';
    case KEY_9: return '9';
    case KEY_STAR: return '*';
    case KEY_HASH: return '#';
    default: return 0;
  }
}

void clearInput() {
  keyboardState.inputBuffer = "";
  keyboardState.cursorPosition = 0;
}

void backspace() {
  if (keyboardState.inputBuffer.length() > 0) {
    keyboardState.inputBuffer.remove(keyboardState.inputBuffer.length() - 1);
    keyboardState.cursorPosition--;
    if (keyboardState.cursorPosition < 0) keyboardState.cursorPosition = 0;
  }
}

void addToInput(char c) {
  if (keyboardState.inputBuffer.length() < 20) {
    keyboardState.inputBuffer += c;
    keyboardState.cursorPosition++;
  }
}
