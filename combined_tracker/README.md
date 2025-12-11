# Combined Tracker with Display and Keyboard

## Hardware Requirements

### Display
- **OLED Display**: SSD1306 128x64 I2C
- **I2C Address**: 0x3C (default)
- **Connections**:
  - SDA → GPIO 21
  - SCL → GPIO 22
  - VCC → 3.3V
  - GND → GND

### Keyboard
- **4x4 Matrix Keyboard** via PCF8574 I2C expander
- **I2C Address**: 0x20
- **Connections**:
  - SDA → GPIO 21
  - SCL → GPIO 22
  - VCC → 5V
  - GND → GND

### Key Layout
```
[1] [2] [3] [A]
[4] [5] [6] [B]
[7] [8] [9] [C]
[*] [0] [#] [D]
```

## Display Screens

### Main Screen (Default)
- Current mode (TRACKER/GROUND)
- GPS status and satellite count
- GSM network status and signal strength
- LoRa connection status
- Menu hint

### Status Screen
- Detailed system information
- All module states
- Connection details

### GPS Screen
- GPS lock status
- Latitude and longitude (if locked)
- Satellite count
- Real-time updates

### GSM Screen
- Network registration status
- Signal strength indicator
- Last SMS timestamp
- Network information

## Keyboard Controls

### Navigation Keys
- **\* (Star)** - Open main menu (short press) / Long press for quick menu
- **# (Hash)** - Back / Cancel
- **2** - Navigate up in menus
- **8** - Navigate down in menus
- **5** - Select menu item
- **4** - Left / Backspace in input
- **6** - Right in lists

### Function Keys
- **A** - Quick access to system status
- **B** - Quick access to GSM info
- **C** - Quick access to GPS info
- **D** - Open main menu

### Input Mode
- **0-9** - Enter numeric characters
- **\*** - Confirm input
- **#** - Cancel input
- **C** - Backspace

## Menu System

### Main Menu
1. **Tracker Mode** - Switch to GPS tracking mode
2. **Ground Mode** - Switch to ground station mode
3. **Send Message** - Submenu for sending messages
4. **GPS Info** - View GPS details
5. **GSM Info** - View GSM details
6. **System Status** - View system status

### Send Message Submenu
1. **Quick Message 1** - "Return to base"
2. **Quick Message 2** - "Send location"
3. **Custom Message** - Enter custom message

## Features

### Real-Time Display Updates
- GPS coordinates update every 500ms
- GSM signal strength monitoring
- LoRa connection status
- Incoming message notifications

### Message Display
When LoRa or SMS messages are received:
- Automatic pop-up on display
- Shows sender information
- Message preview (word-wrapped)
- Auto-closes after 5 seconds
- Returns to main screen

### Input System
- Numeric input for phone numbers, IDs
- Text wrapping for long inputs
- Visual cursor indicator
- Confirmation before sending

### Multi-Task Architecture
- **Display Task** - Updates screen every 200ms
- **Keyboard Task** - Scans matrix every 50ms
- Independent of GPS, LoRa, SMS tasks
- No blocking operations

## Usage Examples

### Sending a Message via Keyboard
1. Press **\*** to open main menu
2. Navigate to "Send Message" with **2**/**8**
3. Press **5** to select
4. Choose "Custom Message"
5. Press **5** again
6. Type your message using number keys
7. Press **\*** to send
8. Message sent on both LoRa and GSM

### Switching Modes
1. Press **\*** for menu
2. Select "Tracker Mode" or "Ground Mode"
3. Press **5** to confirm
4. Display shows new mode

### Viewing GPS Location
1. Press **C** for quick GPS screen
   OR
2. Press **\*** → Navigate to "GPS Info" → Press **5**
3. View real-time coordinates
4. Press **#** to return

### Checking Received Messages
- Messages automatically pop up on display
- Shows: Type (LoRa/SMS), Sender, Message
- Displays for 5 seconds
- Stored in message history

## Libraries Required

```cpp
#include <U8g2lib.h>          // OLED display
#include <Wire.h>              // I2C communication
#include <TinyGPS++.h>         // GPS parsing
#include <LoRa.h>              // LoRa communication
#include "BluetoothSerial.h"   // Bluetooth
#include "SIM800L.h"           // GSM module
```

## Installation

1. Install required libraries via Arduino Library Manager:
   - U8g2
   - TinyGPS++
   - LoRa
   
2. Copy all files to project folder:
   - combined_tracker.ino
   - DisplayManager.h
   - DisplayManager.cpp
   - KeyboardManager.h
   - KeyboardManager.cpp
   - SIM800L.h
   - SIM800L.cpp

3. Upload to ESP32

4. Connect hardware as per pinout

## Troubleshooting

### Display Not Working
- Check I2C connections (SDA/SCL)
- Verify display address (run I2C scanner)
- Check power supply (3.3V)

### Keyboard Not Responding
- Check PCF8574 connections
- Verify I2C address 0x20
- Test with I2C scanner command
- Check matrix wiring

### Messages Not Displaying
- Ensure FreeRTOS tasks are running
- Check Serial Monitor for task start messages
- Verify mutex operations aren't blocking

## Pin Configuration Summary

```cpp
// I2C (Shared)
#define I2C_SDA 21
#define I2C_SCL 22

// GPS
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17

// SIM800L
#define SIM_RX_PIN 26
#define SIM_TX_PIN 25

// LoRa
#define LORA_SS 5
#define LORA_RST 14
#define LORA_DIO0 27
```

## Notes

- Display and keyboard share I2C bus
- All operations are non-blocking
- Menu system is hierarchical with back navigation
- Input mode supports numeric entry
- Long press detection for special functions
- Automatic message notifications
- Thread-safe with FreeRTOS mutexes
