# Phase 1: Hardware Initialization and Configuration

## Overview

Phase 1 establishes the foundation for the ESP8266 Jacuzzi Controller by implementing boot-safe hardware initialization, centralized configuration management, and I2C device verification. This phase ensures all hardware components are properly initialized in the correct sequence with special attention to boot-strap sensitive pins.

## Implementation Date

Completed: 2026-05-07

## Hardware Configuration

### Board Selection

- **Board**: ESP8266 esp12e
- **Platform**: espressif8266
- **Framework**: Arduino
- **Upload Speed**: 921600 baud
- **Monitor Speed**: 115200 baud

### Pin Mapping

| Function | Pin | GPIO | Notes |
|----------|-----|------|-------|
| Buzzer | D8 | GPIO15 | ⚠️ BOOT-STRAP SENSITIVE - MUST be LOW at boot |
| Temperature Sensor | D5 | GPIO14 | DS18B20 OneWire |
| Water Level Sensor | D7 | GPIO13 | XKC-Y25-V (active-low: LOW=sufficient, HIGH=insufficient) |
| Encoder CLK | D6 | GPIO12 | Rotary encoder clock |
| Encoder DT | A0 | GPIO17 | ⚠️ VALIDATION REQUIRED in Phase 7 |
| Encoder SW | D3 | GPIO0 | ⚠️ BOOT-STRAP SENSITIVE - needs pull-up resistor |

### I2C Devices

| Device | Address | Purpose |
|--------|---------|---------|
| SH110X OLED | 0x3C | 128x64 display |
| PCF8574 | 0x20 | 8-channel relay controller |

**I2C Bus Configuration:**
- Clock Speed: 100kHz (standard mode)
- Both devices detected successfully

### Relay Channel Mapping

| Channel | Bit | Load | Notes |
|---------|-----|------|-------|
| 0 | Bit 0 | Circulation Pump | Foundation for all features |
| 1 | Bit 1 | Massage Pump | Requires circulation |
| 2 | Bit 2 | Jet Pump | Requires circulation |
| 3 | Bit 3 | Water Heater | 3kW load, 5V/30A relay, requires circulation |
| 4 | Bit 4 | Ozone Generator | Requires circulation |
| 5 | Bit 5 | Speaker Relay | Requires circulation |
| 6 | Bit 6 | Light System | Requires circulation |
| 7 | Bit 7 | Spare | RESERVED, permanently OFF |

**Relay Logic:**
- Active-low: HIGH = OFF, LOW = ON
- Boot default: 0xFF (all HIGH = all OFF)
- All relays verified OFF at boot

## Configuration Headers Created

### 1. HardwareConfig.h
- Complete pin mapping with boot-strap warnings
- I2C device addresses
- I2C clock speed configuration
- Relay channel mapping (0-7)
- Relay control constants (RELAY_ALL_OFF, RELAY_ALL_ON)

### 2. SafetyConfig.h
- Temperature valid range: -10°C to 60°C
- Warning threshold: 42°C (enters Warning state)
- Critical threshold: 45°C (enters Fault state)
- Default heater setpoint: 38°C
- Thermal runaway detection parameters
- Sensor fault thresholds
- Water level timing constants

### 3. TimingConfig.h
- Sensor read intervals (temperature: 2s, water level: 500ms)
- Display update interval: 100ms (10Hz)
- Encoder debounce: 50ms
- Feature timing (circulation delay: 2s, heater auto-start: 5s)
- Safe shutdown sequence delays
- I2C transaction timeout: 1000ms
- Buzzer duration constants

### 4. StateDefinitions.h
- 9 system states defined:
  - STATE_BOOT
  - STATE_SELF_CHECK
  - STATE_READY
  - STATE_ACTIVE_CIRCULATION
  - STATE_FEATURE_ENABLED_BATH
  - STATE_WARNING
  - STATE_FAULT
  - STATE_FAULT_INSPECTION
  - STATE_SHUTDOWN
- Complete state descriptions and transitions

### 5. FaultCodes.h
- Bit field enumeration for multiple simultaneous faults
- 6 fault types defined:
  - FAULT_LOW_WATER_LEVEL (Bit 0)
  - FAULT_HIGH_TEMPERATURE (Bit 1)
  - FAULT_THERMAL_RUNAWAY (Bit 2)
  - FAULT_TEMPERATURE_SENSOR (Bit 3)
  - FAULT_I2C_FAILURE (Bit 4)
  - FAULT_PCF8574_FAILURE (Bit 5)
- Auto-clear vs manual acknowledgment documented

## Boot Sequence

### Initialization Order (CRITICAL)

1. **Boot-strap sensitive pins FIRST:**
   - D8 (GPIO15) buzzer set to LOW
   - D3 (GPIO0) encoder SW configured with INPUT_PULLUP

2. **Serial debug initialization:**
   - 115200 baud
   - 3-second timeout for connection

3. **I2C bus initialization:**
   - Wire.begin()
   - Clock set to 100kHz

4. **I2C device detection:**
   - OLED at 0x3C: FOUND ✓
   - PCF8574 at 0x20: FOUND ✓

5. **PCF8574 relay initialization:**
   - Write 0xFF (all relays OFF)
   - Verified successful

