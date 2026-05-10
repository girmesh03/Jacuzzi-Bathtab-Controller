#include "SafetySystem.h"
#include "StateMachine.h"

// ============================================================================
// Constructor
// ============================================================================

SafetySystem::SafetySystem(SensorManager& sensors, RelayController& relays, StateMachine& sm)
    : sensorManager(sensors),
      relayController(relays),
      stateMachine(sm),
      circulationSelected(false),
      circulationStarted(false),
      circulationSelectTime(0),
      circulationStartTime(0),
      heaterActive(false),
      heaterAutoStartEnabled(true),  // Auto-start enabled by default
      heaterAutoStartPending(false),
      heaterAutoStartTime(0),
      userTargetTemperature(0.0f),
      hasUserTargetTemperature(false),
      lastHeaterPreconditionsMet(true),  // Initialized to true so first failure prints
      thermalRunawayAcknowledged(false),
      lastThermalRunawayCheck(0) {
}

// ============================================================================
// Initialization
// ============================================================================

void SafetySystem::begin() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F(""));
        Serial.println(F("Initializing Safety System..."));
    #endif
    
    // Initialize all state variables
    circulationSelected = false;
    circulationStarted = false;
    heaterActive = false;
    heaterAutoStartEnabled = true;
    heaterAutoStartPending = false;
    hasUserTargetTemperature = false;
    thermalRunawayAcknowledged = false;
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("Safety System initialized"));
        Serial.print(F("  Heater auto-start: "));
        Serial.println(heaterAutoStartEnabled ? F("ENABLED") : F("DISABLED"));
        Serial.print(F("  Default target temperature: "));
        Serial.print(TEMP_DEFAULT_SETPOINT, 1);
        Serial.println(F(" °C"));
    #endif
}

// ============================================================================
// Update (Non-Blocking)
// ============================================================================

void SafetySystem::update() {
    // Process circulation delayed start
    processCirculationDelayedStart();
    
    // Monitor water level while circulation active
    if (circulationStarted) {
        monitorCirculationWaterLevel();
    }
    
    // Process heater auto-start
    if (heaterAutoStartPending) {
        processHeaterAutoStart();
    }
    
    // Process heater temperature control loop
    // Always runs (not just when heaterActive) to handle reactivation when temp drops below setpoint
    processHeaterTemperatureControl();
    
    // Detect faults (called automatically)
    detectFaults();
}

// ============================================================================
// Precondition Checking
// ============================================================================

bool SafetySystem::checkCirculationPreconditions() {
    // Preconditions: water level sufficient AND temp sensor operational AND no faults
    bool waterLevelOK = sensorManager.isWaterLevelSufficient();
    bool tempSensorOK = sensorManager.isTemperatureSensorOperational();
    bool noFaults = (stateMachine.getActiveFaults() == FAULT_NONE);
    
    #ifdef ENABLE_SERIAL_DEBUG
        if (!waterLevelOK || !tempSensorOK || !noFaults) {
            Serial.println(F("[SAFETY] Circulation preconditions NOT met:"));
            if (!waterLevelOK) Serial.println(F("  - Water level insufficient"));
            if (!tempSensorOK) Serial.println(F("  - Temperature sensor not operational"));
            if (!noFaults) {
                Serial.print(F("  - Active faults: 0x"));
                Serial.println(stateMachine.getActiveFaults(), HEX);
            }
        }
    #endif
    
    return waterLevelOK && tempSensorOK && noFaults;
}

