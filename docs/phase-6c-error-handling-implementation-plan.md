# Phase 6C: Complete Error Handling System - Implementation Plan

## Overview

**Goal**: Implement complete error handling system across all components (Phases 1-6) to ensure boot fault persistence, continuous fault detection, forbidden action feedback, and proper fault management.

**Scope**: 
- Boot fault persistence in Self_Check state
- Continuous fault detection in main loop
- Forbidden action warning/feedback UI
- Auto-clear vs manual acknowledgment logic
- Multi-fault management integration
- State machine guard conditions

**Requirements**: 1.9-1.10, 2.7-2.11, 3.4-3.6, 5.3, 6.3, 7.7, 8.5-8.7, 11.1-11.18, 19.1-19.7
**Design**: Safety System, State Machine, UI Manager, Sensor Manager
**Integration**: `lib/StateMachine/StateMachine.cpp`, `lib/SafetySystem/SafetySystem.cpp`, `lib/UIManager/UIManager.cpp`, `src/main.cpp`

---

## Pre-Implementation Analysis

### Critical Missing Functionality

**1. Boot Fault Persistence** (Requirements 1.9, 1.10, 8.5-8.7, 11.2-11.3):
- ❌ System does NOT properly remain in FAULT state from boot until ALL conditions resolved
- ❌ Self_Check state verification incomplete
- ❌ System may progress to Ready with unsafe conditions
- ❌ Boot fault persistence NOT enforced

**2. Continuous Fault Detection** (Requirements 11.1-11.18):
- ❌ Missing continuous monitoring in main loop
- ❌ Fault detection only happens during sensor reads
- ❌ No continuous checking for: low water (10s duration), high temp warning vs critical, thermal runaway (CHANGE/DELTA), I2C/PCF8574 failures
- ❌ Automatic transition to FAULT state NOT implemented

**3. Forbidden Action Warning/Feedback** (Requirements 5.3, 6.3, 7.7):
- ❌ When user tries to activate feature without preconditions met, NO error indication displayed
- ❌ User gets no feedback why action was denied
- ❌ UI does NOT show forbidden action overlay

**4. Auto-Clear vs Manual Acknowledgment** (Requirements 11.16-11.17, 19.5-19.7):
- ❌ Auto-clear faults NOT properly distinguished from manual acknowledgment faults
- ❌ Thermal runaway does NOT require manual acknowledgment
- ❌ Fault clearing logic incomplete

**5. Multi-Fault Management** (Requirements 11.8-11.11):
- ⚠️ Fault bit field exists but NOT fully utilized
- ⚠️ Fault priority ordering NOT enforced
- ⚠️ Fault detection incomplete

**6. State Machine Integration**:
- ❌ Missing boot fault persistence logic in STATE_SELF_CHECK
- ❌ Missing continuous monitoring in STATE_ACTIVE_CIRCULATION and STATE_FEATURE_ENABLED_BATH
- ❌ Missing automatic transition to STATE_FAULT when any fault detected
- ❌ Missing guard conditions for state transitions

---

## Component 1: StateMachine - Boot Fault Persistence

### Requirements Analysis

**Requirements 1.9, 1.10, 8.5-8.7, 11.2-11.3**:
- WHEN Controller boots, IF water level insufficient OR temperature unsafe OR sensor fails, THEN enter Fault state and persist until ALL resolved
- WHEN Controller boots with unsafe conditions, System SHALL NOT progress to Ready until ALL preconditions AND safety conditions satisfied
- Self_Check MUST verify ALL preconditions before allowing transition to Ready

### Implementation Checklist

#### 1.1 Read Current StateMachine Implementation

- [ ] Read `lib/StateMachine/StateMachine.cpp` to understand current Self_Check logic
- [ ] Read `include/StateMachine.h` to understand interface
- [ ] Identify where Self_Check state verification happens
- [ ] Identify transition logic from Self_Check to Ready
- [ ] Identify transition logic from Self_Check to Fault

