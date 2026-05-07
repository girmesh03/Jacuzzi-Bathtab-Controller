# Phase 1: Hardware Initialization and Configuration - Implementation Checklist

## Phase Overview
**Goal**: Complete pin initialization with boot-safety, I2C bus initialization, device verification, centralized configuration headers, and integration with `src/main.cpp`.

**Requirements**: 1.1-1.15, 14.1-14.15, 18.1-18.14  
**Design**: Hardware Abstraction Layer, Module Organization, Boot-Safe Pin Initialization  
**Tasks**: 1-5 (67 total tasks)

---

## Task 1: Set up project structure and centralized configuration headers

### Pre-Implementation Checklist
- [ ] Read existing `platformio.ini` to understand current configuration
- [ ] Read existing `src/main.cpp` to understand current state
- [ ] Verify `.gitignore` includes `.pio/` directory
- [ ] Confirm no hardcoded constants exist in current codebase

### Implementation Checklist

#### 1.1 Create `include/HardwareConfig.h`
- [ ] Create file with `#pragma once` header guard
- [ ] Define **ALL** pin assignments as constants:
  - [ ] `PIN_BUZZER` = 15 (D8/GPIO15) with comment: "⚠️ BOOT-STRAP SENSITIVE - must be LOW at boot"
  - [ ] `PIN_TEMP_SENSOR` = 14 (D5/GPIO14) with comment: "DS18B20 OneWire"
  - [ ] `PIN_WATER_LEVEL` = 13 (D7/GPIO13) with comment: "XKC-Y25-V active-low"
  - [ ] `PIN_ENCODER_CLK` = 12 (D6/GPIO12)
  - [ ] `PIN_ENCODER_DT` = 17 (A0/ADC0/GPIO17) with comment: "⚠️ VALIDATION REQUIRED in Phase 7"
  - [ ] `PIN_ENCODER_SW` = 0 (D3/GPIO0) with comment: "⚠️ BOOT-STRAP SENSITIVE - needs pull-up"
- [ ] Define I2C addresses:
  - [ ] `I2C_ADDRESS_OLED` = 0x3C
  - [ ] `I2C_ADDRESS_PCF8574` = 0x20
- [ ] Define I2C configuration:
  - [ ] `I2C_CLOCK_SPEED` = 100000 (100kHz standard mode)
- [ ] Define relay channel mapping (0-7):
  - [ ] `RELAY_CHANNEL_CIRCULATION` = 0
  - [ ] `RELAY_CHANNEL_MASSAGE` = 1
  - [ ] `RELAY_CHANNEL_JET` = 2
  - [ ] `RELAY_CHANNEL_HEATER` = 3 with comment: "3kW load, 5V/30A relay"
  - [ ] `RELAY_CHANNEL_OZONE` = 4
  - [ ] `RELAY_CHANNEL_SPEAKER` = 5
  - [ ] `RELAY_CHANNEL_LIGHTS` = 6
  - [ ] `RELAY_CHANNEL_SPARE` = 7 with comment: "Reserved, permanently OFF"
- [ ] Add documentation comments explaining boot-strap sensitivity
- [ ] Add documentation comments explaining active-low relay logic
- [ ] Verify NO hardcoded values (all are named constants)

#### 1.2 Create `include/SafetyConfig.h`
- [ ] Create file with `#pragma once` header guard
- [ ] Define temperature thresholds:
  - [ ] `TEMP_VALID_MIN` (e.g., -10.0f) - minimum valid reading
  - [ ] `TEMP_VALID_MAX` (e.g., 60.0f) - maximum valid reading
  - [ ] `TEMP_WARNING_THRESHOLD` (e.g., 42.0f) - enters Warning state
  - [ ] `TEMP_CRITICAL_THRESHOLD` (e.g., 45.0f) - enters Fault state
  - [ ] `TEMP_DEFAULT_SETPOINT` (e.g., 38.0f) - default heater target
- [ ] Define thermal runaway detection:
  - [ ] `TEMP_HISTORY_SIZE` (e.g., 10) - number of readings to track
  - [ ] `TEMP_DELTA_THRESHOLD` (e.g., 5.0f) - max delta in °C
  - [ ] `TEMP_RATE_THRESHOLD` (e.g., 0.5f) - max rate in °C/second