bool SafetySystem::checkHeaterPreconditions() {
    // ABSOLUTE RULE: Circulation pump MUST be actively running
    if (!circulationStarted) {
        #ifdef ENABLE_SERIAL_DEBUG
            if (lastHeaterPreconditionsMet) {
                Serial.println(F("[SAFETY] Heater preconditions NOT met: Circulation not active (ABSOLUTE RULE)"));
            }
        #endif
        lastHeaterPreconditionsMet = false;
        return false;
    }
    
    // Check thermal runaway acknowledgment (Requirement 2.11, 19.5)
    if (stateMachine.hasFault(FAULT_THERMAL_RUNAWAY) && !thermalRunawayAcknowledged) {
        #ifdef ENABLE_SERIAL_DEBUG
            if (lastHeaterPreconditionsMet) {
                Serial.println(F("[SAFETY] Heater preconditions NOT met: Thermal runaway requires manual acknowledgment"));
            }
        #endif
        lastHeaterPreconditionsMet = false;
        return false;
    }
    
    // Additional preconditions
    float currentTemp = sensorManager.getTemperature();
    bool tempValid = (currentTemp != TEMP_INVALID_VALUE);
    bool waterLevelOK = sensorManager.isWaterLevelSufficient();
    bool tempBelowWarning = (currentTemp < TEMP_WARNING_THRESHOLD);
    bool noFaults = (stateMachine.getActiveFaults() == FAULT_NONE);
    bool allMet = tempValid && waterLevelOK && tempBelowWarning && noFaults;
    
    #ifdef ENABLE_SERIAL_DEBUG
        if (!allMet && lastHeaterPreconditionsMet) {
            Serial.println(F("[SAFETY] Heater preconditions NOT met:"));
            if (!tempValid) Serial.println(F("  - Temperature sensor reading invalid"));
            if (!waterLevelOK) Serial.println(F("  - Water level insufficient"));
            if (!tempBelowWarning) {
                Serial.print(F("  - Temperature above warning threshold ("));
                Serial.print(currentTemp, 1);
                Serial.print(F(" >= "));
                Serial.print(TEMP_WARNING_THRESHOLD, 1);
                Serial.println(F(" °C)"));
            }
            if (!noFaults) {
                Serial.print(F("  - Active faults: 0x"));
                Serial.println(stateMachine.getActiveFaults(), HEX);
            }
        } else if (allMet && !lastHeaterPreconditionsMet) {
            Serial.println(F("[SAFETY] Heater preconditions MET"));
        }
    #endif
    
    lastHeaterPreconditionsMet = allMet;
    return allMet;
}

bool SafetySystem::checkFeaturePreconditions() {
    // Preconditions: circulation pump MUST be active
    if (!circulationStarted) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Feature preconditions NOT met: Circulation not active"));
        #endif
        return false;
    }
    
    return true;
}

// ============================================================================
// Circulation Pump Control
// ============================================================================

bool SafetySystem::requestCirculationStart() {
    // Check preconditions
    if (!checkCirculationPreconditions()) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Circulation start request DENIED - preconditions not met"));
        #endif
        return false;
    }
    
    // Start delayed countdown
    circulationSelected = true;
    circulationSelectTime = millis();
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[SAFETY] Circulation start request ACCEPTED"));
        Serial.print(F("[SAFETY] Delayed start countdown: "));
        Serial.print(CIRCULATION_DELAYED_START_MS);
        Serial.println(F(" ms"));
    #endif
    
    return true;
}

void SafetySystem::cancelCirculationStart() {
    if (circulationSelected && !circulationStarted) {
        circulationSelected = false;
        
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Circulation start CANCELLED by user"));
        #endif
    }
}

void SafetySystem::requestCirculationStop() {
    if (circulationStarted) {
        deactivateCirculationPump();
    } else if (circulationSelected) {
        cancelCirculationStart();
    }
}

bool SafetySystem::isCirculationSelected() const {
    return circulationSelected && !circulationStarted;
}

bool SafetySystem::isCirculationStarted() const {
    return circulationStarted;
}

unsigned long SafetySystem::getCirculationCountdownRemaining() const {
    if (!circulationSelected || circulationStarted) {
        return 0;
    }
    
    unsigned long elapsed = millis() - circulationSelectTime;
    if (elapsed >= CIRCULATION_DELAYED_START_MS) {
        return 0;
    }
    
    return CIRCULATION_DELAYED_START_MS - elapsed;
}