**Files**: `lib/StateMachine/StateMachine.cpp`, `include/StateMachine.h`

#### 1.2 Implement Complete Self_Check Verification

- [ ] In Self_Check state, verify ALL preconditions:
  - [ ] Water level sensor indicates sufficient water
  - [ ] Temperature sensor operational (not in fault state)
  - [ ] Temperature reading within safe range (TEMP_VALID_MIN to TEMP_VALID_MAX)
  - [ ] I2C devices responding (OLED at 0x3C, PCF8574 at 0x20)
  - [ ] All relays in OFF state (HIGH signal)
- [ ] IF ANY precondition fails, transition to Fault state
- [ ] Store which precondition(s) failed in fault bit field
- [ ] Add debug logging for each precondition check

**File**: `lib/StateMachine/StateMachine.cpp`

**Code pattern**:
```cpp
void StateMachine::updateSelfCheckState() {
    // CRITICAL: Boot fault persistence
    // System MUST NOT progress to Ready until ALL conditions safe
    
    bool allPreconditionsSatisfied = true;
    uint8_t bootFaults = FAULT_NONE;
    
    // Check 1: Water level sufficient
    if (!sensorManager.isWaterLevelSufficient()) {
        allPreconditionsSatisfied = false;
        bootFaults |= FAULT_LOW_WATER_LEVEL;
        DEBUG_PRINTLN(F("[StateMachine] Self_Check FAILED: Insufficient water"));
    }
    
    // Check 2: Temperature sensor operational
    if (!sensorManager.isTemperatureSensorOperational()) {
        allPreconditionsSatisfied = false;
        bootFaults |= FAULT_TEMPERATURE_SENSOR;
        DEBUG_PRINTLN(F("[StateMachine] Self_Check FAILED: Temp sensor not operational"));
    }
    
    // Check 3: Temperature within safe range
    float currentTemp = sensorManager.getTemperature();
    if (currentTemp < TEMP_VALID_MIN || currentTemp > TEMP_VALID_MAX) {
        allPreconditionsSatisfied = false;
        bootFaults |= FAULT_HIGH_TEMPERATURE;
        DEBUG_PRINTLN(F("[StateMachine] Self_Check FAILED: Temp out of range"));
    }
    
    // Check 4: I2C devices responding
    // (This check may need to be added to I2CBusManager or done during init)
    
    // Check 5: All relays OFF
    // (This check may need to be added to RelayController)
    
    // Transition logic
    if (allPreconditionsSatisfied) {
        // ALL conditions satisfied - transition to Ready
        transitionTo(STATE_READY);
    } else {
        // ANY condition failed - transition to Fault and persist
        safetySystem.setActiveFaults(bootFaults);
        transitionTo(STATE_FAULT);
    }
}
```

#### 1.3 Implement Boot Fault Persistence

- [ ] When transitioning to Fault from Self_Check, mark as "boot fault"
- [ ] Boot faults persist until ALL underlying conditions resolved
- [ ] System does NOT automatically retry Self_Check
- [ ] User must acknowledge or conditions must clear before retry

**File**: `lib/StateMachine/StateMachine.cpp`

#### 1.4 Add State Transition Guards

- [ ] Add guard condition for Self_Check → Ready transition
- [ ] Guard MUST verify ALL preconditions satisfied
- [ ] Add guard condition for Fault → Self_Check transition
- [ ] Guard MUST verify ALL faults cleared

**File**: `lib/StateMachine/StateMachine.cpp`

**Code pattern**:
```cpp
bool StateMachine::canTransitionToReady() {
    // Guard condition: ALL preconditions must be satisfied
    return sensorManager.isWaterLevelSufficient() &&
           sensorManager.isTemperatureSensorOperational() &&
           sensorManager.getTemperature() >= TEMP_VALID_MIN &&
           sensorManager.getTemperature() <= TEMP_VALID_MAX &&
           !safetySystem.hasActiveFaults();
}
```

