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
#include "InputHandler.h"

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
UIManager uiManager(sensorManager, safetySystem, stateMachine);
InputHandler inputHandler;

// ============================================================================
// Setup Function
// ============================================================================
void setup() {
    // ========================================================================
    // CRITICAL: Boot-strap sensitive pins MUST be initialized FIRST
    // ========================================================================
    
    // D8 (GPIO15) - Buzzer: MUST be LOW at boot (boot-strap sensitive)
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    // D3 (GPIO0) - Encoder SW: Configure with pull-up (boot-strap sensitive)
    pinMode(PIN_ENCODER_SW, INPUT_PULLUP);
    
    // ========================================================================
    // CRITICAL: Enter Boot State BEFORE Hardware Initialization
    // Requirements 8.2, 8.3: Boot state is initial state DURING hardware init
    // ========================================================================
    
    // Initialize state machine - enters Boot state FIRST
    stateMachine.begin();
    
    // ========================================================================
    // Hardware Initialization WHILE IN Boot State
    // ========================================================================
    
    // ------------------------------------------------------------------------
    // Serial Debug Initialization
    // ------------------------------------------------------------------------
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.begin(SERIAL_BAUD_RATE);
        while (!Serial && millis() < SERIAL_WAIT_TIMEOUT_MS);
        Serial.println(F(""));
        Serial.println(F("========================================"));
        Serial.println(F("ESP8266 Jacuzzi Controller"));
        Serial.println(F("Boot State: Hardware Initialization"));
        Serial.println(F("========================================"));
        Serial.println(F(""));
    #endif
    
    // ------------------------------------------------------------------------
    // I2C Bus Initialization (BEFORE accessing any I2C devices)
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F("[BOOT] Initializing I2C bus..."));
    Wire.begin();
    Wire.setClock(I2C_CLOCK_SPEED);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[BOOT] I2C bus initialized at "));
        Serial.print(I2C_CLOCK_SPEED / 1000);
        Serial.println(F(" kHz"));
    #endif
    
    // ------------------------------------------------------------------------
    // I2C Device Detection
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("[BOOT] Scanning I2C devices..."));
    
    // Verify OLED at 0x3C
    Wire.beginTransmission(I2C_ADDRESS_OLED);
    uint8_t oledError = Wire.endTransmission();
    oledFound = (oledError == 0);
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[BOOT] OLED (0x"));
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
        Serial.print(F("[BOOT] PCF8574 (0x"));
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
    DEBUG_PRINTLN(F("[BOOT] Initializing PCF8574 relay controller..."));
    
    if (pcf8574Found) {
        Wire.beginTransmission(I2C_ADDRESS_PCF8574);
        Wire.write(RELAY_ALL_OFF); // Active-low: 0xFF = all HIGH = all OFF
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            DEBUG_PRINTLN(F("[BOOT] All relays set to OFF (0xFF)"));
            DEBUG_PRINTLN(F("[BOOT] Active-low logic: HIGH = OFF, LOW = ON"));
        } else {
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.print(F("[BOOT] ERROR: PCF8574 write failed with code "));
                Serial.println(error);
            #endif
        }
    } else {
        DEBUG_PRINTLN(F("[BOOT] WARNING: PCF8574 not found, skipping relay initialization"));
    }
    
    // ------------------------------------------------------------------------
    // Sensor Pin Configuration
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("[BOOT] Configuring sensor pins..."));
    
    // Temperature sensor (DS18B20 on OneWire)
    pinMode(PIN_TEMP_SENSOR, INPUT);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[BOOT] Temperature sensor: D5 (GPIO"));
        Serial.print(PIN_TEMP_SENSOR);
        Serial.println(F(")"));
    #endif
    
    // Water level sensor (XKC-Y25-V, active-low)
    pinMode(PIN_WATER_LEVEL, INPUT);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[BOOT] Water level sensor: D7 (GPIO"));
        Serial.print(PIN_WATER_LEVEL);
        Serial.println(F(") - active-low"));
    #endif
    
    // ------------------------------------------------------------------------
    // Encoder Pin Configuration
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("[BOOT] Configuring encoder pins..."));
    
    // Encoder CLK
    pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[BOOT] Encoder CLK: D6 (GPIO"));
        Serial.print(PIN_ENCODER_CLK);
        Serial.println(F(")"));
    #endif
    
    // Encoder DT (D4/GPIO2 - D0 reserved for heartbeat LED, D1/D2 are I2C, A0 unreliable as digital input)
    pinMode(PIN_ENCODER_DT, INPUT);
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[BOOT] Encoder DT: D4 (GPIO"));
        Serial.print(PIN_ENCODER_DT);
        Serial.println(F(")"));
    #endif
    
    // Encoder SW already configured above (boot-strap sensitive)
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[BOOT] Encoder SW: D3 (GPIO"));
        Serial.print(PIN_ENCODER_SW);
        Serial.println(F(") - boot-strap sensitive"));
    #endif
    
    // ------------------------------------------------------------------------
    // Module Initialization (WHILE IN Boot State)
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("[BOOT] Initializing modules..."));
    
    // Sensor Manager Initialization (Phase 2)
    sensorManager.begin();
    
    // I2C Bus Manager Initialization (Phase 3)
    i2cBus.begin();
    
    // Relay Controller Initialization (Phase 3)
    relayController.begin();
    
    // Safety System Initialization (Phase 5)
    safetySystem.begin();
    
    // UI Manager Initialization (Phase 6)
    uiManager.begin();
    
    // Input Handler Initialization (Phase 7 - Minimal Support for Phase 6 Testing)
    inputHandler.begin();
    
    // ------------------------------------------------------------------------
    // Hardware Initialization Complete
    // ------------------------------------------------------------------------
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("========================================"));
    DEBUG_PRINTLN(F("[BOOT] Hardware initialization complete"));
    DEBUG_PRINTLN(F("========================================"));
    DEBUG_PRINTLN(F(""));
    
    // Summary
    DEBUG_PRINTLN(F("[BOOT] Status Summary:"));
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("[BOOT]   Boot-strap pins: "));
        Serial.println(F("SAFE (D8 LOW, D3 with pull-up)"));
        Serial.print(F("[BOOT]   I2C devices: "));
        if (oledFound && pcf8574Found) {
            Serial.println(F("ALL FOUND"));
        } else {
            Serial.print(F("MISSING (OLED: "));
            Serial.print(oledFound ? F("OK") : F("FAIL"));
            Serial.print(F(", PCF8574: "));
            Serial.print(pcf8574Found ? F("OK") : F("FAIL"));
            Serial.println(F(")"));
        }
        Serial.print(F("[BOOT]   Relays: "));
        Serial.println(F("ALL OFF"));
        Serial.println(F(""));
    #endif
    
    // ========================================================================
    // Signal Hardware Initialization Complete to State Machine
    // State machine will detect this in loop() and transition Boot → Self_Check
    // ========================================================================
    stateMachine.setHardwareInitComplete();
    
    DEBUG_PRINTLN(F("[BOOT] Signaling hardware init complete to state machine"));
    DEBUG_PRINTLN(F("[BOOT] State machine will transition Boot → Self_Check in loop()"));
    DEBUG_PRINTLN(F(""));
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
    
    // ------------------------------------------------------------------------
    // Phase 7: User Interface - Input (Minimal Support for Phase 6 Testing)
    // ------------------------------------------------------------------------
    
    // Update input handler (non-blocking)
    inputHandler.update();
    
    // Handle encoder input based on current UI state
    UIState currentUIState = uiManager.getCurrentUIState();
    
    // Ready UI → Main Menu (button press)
    if (currentUIState == UI_READY && inputHandler.wasButtonPressed()) {
        // Transition to Main Menu UI
        DEBUG_PRINTLN(F("[INPUT] Button pressed in Ready UI - transitioning to Main Menu"));
        uiManager.forceUIState(UI_MAIN_MENU);
    }
    
    // Main Menu (only one item: Start circulation)
    if (currentUIState == UI_MAIN_MENU) {
        // Consume stale encoder events to prevent leaking into Circulation UI
        inputHandler.wasRotatedCW();
        inputHandler.wasRotatedCCW();
        
        // Long press - return to Ready UI
        if (inputHandler.isButtonHeld()) {
            DEBUG_PRINTLN(F("[INPUT] Button HELD in Main Menu - returning to Ready UI"));
            uiManager.forceUIState(UI_READY);
        }
        // Button press - enter Circulation UI
        else if (inputHandler.wasButtonPressed()) {
            DEBUG_PRINTLN(F("[INPUT] Button pressed in Main Menu - entering Circulation UI"));
            uiManager.selectMainMenuItem();
        }
    }
    
    // Circulation UI navigation
    if (currentUIState == UI_CIRCULATION) {
        // Long press - return to Ready UI
        if (inputHandler.isButtonHeld()) {
            DEBUG_PRINTLN(F("[INPUT] Button HELD in Circulation UI - returning to Ready UI"));
            
            // Stop circulation if active
            if (safetySystem.isCirculationStarted()) {
                DEBUG_PRINTLN(F("[INPUT] Stopping circulation..."));
                safetySystem.requestCirculationStop();
            } else if (safetySystem.isCirculationSelected()) {
                safetySystem.cancelCirculationStart();
            }
            
            uiManager.forceUIState(UI_READY);
        }
        // CRITICAL FIX: Hardware encoder CLK/DT pins are physically reversed
        // When user rotates CW physically, hardware reports CCW (and vice versa)
        // So we swap the direction values to compensate
        
        // Rotary encoder navigation (up/down through 8 items)
        else if (inputHandler.wasRotatedCW()) {
            // Hardware reports CW, but user actually rotated CCW physically
            // Check if on thermometer screen (index 7)
            if (uiManager.getCirculationMenuSelectedIndex() == 7 && uiManager.isTemperatureAdjustmentActive()) {
                // Adjusting temperature - user rotated CCW physically = decrease
                uiManager.adjustTemperature(-1);
                DEBUG_PRINTLN(F("[INPUT] Rotary CW (HW) / CCW (Physical) - Temperature decrease"));
            } else {
                // Normal menu navigation - user rotated CCW physically = backward
                uiManager.navigateCirculationMenu(-1);
                DEBUG_PRINTLN(F("[INPUT] Rotary CW (HW) / CCW (Physical) - Circulation Menu navigate backward"));
            }
        } else if (inputHandler.wasRotatedCCW()) {
            // Hardware reports CCW, but user actually rotated CW physically
            // Check if on thermometer screen (index 7)
            if (uiManager.getCirculationMenuSelectedIndex() == 7 && uiManager.isTemperatureAdjustmentActive()) {
                // Adjusting temperature - user rotated CW physically = increase
                uiManager.adjustTemperature(1);
                DEBUG_PRINTLN(F("[INPUT] Rotary CCW (HW) / CW (Physical) - Temperature increase"));
            } else {
                // Normal menu navigation - user rotated CW physically = forward
                uiManager.navigateCirculationMenu(1);
                DEBUG_PRINTLN(F("[INPUT] Rotary CCW (HW) / CW (Physical) - Circulation Menu navigate forward"));
            }
        }
        
        // Button press - toggle selected item or confirm temperature
        if (inputHandler.wasButtonPressed()) {
            uint8_t selectedIndex = uiManager.getCirculationMenuSelectedIndex();
            DEBUG_PRINT(F("[INPUT] Button pressed in Circulation UI - selected index: "));
            DEBUG_PRINTLN(selectedIndex);
            
            // Handle item toggle based on selected index
            switch (selectedIndex) {
                case 0:
                    // Circulation pump toggle
                    if (safetySystem.isCirculationStarted()) {
                        DEBUG_PRINTLN(F("[INPUT] Stopping circulation..."));
                        safetySystem.requestCirculationStop();
                        
                        // CRITICAL: When circulation stops, return to Ready UI
                        // This will also stop all features (handled by SafetySystem)
                        DEBUG_PRINTLN(F("[INPUT] Returning to Ready UI"));
                        uiManager.forceUIState(UI_READY);
                    } else {
                        DEBUG_PRINTLN(F("[INPUT] Starting circulation..."));
                        if (!safetySystem.requestCirculationStart()) {
                            uiManager.showDenialMessage("Not ready");
                        }
                        uiManager.requestRedraw();
                    }
                    break;
                    
                case 1:
                    // Massage pump toggle
                    if (safetySystem.isFeatureActive(RELAY_CHANNEL_MASSAGE)) {
                        DEBUG_PRINTLN(F("[INPUT] Stopping massage..."));
                        safetySystem.requestFeatureStop(RELAY_CHANNEL_MASSAGE);
                    } else {
                        DEBUG_PRINTLN(F("[INPUT] Starting massage..."));
                        if (!safetySystem.requestFeatureStart(RELAY_CHANNEL_MASSAGE)) {
                            uiManager.showDenialMessage("Start", "Circulation");
                        }
                    }
                    uiManager.requestRedraw();
                    break;
                    
                case 2:
                    // Jet pump toggle
                    if (safetySystem.isFeatureActive(RELAY_CHANNEL_JET)) {
                        DEBUG_PRINTLN(F("[INPUT] Stopping jet..."));
                        safetySystem.requestFeatureStop(RELAY_CHANNEL_JET);
                    } else {
                        DEBUG_PRINTLN(F("[INPUT] Starting jet..."));
                        if (!safetySystem.requestFeatureStart(RELAY_CHANNEL_JET)) {
                            uiManager.showDenialMessage("Start", "Circulation");
                        }
                    }
                    uiManager.requestRedraw();
                    break;
                    
                case 3:
                    // Water heater toggle
                    if (safetySystem.isHeaterActive()) {
                        DEBUG_PRINTLN(F("[INPUT] Stopping heater..."));
                        safetySystem.requestHeaterStop();
                    } else {
                        DEBUG_PRINTLN(F("[INPUT] Starting heater..."));
                        if (!safetySystem.requestHeaterStart()) {
                            uiManager.showDenialMessage("Start", "Circulation");
                        }
                    }
                    uiManager.requestRedraw();
                    break;
                    
                case 4:
                    // Ozone generator toggle
                    if (safetySystem.isFeatureActive(RELAY_CHANNEL_OZONE)) {
                        DEBUG_PRINTLN(F("[INPUT] Stopping ozone..."));
                        safetySystem.requestFeatureStop(RELAY_CHANNEL_OZONE);
                    } else {
                        DEBUG_PRINTLN(F("[INPUT] Starting ozone..."));
                        if (!safetySystem.requestFeatureStart(RELAY_CHANNEL_OZONE)) {
                            uiManager.showDenialMessage("Start", "Circulation");
                        }
                    }
                    uiManager.requestRedraw();
                    break;
                    
                case 5:
                    // Speaker relay toggle
                    if (safetySystem.isFeatureActive(RELAY_CHANNEL_SPEAKER)) {
                        DEBUG_PRINTLN(F("[INPUT] Stopping speaker..."));
                        safetySystem.requestFeatureStop(RELAY_CHANNEL_SPEAKER);
                    } else {
                        DEBUG_PRINTLN(F("[INPUT] Starting speaker..."));
                        if (!safetySystem.requestFeatureStart(RELAY_CHANNEL_SPEAKER)) {
                            uiManager.showDenialMessage("Start", "Circulation");
                        }
                    }
                    uiManager.requestRedraw();
                    break;
                    
                case 6:
                    // Light system toggle
                    if (safetySystem.isFeatureActive(RELAY_CHANNEL_LIGHTS)) {
                        DEBUG_PRINTLN(F("[INPUT] Stopping lights..."));
                        safetySystem.requestFeatureStop(RELAY_CHANNEL_LIGHTS);
                    } else {
                        DEBUG_PRINTLN(F("[INPUT] Starting lights..."));
                        if (!safetySystem.requestFeatureStart(RELAY_CHANNEL_LIGHTS)) {
                            uiManager.showDenialMessage("Start", "Circulation");
                        }
                    }
                    uiManager.requestRedraw();
                    break;
                    
                case 7:
                    // Temperature display - button press behavior depends on adjustment state
                    if (uiManager.isTemperatureAdjustmentActive()) {
                        // Confirm temperature setting
                        DEBUG_PRINT(F("[INPUT] Confirming temperature setting: "));
                        DEBUG_PRINT(uiManager.getAdjustedTemperature());
                        DEBUG_PRINTLN(F(" °C"));
                        
                        // Set the new target temperature
                        safetySystem.setUserTargetTemperature(uiManager.getAdjustedTemperature());
                        
                        // Confirm in UI
                        uiManager.confirmTemperatureSetting();
                    } else {
                        // Start temperature adjustment
                        DEBUG_PRINTLN(F("[INPUT] Starting temperature adjustment"));
                        uiManager.adjustTemperature(0);  // Initialize adjustment mode
                    }
                    break;
            }
        }
    }
    
    // Warning UI - acknowledge warning (button press)
    if (currentUIState == UI_WARNING && inputHandler.wasButtonPressed()) {
        DEBUG_PRINTLN(F("[INPUT] Button pressed in Warning UI - acknowledging"));

        if (safetySystem.isThermalRunawayAcknowledgmentRequired()) {
            safetySystem.acknowledgeThermalRunaway();
        }

        stateMachine.requestTransition(STATE_FEATURE_ENABLED_BATH);
        uiManager.forceUIState(UI_CIRCULATION);
    }

    // Fault UI - enter fault inspection (button press or held)
    if (currentUIState == UI_FAULT &&
        (inputHandler.wasButtonPressed() || inputHandler.isButtonHeld())) {

        DEBUG_PRINTLN(F("[INPUT] Fault UI - entering Fault Inspection"));
        uiManager.resetFaultInspectionIndex();
        uiManager.forceUIState(UI_FAULT_INSPECTION);
    }

    // Fault Inspection UI - browse faults
    if (currentUIState == UI_FAULT_INSPECTION) {
        // Rotary scrolls through faults (direction swapped to match physical rotation)
        if (inputHandler.wasRotatedCW()) {
            uiManager.navigateFaultInspection(-1);
            DEBUG_PRINTLN(F("[INPUT] Fault Inspection navigate backward"));
        } else if (inputHandler.wasRotatedCCW()) {
            uiManager.navigateFaultInspection(1);
            DEBUG_PRINTLN(F("[INPUT] Fault Inspection navigate forward"));
        }

        // Button press or held - return to Fault UI
        if (inputHandler.wasButtonPressed() || inputHandler.isButtonHeld()) {
            DEBUG_PRINTLN(F("[INPUT] Fault Inspection - returning to Fault UI"));
            uiManager.forceUIState(UI_FAULT);
        }
    }

    // Update UI manager (non-blocking)
    uiManager.update(stateMachine.getCurrentState());
    
    // Periodic debug output (every 5 seconds)
    // #ifdef ENABLE_SERIAL_DEBUG
    //     static unsigned long lastDebugTime = 0;
    //     unsigned long currentTime = millis();
        
    //     if (currentTime - lastDebugTime >= 5000) {
    //         Serial.println(F(""));
    //         Serial.println(F("--- System Status ---"));
            
    //         // State machine status
    //         Serial.print(F("Current State: "));
    //         Serial.println(stateMachine.getStateName(stateMachine.getCurrentState()));
            
    //         // Fault status
    //         if (stateMachine.getActiveFaults() != FAULT_NONE) {
    //             Serial.print(F("Active Faults: 0x"));
    //             Serial.print(stateMachine.getActiveFaults(), HEX);
    //             Serial.print(F(" ("));
    //             Serial.print(stateMachine.getFaultCount());
    //             Serial.println(F(" fault(s))"));
    //         }
            
    //         // Temperature sensor status
    //         Serial.print(F("Temperature: "));
    //         if (sensorManager.isTemperatureSensorOperational()) {
    //             Serial.print(sensorManager.getTemperature(), 1);
    //             Serial.println(F(" °C"));
    //         } else {
    //             Serial.println(F("SENSOR FAULT"));
    //         }
            
    //         // Water level sensor status
    //         Serial.print(F("Water Level: "));
    //         Serial.println(sensorManager.isWaterLevelSufficient() ? F("Sufficient") : F("Insufficient"));
            
    //         if (sensorManager.hasWaterLevelFault()) {
    //             Serial.println(F("  WATER LEVEL FAULT"));
    //         }
            
    //         // Phase 5: Safety System Status
    //         Serial.println(F(""));
    //         Serial.println(F("--- Phase 5: Safety System Status ---"));
            
    //         // Circulation status
    //         Serial.print(F("Circulation: "));
    //         if (safetySystem.isCirculationStarted()) {
    //             Serial.println(F("STARTED (pump running)"));
    //         } else if (safetySystem.isCirculationSelected()) {
    //             unsigned long remaining = safetySystem.getCirculationCountdownRemaining();
    //             Serial.print(F("SELECTED (countdown: "));
    //             Serial.print(remaining);
    //             Serial.println(F(" ms)"));
    //         } else {
    //             Serial.println(F("STOPPED"));
    //         }
            
    //         // Heater status
    //         Serial.print(F("Heater: "));
    //         if (safetySystem.isHeaterActive()) {
    //             Serial.print(F("ACTIVE (target: "));
    //             Serial.print(safetySystem.getTargetTemperature(), 1);
    //             Serial.println(F(" °C)"));
    //         } else {
    //             Serial.print(F("INACTIVE (auto-start: "));
    //             Serial.print(safetySystem.isHeaterAutoStartEnabled() ? F("ENABLED") : F("DISABLED"));
    //             Serial.println(F(")"));
    //         }
            
    //         // Target temperature
    //         Serial.print(F("Target Temperature: "));
    //         Serial.print(safetySystem.getTargetTemperature(), 1);
    //         Serial.println(F(" °C"));
            
    //         // Feature status (massage, jet, ozone, speaker, lights)
    //         Serial.print(F("Massage: "));
    //         Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_MASSAGE) ? F("ON") : F("OFF"));
    //         Serial.print(F("Jet: "));
    //         Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_JET) ? F("ON") : F("OFF"));
    //         Serial.print(F("Ozone: "));
    //         Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_OZONE) ? F("ON") : F("OFF"));
    //         Serial.print(F("Speaker: "));
    //         Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_SPEAKER) ? F("ON") : F("OFF"));
    //         Serial.print(F("Lights: "));
    //         Serial.println(safetySystem.isFeatureActive(RELAY_CHANNEL_LIGHTS) ? F("ON") : F("OFF"));
            
    //         // Thermal runaway acknowledgment status
    //         if (safetySystem.isThermalRunawayAcknowledgmentRequired()) {
    //             Serial.println(F(""));
    //             Serial.println(F("⚠️  THERMAL RUNAWAY REQUIRES MANUAL ACKNOWLEDGMENT"));
    //         }
            
    //         Serial.println(F(""));
            
    //         lastDebugTime = currentTime;
    //     }
    // #endif
    
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
