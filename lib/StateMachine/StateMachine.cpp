#include "StateMachine.h"

// ============================================================================
// Constructor
// ============================================================================
StateMachine::StateMachine(SensorManager& sensors, RelayController& relays, I2CBusManager& i2c)
    : sensorManager(sensors)
    , relayController(relays)
    , i2cBus(i2c)
    , currentState(STATE_BOOT)
    , previousState(STATE_BOOT)
    , stateEntryTime(0)
    , activeFaults(FAULT_NONE)
    , hardwareInitComplete(false)
    , selfCheckComplete(false)
{
}

// ============================================================================
// Initialization
// ============================================================================
void StateMachine::begin() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F(""));
        Serial.println(F("Initializing State Machine..."));
    #endif
    
    // Start in Boot state
    currentState = STATE_BOOT;
    previousState = STATE_BOOT;
    stateEntryTime = millis();
    activeFaults = FAULT_NONE;
    
    // Execute Boot state entry action
    onEnterBoot();
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("State Machine initialized"));
        Serial.print(F("Initial state: "));
        Serial.println(getStateName(currentState));
        Serial.println(F(""));
    #endif
}

// ============================================================================
// Update (Non-Blocking)
// ============================================================================
void StateMachine::update() {
    // State-specific update logic
    switch (currentState) {
        case STATE_BOOT: {
            // Check if hardware initialization is complete
            // In real implementation, this would check actual hardware init status
            // For now, we assume hardware init completes immediately
            if (!hardwareInitComplete) {
                hardwareInitComplete = true;
            }
            
            // CRITICAL: Boot state must last at least 3 seconds for Power-Up UI display
            // Requirement 9.16: Power-Up UI visible for at least 3 seconds
            unsigned long bootDuration = millis() - stateEntryTime;
            if (bootDuration >= BOOT_STATE_MINIMUM_DURATION_MS && guardBootToSelfCheck()) {
                executeTransition(STATE_SELF_CHECK);
            }
            break;
        }
            
        case STATE_SELF_CHECK: {
            // Perform self-check
            if (!selfCheckComplete) {
                performSelfCheck();
                selfCheckComplete = true;
            }
            
            // CRITICAL: Self-check state must last at least 2 seconds for Initialization UI visibility
            // This allows users to see the initialization progress screen
            unsigned long selfCheckDuration = millis() - stateEntryTime;
            
            if (selfCheckDuration >= SELF_CHECK_MINIMUM_DURATION_MS) {
                // Minimum duration elapsed - now transition based on results
                if (activeFaults != FAULT_NONE) {
                    // Faults detected - enter Fault state
                    executeTransition(STATE_FAULT);
                } else if (guardSelfCheckToReady()) {
                    // All preconditions satisfied - enter Ready state
                    executeTransition(STATE_READY);
                }
            }
            break;
        }
            
        case STATE_READY:
            // Monitor for faults
            if (activeFaults != FAULT_NONE) {
                executeTransition(STATE_FAULT);
            }
            // User input will trigger transitions to Active_Circulation
            // (implemented in Phase 7 - User Input)
            break;
            
        case STATE_ACTIVE_CIRCULATION:
            // Monitor for faults
            if (activeFaults != FAULT_NONE) {
                executeTransition(STATE_FAULT);
            }
            // User input will trigger transitions to Feature_Enabled_Bath
            // (implemented in Phase 7 - User Input)
            break;
            
        case STATE_FEATURE_ENABLED_BATH:
            // Monitor for faults and warnings
            if (activeFaults != FAULT_NONE) {
                executeTransition(STATE_FAULT);
            }
            // Warning state transitions (implemented in Phase 5 - Safety System)
            break;
            
        case STATE_WARNING:
            // Monitor for fault escalation
            if (activeFaults != FAULT_NONE) {
                executeTransition(STATE_FAULT);
            }
            // Warning clear transitions (implemented in Phase 5 - Safety System)
            break;
            
        case STATE_FAULT:
            // Monitor sensor recovery and auto-clear faults
            // Temperature sensor fault auto-clears when sensor becomes operational
            if (hasFault(FAULT_TEMPERATURE_SENSOR) && sensorManager.isTemperatureSensorOperational()) {
                clearFault(FAULT_TEMPERATURE_SENSOR);
            }
            
            // Water level fault auto-clears when water level becomes sufficient AND fault flag clears
            // CRITICAL: Check both actual water level AND fault flag to ensure stabilization
            if (hasFault(FAULT_LOW_WATER_LEVEL) && 
                sensorManager.isWaterLevelSufficient() && 
                !sensorManager.hasWaterLevelFault()) {
                clearFault(FAULT_LOW_WATER_LEVEL);
            }
            
            // I2C fault auto-clears when devices respond
            if (hasFault(FAULT_I2C_FAILURE) && i2cBus.isDeviceResponding(I2C_ADDRESS_OLED)) {
                clearFault(FAULT_I2C_FAILURE);
            }
            
            // PCF8574 fault auto-clears when device responds
            if (hasFault(FAULT_PCF8574_FAILURE) && relayController.isPCF8574Responding()) {
                clearFault(FAULT_PCF8574_FAILURE);
            }
            
            // Check if faults have been cleared
            if (activeFaults == FAULT_NONE && guardFaultToSelfCheck()) {
                // All faults cleared - re-validate system
                selfCheckComplete = false;
                executeTransition(STATE_SELF_CHECK);
            }
            // User input can trigger Fault_Inspection
            // (implemented in Phase 7 - User Input)
            break;
            
        case STATE_FAULT_INSPECTION:
            // User browsing faults
            // User input will trigger return to Fault state
            // (implemented in Phase 7 - User Input)
            
            // Check if faults cleared during inspection
            if (activeFaults == FAULT_NONE && guardFaultToSelfCheck()) {
                selfCheckComplete = false;
                executeTransition(STATE_SELF_CHECK);
            }
            break;
            
        case STATE_SHUTDOWN:
            // Check if shutdown is complete
            if (relayController.isSafeShutdownComplete()) {
                // Shutdown complete - transition based on fault status
                if (activeFaults != FAULT_NONE) {
                    executeTransition(STATE_FAULT);
                } else {
                    executeTransition(STATE_READY);
                }
            }
            break;
    }
}