#### 1.5 Test Boot Fault Persistence

- [ ] Test boot with insufficient water - verify stays in Fault
- [ ] Test boot with sensor disconnected - verify stays in Fault
- [ ] Test boot with temperature out of range - verify stays in Fault
- [ ] Test boot with ALL conditions safe - verify transitions to Ready
- [ ] Test fault recovery - verify transitions back to Self_Check then Ready

---

## Component 2: SafetySystem - Continuous Fault Detection

### Requirements Analysis

**Requirements 11.1-11.18, 2.7-2.11, 3.4-3.6**:
- System SHALL detect fault conditions continuously
- Fault types: Low water (10s duration), High temp warning (42°C) vs critical (45°C), Thermal runaway (unexpected CHANGE/DELTA), Sensor failures, I2C/PCF8574 failures
- WHEN fault detected, System SHALL enter Fault state
- Auto-clear vs manual acknowledgment logic

### Implementation Checklist

#### 2.1 Read Current SafetySystem Implementation

- [ ] Read `lib/SafetySystem/SafetySystem.cpp` to understand current fault detection
- [ ] Read `include/SafetySystem.h` to understand interface
- [ ] Identify existing fault detection methods
- [ ] Identify fault bit field storage
- [ ] Identify fault clearing logic

**Files**: `lib/SafetySystem/SafetySystem.cpp`, `include/SafetySystem.h`

#### 2.2 Implement Continuous Fault Detection Method

- [ ] Create `void SafetySystem::detectFaults()` method
- [ ] Call this method from main loop every iteration
- [ ] Check ALL fault conditions:
  - [ ] Low water level (extended duration - 10 seconds)
  - [ ] High temperature warning (42°C) - enters Warning state
  - [ ] Critical overtemperature (45°C) - enters Fault state
  - [ ] Thermal runaway (unexpected CHANGE/DELTA) - enters Fault state
  - [ ] Temperature sensor failure (consecutive invalid readings)
  - [ ] I2C communication failure (transaction timeouts)
  - [ ] PCF8574 failure (communication fails)
- [ ] Update fault bit field when faults detected
- [ ] Trigger state machine transition to Fault/Warning state

**File**: `lib/SafetySystem/SafetySystem.cpp`

**Code pattern**:
```cpp
void SafetySystem::detectFaults() {
    // CRITICAL: Continuous fault detection
    // Called every loop iteration to monitor ALL fault conditions
    
    uint8_t newFaults = FAULT_NONE;
    bool warningCondition = false;
    
    // Fault 1: Low water level (extended duration)
    if (!sensorManager.isWaterLevelSufficient()) {
        unsigned long insufficientDuration = millis() - waterLevelFaultStartTime;
        if (insufficientDuration >= WATER_LEVEL_FAULT_DURATION_MS) {
            newFaults |= FAULT_LOW_WATER_LEVEL;
            DEBUG_PRINTLN(F("[Safety] FAULT: Low water level"));
        }
    } else {
        waterLevelFaultStartTime = millis();  // Reset timer
    }
    
    // Fault 2: High temperature warning vs critical
    float currentTemp = sensorManager.getTemperature();
    if (currentTemp >= TEMP_CRITICAL_THRESHOLD) {
        // Critical overtemperature - FAULT
        newFaults |= FAULT_HIGH_TEMPERATURE;
        DEBUG_PRINTLN(F("[Safety] FAULT: Critical overtemperature"));
    } else if (currentTemp >= TEMP_WARNING_THRESHOLD) {
        // High temperature warning - WARNING state
        warningCondition = true;
        DEBUG_PRINTLN(F("[Safety] WARNING: High temperature"));
    }
    
    // Fault 3: Thermal runaway (unexpected CHANGE/DELTA)
    if (detectThermalRunaway()) {
        newFaults |= FAULT_THERMAL_RUNAWAY;
        thermalRunawayAcknowledged = false;  // Requires manual ack
        DEBUG_PRINTLN(F("[Safety] FAULT: Thermal runaway detected"));
    }
    
    // Fault 4: Temperature sensor failure
    if (sensorManager.hasTemperatureSensorFault()) {
        newFaults |= FAULT_TEMPERATURE_SENSOR;
        DEBUG_PRINTLN(F("[Safety] FAULT: Temperature sensor failure"));
    }
    
    // Fault 5: I2C communication failure
    // (Check with I2CBusManager)
    
    // Fault 6: PCF8574 failure
    // (Check with RelayController)
    
    // Update active faults
    if (newFaults != FAULT_NONE) {
        activeFaults |= newFaults;
        // Trigger state machine transition to Fault
        stateMachine.requestFaultState(activeFaults);
    }
    
    // Handle warning condition
    if (warningCondition && activeFaults == FAULT_NONE) {
        // Trigger state machine transition to Warning
        stateMachine.requestWarningState();
    }
}
```

