# Code Conventions and Standards

## Absolute Rules (Non-Negotiable)

### 1. NO Hardcoded Constants
**ABSOLUTE RULE**: All constants MUST be defined in `include/*` headers. NO hardcoded values anywhere in the codebase.

- Pin assignments → `include/HardwareConfig.h`
- Temperature thresholds, safety limits → `include/SafetyConfig.h`
- All timing values → `include/TimingConfig.h`
- State enumerations → `include/StateDefinitions.h`
- Fault codes → `include/FaultCodes.h`

**Violation Examples (FORBIDDEN)**:
```cpp
// ❌ WRONG - Hardcoded values
if (temperature > 42.0) { ... }
delay(2000);
Wire.begin(0x3C);
pinMode(15, OUTPUT);
```

**Correct Examples**:
```cpp
// ✅ CORRECT - Constants from headers
if (temperature > TEMP_WARNING_THRESHOLD) { ... }
// Use millis() timing, not delay()
if (i2cAddress == I2C_ADDRESS_OLED) { ... }
pinMode(PIN_BUZZER, OUTPUT);
```

### 2. NO delay() Calls
**ABSOLUTE RULE**: NO `delay()` calls in application logic. Use `millis()`-based timing.

```cpp
// ❌ WRONG - Blocks execution
delay(1000);

// ✅ CORRECT - Non-blocking timing
unsigned long currentTime = millis();
if (currentTime - lastTime >= INTERVAL_MS) {
    lastTime = currentTime;
    // Execute timed operation
}
```

### 3. Bitmap-Only UI
**ABSOLUTE RULE**: NO text rendering functions. All UI elements use bitmaps.

```cpp
// ❌ WRONG - Text rendering
display.print("Temperature: ");
display.println(temp);
display.drawChar(x, y, 'A', WHITE, BLACK, 1);

// ✅ CORRECT - Bitmap rendering
drawScaledBitmap(thermometer_bitmap, x, y, w, h);
drawBitmapGlyph('A', x, y);  // From bitmap glyph table
```

### 4. Scale-by-2 Rule
**ABSOLUTE RULE**: All bitmaps MUST be scaled down by factor of 2 when displayed.

- Design bitmaps at 2x resolution
- Display at 1x by sampling every other pixel
- Ensures consistent visual appearance

### 5. Exact Bitmap Names
**ABSOLUTE RULE**: All bitmaps MUST have exact names ending in `_bitmap` suffix:

1. `water_drop_bitmap`
2. `circulation_bitmap`
3. `massage_bitmap`
4. `jet_bitmap`
5. `heater_bitmap`
6. `speaker_bitmap`
7. `ozone_bitmap`
8. `light_bulb_bitmap`
9. `settings_bitmap`
10. `thermometer_bitmap`
11. `low_water_level_error_bitmap`
12. `high_temperature_error_bitmap`
13. `sensor_error_bitmap`

### 6. NO Link Time Optimization
**ABSOLUTE RULE**: NO `flto` in `platformio.ini` build flags.

```ini
; ❌ WRONG
build_flags = -flto

; ✅ CORRECT - No flto
build_flags = -DENABLE_SERIAL_DEBUG
```

### 7. Build Source Filter Required
**ABSOLUTE RULE**: `build_src_filter` MUST be in `platformio.ini`.

```ini
build_src_filter =
    +<*>
    +<../lib/*/*.cpp>
```

## Code Style

### Naming Conventions

**Classes**: PascalCase
```cpp
class SensorManager { };
class RelayController { };
```

**Functions/Methods**: camelCase
```cpp
void readTemperature();
bool isWaterLevelSufficient();
```

**Constants**: UPPER_SNAKE_CASE
```cpp
#define TEMP_WARNING_THRESHOLD 42.0
#define PIN_BUZZER 15
const unsigned long SENSOR_READ_INTERVAL_MS = 2000;
```

**Variables**: camelCase
```cpp
float currentTemperature;
bool isCirculationActive;
unsigned long lastReadTime;
```

**Bitmaps**: snake_case with `_bitmap` suffix
```cpp
const unsigned char water_drop_bitmap[] PROGMEM = { ... };
const unsigned char circulation_bitmap[] PROGMEM = { ... };
```

**Enums**: PascalCase for type, UPPER_SNAKE_CASE for values
```cpp
enum SystemState {
    STATE_BOOT,
    STATE_SELF_CHECK,
    STATE_READY
};

enum FaultCode {
    FAULT_NONE = 0,
    FAULT_LOW_WATER_LEVEL = (1 << 0),
    FAULT_HIGH_TEMPERATURE = (1 << 1)
};
```

### File Organization

**Headers**: PascalCase with `.h` extension
- `SensorManager.h`
- `RelayController.h`
- `HardwareConfig.h`

**Implementation**: PascalCase with `.cpp` extension
- `SensorManager.cpp`
- `RelayController.cpp`

**Location Pattern**:
- Public API: `include/<Module>.h`
- Implementation: `lib/<Module>/<Module>.cpp`
- Main integration: `src/main.cpp`

### Header Guards

Use `#pragma once` (preferred for ESP8266):
```cpp
#pragma once

// Header content
```

### Include Order