6. **Sensor pin configuration:**
   - Temperature sensor (D5)
   - Water level sensor (D7)

7. **Encoder pin configuration:**
   - CLK (D6)
   - DT (A0)
   - SW (D3) - already configured

## Hardware Validation Results

### Test Results

✅ **Build and Upload:** Successful
✅ **Serial Monitor:** All initialization messages displayed correctly
✅ **I2C Devices:** Both OLED (0x3C) and PCF8574 (0x20) detected
✅ **Boot-strap Pins:** D8 LOW at boot (no buzzer sound), D3 with pull-up working correctly
✅ **Boot with Button Pressed:** System boots normally (no programming mode)
✅ **Relay State:** All relays OFF at boot (verified via serial output)

### Serial Monitor Output

```
========================================
ESP8266 Jacuzzi Controller
Phase 1: Hardware Initialization
========================================

Initializing I2C bus...
I2C bus initialized at 100 kHz

Scanning I2C devices...
OLED (0x3C): FOUND
PCF8574 (0x20): FOUND

Initializing PCF8574 relay controller...
All relays set to OFF (0xFF)
Active-low logic: HIGH = OFF, LOW = ON

Configuring sensor pins...
Temperature sensor: D5 (GPIO14)
Water level sensor: D7 (GPIO13) - active-low

Configuring encoder pins...
Encoder CLK: D6 (GPIO12)
Encoder DT: A0 (GPIO17) - VALIDATION REQUIRED in Phase 7
Encoder SW: D3 (GPIO0) - boot-strap sensitive

========================================
Hardware initialization complete
========================================

Status Summary:
  Boot-strap pins: SAFE (D8 LOW, D3 with pull-up)
  I2C devices: ALL FOUND
  Relays: ALL OFF
```

## Build Configuration

### platformio.ini

- **Libraries:**
  - Adafruit SH110X@^2.1.14 (exact version)
  - Adafruit GFX Library@^1.11.0
  - OneWire@^2.3.7
  - DallasTemperature@^3.11.0
  - ezButton@^1.0.6

- **Build Flags:**
  - `-DENABLE_SERIAL_DEBUG` (conditional debug output)
  - NO `-flto` (Link Time Optimization disabled - absolute rule)

- **Build Source Filter:**
  - `+<*>` (include all src files)
  - `+<../lib/*/*.cpp>` (include lib implementations)

## Circuit Requirements

### Boot-Strap Pin Safety

**D8 (GPIO15) - Buzzer:**
- MUST be LOW at boot
- Initialized FIRST in setup() before any other operations
- Active-high buzzer circuit (HIGH = sound, LOW = silent)

**D3 (GPIO0) - Encoder Switch:**
- Circuit MUST include pull-up resistor
- Configured with INPUT_PULLUP
- Tested: Boot with button pressed works correctly (no programming mode)

### I2C Bus

- Standard mode: 100kHz
- Shared bus between OLED and PCF8574
- No address conflicts detected

### Relay Control

- Active-low logic through PCF8574
- HIGH = relay OFF (coil de-energized)
- LOW = relay ON (coil energized)
- All relays default to OFF (0xFF) at boot

## Known Issues and Caveats

### Pin A0 (GPIO17) Validation

- **Status:** Requires validation in Phase 7
- **Purpose:** Rotary encoder DT input
- **Issue:** A0 is ADC pin, may not function correctly as digital input
- **Alternatives:** D0 (GPIO16) or D4 (GPIO2) if A0 fails validation
- **Action:** Test in Phase 7 with actual encoder hardware

### Spare Relay Channel

- Channel 7 (Bit 7) reserved for future use
- Permanently OFF in current implementation
- Can be assigned to additional load in future phases if needed

## Memory Usage

- **Configuration Headers:** All constants in PROGMEM-compatible format
- **NO Hardcoded Values:** All constants centralized in include/* headers
- **Debug Output:** Conditional compilation with ENABLE_SERIAL_DEBUG flag

## Next Steps

**Phase 2: Sensor Integration**
- Implement DS18B20 temperature sensor driver (non-blocking)
- Implement XKC-Y25-V water level sensor driver
- Integrate with src/main.cpp event loop
- Validate sensor readings on hardware

## Lessons Learned

1. **Boot-strap pin initialization order is CRITICAL** - D8 and D3 must be configured first
2. **I2C device detection before use** - Prevents crashes if devices not connected
3. **Active-low relay logic** - 0xFF = all OFF, not 0x00
4. **Debug macro limitations** - Serial.print() with format specifiers requires direct calls, not macros
5. **F() macro for strings** - Saves RAM by storing strings in flash memory

## Files Created/Modified

### Created:
- `include/HardwareConfig.h`
- `include/SafetyConfig.h`
- `include/TimingConfig.h`
- `include/StateDefinitions.h`
- `include/FaultCodes.h`
- `docs/phase-1-hardware-initialization.md`

### Modified:
- `platformio.ini` (added build flags)
- `src/main.cpp` (complete hardware initialization)

## Approval

**User Approval:** ✅ Granted on 2026-05-07
**Hardware Validation:** ✅ All tests passed
**Ready for Phase 2:** ✅ Yes