#### 2.3 Implement Thermal Runaway Detection

- [ ] Maintain temperature history buffer (already exists in SensorManager)
- [ ] Calculate temperature delta over time window
- [ ] Calculate rate of change (°C/second)
- [ ] IF delta exceeds threshold OR rate exceeds threshold, detect thermal runaway
- [ ] Set thermal runaway flag requiring manual acknowledgment

**File**: `lib/SafetySystem/SafetySystem.cpp`

**Code pattern**:
```cpp
bool SafetySystem::detectThermalRunaway() {
    // CRITICAL: Thermal runaway = unexpected temperature CHANGE/DELTA
    // NOT just absolute threshold
    
    // Get temperature history from SensorManager
    // Calculate delta and rate
    
    float tempDelta = calculateTemperatureDelta();
    float tempRate = calculateTemperatureRate();
    
    if (fabs(tempDelta) > TEMP_DELTA_THRESHOLD || 
        fabs(tempRate) > TEMP_RATE_THRESHOLD) {
        return true;  // Thermal runaway detected
    }
    
    return false;
}
```

#### 2.4 Implement Auto-Clear vs Manual Acknowledgment Logic

- [ ] Add method `void SafetySystem::updateFaultClearing()`
- [ ] For each fault type, determine if auto-clear or manual acknowledgment
- [ ] Auto-clear faults:
  - [ ] Low water level: Clear after stabilization period of sufficient water
  - [ ] High temperature warning: Clear when temperature drops below threshold
  - [ ] Sensor communication: Clear after consecutive valid readings
  - [ ] I2C/PCF8574 communication: Clear when communication restored
- [ ] Manual acknowledgment faults:
  - [ ] Thermal runaway: Requires explicit user acknowledgment before heater can operate

**File**: `lib/SafetySystem/SafetySystem.cpp`