1. Arduino/system headers
2. External library headers
3. Project configuration headers
4. Project module headers

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SH110X.h>

#include "HardwareConfig.h"
#include "SafetyConfig.h"
#include "TimingConfig.h"

#include "SensorManager.h"
#include "RelayController.h"
```

## Memory Management

### PROGMEM Usage

Store all constants in flash memory:
```cpp
// Bitmaps
const unsigned char water_drop_bitmap[] PROGMEM = { ... };

// Strings
const char errorMsg[] PROGMEM = "Sensor Error";

// Access with pgm_read_byte()
uint8_t byte = pgm_read_byte(&water_drop_bitmap[index]);
```

### Static Allocation

Prefer static allocation over dynamic:
```cpp
// ✅ CORRECT - Static allocation
float temperatureHistory[TEMP_HISTORY_SIZE];
uint8_t relayState;

// ❌ AVOID - Dynamic allocation
float* temperatureHistory = new float[size];  // Causes heap fragmentation
```

### F() Macro for Strings

Use F() macro for serial debug strings:
```cpp
#ifdef ENABLE_SERIAL_DEBUG
    Serial.println(F("Initialization complete"));
#endif
```

## Safety-Critical Code

### Boot-Strap Sensitive Pins

**D8 (GPIO15) - Buzzer**: MUST be LOW at boot
```cpp
// Initialize FIRST, before any other operations
pinMode(PIN_BUZZER, OUTPUT);
digitalWrite(PIN_BUZZER, LOW);
```

**D3 (GPIO0) - Encoder SW**: Circuit must include pull-up
```cpp
// Configure with pull-up
pinMode(PIN_ENCODER_SW, INPUT_PULLUP);
```

### Active-Low Relay Logic

HIGH = OFF, LOW = ON
```cpp
// Activate relay (active-low)
relayState &= ~(1 << channel);  // Clear bit

// Deactivate relay (active-low)
relayState |= (1 << channel);   // Set bit
```

### Circulation Dependency

**ABSOLUTE RULE**: Heater NEVER activates without circulation
```cpp
bool SafetySystem::checkHeaterPreconditions() {
    // ABSOLUTE RULE: Circulation MUST be active
    if (!relayController.getRelayState(RELAY_CIRCULATION_PUMP)) {
        return false;
    }
    // ... other checks
}
```

## Comments and Documentation

### Function Comments

Document public API functions:
```cpp
/**
 * @brief Reads temperature sensor non-blocking
 * @return true if reading complete, false if waiting
 */
bool readTemperature();
```

### Safety-Critical Comments

Mark safety-critical logic:
```cpp
// SAFETY: Heater requires circulation pump active (absolute rule)
if (!isCirculationActive()) {
    return false;
}

// SAFETY: Boot-strap sensitive pin - must be LOW at boot
digitalWrite(PIN_BUZZER, LOW);
```

### TODO Comments

Use consistent format:
```cpp
// TODO(Phase 7): Validate A0 pin for encoder DT input
// FIXME: Handle I2C timeout more gracefully
```

## Debug Output

### Conditional Compilation

Use `ENABLE_SERIAL_DEBUG` flag:
```cpp
#ifdef ENABLE_SERIAL_DEBUG
    Serial.print(F("Temperature: "));
    Serial.println(temperature);
#endif
```

### Debug Macros

Define helper macros:
```cpp
#ifdef ENABLE_SERIAL_DEBUG
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
#endif
```

## Error Handling

### Return Values

Use boolean for success/failure:
```cpp
bool initializeSensor() {
    if (!sensor.begin()) {
        return false;  // Initialization failed
    }
    return true;  // Success
}
```

### Fault State Entry

Always execute safe shutdown:
```cpp
void enterFaultState(FaultCode fault) {
    activeFaults |= fault;  // Set fault bit
    executeSafeShutdown();
    displayFault(fault);
}
```

## Integration with main.cpp

### Global Instances

Declare module instances globally:
```cpp
// Global instances
I2CBusManager i2cBus;
SensorManager sensors;
RelayController relays;
StateMachine stateMachine;
```

### setup() Pattern

```cpp
void setup() {
    // 1. Boot-sensitive pins FIRST
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    // 2. I2C bus initialization
    Wire.begin();
    Wire.setClock(100000);
    
    // 3. Module initialization
    i2cBus.begin();
    sensors.begin();
    relays.begin();
    
    // 4. State machine initialization
    stateMachine.begin();
}
```

### loop() Pattern

```cpp
void loop() {
    // Non-blocking event loop
    sensors.update();
    relays.update();
    stateMachine.update();
    safety.update();
    ui.update();
    input.update();
}
```

## Testing Approach

### Manual Hardware Validation

Each phase requires:
1. Build and upload to ESP8266
2. Manual testing on physical hardware
3. Verification of all acceptance criteria
4. User approval before proceeding

### No Automated Tests

This project uses **manual hardware validation only**:
- No unit test framework
- No integration test framework
- No property-based testing
- Hardware-in-the-loop testing only

### Debug Verification

Use serial debug output for verification:
```cpp
#ifdef ENABLE_SERIAL_DEBUG
    Serial.print(F("State transition: "));
    Serial.print(oldState);
    Serial.print(F(" -> "));
    Serial.println(newState);
#endif
```