// ============================================================================
// State Queries
// ============================================================================

SystemState StateMachine::getCurrentState() const {
    return currentState;
}

SystemState StateMachine::getPreviousState() const {
    return previousState;
}

const char* StateMachine::getStateName(SystemState state) const {
    switch (state) {
        case STATE_BOOT: return "Boot";
        case STATE_SELF_CHECK: return "Self_Check";
        case STATE_READY: return "Ready";
        case STATE_ACTIVE_CIRCULATION: return "Active_Circulation";
        case STATE_FEATURE_ENABLED_BATH: return "Feature_Enabled_Bath";
        case STATE_WARNING: return "Warning";
        case STATE_FAULT: return "Fault";
        case STATE_FAULT_INSPECTION: return "Fault_Inspection";
        case STATE_SHUTDOWN: return "Shutdown";
        default: return "Unknown";
    }
}

// ============================================================================
// Fault Management
// ============================================================================

uint8_t StateMachine::getActiveFaults() const {
    return activeFaults;
}

bool StateMachine::hasFault(FaultCode fault) const {
    return (activeFaults & fault) != 0;
}

void StateMachine::setFault(FaultCode fault) {
    bool wasNoFault = (activeFaults == FAULT_NONE);
    activeFaults |= fault;
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[STATE] Fault set: 0x"));
        Serial.print(fault, HEX);
        Serial.print(F(" (Active faults: 0x"));
        Serial.print(activeFaults, HEX);
        Serial.println(F(")"));
    #endif
    
    // If this is the first fault, trigger transition to Fault state
    // EXCEPTION: Do NOT auto-transition during Self_Check or Boot states
    // These states manage their own transitions with minimum duration requirements
    if (wasNoFault && 
        currentState != STATE_FAULT && 
        currentState != STATE_FAULT_INSPECTION &&
        currentState != STATE_SELF_CHECK &&
        currentState != STATE_BOOT) {
        executeTransition(STATE_FAULT);
    }
}

void StateMachine::clearFault(FaultCode fault) {
    activeFaults &= ~fault;
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[STATE] Fault cleared: 0x"));
        Serial.print(fault, HEX);
        Serial.print(F(" (Active faults: 0x"));
        Serial.print(activeFaults, HEX);
        Serial.println(F(")"));
    #endif
}