**Code pattern**:
```cpp
void SafetySystem::updateFaultClearing() {
    // CRITICAL: Auto-clear vs manual acknowledgment
    
    uint8_t faultsToClear = FAULT_NONE;
    
    // Auto-clear: Low water level
    if (activeFaults & FAULT_LOW_WATER_LEVEL) {
        if (sensorManager.isWaterLevelSufficient()) {
            unsigned long stabilizationDuration = millis() - waterLevelRecoveryStartTime;
            if (stabilizationDuration >= WATER_LEVEL_STABILIZATION_MS) {
                faultsToClear |= FAULT_LOW_WATER_LEVEL;
                DEBUG_PRINTLN(F("[Safety] Auto-clear: Low water level"));
            }
        } else {
            waterLevelRecoveryStartTime = millis();  // Reset timer
        }
    }
    
    // Auto-clear: High temperature warning
    // (Handled in detectFaults - warning condition clears automatically)
    
    // Auto-clear: Temperature sensor
    if (activeFaults & FAULT_TEMPERATURE_SENSOR) {
        if (sensorManager.isTemperatureSensorOperational()) {
            faultsToClear |= FAULT_TEMPERATURE_SENSOR;
            DEBUG_PRINTLN(F("[Safety] Auto-clear: Temperature sensor"));
        }
    }
    
    // Manual acknowledgment: Thermal runaway
    if (activeFaults & FAULT_THERMAL_RUNAWAY) {
        // Does NOT auto-clear
        // Requires explicit user acknowledgment
        if (thermalRunawayAcknowledged) {
            faultsToClear |= FAULT_THERMAL_RUNAWAY;
            DEBUG_PRINTLN(F("[Safety] Manual clear: Thermal runaway acknowledged"));
        }
    }
    
    // Clear faults
    activeFaults &= ~faultsToClear;
    
    // If all faults cleared, notify state machine
    if (activeFaults == FAULT_NONE) {
        stateMachine.requestSelfCheckState();  // Re-validate before Ready
    }
}
```

#### 2.5 Add Thermal Runaway Acknowledgment Method

- [ ] Add `void SafetySystem::acknowledgeThermalRunaway()` method
- [ ] Set `thermalRunawayAcknowledged = true`
- [ ] Allow fault clearing logic to proceed
- [ ] Add UI integration for user acknowledgment

**File**: `lib/SafetySystem/SafetySystem.cpp`

**Code pattern**:
```cpp
void SafetySystem::acknowledgeThermalRunaway() {
    if (activeFaults & FAULT_THERMAL_RUNAWAY) {
        thermalRunawayAcknowledged = true;
        DEBUG_PRINTLN(F("[Safety] Thermal runaway acknowledged by user"));
    }
}
```

#### 2.6 Implement Multi-Fault Priority Ordering

- [ ] Define fault priority order: Low water > High temp > Thermal runaway > Sensor > I2C > PCF8574
- [ ] When displaying faults, show highest priority first
- [ ] Fault bit field already supports multiple simultaneous faults

**File**: `lib/SafetySystem/SafetySystem.cpp`

**Code pattern**:
```cpp
uint8_t SafetySystem::getHighestPriorityFault() {
    // Priority order (highest to lowest)
    if (activeFaults & FAULT_LOW_WATER_LEVEL) return FAULT_LOW_WATER_LEVEL;
    if (activeFaults & FAULT_HIGH_TEMPERATURE) return FAULT_HIGH_TEMPERATURE;
    if (activeFaults & FAULT_THERMAL_RUNAWAY) return FAULT_THERMAL_RUNAWAY;
    if (activeFaults & FAULT_TEMPERATURE_SENSOR) return FAULT_TEMPERATURE_SENSOR;
    if (activeFaults & FAULT_I2C_FAILURE) return FAULT_I2C_FAILURE;
    if (activeFaults & FAULT_PCF8574_FAILURE) return FAULT_PCF8574_FAILURE;
    return FAULT_NONE;
}
```

---

## Component 3: UIManager - Forbidden Action Feedback

### Requirements Analysis

**Requirements 5.3, 6.3, 7.7**:
- IF ANY precondition or safety condition fails, System SHALL deny activation and display appropriate error indication
- IF Circulation not active, System SHALL deny Water_Heater activation and display error message
- IF Circulation not active, System SHALL deny feature activation and display error message

### Implementation Checklist

#### 3.1 Read Current UIManager Implementation

- [ ] Read `lib/UIManager/UIManager.cpp` to understand current UI rendering
- [ ] Read `include/UIManager.h` to understand interface
- [ ] Identify where to add forbidden action feedback overlay

**Files**: `lib/UIManager/UIManager.cpp`, `include/UIManager.h`

#### 3.2 Add Forbidden Action Feedback State

