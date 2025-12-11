# Combined GPS Tracker - PlatformIO Project

## Project Structure
```
combined_tracker/
├── platformio.ini          # PlatformIO configuration
├── include/                # Header files (.h)
│   ├── Config.h
│   ├── Globals.h
│   ├── Tasks.h
│   ├── DisplayManager.h
│   ├── KeyboardManager.h
│   ├── SIM800L.h
│   └── Utils.h
├── src/                    # Source files (.cpp)
│   ├── main.cpp           # Main program (was combined_tracker.ino)
│   ├── Tasks.cpp
│   ├── DisplayManager.cpp
│   ├── KeyboardManager.cpp
│   ├── SIM800L.cpp
│   └── Utils.cpp
└── lib/                    # Custom libraries (empty)
```

## Setup

### 1. Install PlatformIO
If not already installed:
- **VS Code**: Install PlatformIO IDE extension
- **Command line**: `pip install platformio`

### 2. Build Project
```bash
cd combined_tracker
platformio run
```

### 3. Upload to ESP32
```bash
platformio run --target upload
```

### 4. Monitor Serial Output
```bash
platformio device monitor
```

Or use the all-in-one command:
```bash
platformio run --target upload && platformio device monitor
```

## Configuration

### platformio.ini Settings
- **Board**: ESP32 Dev Module
- **Framework**: Arduino
- **Monitor Speed**: 115200 baud
- **Libraries**: TinyGPSPlus, LoRa, U8g2

### Pin Configuration
See `include/Config.h` for all pin definitions:
- LoRa: SS=5, RST=14, DIO0=27
- SIM800L: RX=26, TX=25
- GPS: Serial0 (RX0, TX0)
- Display: I2C (SDA=21, SCL=22)
- Keyboard: Matrix 4x4

## PlatformIO Commands

| Command | Description |
|---------|-------------|
| `pio run` | Build project |
| `pio run -t upload` | Upload to board |
| `pio run -t clean` | Clean build files |
| `pio device monitor` | Serial monitor |
| `pio device list` | List connected devices |
| `pio lib list` | Show installed libraries |

## Features
- Multi-tasking with FreeRTOS (6 tasks)
- GPS tracking with TinyGPS++
- LoRa communication (433/866 MHz)
- GSM/SMS with SIM800L
- Bluetooth serial logging
- OLED display (128x64)
- 4x4 Keyboard input
- Dual mode: Tracker / Ground Station

## Troubleshooting

### Upload Issues
- Check USB cable and port
- Press BOOT button on ESP32 during upload
- Reduce upload speed: `upload_speed = 115200`

### Library Issues
```bash
pio lib install
```

### Clean Build
```bash
pio run -t clean
platformio run
```

## Bluetooth Connection
Device name: `GPS_Tracker_Combined`
- Use Serial Bluetooth Terminal app
- Commands: `tracker`, `ground`, `status`, `sms <message>`, `help`