- [ ] Define sensor fault thresholds:
  - [ ] `SENSOR_ERROR_THRESHOLD` (e.g., 3) - consecutive errors before fault
  - [ ] `SENSOR_RECOVERY_THRESHOLD` (e.g., 3) - consecutive valid readings to clear
- [ ] Define water level timing:
  - [ ] `WATER_LEVEL_FAULT_DURATION_MS` (e.g., 10000) - duration before fault
  - [ ] `WATER_LEVEL_STABILIZATION_MS` (e.g., 5000) - stabilization period
- [ ] Add documentation comments for each threshold
- [ ] Verify all values are float or unsigned long as appropriate

#### 1.3 Create `include/TimingConfig.h`
- [ ] Create file with `#pragma once` header guard
- [ ] Define sensor timing:
  - [ ] `TEMP_SENSOR_READ_INTERVAL_MS` (e.g., 2000) - 2 seconds
  - [ ] `TEMP_SENSOR_CONVERSION_TIME_MS` (e.g., 750) - DS18B20 conversion
  - [ ] `WATER_LEVEL_READ_INTERVAL_MS` (e.g., 500) - fast safety response
- [ ] Define display timing:
  - [ ] `DISPLAY_UPDATE_INTERVAL_MS` (e.g., 100) - 10Hz max
- [ ] Define input timing:
  - [ ] `ENCODER_DEBOUNCE_MS` (e.g., 50) - button debounce
  - [ ] `ENCODER_HOLD_DURATION_MS` (e.g., 1000) - long press
- [ ] Define feature timing:
  - [ ] `CIRCULATION_DELAYED_START_MS` (e.g., 2000) - countdown before start
  - [ ] `HEATER_AUTO_START_DELAY_MS` (e.g., 5000) - auto-start after circulation
- [ ] Define shutdown timing:
  - [ ] `SHUTDOWN_HEATER_DELAY_MS` (e.g., 0) - immediate
  - [ ] `SHUTDOWN_OZONE_DELAY_MS` (e.g., 100)
  - [ ] `SHUTDOWN_JET_DELAY_MS` (e.g., 100)
  - [ ] `SHUTDOWN_MASSAGE_DELAY_MS` (e.g., 100)
  - [ ] `SHUTDOWN_SPEAKER_LIGHTS_DELAY_MS` (e.g., 100)
  - [ ] `SHUTDOWN_CIRCULATION_DELAY_MS` (e.g., 200) - last
- [ ] Define I2C timing:
  - [ ] `I2C_TRANSACTION_TIMEOUT_MS` (e.g., 1000)
- [ ] Define buzzer timing:
  - [ ] `BUZZER_BRIEF_MS` (e.g., 50)
  - [ ] `BUZZER_CONFIRMATION_MS` (e.g., 200)
  - [ ] `BUZZER_WARNING_MS` (e.g., 500)
  - [ ] `BUZZER_ALERT_MS` (e.g., 1000)
- [ ] Add documentation comments for each timing value
- [ ] Verify all values are unsigned long

#### 1.4 Create `include/StateDefinitions.h`
- [ ] Create file with `#pragma once` header guard
- [ ] Define `enum SystemState`:
  - [ ] `STATE_BOOT` - initial state during hardware init
  - [ ] `STATE_SELF_CHECK` - safety and sensor validation
  - [ ] `STATE_READY` - safe, accepting commands
  - [ ] `STATE_ACTIVE_CIRCULATION` - circulation pump running
  - [ ] `STATE_FEATURE_ENABLED_BATH` - circulation + features active
  - [ ] `STATE_WARNING` - non-critical warning
  - [ ] `STATE_FAULT` - safety/operational fault
  - [ ] `STATE_FAULT_INSPECTION` - browsing active faults
  - [ ] `STATE_SHUTDOWN` - controlled shutdown in progress
- [ ] Add documentation comment for each state
- [ ] Verify enum uses PascalCase for type, UPPER_SNAKE_CASE for values