- [ ] Add private member: `bool forbiddenActionActive`
- [ ] Add private member: `const char* forbiddenActionMessage`
- [ ] Add private member: `unsigned long forbiddenActionStartTime`
- [ ] Initialize in constructor

**File**: `include/UIManager.h`

**Code to add**:
```cpp
// In private members section:
bool forbiddenActionActive;              // True when forbidden action feedback is displayed
const char* forbiddenActionMessage;      // Message to display
unsigned long forbiddenActionStartTime;  // When feedback started (for auto-dismiss)
```

#### 3.3 Implement Forbidden Action Feedback Method

- [ ] Add method `void UIManager::showForbiddenActionFeedback(const char* message)`
- [ ] Set `forbiddenActionActive = true`
- [ ] Store message
- [ ] Record start time
- [ ] Request redraw

**File**: `lib/UIManager/UIManager.cpp`

**Code to add**:
```cpp
void UIManager::showForbiddenActionFeedback(const char* message) {
    forbiddenActionActive = true;
    forbiddenActionMessage = message;
    forbiddenActionStartTime = millis();
    needsRedraw = true;
    
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINT(F("[UI] Forbidden action: "));
        DEBUG_PRINTLN(message);
    #endif
}
```

#### 3.4 Implement Forbidden Action Overlay Rendering

- [ ] In `renderCurrentScreen()`, check if `forbiddenActionActive`
- [ ] If active, render forbidden action overlay on top of current screen
- [ ] Display error message in bordered box
- [ ] Auto-dismiss after timeout (e.g., 3 seconds)

**File**: `lib/UIManager/UIManager.cpp`

**Code to add**:
```cpp
void UIManager::renderForbiddenActionOverlay() {
    // Draw error box at bottom of screen
    int16_t boxX = 4;
    int16_t boxY = 48;
    int16_t boxW = 120;
    int16_t boxH = 14;
    
    // Draw box border
    display.drawRect(boxX, boxY, boxW, boxH, SH110X_WHITE);
    
    // Draw error message
    int16_t textX = boxX + 4;
    int16_t textY = boxY + 3;
    drawTextUnscaled(forbiddenActionMessage, textX, textY);
    
    // Auto-dismiss after timeout
    if (millis() - forbiddenActionStartTime >= 3000) {
        forbiddenActionActive = false;
        needsRedraw = true;
    }
}
```

#### 3.5 Integrate Forbidden Action Feedback with Feature Activation

- [ ] When user tries to activate circulation without preconditions, show feedback
- [ ] When user tries to activate heater without circulation, show feedback
- [ ] When user tries to activate features without circulation, show feedback

**File**: `lib/UIManager/UIManager.cpp` (or feature control logic)

**Code pattern**:
```cpp
// Example: When user tries to activate heater without circulation
if (!relayController.getRelayState(RELAY_CIRCULATION_PUMP)) {
    uiManager.showForbiddenActionFeedback("Circulation required");
    return;  // Deny activation
}
```

---

## Component 4: main.cpp - Continuous Monitoring Integration

### Requirements Analysis

**Requirements 11.1, 13.1-13.12**:
- System SHALL implement Event_Loop that processes events without blocking
- Continuous fault detection must be integrated into main loop
- All subsystems updated every loop iteration

### Implementation Checklist

#### 4.1 Read Current main.cpp Implementation

- [ ] Read `src/main.cpp` to understand current event loop
- [ ] Identify where to add continuous fault detection
- [ ] Identify where to add fault clearing logic

**File**: `src/main.cpp`

#### 4.2 Integrate Continuous Fault Detection in Loop

- [ ] Add `safetySystem.detectFaults()` call in main loop
- [ ] Add `safetySystem.updateFaultClearing()` call in main loop
- [ ] Ensure called every loop iteration
- [ ] Add debug logging

**File**: `src/main.cpp`

