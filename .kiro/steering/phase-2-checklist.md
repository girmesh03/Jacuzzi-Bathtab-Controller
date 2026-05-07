# Phase 2: Sensor Integration - Implementation Checklist

## Phase Overview
**Goal**: Implement non-blocking DS18B20 temperature sensor driver and XKC-Y25-V water level sensor driver with validation, fault detection, and integration with `src/main.cpp`.

**Requirements**: 2.1-2.14, 3.1-3.10, 13.4  
**Design**: Sensor Manager, Non-Blocking Architecture, Temperature History Buffer  
**Tasks**: 6-10 (67 total tasks)

---

## Task 6: Implement DS18B20 temperature sensor driver (non-blocking)

### Pre-Implementation Checklist
- [ ] Read Phase 1 documentation (`docs/phase-1-hardware-initialization.md`)
- [ ] Verify Phase 1 complete (all configuration headers exist)
- [ ] Read `include/SafetyConfig.h` for temperature thresholds
- [ ] Read `include/TimingConfig.h` for sensor timing
- [ ] Read `include/HardwareConfig.h` for pin assignments
- [ ] Understand DS18B20 OneWire protocol and conversion time (~750ms)
- [ ] Understand non-blocking state machine pattern

### Implementation Checklist

#### 6.1 Create `include/SensorManager.h`
- [ ] Create file with `#pragma once` header guard
- [ ] Include required headers:
  - [ ] `<Arduino.h>`
  - [ ] `<OneWire.h>`
  - [ ] `<DallasTemperature.h>`
  - [ ] `"HardwareConfig.h"`
  - [ ] `"SafetyConfig.h"`
  - [ ] `"TimingConfig.h"`
- [ ] Define temperature sensor states enum:
  ```cpp
  enum TempSensorState {
      TEMP_IDLE,
      TEMP_REQUEST,
      TEMP_WAITING,
      TEMP_PROCESS
  };
  ```
- [ ] Define `SensorManager` class:
  - [ ] Public methods:
    - [ ] `void begin()` - Initialize sensors
    - [ ] `void update()` - Non-blocking update (call in loop)
    - [ ] `float getTemperature()` - Get current temperature
    - [ ] `bool isTemperatureSensorOperational()` - Sensor status
    - [ ] `bool isWaterLevelSufficient()` - Water level status
    - [ ] `uint8_t getTemperatureSensorErrorCount()` - Error counter
    - [ ] `bool hasTemperatureSensorFault()` - Fault status
    - [ ] `bool hasWaterLevelFault()` - Water level fault status
  - [ ] Private members:
    - [ ] `OneWire oneWire` - OneWire instance
    - [ ] `DallasTemperature sensors` - Dallas Temperature instance
    - [ ] `TempSensorState tempState` - Current state
    - [ ] `float currentTemperature` - Latest reading
    - [ ] `unsigned long lastTempReadTime` - Timing
    - [ ] `unsigned long tempRequestTime` - Conversion timing
    - [ ] `uint8_t tempErrorCount` - Consecutive errors
    - [ ] `uint8_t tempValidCount` - Consecutive valid readings
    - [ ] `bool tempSensorOperational` - Sensor status
    - [ ] `float temperatureHistory[TEMP_HISTORY_SIZE]` - History buffer
    - [ ] `uint8_t historyIndex` - Current index
    - [ ] `uint8_t historyCount` - Number of readings
    - [ ] `unsigned long lastWaterLevelReadTime` - Water level timing
    - [ ] `bool waterLevelSufficient` - Water level status
    - [ ] `unsigned long waterLevelFaultStartTime` - Fault timing
    - [ ] `unsigned long waterLevelRecoveryStartTime` - Recovery timing
  - [ ] Private methods:
    - [ ] `void updateTemperatureSensor()` - State machine
    - [ ] `void updateWaterLevelSensor()` - Polling
    - [ ] `bool validateTemperature(float temp)` - Range check
    - [ ] `void addToHistory(float temp)` - History management
- [ ] Add documentation comments for all public methods
- [ ] Verify NO hardcoded constants (all from config headers)