void SafetySystem::processCirculationDelayedStart() {
    // Check if countdown complete
    if (circulationSelected && !circulationStarted) {
        unsigned long elapsed = millis() - circulationSelectTime;
        
        if (elapsed >= CIRCULATION_DELAYED_START_MS) {
            // Verify preconditions still met
            if (checkCirculationPreconditions()) {
                activateCirculationPump();
            } else {
                // Preconditions no longer met, cancel
                circulationSelected = false;
                
                #ifdef ENABLE_SERIAL_DEBUG
                    Serial.println(F("[SAFETY] Circulation start CANCELLED - preconditions no longer met"));
                #endif
            }
        }
    }
}

void SafetySystem::activateCirculationPump() {
    // Activate relay
    if (relayController.setRelay(RELAY_CHANNEL_CIRCULATION, true)) {
        circulationStarted = true;
        circulationSelected = false;
        circulationStartTime = millis();
        
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Circulation pump STARTED"));
        #endif
        
        // Start heater auto-start countdown if enabled
        if (heaterAutoStartEnabled && checkHeaterPreconditions()) {
            heaterAutoStartPending = true;
            heaterAutoStartTime = millis();
            
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SAFETY] Heater auto-start countdown STARTED"));
                Serial.print(F("[SAFETY] Auto-start delay: "));
                Serial.print(HEATER_AUTO_START_DELAY_MS);
                Serial.println(F(" ms"));
            #endif
        }
    } else {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] ERROR: Failed to activate circulation relay"));
        #endif
    }
}

void SafetySystem::deactivateCirculationPump() {
    // Deactivate heater first (if active)
    if (heaterActive) {
        deactivateHeater();
    }
    
    // Cancel heater auto-start if pending
    if (heaterAutoStartPending) {
        heaterAutoStartPending = false;
        
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Heater auto-start CANCELLED (circulation stopped)"));
        #endif
    }
    
    // Deactivate all dependent features
    deactivateAllDependentFeatures();
    
    // Deactivate circulation relay
    if (relayController.setRelay(RELAY_CHANNEL_CIRCULATION, false)) {
        circulationStarted = false;
        
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Circulation pump STOPPED"));
        #endif
    } else {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] ERROR: Failed to deactivate circulation relay"));
        #endif
    }
}

void SafetySystem::monitorCirculationWaterLevel() {
    // Check for water loss while circulation active
    if (!sensorManager.isWaterLevelSufficient()) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] CRITICAL: Water loss detected while circulation active"));
            Serial.println(F("[SAFETY] Executing safe shutdown..."));
        #endif
        
        // Set fault and trigger safe shutdown
        stateMachine.setFault(FAULT_LOW_WATER_LEVEL);
    }
}

// ============================================================================
// Water Heater Control
// ============================================================================

bool SafetySystem::requestHeaterStart() {
    // Check preconditions
    if (!checkHeaterPreconditions()) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Heater start request DENIED - preconditions not met"));
        #endif
        return false;
    }
    
    // Activate heater
    activateHeater();
    
    return true;
}

void SafetySystem::requestHeaterStop() {
    if (heaterActive) {
        deactivateHeater();
    }
}

bool SafetySystem::isHeaterActive() const {
    return heaterActive;
}

void SafetySystem::setUserTargetTemperature(float targetTemp) {
    userTargetTemperature = targetTemp;
    hasUserTargetTemperature = true;
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[SAFETY] User target temperature set: "));
        Serial.print(targetTemp, 1);
        Serial.println(F(" °C"));
    #endif
}

void SafetySystem::clearUserTargetTemperature() {
    hasUserTargetTemperature = false;
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[SAFETY] User target temperature cleared (using default)"));
    #endif
}

float SafetySystem::getTargetTemperature() const {
    if (hasUserTargetTemperature) {
        return userTargetTemperature;
    } else {
        return TEMP_DEFAULT_SETPOINT;
    }
}