**Code to add** (in `loop()` function):
```cpp
void loop() {
    // Update all subsystems (non-blocking)
    sensorManager.update();
    relayController.update();
    
    // CRITICAL: Continuous fault detection
    safetySystem.detectFaults();
    safetySystem.updateFaultClearing();
    
    stateMachine.update();
    uiManager.update(stateMachine.getCurrentState());
    inputHandler.update();
    
    // Event loop remains responsive
}
```

#### 4.3 Test Continuous Monitoring

- [ ] Test fault detection during operation
- [ ] Test automatic transition to Fault state
- [ ] Test fault clearing and recovery
- [ ] Test multiple simultaneous faults

---

## Component 5: State Machine Guard Conditions

### Requirements Analysis

**Requirements 8.13**:
- System SHALL define Guard_Condition functions for all state transitions
- Guards verify preconditions before allowing transition

### Implementation Checklist

#### 5.1 Implement Guard Conditions for All Transitions

- [ ] Add guard for Ready → Active_Circulation
- [ ] Add guard for Active_Circulation → Feature_Enabled_Bath
- [ ] Add guard for any state → Fault
- [ ] Add guard for Fault → Self_Check
- [ ] Add guard for Self_Check → Ready

**File**: `lib/StateMachine/StateMachine.cpp`

**Code pattern**:
```cpp
bool StateMachine::canTransitionToActiveCirculation() {
    // Guard: Circulation pump preconditions
    return sensorManager.isWaterLevelSufficient() &&
           sensorManager.isTemperatureSensorOperational() &&
           !safetySystem.hasActiveFaults();
}

bool StateMachine::canTransitionToFeatureEnabledBath() {
    // Guard: Circulation must be active
    return (currentState == STATE_ACTIVE_CIRCULATION) &&
           relayController.getRelayState(RELAY_CIRCULATION_PUMP);
}
```

#### 5.2 Integrate Guards into Transition Logic

- [ ] Check guard condition before allowing transition
- [ ] If guard fails, deny transition and log reason
- [ ] Optionally show forbidden action feedback

**File**: `lib/StateMachine/StateMachine.cpp`

**Code pattern**:
```cpp
void StateMachine::requestTransitionToActiveCirculation() {
    if (canTransitionToActiveCirculation()) {
        transitionTo(STATE_ACTIVE_CIRCULATION);
    } else {
        DEBUG_PRINTLN(F("[StateMachine] Transition denied: Preconditions not met"));
        // Optionally show UI feedback
    }
}
```

---

## Integration and Testing

### Integration Checklist

#### Update All Components

- [ ] StateMachine: Boot fault persistence, guard conditions
- [ ] SafetySystem: Continuous fault detection, auto-clear vs manual ack
- [ ] UIManager: Forbidden action feedback overlay
- [ ] SensorManager: Fault detection integration (if needed)
- [ ] main.cpp: Continuous monitoring in event loop

#### Test Boot Fault Persistence

- [ ] Test boot with insufficient water - verify stays in Fault
- [ ] Test boot with sensor failure - verify stays in Fault
- [ ] Test boot with temperature out of range - verify stays in Fault
- [ ] Test boot with ALL conditions safe - verify transitions to Ready
- [ ] Test fault recovery - verify re-validates before Ready

#### Test Continuous Fault Detection

- [ ] Test low water fault during operation
- [ ] Test high temperature warning vs critical
- [ ] Test thermal runaway detection
- [ ] Test sensor failure during operation
- [ ] Test automatic transition to Fault state

#### Test Forbidden Action Feedback

- [ ] Try to activate circulation without preconditions - verify feedback
- [ ] Try to activate heater without circulation - verify feedback
- [ ] Try to activate features without circulation - verify feedback
- [ ] Verify feedback auto-dismisses after timeout

#### Test Auto-Clear vs Manual Acknowledgment