#### 6.2 Create `lib/SensorManager/SensorManager.cpp`
- [ ] Create file with includes:
  - [ ] `"SensorManager.h"`
  - [ ] `"FaultCodes.h"` (for future fault handling)
- [ ] Implement `begin()`:
  - [ ] Initialize OneWire on `PIN_TEMP_SENSOR`
  - [ ] Initialize DallasTemperature library
  - [ ] Call `sensors.begin()`
  - [ ] Verify sensor count > 0
  - [ ] Set resolution to 12-bit (if needed)
  - [ ] Initialize state to `TEMP_IDLE`
  - [ ] Initialize all timing variables
  - [ ] Initialize error counters to 0
  - [ ] Initialize temperature to invalid value (e.g., -999.0f)
  - [ ] Add debug output (when `ENABLE_SERIAL_DEBUG` defined)
- [ ] Implement `update()`:
  - [ ] Call `updateTemperatureSensor()`
  - [ ] Call `updateWaterLevelSensor()`
  - [ ] Non-blocking - return immediately
- [ ] Implement `updateTemperatureSensor()` state machine:
  - [ ] **TEMP_IDLE state**:
    - [ ] Check if `millis() - lastTempReadTime >= TEMP_SENSOR_READ_INTERVAL_MS`
    - [ ] If time elapsed:
      - [ ] Call `sensors.requestTemperatures()`
      - [ ] Store `tempRequestTime = millis()`
      - [ ] Transition to `TEMP_WAITING`
      - [ ] Add debug output
  - [ ] **TEMP_WAITING state**:
    - [ ] Check if `millis() - tempRequestTime >= TEMP_SENSOR_CONVERSION_TIME_MS`
    - [ ] If conversion complete:
      - [ ] Transition to `TEMP_PROCESS`
  - [ ] **TEMP_PROCESS state**:
    - [ ] Read temperature: `float temp = sensors.getTempCByIndex(0)`
    - [ ] Validate reading with `validateTemperature(temp)`
    - [ ] If valid:
      - [ ] Store `currentTemperature = temp`
      - [ ] Add to history: `addToHistory(temp)`
      - [ ] Reset error counter: `tempErrorCount = 0`
      - [ ] Increment valid counter: `tempValidCount++`
      - [ ] If `tempValidCount >= SENSOR_RECOVERY_THRESHOLD`:
        - [ ] Set `tempSensorOperational = true`
        - [ ] Reset valid counter
      - [ ] Add debug output with temperature value
    - [ ] If invalid:
      - [ ] Increment `tempErrorCount++`
      - [ ] Reset valid counter: `tempValidCount = 0`
      - [ ] If `tempErrorCount >= SENSOR_ERROR_THRESHOLD`:
        - [ ] Set `tempSensorOperational = false`
        - [ ] Add debug output (sensor fault)
      - [ ] Add debug output (invalid reading)
    - [ ] Update `lastTempReadTime = millis()`
    - [ ] Transition to `TEMP_IDLE`
- [ ] Implement `validateTemperature(float temp)`:
  - [ ] Check if `temp >= TEMP_VALID_MIN && temp <= TEMP_VALID_MAX`
  - [ ] Check if `temp != DEVICE_DISCONNECTED_C` (DallasTemperature constant)
  - [ ] Return true if valid, false otherwise
- [ ] Implement `addToHistory(float temp)`:
  - [ ] Store in circular buffer: `temperatureHistory[historyIndex] = temp`
  - [ ] Increment index: `historyIndex = (historyIndex + 1) % TEMP_HISTORY_SIZE`
  - [ ] Increment count: `if (historyCount < TEMP_HISTORY_SIZE) historyCount++`
- [ ] Implement getter methods:
  - [ ] `getTemperature()`: Return `currentTemperature`
  - [ ] `isTemperatureSensorOperational()`: Return `tempSensorOperational`
  - [ ] `getTemperatureSensorErrorCount()`: Return `tempErrorCount`
  - [ ] `hasTemperatureSensorFault()`: Return `tempErrorCount >= SENSOR_ERROR_THRESHOLD`
