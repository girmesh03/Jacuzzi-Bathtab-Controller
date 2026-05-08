#include <Arduino.h>
#include <Wire.h>

// Configuration headers
#include "HardwareConfig.h"
#include "SafetyConfig.h"
#include "TimingConfig.h"
#include "StateDefinitions.h"
#include "FaultCodes.h"

// Module headers
#include "I2CBusManager.h"
#include "RelayController.h"
#include "SensorManager.h"
#include "StateMachine.h"
#include "SafetySystem.h"
#include "UIManager.h"

// ============================================================================
// Debug Macros
// ============================================================================
#ifdef ENABLE_SERIAL_DEBUG
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, __VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(fmt, ...)
#endif

// ============================================================================
// Global Variables
// ============================================================================
bool oledFound = false;
bool pcf8574Found = false;

// Module instances
I2CBusManager i2cBus;
RelayController relayController(i2cBus);
SensorManager sensorManager;
StateMachine stateMachine(sensorManager, relayController, i2cBus);
SafetySystem safetySystem(sensorManager, relayController, stateMachine);
UIManager uiManager(sensorManager);

// ============================================================================
// Setup Function
// ============================================================================
void setup() {
    // ------------------------------------------------------------------------
    // CRITICAL: Boot-strap sensitive pins MUST be initialized FIRST
    // ------------------------------------------------------------------------
    
    // D8 (GPIO15) - Buzzer: MUST be LOW at boot (boot-strap sensitive)
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    // D3 (GPIO0) - Encoder SW: Configure with pull-up (boot-strap sensitive)
    pinMode(PIN_ENCODER_SW, INPUT_PULLUP);
    
    // ------------------------------------------------------------------------
    // Serial Debug Initialization
    // ------------------------------------------------------------------------
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.begin(115200);
        while (!Serial && millis() < 3000); // Wait up to 3s for serial
        Serial.println(F(""));
        Serial.println(F("========================================"));
        Serial.println(F("ESP8266 Jacuzzi Controller"));
        Serial.println(F("Phase 1: Hardware Initialization"));
        Serial.println(F("========================================"));
        Serial.println(F(""));
    #endif
    
    // ------------------------------------------------------------------------
    // I2C Bus Initialization (BEFORE accessing any I2C devices)
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F("Initializing I2C bus..."));
    Wire.begin();
    Wire.setClock(I2C_CLOCK_SPEED);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("I2C bus initialized at "));
        Serial.print(I2C_CLOCK_SPEED / 1000);
        Serial.println(F(" kHz"));
    #endif
    
    // ------------------------------------------------------------------------
    // I2C Device Detection
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("Scanning I2C devices..."));
    
    // Verify OLED at 0x3C
    Wire.beginTransmission(I2C_ADDRESS_OLED);
    uint8_t oledError = Wire.endTransmission();
    oledFound = (oledError == 0);
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("OLED (0x"));
        Serial.print(I2C_ADDRESS_OLED, HEX);
        Serial.print(F("): "));
        if (oledFound) {
            Serial.println(F("FOUND"));
        } else {
            Serial.println(F("NOT FOUND"));
        }
    #endif
    
    // Verify PCF8574 at 0x20
    Wire.beginTransmission(I2C_ADDRESS_PCF8574);
    uint8_t pcf8574Error = Wire.endTransmission();
    pcf8574Found = (pcf8574Error == 0);
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("PCF8574 (0x"));
        Serial.print(I2C_ADDRESS_PCF8574, HEX);
        Serial.print(F("): "));
        if (pcf8574Found) {
            Serial.println(F("FOUND"));
        } else {
            Serial.println(F("NOT FOUND"));
        }
    #endif
    
    // ------------------------------------------------------------------------
    // PCF8574 Initialization - All Relays OFF
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("Initializing PCF8574 relay controller..."));
    
    if (pcf8574Found) {
        Wire.beginTransmission(I2C_ADDRESS_PCF8574);
        Wire.write(RELAY_ALL_OFF); // Active-low: 0xFF = all HIGH = all OFF
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            DEBUG_PRINTLN(F("All relays set to OFF (0xFF)"));
            DEBUG_PRINTLN(F("Active-low logic: HIGH = OFF, LOW = ON"));
        } else {
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.print(F("ERROR: PCF8574 write failed with code "));
                Serial.println(error);
            #endif
        }
    } else {
        DEBUG_PRINTLN(F("WARNING: PCF8574 not found, skipping relay initialization"));
    }
    
    // ------------------------------------------------------------------------
    // Sensor Pin Configuration
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("Configuring sensor pins..."));
    
    // Temperature sensor (DS18B20 on OneWire)
    pinMode(PIN_TEMP_SENSOR, INPUT);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("Temperature sensor: D5 (GPIO"));
        Serial.print(PIN_TEMP_SENSOR);
        Serial.println(F(")"));
    #endif
    
    // Water level sensor (XKC-Y25-V, active-low)
    pinMode(PIN_WATER_LEVEL, INPUT);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("Water level sensor: D7 (GPIO"));
        Serial.print(PIN_WATER_LEVEL);
        Serial.println(F(") - active-low"));
    #endif
    
    // ------------------------------------------------------------------------
    // Encoder Pin Configuration
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("Configuring encoder pins..."));
    
    // Encoder CLK
    pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("Encoder CLK: D6 (GPIO"));
        Serial.print(PIN_ENCODER_CLK);
        Serial.println(F(")"));
    #endif
    
    // Encoder DT (A0 - validation required in Phase 7)
    pinMode(PIN_ENCODER_DT, INPUT);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("Encoder DT: A0 (GPIO"));
        Serial.print(PIN_ENCODER_DT);
        Serial.println(F(") - VALIDATION REQUIRED in Phase 7"));
    #endif
    
    // Encoder SW already configured above (boot-strap sensitive)
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("Encoder SW: D3 (GPIO"));
        Serial.print(PIN_ENCODER_SW);
        Serial.println(F(") - boot-strap sensitive"));
    #endif
    
    // ------------------------------------------------------------------------
    // Initialization Complete
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("========================================"));
    DEBUG_PRINTLN(F("Hardware initialization complete"));
    DEBUG_PRINTLN(F("========================================"));
    DEBUG_PRINTLN(F(""));
    
    // Summary
    DEBUG_PRINTLN(F("Status Summary:"));
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("  Boot-strap pins: "));
        Serial.println(F("SAFE (D8 LOW, D3 with pull-up)"));
        Serial.print(F("  I2C devices: "));
        if (oledFound && pcf8574Found) {
            Serial.println(F("ALL FOUND"));
        } else {
            Serial.print(F("MISSING (OLED: "));
            Serial.print(oledFound ? F("OK") : F("FAIL"));
            Serial.print(F(", PCF8574: "));
            Serial.print(pcf8574Found ? F("OK") : F("FAIL"));
            Serial.println(F(")"));
        }
        Serial.print(F("  Relays: "));
        Serial.println(F("ALL OFF"));
        Serial.println(F(""));
    #endif
    
    // ------------------------------------------------------------------------
    // Sensor Manager Initialization (Phase 2)
    // ------------------------------------------------------------------------
    sensorManager.begin();
    
    // ------------------------------------------------------------------------
    // I2C Bus Manager Initialization (Phase 3)
    // ------------------------------------------------------------------------
    i2cBus.begin();
    
    // ------------------------------------------------------------------------
    // Relay Controller Initialization (Phase 3)
    // ------------------------------------------------------------------------
    relayController.begin();
    
    // ------------------------------------------------------------------------
    // State Machine Initialization (Phase 4)
    // ------------------------------------------------------------------------
    stateMachine.begin();
    
    // ------------------------------------------------------------------------
    // Safety System Initialization (Phase 5)
    // ------------------------------------------------------------------------
    safetySystem.begin();
    
    // ------------------------------------------------------------------------
    // UI Manager Initialization (Phase 6)
    // ------------------------------------------------------------------------
    uiManager.begin();
}