#### 1.5 Create `include/FaultCodes.h`
- [ ] Create file with `#pragma once` header guard
- [ ] Define `enum FaultCode` as bit field:
  - [ ] `FAULT_NONE` = 0
  - [ ] `FAULT_LOW_WATER_LEVEL` = (1 << 0)
  - [ ] `FAULT_HIGH_TEMPERATURE` = (1 << 1)
  - [ ] `FAULT_THERMAL_RUNAWAY` = (1 << 2)
  - [ ] `FAULT_TEMPERATURE_SENSOR` = (1 << 3)
  - [ ] `FAULT_I2C_FAILURE` = (1 << 4)
  - [ ] `FAULT_PCF8574_FAILURE` = (1 << 5)
- [ ] Add documentation comment explaining bit field usage
- [ ] Add documentation comment for each fault code
- [ ] Verify bit field allows multiple simultaneous faults

#### 1.6 Configure `platformio.ini`
- [ ] Verify `[env:esp12e]` section exists
- [ ] Set `platform = espressif8266`
- [ ] Set `board = esp12e`
- [ ] Set `framework = arduino`
- [ ] Add `lib_deps`:
  - [ ] `adafruit/Adafruit SH110X@^2.1.14` (exact version)
  - [ ] `adafruit/Adafruit GFX Library@^1.11.0`
  - [ ] `paulstoffregen/OneWire@^2.3.7`
  - [ ] `milesburton/DallasTemperature@^3.11.0`
  - [ ] `arduinogetstarted/ezButton@^1.0.6`
- [ ] Add `build_src_filter`:
  - [ ] `+<*>`
  - [ ] `+<../lib/*/*.cpp>`
- [ ] Add `build_flags`:
  - [ ] `-DENABLE_SERIAL_DEBUG` (exact name with D prefix)
  - [ ] Verify NO `-flto` flag (absolute rule)
- [ ] Set `monitor_speed = 115200`
- [ ] Set `upload_speed = 921600`
- [ ] Verify configuration matches requirements exactly

### Validation Checklist
- [ ] All 5 configuration headers created in `include/` directory
- [ ] `platformio.ini` configured correctly
- [ ] NO hardcoded constants anywhere (all in headers)
- [ ] All pin assignments documented with boot-strap warnings
- [ ] All timing values use unsigned long type
- [ ] All temperature values use float type
- [ ] Enum definitions follow naming conventions
- [ ] Bit field fault codes allow multiple simultaneous faults
- [ ] NO `flto` in build flags
- [ ] `build_src_filter` includes lib implementations
- [ ] Exact library version `Adafruit SH110X@^2.1.14` specified

---

## Task 2: Implement boot-safe hardware initialization in src/main.cpp

### Pre-Implementation Checklist
- [ ] Read all configuration headers created in Task 1
- [ ] Understand boot-strap sensitive pins (D3, D8)
- [ ] Understand I2C initialization sequence
- [ ] Understand active-low relay logic

### Implementation Checklist

#### 2.1 Include Required Headers
- [ ] `#include <Arduino.h>`
- [ ] `#include <Wire.h>`
- [ ] `#include "HardwareConfig.h"`
- [ ] `#include "SafetyConfig.h"`
- [ ] `#include "TimingConfig.h"`
- [ ] `#include "StateDefinitions.h"`
- [ ] `#include "FaultCodes.h"`
- [ ] Verify include order: system, external, project config, project modules

#### 2.2 Implement Boot-Strap Sensitive Pin Initialization (FIRST)
- [ ] In `setup()`, initialize D8 (GPIO15) buzzer FIRST:
  ```cpp
  // CRITICAL: Boot-strap sensitive pin - MUST be LOW at boot
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  ```
- [ ] Initialize D3 (GPIO0) encoder switch with pull-up:
  ```cpp
  // CRITICAL: Boot-strap sensitive pin - needs pull-up
  pinMode(PIN_ENCODER_SW, INPUT_PULLUP);
  ```
- [ ] Add safety comments explaining boot-strap sensitivity
- [ ] Verify these are initialized BEFORE any other operations

#### 2.3 Initialize Serial Debug Output
- [ ] Add conditional serial initialization:
  ```cpp
  #ifdef ENABLE_SERIAL_DEBUG
    Serial.begin(115200);
    while (!Serial && millis() < 3000); // Wait up to 3s
    Serial.println(F("ESP8266 Jacuzzi Controller"));
    Serial.println(F("Initializing..."));
  #endif
  ```
- [ ] Use F() macro for all debug strings
- [ ] Verify baud rate matches `monitor_speed` in platformio.ini