- [ ] Verify NO `delay()` calls anywhere
- [ ] Verify NO hardcoded constants
- [ ] Add debug output at key points (when `ENABLE_SERIAL_DEBUG` defined)

#### 6.3 Integrate with `src/main.cpp`
- [ ] Add include: `#include "SensorManager.h"`
- [ ] Declare global instance: `SensorManager sensorManager;`
- [ ] In `setup()`, after hardware initialization:
  - [ ] Call `sensorManager.begin()`
  - [ ] Add debug output confirming sensor initialization
- [ ] In `loop()`:
  - [ ] Call `sensorManager.update()`
  - [ ] Add periodic debug output (e.g., every 5 seconds):
    ```cpp
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime >= 5000) {
        DEBUG_PRINT(F("Temperature: "));
        DEBUG_PRINT(sensorManager.getTemperature());
        DEBUG_PRINTLN(F(" °C"));
        lastDebugTime = millis();
    }
    ```
- [ ] Verify integration compiles without errors

### Validation Checklist
- [ ] `SensorManager.h` created with complete interface
- [ ] `SensorManager.cpp` created with full implementation
- [ ] Non-blocking state machine implemented (IDLE → REQUEST → WAITING → PROCESS)
- [ ] Temperature reading every 2 seconds (from `TimingConfig.h`)
- [ ] Conversion time ~750ms respected
- [ ] Temperature validation against `TEMP_VALID_MIN` and `TEMP_VALID_MAX`
- [ ] Error counter increments on invalid readings
- [ ] Sensor fault triggered after `SENSOR_ERROR_THRESHOLD` consecutive errors
- [ ] Valid counter resets error state after `SENSOR_RECOVERY_THRESHOLD` valid readings
- [ ] Temperature history buffer implemented (circular buffer)
- [ ] Integrated with `src/main.cpp`
- [ ] NO `delay()` calls
- [ ] NO hardcoded constants
- [ ] Debug output at key points
- [ ] Code compiles without errors or warnings

---

## Task 7: Implement XKC-Y25-V water level sensor driver

### Pre-Implementation Checklist
- [ ] Verify Task 6 complete (temperature sensor working)
- [ ] Read `include/SafetyConfig.h` for water level timing
- [ ] Read `include/TimingConfig.h` for read interval
- [ ] Read `include/HardwareConfig.h` for pin assignment
- [ ] Understand active-low logic (LOW = sufficient, HIGH = insufficient)

### Implementation Checklist

#### 7.1 Update `include/SensorManager.h`
- [ ] Add water level sensor methods (if not already added in Task 6):
  - [ ] `bool isWaterLevelSufficient()` - Current status
  - [ ] `bool hasWaterLevelFault()` - Fault status
  - [ ] `unsigned long getWaterLevelFaultDuration()` - Time in fault
- [ ] Add private members (if not already added):
  - [ ] `unsigned long lastWaterLevelReadTime`
  - [ ] `bool waterLevelSufficient`
  - [ ] `unsigned long waterLevelFaultStartTime`
  - [ ] `unsigned long waterLevelRecoveryStartTime`
  - [ ] `bool waterLevelFaultActive`
- [ ] Verify all additions follow naming conventions

#### 7.2 Update `lib/SensorManager/SensorManager.cpp`
- [ ] Update `begin()`:
  - [ ] Configure water level pin: `pinMode(PIN_WATER_LEVEL, INPUT)`
  - [ ] Read initial state
  - [ ] Initialize timing variables
  - [ ] Initialize fault state to false
  - [ ] Add debug output