bool SafetySystem::isHeaterAutoStartEnabled() const {
    return heaterAutoStartEnabled;
}

void SafetySystem::setHeaterAutoStart(bool enabled) {
    heaterAutoStartEnabled = enabled;
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[SAFETY] Heater auto-start: "));
        Serial.println(enabled ? F("ENABLED") : F("DISABLED"));
    #endif
}

void SafetySystem::processHeaterAutoStart() {
    unsigned long elapsed = millis() - heaterAutoStartTime;
    
    if (elapsed >= HEATER_AUTO_START_DELAY_MS) {
        // Verify preconditions still met
        if (checkHeaterPreconditions()) {
            activateHeater();
            
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SAFETY] Heater AUTO-STARTED"));
            #endif
        } else {
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SAFETY] Heater auto-start CANCELLED - preconditions no longer met"));
            #endif
        }
        
        heaterAutoStartPending = false;
    }
}

void SafetySystem::processHeaterTemperatureControl() {
    float currentTemp = sensorManager.getTemperature();
    float targetTemp = getTargetTemperature();
    
    if (currentTemp >= targetTemp) {
        // SP <= PV: target reached or exceeded - deactivate if active
        if (heaterActive) {
            deactivateHeater();

            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SAFETY] Heater deactivated - target temperature reached"));
                Serial.print(F("  Current: "));
                Serial.print(currentTemp, 1);
                Serial.print(F(" °C, Target: "));
                Serial.print(targetTemp, 1);
                Serial.println(F(" °C"));
            #endif
        }
    } else {
        // SP > PV: below target - activate if inactive and preconditions met
        if (!heaterActive && checkHeaterPreconditions()) {
            activateHeater();

            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SAFETY] Heater activated - temperature below target"));
                Serial.print(F("  Current: "));
                Serial.print(currentTemp, 1);
                Serial.print(F(" °C, Target: "));
                Serial.print(targetTemp, 1);
                Serial.println(F(" °C"));
            #endif
        }
    }
}

void SafetySystem::activateHeater() {
    // CRITICAL FIX: Check if current temperature is already at or above target
    // This prevents momentary heater activation when setpoint < current temperature
    float currentTemp = sensorManager.getTemperature();
    float targetTemp = getTargetTemperature();
    
    if (currentTemp >= targetTemp) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Heater activation SKIPPED - temperature already at or above target"));
            Serial.print(F("  Current: "));
            Serial.print(currentTemp, 1);
            Serial.print(F(" °C, Target: "));
            Serial.print(targetTemp, 1);
            Serial.println(F(" °C"));
        #endif
        
        // Clear auto-start pending flag
        heaterAutoStartPending = false;
        return;
    }
    
    if (relayController.setRelay(RELAY_CHANNEL_HEATER, true)) {
        heaterActive = true;
        heaterAutoStartPending = false;
        
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Water heater STARTED"));
            Serial.print(F("  Current temperature: "));
            Serial.print(currentTemp, 1);
            Serial.println(F(" °C"));
            Serial.print(F("  Target temperature: "));
            Serial.print(targetTemp, 1);
            Serial.println(F(" °C"));
        #endif
    } else {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] ERROR: Failed to activate heater relay"));
        #endif
    }
}

void SafetySystem::deactivateHeater() {
    if (relayController.setRelay(RELAY_CHANNEL_HEATER, false)) {
        heaterActive = false;
        
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] Water heater STOPPED"));
        #endif
    } else {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] ERROR: Failed to deactivate heater relay"));
        #endif
    }
}

// ============================================================================
// Feature Control
// ============================================================================