void StateMachine::clearAllFaults() {
    activeFaults = FAULT_NONE;
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] All faults cleared"));
    #endif
}

uint8_t StateMachine::getFaultCount() const {
    // Count number of set bits in activeFaults
    uint8_t count = 0;
    uint8_t faults = activeFaults;
    while (faults) {
        count += faults & 1;
        faults >>= 1;
    }
    return count;
}

// ============================================================================
// State Transition Requests
// ============================================================================

bool StateMachine::requestTransition(SystemState newState) {
    if (canTransition(currentState, newState)) {
        executeTransition(newState);
        return true;
    }
    return false;
}

// ============================================================================
// Precondition Checks
// ============================================================================

bool StateMachine::checkAllPreconditions() const {
    // Check all preconditions for system operation
    bool waterLevelOK = sensorManager.isWaterLevelSufficient();
    bool tempSensorOK = sensorManager.isTemperatureSensorOperational();
    bool tempInRange = (sensorManager.getTemperature() >= TEMP_VALID_MIN && 
                        sensorManager.getTemperature() <= TEMP_VALID_MAX);
    bool noFaults = (activeFaults == FAULT_NONE);
    
    return waterLevelOK && tempSensorOK && tempInRange && noFaults;
}

bool StateMachine::checkCirculationPreconditions() const {
    // Preconditions for circulation pump activation
    bool waterLevelOK = sensorManager.isWaterLevelSufficient();
    bool tempSensorOK = sensorManager.isTemperatureSensorOperational();
    bool noFaults = (activeFaults == FAULT_NONE);
    
    return waterLevelOK && tempSensorOK && noFaults;
}

bool StateMachine::checkHeaterPreconditions() const {
    // Preconditions for heater activation
    // ABSOLUTE RULE: Circulation MUST be active
    bool circulationActive = relayController.getRelayState(RELAY_CHANNEL_CIRCULATION);
    bool waterLevelOK = sensorManager.isWaterLevelSufficient();
    bool tempBelowWarning = (sensorManager.getTemperature() < TEMP_WARNING_THRESHOLD);
    bool noFaults = (activeFaults == FAULT_NONE);
    
    return circulationActive && waterLevelOK && tempBelowWarning && noFaults;
}

// ============================================================================
// State Transition Logic
// ============================================================================

bool StateMachine::canTransition(SystemState from, SystemState to) const {
    // Check if transition is allowed based on guard conditions
    if (from == to) {
        return false;  // No self-transitions
    }
    
    switch (from) {
        case STATE_BOOT:
            return (to == STATE_SELF_CHECK && guardBootToSelfCheck());
            
        case STATE_SELF_CHECK:
            if (to == STATE_READY) return guardSelfCheckToReady();
            if (to == STATE_FAULT) return guardSelfCheckToFault();
            return false;
            
        case STATE_READY:
            if (to == STATE_ACTIVE_CIRCULATION) return guardReadyToActiveCirculation();
            if (to == STATE_FAULT) return guardToFault();
            return false;
            
        case STATE_ACTIVE_CIRCULATION:
            if (to == STATE_FEATURE_ENABLED_BATH) return guardActiveCirculationToFeatureEnabledBath();
            if (to == STATE_READY) return true;  // Can always deactivate circulation
            if (to == STATE_FAULT) return guardToFault();
            return false;
            
        case STATE_FEATURE_ENABLED_BATH:
            if (to == STATE_ACTIVE_CIRCULATION) return true;  // Can deactivate features
            if (to == STATE_WARNING) return true;  // Can enter warning state
            if (to == STATE_FAULT) return guardToFault();
            return false;
            
        case STATE_WARNING:
            if (to == STATE_FEATURE_ENABLED_BATH) return true;  // Warning clears
            if (to == STATE_FAULT) return guardToFault();
            return false;
            
        case STATE_FAULT:
            if (to == STATE_FAULT_INSPECTION) return guardFaultToFaultInspection();
            if (to == STATE_SELF_CHECK) return guardFaultToSelfCheck();
            if (to == STATE_SHUTDOWN) return true;  // Can always initiate shutdown
            return false;
            
        case STATE_FAULT_INSPECTION:
            if (to == STATE_FAULT) return true;  // Can exit inspection
            if (to == STATE_SELF_CHECK) return guardFaultToSelfCheck();
            return false;
            
        case STATE_SHUTDOWN:
            if (to == STATE_FAULT) return true;  // Shutdown complete with faults
            if (to == STATE_READY) return true;  // Shutdown complete without faults
            return false;
            
        default:
            return false;
    }
}