- [ ] Implement `updateWaterLevelSensor()`:
  - [ ] Check if `millis() - lastWaterLevelReadTime >= WATER_LEVEL_READ_INTERVAL_MS`
  - [ ] If time elapsed:
    - [ ] Read pin: `bool pinState = digitalRead(PIN_WATER_LEVEL)`
    - [ ] Interpret active-low: `bool sufficient = (pinState == LOW)`
    - [ ] Update `waterLevelSufficient = sufficient`
    - [ ] **If insufficient water detected**:
      - [ ] If first detection: `waterLevelFaultStartTime = millis()`
      - [ ] Check duration: `unsigned long duration = millis() - waterLevelFaultStartTime`
      - [ ] If `duration >= WATER_LEVEL_FAULT_DURATION_MS`:
        - [ ] Set `waterLevelFaultActive = true`
        - [ ] Add debug output (water level fault)
      - [ ] Reset recovery timer
    - [ ] **If sufficient water detected**:
      - [ ] Reset fault start timer
      - [ ] If fault was active:
        - [ ] If first sufficient reading: `waterLevelRecoveryStartTime = millis()`
        - [ ] Check stabilization: `unsigned long recoveryDuration = millis() - waterLevelRecoveryStartTime`
        - [ ] If `recoveryDuration >= WATER_LEVEL_STABILIZATION_MS`:
          - [ ] Clear fault: `waterLevelFaultActive = false`
          - [ ] Add debug output (fault cleared)
    - [ ] Update `lastWaterLevelReadTime = millis()`
    - [ ] Add debug output (water level status)
- [ ] Implement getter methods:
  - [ ] `isWaterLevelSufficient()`: Return `waterLevelSufficient`
  - [ ] `hasWaterLevelFault()`: Return `waterLevelFaultActive`
  - [ ] `getWaterLevelFaultDuration()`: Return duration if fault active
- [ ] Verify active-low logic correct (LOW = sufficient)
- [ ] Verify NO `delay()` calls
- [ ] Verify NO hardcoded constants

#### 7.3 Update `src/main.cpp` Integration
- [ ] In `loop()`, add periodic water level debug output:
  ```cpp
  static unsigned long lastWaterDebugTime = 0;
  if (millis() - lastWaterDebugTime >= 5000) {
      DEBUG_PRINT(F("Water Level: "));
      DEBUG_PRINTLN(sensorManager.isWaterLevelSufficient() ? F("Sufficient") : F("Insufficient"));
      if (sensorManager.hasWaterLevelFault()) {
          DEBUG_PRINTLN(F("Water Level FAULT"));
      }
      lastWaterDebugTime = millis();
  }
  ```
- [ ] Verify integration compiles without errors

### Validation Checklist
- [ ] Water level sensor reading implemented
- [ ] Active-low logic correct (LOW = sufficient, HIGH = insufficient)
- [ ] Non-blocking polling every 500ms (from `TimingConfig.h`)
- [ ] Fault triggered after extended duration (from `SafetyConfig.h`)
- [ ] Fault cleared after stabilization period (from `SafetyConfig.h`)
- [ ] Integrated with `src/main.cpp`
- [ ] NO `delay()` calls
- [ ] NO hardcoded constants
- [ ] Debug output for water level status
- [ ] Code compiles without errors or warnings

---

## Task 8: Manual hardware validation checkpoint

### Pre-Validation Checklist
- [ ] Code compiles without errors
- [ ] Code compiles without warnings
- [ ] Temperature sensor driver complete
- [ ] Water level sensor driver complete
- [ ] Both sensors integrated with `src/main.cpp`

### User Validation Instructions

**Ask user to perform the following tests on actual ESP8266 hardware:**

#### 8.1 Build and Upload
- [ ] User runs: `pio run -t upload`
- [ ] Upload completes successfully
- [ ] No compilation errors
- [ ] No upload errors

#### 8.2 Temperature Sensor Verification
- [ ] User runs: `pio device monitor`
- [ ] Serial output shows temperature readings every 2 seconds
- [ ] Temperature values are reasonable (e.g., 20-30°C room temperature)
- [ ] User verifies non-blocking operation (no delays observed)
- [ ] User tests sensor disconnection:
  - [ ] Disconnect DS18B20 sensor
  - [ ] Verify error counter increments
  - [ ] Verify sensor fault triggered after 3 consecutive errors
  - [ ] Reconnect sensor
  - [ ] Verify sensor recovers after 3 valid readings
  - [ ] Verify fault clears

#### 8.3 Water Level Sensor Verification
- [ ] User verifies water level readings every 500ms
- [ ] User tests active-low logic:
  - [ ] Sensor LOW (sufficient water): System reports "Sufficient"
  - [ ] Sensor HIGH (insufficient water): System reports "Insufficient"
