#ifndef KEYBOARD_MANAGER_H
#define KEYBOARD_MANAGER_H

#include <Wire.h>

// I2C pins
#define I2C_SDA 21
#define I2C_SCL 22

// PCF8574 I2C address
#define PCF8574_ADDR 0x20

// Matrix dimensions
#define ROWS 4
#define COLS 4

// Key actions
enum KeyAction {
  KEY_NONE = 0,
  KEY_1 = '1',
  KEY_2 = '2',
  KEY_3 = '3',
  KEY_A = 'A',
  KEY_4 = '4',
  KEY_5 = '5',
  KEY_6 = '6',
  KEY_B = 'B',
  KEY_7 = '7',
  KEY_8 = '8',
  KEY_9 = '9',
  KEY_C = 'C',
  KEY_STAR = '*',
  KEY_0 = '0',
  KEY_HASH = '#',
  KEY_D = 'D',
  
  // Special actions (long press)
  KEY_MENU = 128,
  KEY_BACK = 129,
  KEY_SELECT = 130,
  KEY_UP = 131,
  KEY_DOWN = 132,
  KEY_LEFT = 133,
  KEY_RIGHT = 134
};

// Keyboard state
struct KeyboardState {
  bool initialized = false;
  KeyAction lastKey = KEY_NONE;
  unsigned long lastKeyTime = 0;
  unsigned long keyHoldTime = 0;
  bool keyPressed[16] = {false};
  
  String inputBuffer = "";
  int cursorPosition = 0;
};

extern KeyboardState keyboardState;

// Keyboard functions
void initializeKeyboard();
void scanKeyboard();
void handleKeyPress(KeyAction key);
char keyToChar(KeyAction key);
void clearInput();
void backspace();
void addToInput(char c);

#endif