#### 2.4 Initialize I2C Bus (BEFORE I2C Devices)
- [ ] Initialize I2C bus:
  ```cpp
  Wire.begin();
  Wire.setClock(I2C_CLOCK_SPEED);
  ```
- [ ] Add debug output:
  ```cpp
  #ifdef ENABLE_SERIAL_DEBUG
    Serial.println(F("I2C bus initialized at 100kHz"));
  #endif
  ```
- [ ] Verify I2C initialized BEFORE accessing OLED or PCF8574

#### 2.5 Verify I2C Device Addresses
- [ ] Scan for OLED at 0x3C:
  ```cpp
  Wire.beginTransmission(I2C_ADDRESS_OLED);
  uint8_t oledError = Wire.endTransmission();
  bool oledFound = (oledError == 0);
  ```
- [ ] Scan for PCF8574 at 0x20:
  ```cpp
  Wire.beginTransmission(I2C_ADDRESS_PCF8574);
  uint8_t pcf8574Error = Wire.endTransmission();
  bool pcf8574Found = (pcf8574Error == 0);
  ```
- [ ] Add debug output for each device
- [ ] Store detection results for later fault handling

#### 2.6 Initialize PCF8574 with All Relays OFF
- [ ] Write 0xFF to PCF8574 (all HIGH = all OFF):
  ```cpp
  Wire.beginTransmission(I2C_ADDRESS_PCF8574);
  Wire.write(0xFF); // Active-low: HIGH = OFF
  uint8_t error = Wire.endTransmission();
  ```
- [ ] Verify write succeeded
- [ ] Add debug output confirming all relays OFF
- [ ] Add comment explaining active-low logic

#### 2.7 Configure Sensor Pins
- [ ] Configure temperature sensor pin:
  ```cpp
  pinMode(PIN_TEMP_SENSOR, INPUT);
  ```
- [ ] Configure water level sensor pin:
  ```cpp
  pinMode(PIN_WATER_LEVEL, INPUT);
  ```
- [ ] Add debug output for sensor pin configuration

#### 2.8 Configure Remaining Encoder Pins
- [ ] Configure encoder CLK pin:
  ```cpp
  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  ```
- [ ] Configure encoder DT pin (A0):
  ```cpp
  pinMode(PIN_ENCODER_DT, INPUT);
  ```
- [ ] Add comment noting A0 validation required in Phase 7
- [ ] Add debug output for encoder pin configuration

#### 2.9 Implement Conditional Debug Output Framework
- [ ] Define debug macros at top of file:
  ```cpp
  #ifdef ENABLE_SERIAL_DEBUG
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
  #else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(fmt, ...)
  #endif
  ```
- [ ] Use macros throughout initialization code
- [ ] Verify debug code excluded when flag not defined

#### 2.10 Implement Basic Event Loop Structure
- [ ] Create minimal `loop()` function:
  ```cpp
  void loop() {
    // Event loop will be populated in subsequent phases
    // Non-blocking architecture - NO delay() calls
  }
  ```
- [ ] Add comment explaining event loop will be populated
- [ ] Verify NO delay() calls anywhere

### Validation Checklist
- [ ] Boot-strap sensitive pins (D8, D3) initialized FIRST
- [ ] D8 (buzzer) set to LOW before any other operations
- [ ] D3 (encoder SW) configured with INPUT_PULLUP
- [ ] I2C bus initialized at 100kHz BEFORE accessing devices
- [ ] OLED (0x3C) and PCF8574 (0x20) addresses verified
- [ ] PCF8574 initialized with 0xFF (all relays OFF)
- [ ] All sensor pins configured
- [ ] All encoder pins configured
- [ ] Serial debug output at 115200 baud (when enabled)
- [ ] Debug macros defined and used consistently
- [ ] F() macro used for all debug strings
- [ ] NO delay() calls in code
- [ ] NO hardcoded constants (all from headers)
- [ ] Include order correct (system, external, config, modules)
- [ ] Comments explain boot-strap sensitivity and active-low logic

---

## Task 3: Manual hardware validation checkpoint

### Pre-Validation Checklist
- [ ] Code compiles without errors
- [ ] Code compiles without warnings
- [ ] All configuration headers included correctly
- [ ] platformio.ini configured correctly

### User Validation Instructions