- [ ] User tests fault detection:
  - [ ] Simulate insufficient water for 10+ seconds
  - [ ] Verify water level fault triggered
  - [ ] Restore sufficient water
  - [ ] Verify fault clears after 5 seconds stabilization

#### 8.4 Sensor Integration Verification
- [ ] User confirms both sensors update independently
- [ ] User confirms no blocking behavior
- [ ] User confirms temperature history buffer working (if testable)
- [ ] User confirms error counters working correctly

#### 8.5 Fault Detection Verification
- [ ] User tests temperature sensor fault:
  - [ ] Disconnect sensor
  - [ ] Verify fault after 3 errors
  - [ ] Reconnect sensor
  - [ ] Verify recovery after 3 valid readings
- [ ] User tests water level fault:
  - [ ] Simulate low water for 10+ seconds
  - [ ] Verify fault triggered
  - [ ] Restore water
  - [ ] Verify fault clears after stabilization

### User Approval Gate
- [ ] User reports all validation tests passed
- [ ] User confirms temperature readings accurate
- [ ] User confirms water level detection working (active-low)
- [ ] User confirms fault detection and recovery working
- [ ] User confirms no blocking behavior
- [ ] User provides explicit approval to proceed to Task 9
- [ ] User confirms no issues or unexpected behavior

**STOP: Do not proceed to Task 9 without explicit user approval**

---

## Task 9: Document Phase 2 results

### Documentation Checklist

#### 9.1 Create `docs/phase-2-sensor-integration.md`
- [ ] Document temperature sensor implementation:
  - [ ] DS18B20 on pin D5 (GPIO14)
  - [ ] Non-blocking state machine (IDLE → REQUEST → WAITING → PROCESS)
  - [ ] Read interval: 2 seconds
  - [ ] Conversion time: ~750ms
  - [ ] Validation range: -10°C to 60°C (or actual values)
  - [ ] Error threshold: 3 consecutive errors
  - [ ] Recovery threshold: 3 consecutive valid readings
- [ ] Document water level sensor implementation:
  - [ ] XKC-Y25-V on pin D7 (GPIO13)
  - [ ] Active-low logic (LOW = sufficient, HIGH = insufficient)
  - [ ] Read interval: 500ms
  - [ ] Fault duration: 10 seconds
  - [ ] Stabilization period: 5 seconds
- [ ] Document temperature history buffer:
  - [ ] Buffer size: 10 readings (or actual value)
  - [ ] Circular buffer implementation
  - [ ] Purpose: Thermal runaway detection (Phase 5)
- [ ] Document hardware validation results:
  - [ ] Temperature sensor accuracy verified
  - [ ] Water level sensor active-low logic verified
  - [ ] Fault detection verified
  - [ ] Fault recovery verified
  - [ ] Non-blocking operation verified
- [ ] Document timing constants used:
  - [ ] All values from `TimingConfig.h`
  - [ ] All thresholds from `SafetyConfig.h`
- [ ] Document any sensor-specific notes:
  - [ ] Calibration requirements (if any)
  - [ ] Circuit requirements
  - [ ] Any issues encountered and resolved
- [ ] Add troubleshooting section
- [ ] Add next steps (Phase 3 preview)

### Validation Checklist
- [ ] Documentation file created in `docs/` directory
- [ ] File named `phase-2-sensor-integration.md`
- [ ] All sections completed
- [ ] Hardware validation results documented
- [ ] Timing constants documented
- [ ] Any issues or caveats documented
- [ ] Next steps clearly stated

---

## Task 10: Phase 2 post-git workflow

### Pre-Git Checklist
- [ ] All code changes complete
- [ ] All documentation complete
- [ ] User approval received
- [ ] No uncommitted changes from previous work

### Git Workflow Checklist

#### 10.1 Verify Current State
- [ ] Run: `git status`
- [ ] Verify on correct feature branch
- [ ] Review all modified/created files
- [ ] Confirm all changes are Phase 2 related

#### 10.2 Review Changes
- [ ] Run: `git diff`
- [ ] Review all code changes
- [ ] Verify NO hardcoded constants
- [ ] Verify NO delay() calls
- [ ] Verify non-blocking implementation