void StateMachine::executeTransition(SystemState newState) {
    if (currentState == newState) {
        return;  // Already in target state
    }
    
    // Log transition
    logStateTransition(currentState, newState);
    
    // Execute exit action for current state
    switch (currentState) {
        case STATE_BOOT: onExitBoot(); break;
        case STATE_SELF_CHECK: onExitSelfCheck(); break;
        case STATE_READY: onExitReady(); break;
        case STATE_ACTIVE_CIRCULATION: onExitActiveCirculation(); break;
        case STATE_FEATURE_ENABLED_BATH: onExitFeatureEnabledBath(); break;
        case STATE_WARNING: onExitWarning(); break;
        case STATE_FAULT: onExitFault(); break;
        case STATE_FAULT_INSPECTION: onExitFaultInspection(); break;
        case STATE_SHUTDOWN: onExitShutdown(); break;
    }
    
    // Update state
    previousState = currentState;
    currentState = newState;
    stateEntryTime = millis();
    
    // Execute entry action for new state
    switch (newState) {
        case STATE_BOOT: onEnterBoot(); break;
        case STATE_SELF_CHECK: onEnterSelfCheck(); break;
        case STATE_READY: onEnterReady(); break;
        case STATE_ACTIVE_CIRCULATION: onEnterActiveCirculation(); break;
        case STATE_FEATURE_ENABLED_BATH: onEnterFeatureEnabledBath(); break;
        case STATE_WARNING: onEnterWarning(); break;
        case STATE_FAULT: onEnterFault(); break;
        case STATE_FAULT_INSPECTION: onEnterFaultInspection(); break;
        case STATE_SHUTDOWN: onEnterShutdown(); break;
    }
}

// ============================================================================
// Entry Actions
// ============================================================================

void StateMachine::onEnterBoot() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Entered Boot state"));
    #endif
    // Hardware initialization happens in main.cpp setup()
}

void StateMachine::onEnterSelfCheck() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Entered Self_Check state"));
        Serial.println(F("[STATE] Performing self-check..."));
    #endif
    // Self-check will be performed in update()
}

void StateMachine::onEnterReady() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Entered Ready state"));
        Serial.println(F("[STATE] System ready - all preconditions satisfied"));
    #endif
    // Display ready screen (Phase 6 - UI)
}

void StateMachine::onEnterActiveCirculation() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Entered Active_Circulation state"));
    #endif
    // Circulation pump control (Phase 9 - Feature Control)
}

void StateMachine::onEnterFeatureEnabledBath() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Entered Feature_Enabled_Bath state"));
    #endif
    // Feature control (Phase 9 - Feature Control)
}

void StateMachine::onEnterWarning() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Entered Warning state"));
    #endif
    // Display warning overlay (Phase 6 - UI)
}

void StateMachine::onEnterFault() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Entered Fault state"));
        Serial.print(F("[STATE] Active faults: 0x"));
        Serial.println(activeFaults, HEX);
    #endif
    
    // Execute safe shutdown
    if (!relayController.isSafeShutdownInProgress()) {
        relayController.startSafeShutdown();
    }
    
    // Display fault screen (Phase 6 - UI)
}

void StateMachine::onEnterFaultInspection() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Entered Fault_Inspection state"));
        Serial.print(F("[STATE] Fault count: "));
        Serial.println(getFaultCount());
    #endif
    // Display fault inspection UI (Phase 6 - UI)
}

void StateMachine::onEnterShutdown() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Entered Shutdown state"));
    #endif
    
    // Start safe shutdown sequence
    if (!relayController.isSafeShutdownInProgress()) {
        relayController.startSafeShutdown();
    }
}

// ============================================================================
// Exit Actions
// ============================================================================

void StateMachine::onExitBoot() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Exiting Boot state"));
    #endif
}

void StateMachine::onExitSelfCheck() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Exiting Self_Check state"));
    #endif
}

void StateMachine::onExitReady() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Exiting Ready state"));
    #endif
}

void StateMachine::onExitActiveCirculation() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Exiting Active_Circulation state"));
    #endif
}

void StateMachine::onExitFeatureEnabledBath() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Exiting Feature_Enabled_Bath state"));
    #endif
}