bool SafetySystem::requestFeatureStart(uint8_t channel) {
    // Check preconditions
    if (!checkFeaturePreconditions()) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[SAFETY] Feature start request DENIED (channel "));
            Serial.print(channel);
            Serial.println(F(") - circulation not active"));
        #endif
        return false;
    }
    
    // Activate feature relay
    if (relayController.setRelay(channel, true)) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[SAFETY] Feature STARTED (channel "));
            Serial.print(channel);
            Serial.println(F(")"));
        #endif
        return true;
    } else {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[SAFETY] ERROR: Failed to activate feature relay (channel "));
            Serial.print(channel);
            Serial.println(F(")"));
        #endif
        return false;
    }
}

void SafetySystem::requestFeatureStop(uint8_t channel) {
    if (relayController.setRelay(channel, false)) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[SAFETY] Feature STOPPED (channel "));
            Serial.print(channel);
            Serial.println(F(")"));
        #endif
    } else {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[SAFETY] ERROR: Failed to deactivate feature relay (channel "));
            Serial.print(channel);
            Serial.println(F(")"));
        #endif
    }
}

bool SafetySystem::isFeatureActive(uint8_t channel) const {
    return relayController.getRelayState(channel);
}

void SafetySystem::deactivateAllDependentFeatures() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[SAFETY] Deactivating all dependent features..."));
    #endif
    
    // Deactivate all features that depend on circulation
    relayController.setRelay(RELAY_CHANNEL_MASSAGE, false);
    relayController.setRelay(RELAY_CHANNEL_JET, false);
    relayController.setRelay(RELAY_CHANNEL_OZONE, false);
    relayController.setRelay(RELAY_CHANNEL_SPEAKER, false);
    relayController.setRelay(RELAY_CHANNEL_LIGHTS, false);
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[SAFETY] All dependent features deactivated"));
    #endif
}

// ============================================================================
// Thermal Runaway Detection
// ============================================================================

bool SafetySystem::detectThermalRunaway() {
    // Only check periodically (every 2 seconds)
    unsigned long currentTime = millis();
    if (currentTime - lastThermalRunawayCheck < TEMP_SENSOR_READ_INTERVAL_MS) {
        return false;
    }
    lastThermalRunawayCheck = currentTime;
    
    // Calculate temperature delta and rate
    float delta = calculateTemperatureDelta();
    float rate = calculateTemperatureRate();
    
    // Check thresholds
    bool deltaExceeded = (delta > TEMP_DELTA_THRESHOLD);
    bool rateExceeded = (rate > TEMP_RATE_THRESHOLD);
    
    if (deltaExceeded || rateExceeded) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[SAFETY] THERMAL RUNAWAY DETECTED"));
            Serial.print(F("  Temperature delta: "));
            Serial.print(delta, 2);
            Serial.print(F(" °C (threshold: "));
            Serial.print(TEMP_DELTA_THRESHOLD, 2);
            Serial.println(F(" °C)"));
            Serial.print(F("  Rate of change: "));
            Serial.print(rate, 3);
            Serial.print(F(" °C/s (threshold: "));
            Serial.print(TEMP_RATE_THRESHOLD, 3);
            Serial.println(F(" °C/s)"));
        #endif
        
        return true;
    }
    
    return false;
}

void SafetySystem::acknowledgeThermalRunaway() {
    thermalRunawayAcknowledged = true;
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("[SAFETY] Thermal runaway ACKNOWLEDGED by user"));
    #endif
    
    // Clear thermal runaway fault
    stateMachine.clearFault(FAULT_THERMAL_RUNAWAY);
}

bool SafetySystem::isThermalRunawayAcknowledgmentRequired() const {
    return stateMachine.hasFault(FAULT_THERMAL_RUNAWAY) && !thermalRunawayAcknowledged;
}

float SafetySystem::calculateTemperatureDelta() {
    // Get temperature history count
    uint8_t historyCount = sensorManager.getTemperatureHistoryCount();
    
    if (historyCount < 2) {
        return 0.0f;  // Not enough history
    }
    
    // Calculate delta between oldest and newest temperatures
    float oldest = sensorManager.getOldestTemperature();
    float newest = sensorManager.getNewestTemperature();
    
    // Return absolute delta
    return abs(newest - oldest);
}

