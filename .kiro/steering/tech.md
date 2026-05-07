# Technology Stack

## Build System

**PlatformIO** - Modern embedded development platform for ESP8266

### Platform Configuration

- **Board**: `esp12e` (ESP8266)
- **Platform**: `espressif8266`
- **Framework**: `arduino`
- **Monitor Speed**: 115200 baud
- **Upload Speed**: 921600 baud

### Critical Build Settings

- **NO Link Time Optimization**: `flto` is explicitly disabled (absolute rule)
- **Build Source Filter**: `build_src_filter = +<*> +<../lib/*/*.cpp>` to include lib implementations
- **Debug Flag**: `DENABLE_SERIAL_DEBUG` for conditional serial output

## Libraries and Dependencies

### External Libraries (PlatformIO)

```ini
lib_deps =
    adafruit/Adafruit SH110X@^2.1.14      # OLED display driver (exact version required)
    adafruit/Adafruit GFX Library@^1.11.0 # Graphics library
    paulstoffregen/OneWire@^2.3.7         # OneWire protocol for DS18B20
    milesburton/DallasTemperature@^3.11.0 # DS18B20 temperature sensor
    arduinogetstarted/ezButton@^1.0.6     # Button debouncing
```

**CRITICAL**: Use exact version `Adafruit SH110X@^2.1.14` - do not upgrade without testing

### Hardware Components

- **MCU**: ESP8266 (esp12e variant)
- **Display**: SH110X OLED 128x64 (I2C address 0x3C)
- **GPIO Expander**: PCF8574 (I2C address 0x20) for 8-channel relay control
- **Temperature Sensor**: DS18B20 (OneWire on GPIO14/D5)
- **Water Level Sensor**: XKC-Y25-V (Digital input on GPIO13/D7, active-low)
- **Rotary Encoder**: CLK on GPIO12/D6, DT on ADC0/GPIO17/A0, SW on GPIO0/D3
- **Buzzer**: GPIO15/D8 (boot-sensitive pin, must default LOW)

### I2C Bus

- **Speed**: 100kHz (standard mode for reliability)
- **Devices**: OLED (0x3C), PCF8574 (0x20)
- **Management**: Serialized transactions with lock/release mechanism

## Common Commands

### Build and Upload

```bash
# Build the project
pio run

# Upload to device
pio run --target upload

# Build and upload in one command
pio run -t upload

# Clean build artifacts
pio run --target clean
```

### Monitoring and Debugging

```bash
# Open serial monitor
pio device monitor

# Build, upload, and monitor
pio run -t upload && pio device monitor

# Monitor with specific baud rate
pio device monitor --baud 115200
```

### Library Management

```bash
# Update libraries
pio pkg update

# List installed libraries
pio pkg list

# Install specific library
pio pkg install "adafruit/Adafruit SH110X@^2.1.14"
```

### Project Management

```bash
# Initialize new PlatformIO project
pio project init --board esp12e

# Check project configuration
pio project config
```

## Development Workflow

### Manual Hardware Validation

This project uses **manual hardware validation only** - no automated test framework. Each development phase requires:

1. Build and upload firmware to actual ESP8266 hardware
2. Manually test functionality on physical device
3. Verify sensor readings, relay operations, display output
4. Document results before proceeding to next phase

### Git Workflow (Per Phase)

```bash
# Create feature branch for phase
git checkout -b feature/phase-N-description

# Stage and commit changes
git add .
git commit -m "Phase N: Description"

# Push to remote
git push origin feature/phase-N-description

# Merge to main (after validation)
git checkout main
git merge feature/phase-N-description

# Delete feature branch
git branch -d feature/phase-N-description
git push origin --delete feature/phase-N-description
```

## Memory Constraints

- **CPU**: 80MHz
- **RAM**: 80KB (use static allocation, minimize heap usage)
- **Flash**: 4MB
- **PROGMEM**: Store all constants (bitmaps, strings) in flash memory

## Architecture Constraints

- **Non-Blocking**: NO `delay()` calls in application logic (use `millis()` timing)
- **Event Loop**: All operations integrated into main event loop
- **Centralized Configuration**: ALL constants in `include/*` headers (NO hardcoded values)
- **Bitmap-Only UI**: NO text rendering functions (use pre-defined bitmaps only)