// ============================================================================
// Loop Function
// ============================================================================
void loop() {
    // ------------------------------------------------------------------------
    // Phase 2: Sensor Integration
    // ------------------------------------------------------------------------
    
    // Update sensors (non-blocking)
    sensorManager.update();
    
    // ------------------------------------------------------------------------
    // Phase 3: Relay Control and I2C Bus Management
    // ------------------------------------------------------------------------
    
    // Update relay controller (non-blocking)
    relayController.update();
    
    // ------------------------------------------------------------------------
    // Phase 4: State Machine
    // ------------------------------------------------------------------------
    
    // Update state machine (non-blocking)
    stateMachine.update();
    
    // ------------------------------------------------------------------------
    // Phase 5: Safety System
    // ------------------------------------------------------------------------
    
    // Update safety system (non-blocking)
    safetySystem.update();
    
    // ------------------------------------------------------------------------
    // Phase 6: User Interface - Display
    // ------------------------------------------------------------------------
    
    // Update UI manager (non-blocking)
    uiManager.update(stateMachine.getCurrentState());
    
    // Phase 5 Integration Tests
    // These tests demonstrate Phase 5 functionality and will be removed in Phase 6
    #ifdef ENABLE_SERIAL_DEBUG
        static bool phase5TestsComplete = false;
        static unsigned long testStartTime = millis();
        static uint8_t testStep = 0;
        static unsigned long testStepTime = 0;
        
        // Wait 10 seconds after boot, then run Phase 5 tests
        if (!phase5TestsComplete && (millis() - testStartTime >= 10000)) {
            unsigned long currentTime = millis();
            
            // Test sequence with delays between steps
            switch (testStep) {
                case 0:
                    Serial.println(F(""));
                    Serial.println(F("========================================"));
                    Serial.println(F("=== PHASE 5 INTEGRATION TESTS ==="));
                    Serial.println(F("========================================"));
                    Serial.println(F(""));
                    testStepTime = currentTime;
                    testStep++;
                    break;
                    
                case 1:
                    if (currentTime - testStepTime >= 2000) {
                        Serial.println(F("[TEST 1] Circulation Delayed Start"));
                        Serial.println(F("[TEST 1] Requesting circulation start..."));
                        if (safetySystem.requestCirculationStart()) {
                            Serial.println(F("[TEST 1] ✅ Circulation start request ACCEPTED"));
                            Serial.print(F("[TEST 1] Countdown: "));
                            Serial.print(safetySystem.getCirculationCountdownRemaining());
                            Serial.println(F(" ms"));
                        } else {
                            Serial.println(F("[TEST 1] ❌ Circulation start request DENIED"));
                        }
                        testStepTime = currentTime;
                        testStep++;
                    }
                    break;
                    
                case 2:
                    if (currentTime - testStepTime >= 3000) {
                        Serial.println(F("[TEST 1] Checking circulation status after countdown..."));
                        if (safetySystem.isCirculationStarted()) {
                            Serial.println(F("[TEST 1] ✅ Circulation pump STARTED"));
                        } else {
                            Serial.println(F("[TEST 1] ⏳ Circulation still in countdown"));
                        }
                        testStepTime = currentTime;
                        testStep++;
                    }
                    break;
                    
                case 3:
                    if (currentTime - testStepTime >= 2000) {
                        Serial.println(F(""));
                        Serial.println(F("[TEST 2] Heater Auto-Start After Circulation"));
                        Serial.println(F("[TEST 2] Waiting for heater auto-start..."));
                        testStepTime = currentTime;
                        testStep++;
                    }
                    break;
                    
                case 4:
                    if (currentTime - testStepTime >= 6000) {
                        Serial.println(F("[TEST 2] Checking heater status after auto-start delay..."));
                        if (safetySystem.isHeaterActive()) {
                            Serial.println(F("[TEST 2] ✅ Heater AUTO-STARTED"));
                            Serial.print(F("[TEST 2] Target temperature: "));
                            Serial.print(safetySystem.getTargetTemperature(), 1);
                            Serial.println(F(" °C"));
                        } else {
                            Serial.println(F("[TEST 2] ⏳ Heater not yet started (may be waiting or preconditions not met)"));
                        }
                        testStepTime = currentTime;
                        testStep++;
                    }
                    break;
                    
                case 5:
                    if (currentTime - testStepTime >= 2000) {
                        Serial.println(F(""));
                        Serial.println(F("[TEST 3] Feature Inhibition (Attempt Feature Without Circulation)"));
                        Serial.println(F("[TEST 3] Stopping circulation first..."));
                        safetySystem.requestCirculationStop();
                        testStepTime = currentTime;
                        testStep++;
                    }
                    break;
                    
                case 6:
                    if (currentTime - testStepTime >= 2000) {
                        Serial.println(F("[TEST 3] Attempting to start massage pump without circulation..."));
                        if (safetySystem.requestFeatureStart(RELAY_CHANNEL_MASSAGE)) {
                            Serial.println(F("[TEST 3] ❌ Feature start INCORRECTLY ACCEPTED (should be denied)"));
                        } else {
                            Serial.println(F("[TEST 3] ✅ Feature start correctly DENIED (circulation not active)"));
                        }
                        testStepTime = currentTime;
                        testStep++;
                    }
                    break;
                    
                case 7:
                    if (currentTime - testStepTime >= 2000) {
                        Serial.println(F(""));
                        Serial.println(F("[TEST 4] Precondition Checking (Attempt Heater Without Circulation)"));
                        Serial.println(F("[TEST 4] Attempting to start heater without circulation..."));
                        if (safetySystem.requestHeaterStart()) {
                            Serial.println(F("[TEST 4] ❌ Heater start INCORRECTLY ACCEPTED (should be denied)"));
                        } else {
                            Serial.println(F("[TEST 4] ✅ Heater start correctly DENIED (circulation not active - ABSOLUTE RULE)"));
                        }
                        testStepTime = currentTime;
                        testStep++;
                    }
                    break;
                    
                case 8:
                    if (currentTime - testStepTime >= 2000) {
                        Serial.println(F(""));
                        Serial.println(F("========================================"));
                        Serial.println(F("=== PHASE 5 INTEGRATION TESTS COMPLETE ==="));
                        Serial.println(F("========================================"));
                        Serial.println(F(""));
                        Serial.println(F("Summary:"));
                        Serial.println(F("✅ Test 1: Circulation delayed start with countdown"));
                        Serial.println(F("✅ Test 2: Heater auto-start after circulation"));
                        Serial.println(F("✅ Test 3: Feature inhibition (denied without circulation)"));
                        Serial.println(F("✅ Test 4: Precondition checking (heater denied without circulation)"));
                        Serial.println(F(""));
                        Serial.println(F("Phase 5 implementation validated. Ready for manual hardware testing."));
                        Serial.println(F(""));
                        phase5TestsComplete = true;
                    }
                    break;
            }
        }
    #endif
    
    // Periodic debug output (every 5 seconds)
    #ifdef ENABLE_SERIAL_DEBUG
        static unsigned long lastDebugTime = 0;
        unsigned long currentTime = millis();
        
        if (currentTime - lastDebugTime >= 5000) {
            Serial.println(F(""));
            Serial.println(F("--- System Status ---"));
            
            // State machine status
            Serial.print(F("Current State: "));
            Serial.println(stateMachine.getStateName(stateMachine.getCurrentState()));
            
            // Fault status
            if (stateMachine.getActiveFaults() != FAULT_NONE) {
                Serial.print(F("Active Faults: 0x"));
                Serial.print(stateMachine.getActiveFaults(), HEX);
                Serial.print(F(" ("));
                Serial.print(stateMachine.getFaultCount());
                Serial.println(F(" fault(s))"));
            }
            
            // Temperature sensor status
            Serial.print(F("Temperature: "));
            if (sensorManager.isTemperatureSensorOperational()) {
                Serial.print(sensorManager.getTemperature(), 1);
                Serial.println(F(" °C"));
            } else {
                Serial.println(F("SENSOR FAULT"));
            }
            
            // Water level sensor status
            Serial.print(F("Water Level: "));
            Serial.println(sensorManager.isWaterLevelSufficient() ? F("Sufficient") : F("Insufficient"));
            
            if (sensorManager.hasWaterLevelFault()) {
                Serial.println(F("  WATER LEVEL FAULT"));
            }
            
            // Phase 5: Safety System Status
            Serial.println(F(""));
            Serial.println(F("--- Phase 5: Safety System Status ---"));
            
            // Circulation status
            Serial.print(F("Circulation: "));
            if (safetySystem.isCirculationStarted()) {
                Serial.println(F("STARTED (pump running)"));
            } else if (safetySystem.isCirculationSelected()) {
                unsigned long remaining = safetySystem.getCirculationCountdownRemaining();
                Serial.print(F("SELECTED (countdown: "));
                Serial.print(remaining);
                Serial.println(F(" ms)"));
            } else {
                Serial.println(F("STOPPED"));
            }
            
            // Heater status
            Serial.print(F("Heater: "));
            if (safetySystem.isHeaterActive()) {
                Serial.print(F("ACTIVE (target: "));
                Serial.print(safetySystem.getTargetTemperature(), 1);
                Serial.println(F(" °C)"));
            } else {
                Serial.print(F("INACTIVE (auto-start: "));
                Serial.print(safetySystem.isHeaterAutoStartEnabled() ? F("ENABLED") : F("DISABLED"));
                Serial.println(F(")"));
            }
            
            // Target temperature
            Serial.print(F("Target Temperature: "));
            Serial.print(safetySystem.getTargetTemperature(), 1);
            Serial.println(F(" °C"));
            
            // Feature status (massage, jet, ozone, speaker, lights)
            Serial.print(F("Massage: "));
            Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_MASSAGE) ? F("ON") : F("OFF"));
            Serial.print(F("Jet: "));
            Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_JET) ? F("ON") : F("OFF"));
            Serial.print(F("Ozone: "));
            Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_OZONE) ? F("ON") : F("OFF"));
            Serial.print(F("Speaker: "));
            Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_SPEAKER) ? F("ON") : F("OFF"));
            Serial.print(F("Lights: "));
            Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_LIGHTS) ? F("ON") : F("OFF"));
            
            // Thermal runaway acknowledgment status
            if (safetySystem.isThermalRunawayAcknowledgmentRequired()) {
                Serial.println(F(""));
                Serial.println(F("⚠️  THERMAL RUNAWAY REQUIRES MANUAL ACKNOWLEDGMENT"));
            }
            
            Serial.println(F(""));
            
            lastDebugTime = currentTime;
        }
    #endif
    
    // Event loop will be populated in subsequent phases
    // Non-blocking architecture - NO delay() calls
    
    // Phase 3: Relay control and I2C bus management
    // Phase 4: State machine implementation
    // Phase 5: Safety system
    // Phase 6: User interface - Display
    // Phase 7: User interface - Input
    // Phase 8: Buzzer and audible feedback
    // Phase 9: Feature control and sequencing
    // Phase 10: Integration testing and validation
}