**Ask user to perform the following tests on actual ESP8266 hardware:**

#### 3.1 Build and Upload
- [ ] User runs: `pio run -t upload`
- [ ] Upload completes successfully
- [ ] No compilation errors
- [ ] No upload errors

#### 3.2 Serial Monitor Verification
- [ ] User runs: `pio device monitor`
- [ ] Serial output shows initialization messages
- [ ] Boot sequence completes without errors
- [ ] I2C devices detected (0x3C OLED, 0x20 PCF8574)

#### 3.3 Boot-Strap Pin Verification
- [ ] User verifies D8 (buzzer) is LOW at boot (no sound)
- [ ] User verifies D3 (encoder SW) does not force programming mode
- [ ] User tests boot with encoder button pressed (should boot normally)
- [ ] User confirms no boot failures

#### 3.4 I2C Device Detection
- [ ] User confirms OLED detected at 0x3C
- [ ] User confirms PCF8574 detected at 0x20
- [ ] User confirms no I2C address conflicts

#### 3.5 Relay State Verification
- [ ] User confirms all relays are OFF at boot
- [ ] User measures relay outputs (should be HIGH = OFF)
- [ ] User confirms spare channel (8th) is OFF
- [ ] User confirms no relays energize during boot

#### 3.6 Pin Configuration Verification
- [ ] User confirms temperature sensor pin (D5) configured
- [ ] User confirms water level sensor pin (D7) configured
- [ ] User confirms encoder pins (D6, A0, D3) configured
- [ ] User confirms buzzer pin (D8) configured

### User Approval Gate
- [ ] User reports all validation tests passed
- [ ] User provides explicit approval to proceed to Task 4
- [ ] User confirms no issues or unexpected behavior
- [ ] User confirms hardware matches expected configuration

**STOP: Do not proceed to Task 4 without explicit user approval**

---

## Task 4: Document Phase 1 results

### Documentation Checklist

#### 4.1 Create `docs/phase-1-hardware-initialization.md`
- [ ] Document pin configuration:
  - [ ] Complete pin mapping table
  - [ ] Boot-strap sensitive pins (D8, D3) with warnings
  - [ ] Sensor pins (D5, D7)
  - [ ] Encoder pins (D6, A0, D3)
  - [ ] I2C addresses (0x3C, 0x20)
- [ ] Document boot sequence:
  - [ ] Boot-strap pin initialization order
  - [ ] I2C bus initialization
  - [ ] Device detection results
  - [ ] Relay initialization (all OFF)
- [ ] Document hardware validation results:
  - [ ] Boot behavior verified
  - [ ] I2C devices detected
  - [ ] Relay states verified
  - [ ] Pin configurations verified
- [ ] Document any hardware-specific notes:
  - [ ] Board variant (esp12e or alternative)
  - [ ] Circuit requirements for boot-strap pins
  - [ ] Any pin mapping caveats discovered
- [ ] Document configuration headers created:
  - [ ] HardwareConfig.h contents
  - [ ] SafetyConfig.h contents
  - [ ] TimingConfig.h contents
  - [ ] StateDefinitions.h contents
  - [ ] FaultCodes.h contents
- [ ] Document platformio.ini configuration:
  - [ ] Build flags
  - [ ] Library dependencies
  - [ ] Build source filter
- [ ] Add troubleshooting section if any issues encountered
- [ ] Add next steps (Phase 2 preview)

### Validation Checklist
- [ ] Documentation file created in `docs/` directory
- [ ] File named `phase-1-hardware-initialization.md`
- [ ] All sections completed
- [ ] Hardware validation results documented
- [ ] Any issues or caveats documented
- [ ] Next steps clearly stated

---

## Task 5: Phase 1 post-git workflow

### Pre-Git Checklist
- [ ] All code changes complete
- [ ] All documentation complete
- [ ] User approval received
- [ ] No uncommitted changes from previous work

### Git Workflow Checklist

#### 5.1 Verify Current State
- [ ] Run: `git status`
- [ ] Verify on correct feature branch
- [ ] Review all modified/created files
- [ ] Confirm all changes are Phase 1 related

#### 5.2 Review Changes
- [ ] Run: `git diff`
- [ ] Review all code changes
- [ ] Verify NO hardcoded constants
- [ ] Verify NO delay() calls
- [ ] Verify boot-strap pin safety