float SafetySystem::calculateTemperatureRate() {
    // Get temperature history count
    uint8_t historyCount = sensorManager.getTemperatureHistoryCount();
    
    if (historyCount < 2) {
        return 0.0f;  // Not enough history
    }
    
    // Calculate delta
    float delta = calculateTemperatureDelta();
    
    // Calculate time window (history count * read interval)
    float timeWindowSeconds = (historyCount * TEMP_SENSOR_READ_INTERVAL_MS) / 1000.0f;
    
    // Calculate rate (°C/second)
    if (timeWindowSeconds > 0) {
        return delta / timeWindowSeconds;
    }
    
    return 0.0f;
}

// ============================================================================
// Fault Detection
// ============================================================================

void SafetySystem::detectFaults() {
    detectLowWaterLevelFault();
    detectHighTemperatureFault();
    detectThermalRunawayFault();
    detectTemperatureSensorFault();
    detectI2CFault();
    detectPCF8574Fault();
}

bool SafetySystem::hasAnyFault() const {
    return (stateMachine.getActiveFaults() != FAULT_NONE);
}

void SafetySystem::detectLowWaterLevelFault() {
    // Check if water level fault active in sensor manager
    if (sensorManager.hasWaterLevelFault()) {
        // Set fault if not already set
        if (!stateMachine.hasFault(FAULT_LOW_WATER_LEVEL)) {
            stateMachine.setFault(FAULT_LOW_WATER_LEVEL);
            
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SAFETY] FAULT DETECTED: Low water level"));
            #endif
        }
    }
}

void SafetySystem::detectHighTemperatureFault() {
    float currentTemp = sensorManager.getTemperature();
    
    // Check for critical overtemperature
    if (currentTemp >= TEMP_CRITICAL_THRESHOLD) {
        if (!stateMachine.hasFault(FAULT_HIGH_TEMPERATURE)) {
            stateMachine.setFault(FAULT_HIGH_TEMPERATURE);
            
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SAFETY] FAULT DETECTED: Critical overtemperature"));
                Serial.print(F("  Temperature: "));
                Serial.print(currentTemp, 1);
                Serial.print(F(" °C (threshold: "));
                Serial.print(TEMP_CRITICAL_THRESHOLD, 1);
                Serial.println(F(" °C)"));
            #endif
            
            // Deactivate heater immediately
            if (heaterActive) {
                deactivateHeater();
            }
        }
    }
}

void SafetySystem::detectThermalRunawayFault() {
    // Only check if heater is active
    if (heaterActive) {
        if (detectThermalRunaway()) {
            if (!stateMachine.hasFault(FAULT_THERMAL_RUNAWAY)) {
                stateMachine.setFault(FAULT_THERMAL_RUNAWAY);
                thermalRunawayAcknowledged = false;  // Requires manual acknowledgment
                
                // Deactivate heater immediately
                deactivateHeater();
            }
        }
    }
}

void SafetySystem::detectTemperatureSensorFault() {
    // Check if temperature sensor fault active in sensor manager
    if (sensorManager.hasTemperatureSensorFault()) {
        if (!stateMachine.hasFault(FAULT_TEMPERATURE_SENSOR)) {
            stateMachine.setFault(FAULT_TEMPERATURE_SENSOR);
            
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SAFETY] FAULT DETECTED: Temperature sensor failure"));
            #endif
        }
    }
}

void SafetySystem::detectI2CFault() {
    // I2C fault detection will be implemented when I2C bus manager provides status
    // For now, this is a placeholder
}

void SafetySystem::detectPCF8574Fault() {
    // Check if PCF8574 is responding
    if (!relayController.isPCF8574Responding()) {
        if (!stateMachine.hasFault(FAULT_PCF8574_FAILURE)) {
            stateMachine.setFault(FAULT_PCF8574_FAILURE);
            
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SAFETY] FAULT DETECTED: PCF8574 relay controller failure"));
            #endif
        }
    }
}