- [ ] Test low water fault auto-clears after stabilization
- [ ] Test high temp warning auto-clears when temp drops
- [ ] Test sensor fault auto-clears after valid readings
- [ ] Test thermal runaway requires manual acknowledgment
- [ ] Test heater cannot activate until thermal runaway acknowledged

#### Test Multi-Fault Management

- [ ] Trigger multiple simultaneous faults
- [ ] Verify fault bit field stores all faults
- [ ] Verify highest priority fault displayed first
- [ ] Verify fault browsing works with multiple faults
- [ ] Verify all faults must clear before Ready

---

## Acceptance Criteria

### Boot Fault Persistence

- [ ] System remains in Fault state from boot until ALL conditions resolved
- [ ] Self_Check verifies ALL preconditions before allowing Ready transition
- [ ] System does NOT progress to Ready with unsafe conditions
- [ ] Boot faults persist until explicitly resolved
- [ ] Complies with Requirements 1.9, 1.10, 8.5-8.7, 11.2-11.3

### Continuous Fault Detection

- [ ] Fault detection runs every loop iteration
- [ ] All fault types detected: low water, high temp, thermal runaway, sensor failures, I2C/PCF8574 failures
- [ ] Automatic transition to Fault state when fault detected
- [ ] Fault bit field stores multiple simultaneous faults
- [ ] Complies with Requirements 11.1-11.18

### Forbidden Action Feedback

- [ ] User gets visual feedback when action denied
- [ ] Error message displayed in overlay
- [ ] Feedback auto-dismisses after timeout
- [ ] Works for all forbidden actions (circulation, heater, features)
- [ ] Complies with Requirements 5.3, 6.3, 7.7

### Auto-Clear vs Manual Acknowledgment

- [ ] Auto-clear faults clear automatically when condition resolves
- [ ] Manual acknowledgment faults require explicit user action
- [ ] Thermal runaway requires manual acknowledgment before heater can operate
- [ ] Fault clearing logic distinguishes between types
- [ ] Complies with Requirements 11.16-11.17, 19.5-19.7

### Multi-Fault Management

- [ ] Fault bit field supports multiple simultaneous faults
- [ ] Fault priority ordering enforced
- [ ] Highest priority fault displayed first
- [ ] Fault browsing works with multiple faults
- [ ] Complies with Requirements 11.8-11.11

### State Machine Integration

- [ ] Boot fault persistence logic in Self_Check state
- [ ] Continuous monitoring in all operational states
- [ ] Automatic transition to Fault when fault detected
- [ ] Guard conditions for all state transitions
- [ ] Complies with Requirements 8.5-8.7, 8.13

---

## Documentation

### Files to Update

- [ ] `docs/phase-6c-error-handling-implementation-plan.md` (this file)
- [ ] `docs/phase-6c-error-handling-results.md` (after implementation)
- [ ] Update phase documentation with error handling details

### Documentation Checklist

- [ ] Document all changes made to each component
- [ ] Document boot fault persistence behavior
- [ ] Document continuous fault detection logic
- [ ] Document forbidden action feedback implementation
- [ ] Document auto-clear vs manual acknowledgment logic
- [ ] Document testing results
- [ ] Document any issues encountered

---

## Summary

This implementation plan provides exhaustive checklists for implementing complete error handling system:

1. **StateMachine**: Boot fault persistence in Self_Check, guard conditions
2. **SafetySystem**: Continuous fault detection, thermal runaway, auto-clear vs manual ack
3. **UIManager**: Forbidden action feedback overlay
4. **main.cpp**: Continuous monitoring integration
5. **State Machine**: Guard conditions for all transitions

All changes maintain:
- Non-blocking architecture
- Centralized configuration
- Safety-first approach
- Integration with existing code

**CRITICAL**: This implementation is essential for system safety and MUST be completed before Phase 7 (User Input) to ensure proper error handling when users interact with the system.

Next: Implement Phase 6B (UI Screens Fix) first, then proceed with Phase 6C (Error Handling).