#### 10.3 Stage Changes
- [ ] Run: `git add .`
- [ ] Verify staged files: `git status`
- [ ] Confirm all Phase 2 files staged:
  - [ ] `include/SensorManager.h`
  - [ ] `lib/SensorManager/SensorManager.cpp`
  - [ ] `src/main.cpp` (updated)
  - [ ] `docs/phase-2-sensor-integration.md`

#### 10.4 Commit Changes
- [ ] Run: `git commit -m "feat: Phase 2 - Sensor integration"`
- [ ] Verify commit message follows conventional format
- [ ] Verify commit includes all Phase 2 changes

#### 10.5 Push Feature Branch
- [ ] Run: `git push origin feature/phase-2-sensors`
- [ ] Verify push successful
- [ ] Confirm remote branch exists: `git branch -r`

#### 10.6 Checkout Base Branch
- [ ] Run: `git checkout main`
- [ ] Verify clean state: `git status`
- [ ] Pull latest: `git pull origin main`

#### 10.7 Merge Feature Branch
- [ ] Verify on main branch: `git branch`
- [ ] Run: `git merge feature/phase-2-sensors`
- [ ] Verify merge successful (no conflicts)
- [ ] Verify merged changes: `git log --oneline -5`

#### 10.8 Push Merged Changes
- [ ] Run: `git push origin main`
- [ ] Verify push successful
- [ ] Confirm remote updated: `git log origin/main --oneline -5`

#### 10.9 Delete Feature Branch
- [ ] Verify merge complete: `git branch --merged`
- [ ] Delete local: `git branch -d feature/phase-2-sensors`
- [ ] Delete remote: `git push origin --delete feature/phase-2-sensors`
- [ ] Verify deletion: `git branch -a`

#### 10.10 Final Verification
- [ ] Run: `git status` (should be clean)
- [ ] Run: `git branch -vv` (should show main in sync)
- [ ] Run: `git log --oneline -5` (should show Phase 2 commit)
- [ ] Confirm no orphaned branches: `git branch -a`

### Post-Git Validation
- [ ] All changes committed
- [ ] Feature branch merged to main
- [ ] Feature branch deleted (local and remote)
- [ ] Repository synchronized
- [ ] Working directory clean
- [ ] Ready for Phase 3

---

## Phase 2 Completion Checklist

### Requirements Satisfied
- [ ] Req 2.1: Temperature read every 2 seconds (non-blocking)
- [ ] Req 2.2: Temperature validation within range
- [ ] Req 2.3: Invalid reading increments error counter
- [ ] Req 2.4: Consecutive invalid readings trigger fault
- [ ] Req 2.5: Valid reading after failure resets counter
- [ ] Req 2.12: Temperature sensor operational status tracked
- [ ] Req 2.13: Communication timeout treated as failure
- [ ] Req 2.14: All thresholds in centralized headers
- [ ] Req 3.1: Water level read non-blocking
- [ ] Req 3.2: LOW = sufficient (active-low)
- [ ] Req 3.3: HIGH = insufficient (active-low)
- [ ] Req 3.8: Insufficient water for extended duration triggers fault
- [ ] Req 3.9: Sufficient water clears fault after stabilization
- [ ] Req 3.10: All timing in centralized headers
- [ ] Req 13.4: Non-blocking sensor reading

### Design Elements Implemented
- [ ] Sensor Manager module created
- [ ] Non-blocking state machine for temperature
- [ ] Non-blocking polling for water level
- [ ] Temperature history buffer for thermal runaway detection
- [ ] Error counting and fault detection
- [ ] Fault recovery logic

### Deliverables Complete
- [ ] `SensorManager.h` created
- [ ] `SensorManager.cpp` created
- [ ] Integration with `src/main.cpp`
- [ ] Temperature sensor driver working
- [ ] Water level sensor driver working
- [ ] Phase 2 documentation
- [ ] Git workflow complete

### Ready for Phase 3
- [ ] All Phase 2 tasks complete
- [ ] User approval received
- [ ] Hardware validated
- [ ] Documentation complete
- [ ] Git repository clean
- [ ] No blocking issues