#### 5.3 Stage Changes
- [ ] Run: `git add .`
- [ ] Verify staged files: `git status`
- [ ] Confirm all Phase 1 files staged:
  - [ ] `include/HardwareConfig.h`
  - [ ] `include/SafetyConfig.h`
  - [ ] `include/TimingConfig.h`
  - [ ] `include/StateDefinitions.h`
  - [ ] `include/FaultCodes.h`
  - [ ] `platformio.ini`
  - [ ] `src/main.cpp`
  - [ ] `docs/phase-1-hardware-initialization.md`

#### 5.4 Commit Changes
- [ ] Run: `git commit -m "feat: Phase 1 - Hardware initialization and configuration"`
- [ ] Verify commit message follows conventional format
- [ ] Verify commit includes all Phase 1 changes

#### 5.5 Push Feature Branch
- [ ] Run: `git push origin feature/phase-1-hardware-init`
- [ ] Verify push successful
- [ ] Confirm remote branch exists: `git branch -r`

#### 5.6 Checkout Base Branch
- [ ] Run: `git checkout main`
- [ ] Verify clean state: `git status`
- [ ] Pull latest: `git pull origin main`

#### 5.7 Merge Feature Branch
- [ ] Verify on main branch: `git branch`
- [ ] Run: `git merge feature/phase-1-hardware-init`
- [ ] Verify merge successful (no conflicts)
- [ ] Verify merged changes: `git log --oneline -5`

#### 5.8 Push Merged Changes
- [ ] Run: `git push origin main`
- [ ] Verify push successful
- [ ] Confirm remote updated: `git log origin/main --oneline -5`

#### 5.9 Delete Feature Branch
- [ ] Verify merge complete: `git branch --merged`
- [ ] Delete local: `git branch -d feature/phase-1-hardware-init`
- [ ] Delete remote: `git push origin --delete feature/phase-1-hardware-init`
- [ ] Verify deletion: `git branch -a`

#### 5.10 Final Verification
- [ ] Run: `git status` (should be clean)
- [ ] Run: `git branch -vv` (should show main in sync)
- [ ] Run: `git log --oneline -5` (should show Phase 1 commit)
- [ ] Confirm no orphaned branches: `git branch -a`

### Post-Git Validation
- [ ] All changes committed
- [ ] Feature branch merged to main
- [ ] Feature branch deleted (local and remote)
- [ ] Repository synchronized
- [ ] Working directory clean
- [ ] Ready for Phase 2

---

## Phase 1 Completion Checklist

### Requirements Satisfied
- [ ] Req 1.1: I2C bus initialized before devices
- [ ] Req 1.2: All pins configured per mapping
- [ ] Req 1.3: All relays default to OFF (HIGH)
- [ ] Req 1.3a: No relays energized before safety checks
- [ ] Req 1.4: Temperature sensor initialized
- [ ] Req 1.5: Water level sensor initialized
- [ ] Req 1.6: Rotary encoder initialized
- [ ] Req 1.7: Buzzer initialized in OFF state
- [ ] Req 1.8: I2C devices verified (0x3C, 0x20)
- [ ] Req 1.12: Boot-strap pins (D3, D8) configured safely
- [ ] Req 1.13: All constants in centralized headers
- [ ] Req 1.14: NO hardcoded constants
- [ ] Req 1.15: Serial debug at 115200 baud (when enabled)
- [ ] Req 14.1-14.15: Memory optimization and PROGMEM
- [ ] Req 18.1-18.14: Configuration management and build system

### Design Elements Implemented
- [ ] Hardware Abstraction Layer initialized
- [ ] Boot-safe pin initialization
- [ ] Centralized pin mapping
- [ ] I2C bus management foundation
- [ ] Module organization structure

### Deliverables Complete
- [ ] 5 configuration headers created
- [ ] platformio.ini configured
- [ ] src/main.cpp with boot-safe initialization
- [ ] Serial debug framework
- [ ] Phase 1 documentation
- [ ] Git workflow complete

### Ready for Phase 2
- [ ] All Phase 1 tasks complete
- [ ] User approval received
- [ ] Hardware validated
- [ ] Documentation complete
- [ ] Git repository clean
- [ ] No blocking issues

