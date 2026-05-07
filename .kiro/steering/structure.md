# Project Structure

## Directory Organization

```
.
├── .kiro/                      # Kiro configuration and specs
│   ├── specs/                  # Feature specifications
│   │   └── esp8266-jacuzzi-controller/
│   │       ├── design.md       # Technical design document
│   │       └── tasks.md        # Implementation task list
│   └── steering/               # Project steering rules
│       ├── product.md          # Product overview
│       ├── tech.md             # Technology stack
│       └── structure.md        # This file
│
├── .pio/                       # PlatformIO build artifacts (auto-generated)
│   ├── build/                  # Compiled binaries
│   └── libdeps/                # Downloaded library dependencies
│
├── docs/                       # Phase documentation (created during development)
│   ├── phase-1-hardware-initialization.md
│   ├── phase-2-sensor-integration.md
│   └── ...                     # One doc per phase
│
├── include/                    # Public header files (API definitions)
│   ├── Bitmaps.h              # All bitmap definitions (PROGMEM)
│   ├── HardwareConfig.h       # Pin assignments, I2C addresses
│   ├── SafetyConfig.h         # Temperature thresholds, safety limits
│   ├── TimingConfig.h         # All timing constants
│   ├── StateDefinitions.h     # State machine states and transitions
│   ├── FaultCodes.h           # Fault code enumerations
│   ├── I2CBusManager.h        # I2C bus management interface
│   ├── SensorManager.h        # Sensor reading and validation interface
│   ├── RelayController.h      # Relay control interface
│   ├── StateMachine.h         # State machine interface
│   ├── SafetySystem.h         # Safety system interface
│   ├── UIManager.h            # UI management interface
│   └── InputHandler.h         # Input handling interface
│
├── lib/                        # Private library implementations
│   ├── I2CBusManager/
│   │   └── I2CBusManager.cpp
│   ├── SensorManager/
│   │   └── SensorManager.cpp
│   ├── RelayController/
│   │   └── RelayController.cpp
│   ├── StateMachine/
│   │   └── StateMachine.cpp
│   ├── SafetySystem/
│   │   └── SafetySystem.cpp
│   ├── UIManager/
│   │   └── UIManager.cpp
│   └── InputHandler/
│       └── InputHandler.cpp
│
├── src/                        # Main application source
│   └── main.cpp               # Event loop integration point
│
├── .gitignore                 # Git ignore rules
├── platformio.ini             # PlatformIO configuration
└── README.md                  # Project readme
```

## Module Organization Pattern

### Header/Implementation Split

**Pattern**: `include/<Module>.h` (public API) + `lib/<Module>/<Module>.cpp` (implementation)

This follows PlatformIO's local module specification:
- **Public API** in `include/` - interfaces, constants, type definitions
- **Implementation** in `lib/<Module>/` - actual code, private details
- **Build system** automatically links implementations via `build_src_filter`

### Example Module Structure

```cpp
// include/SensorManager.h (public API)
#pragma once
#include <Arduino.h>

class SensorManager {
public:
    void begin();
    void update();
    float getTemperature();
    bool isWaterLevelSufficient();
private:
    // Private members
};

// lib/SensorManager/SensorManager.cpp (implementation)
#include "SensorManager.h"
#include "HardwareConfig.h"
#include "SafetyConfig.h"

void SensorManager::begin() {
    // Implementation
}
// ... rest of implementation
```

## Configuration Headers

### Centralized Constants (Absolute Rule)

**ALL constants MUST be defined in `include/*` headers - NO hardcoded values anywhere**

#### HardwareConfig.h
- Pin assignments (GPIO mappings)
- I2C device addresses
- Hardware-specific constants

#### SafetyConfig.h
- Temperature thresholds (warning, critical, thermal runaway)
- Water level timing thresholds
- Sensor error thresholds
- Safety limits

#### TimingConfig.h
- Sensor read intervals
- Display refresh rates
- Debounce durations
- Shutdown sequence delays
- Timeout values

#### StateDefinitions.h
- State enumerations
- State transition definitions

#### FaultCodes.h
- Fault bit field definitions
- Fault code enumerations

## Bitmap Organization

### Bitmaps.h Structure

All bitmaps stored in `include/Bitmaps.h` with:
- **PROGMEM storage** (flash memory, not RAM)
- **Exact naming convention**: `<name>_bitmap` suffix
- **Size macros**: `<NAME>_BMPWIDTH` and `<NAME>_BMPHEIGHT`
- **2x resolution**: All bitmaps designed at 2x for scale-by-2 display rule

```cpp
// Example bitmap definition
#define WATER_DROP_BMPWIDTH  128
#define WATER_DROP_BMPHEIGHT 64
const unsigned char water_drop_bitmap[] PROGMEM = {
    0x00, 0x00, 0x00, ...
};
```

### Required Bitmaps (13 total)

1. `water_drop_bitmap` - Water level indicator
2. `circulation_bitmap` - Circulation pump
3. `massage_bitmap` - Massage pump
4. `jet_bitmap` - Jet pump
5. `heater_bitmap` - Water heater
6. `speaker_bitmap` - Speaker relay
7. `ozone_bitmap` - Ozone generator
8. `light_bulb_bitmap` - Light system
9. `settings_bitmap` - Settings/menu
10. `thermometer_bitmap` - Temperature display
11. `low_water_level_error_bitmap` - Low water fault
12. `high_temperature_error_bitmap` - Temperature fault
13. `sensor_error_bitmap` - Sensor failure

## Main Application Integration

### src/main.cpp Pattern

```cpp
#include <Arduino.h>
#include "I2CBusManager.h"
#include "SensorManager.h"
#include "RelayController.h"
#include "StateMachine.h"
#include "SafetySystem.h"
#include "UIManager.h"
#include "InputHandler.h"

// Global instances
I2CBusManager i2cBus;
SensorManager sensors;
RelayController relays;
StateMachine stateMachine;
SafetySystem safety;
UIManager ui;
InputHandler input;

void setup() {
    // 1. Boot-sensitive pins FIRST
    // 2. I2C bus initialization
    // 3. Hardware initialization
    // 4. Module initialization
}

void loop() {
    // Non-blocking event loop
    // Update all modules
    // Process state machine
    // Handle safety checks
    // Update UI
}
```

## Build Artifacts

### .pio/ Directory (Auto-Generated)

- **DO NOT commit** to version control
- **DO NOT manually edit** files in this directory
- Contains compiled binaries and downloaded libraries
- Regenerated on each build

## Documentation

### Phase Documentation Pattern

Each development phase creates a document in `docs/`:
- `phase-N-<description>.md`
- Documents implementation details, validation results, and lessons learned
- Created AFTER phase completion and hardware validation

## Naming Conventions

### Files
- **Headers**: PascalCase (e.g., `SensorManager.h`)
- **Source**: PascalCase (e.g., `SensorManager.cpp`)
- **Config headers**: PascalCase (e.g., `HardwareConfig.h`)

### Code
- **Classes**: PascalCase (e.g., `SensorManager`)
- **Functions**: camelCase (e.g., `getTemperature()`)
- **Constants**: UPPER_SNAKE_CASE (e.g., `TEMP_WARNING_THRESHOLD`)
- **Bitmaps**: snake_case with `_bitmap` suffix (e.g., `water_drop_bitmap`)

### Git Branches
- **Feature branches**: `feature/phase-N-description`
- **Main branch**: `main`