void StateMachine::onExitWarning() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Exiting Warning state"));
    #endif
}

void StateMachine::onExitFault() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Exiting Fault state"));
    #endif
}

void StateMachine::onExitFaultInspection() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Exiting Fault_Inspection state"));
    #endif
}

void StateMachine::onExitShutdown() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Exiting Shutdown state"));
    #endif
}

// ============================================================================
// Guard Conditions
// ============================================================================

bool StateMachine::guardBootToSelfCheck() const {
    // Hardware initialization must be complete
    return hardwareInitComplete;
}

bool StateMachine::guardSelfCheckToReady() const {
    // All preconditions and safety conditions must be satisfied
    return checkAllPreconditions();
}

bool StateMachine::guardSelfCheckToFault() const {
    // Any fault detected during self-check
    return (activeFaults != FAULT_NONE);
}

bool StateMachine::guardReadyToActiveCirculation() const {
    // Circulation preconditions must be satisfied
    return checkCirculationPreconditions();
}

bool StateMachine::guardActiveCirculationToFeatureEnabledBath() const {
    // Circulation must be active (checked by feature control logic)
    return relayController.getRelayState(RELAY_CHANNEL_CIRCULATION);
}

bool StateMachine::guardToFault() const {
    // Any fault condition triggers transition to Fault state
    return (activeFaults != FAULT_NONE);
}

bool StateMachine::guardFaultToFaultInspection() const {
    // Multiple faults must be active for inspection
    return (getFaultCount() > 1);
}

bool StateMachine::guardFaultToSelfCheck() const {
    // All faults must be cleared
    return (activeFaults == FAULT_NONE);
}

// ============================================================================
// Helper Methods
// ============================================================================

void StateMachine::performSelfCheck() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[STATE] Performing self-check..."));
    #endif
    
    // Clear any existing faults before self-check
    activeFaults = FAULT_NONE;
    
    // Check water level
    if (!sensorManager.isWaterLevelSufficient()) {
        setFault(FAULT_LOW_WATER_LEVEL);
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[STATE] Self-check FAIL: Insufficient water level"));
        #endif
    }
    
    // Check temperature sensor
    if (!sensorManager.isTemperatureSensorOperational()) {
        setFault(FAULT_TEMPERATURE_SENSOR);
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[STATE] Self-check FAIL: Temperature sensor not operational"));
        #endif
    }
    
    // Check temperature range
    float temp = sensorManager.getTemperature();
    if (temp < TEMP_VALID_MIN || temp > TEMP_VALID_MAX) {
        setFault(FAULT_TEMPERATURE_SENSOR);
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[STATE] Self-check FAIL: Temperature out of range ("));
            Serial.print(temp, 1);
            Serial.println(F(" °C)"));
        #endif
    }
    
    // Check I2C devices (OLED and PCF8574)
    if (!i2cBus.isDeviceResponding(I2C_ADDRESS_OLED)) {
        setFault(FAULT_I2C_FAILURE);
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[STATE] Self-check FAIL: OLED not responding"));
        #endif
    }
    
    if (!relayController.isPCF8574Responding()) {
        setFault(FAULT_PCF8574_FAILURE);
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[STATE] Self-check FAIL: PCF8574 not responding"));
        #endif
    }
    
    // Check all relays are OFF
    uint8_t relayState = relayController.getRelayStateByte();
    if (relayState != RELAY_ALL_OFF) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[STATE] Self-check WARNING: Relays not all OFF (0x"));
            Serial.print(relayState, HEX);
            Serial.println(F(")"));
        #endif
        // Force all relays OFF
        relayController.setAllRelays(false);
    }
    
    // Report self-check results
    if (activeFaults == FAULT_NONE) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[STATE] Self-check PASSED - all preconditions satisfied"));
        #endif
    } else {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[STATE] Self-check FAILED - "));
            Serial.print(getFaultCount());
            Serial.println(F(" fault(s) detected"));
        #endif
    }
}

void StateMachine::logStateTransition(SystemState from, SystemState to) const {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F(""));
        Serial.println(F("========================================"));
        Serial.print(F("[STATE] Transition: "));
        Serial.print(getStateName(from));
        Serial.print(F(" -> "));
        Serial.println(getStateName(to));
        Serial.println(F("========================================"));
    #endif
}

