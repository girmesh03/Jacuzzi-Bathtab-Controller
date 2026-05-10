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
    
    // Input Handler Initialization (Phase 7)
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
// Forward Declarations for UI Input Handlers
// ============================================================================
static void handleUIInput();
static void handleReadyInput();
static void handleMainMenuInput();
static void handleCirculationInput();
static void handleWarningInput();
static void handleFaultInput();
static void handleFaultInspectionInput();

// ============================================================================
// Loop Function
// ============================================================================
void loop() {
    sensorManager.update();
    relayController.update();
    stateMachine.update();
    safetySystem.update();
    inputHandler.update();

    handleUIInput();

    uiManager.update(stateMachine.getCurrentState());
}

// ============================================================================
// UI Input Handlers
// ============================================================================

static void handleUIInput() {
    switch (uiManager.getCurrentUIState()) {
        case UI_READY:            handleReadyInput();            break;
        case UI_MAIN_MENU:        handleMainMenuInput();         break;
        case UI_CIRCULATION:      handleCirculationInput();      break;
        case UI_WARNING:          handleWarningInput();          break;
        case UI_FAULT:            handleFaultInput();            break;
        case UI_FAULT_INSPECTION: handleFaultInspectionInput();  break;
        default: break;
    }
}

static void handleReadyInput() {
    if (inputHandler.wasButtonPressed()) {
        DEBUG_PRINTLN(F("[INPUT] Button pressed in Ready UI - transitioning to Main Menu"));
        uiManager.forceUIState(UI_MAIN_MENU);
    }
}

static void handleMainMenuInput() {
    inputHandler.wasRotatedCW();
    inputHandler.wasRotatedCCW();

    if (inputHandler.isButtonHeld()) {
        DEBUG_PRINTLN(F("[INPUT] Button HELD in Main Menu - returning to Ready UI"));
        uiManager.forceUIState(UI_READY);
    } else if (inputHandler.wasButtonPressed()) {
        DEBUG_PRINTLN(F("[INPUT] Button pressed in Main Menu - entering Circulation UI"));
        uiManager.selectMainMenuItem();
    }
}

static void handleCirculationInput() {
    if (inputHandler.isButtonHeld()) {
        DEBUG_PRINTLN(F("[INPUT] Button HELD in Circulation UI - returning to Ready UI"));

        if (safetySystem.isCirculationStarted()) {
            DEBUG_PRINTLN(F("[INPUT] Stopping circulation..."));
            safetySystem.requestCirculationStop();
        } else if (safetySystem.isCirculationSelected()) {
            safetySystem.cancelCirculationStart();
        }

        uiManager.forceUIState(UI_READY);
    } else if (inputHandler.wasRotatedCW()) {
        if (uiManager.getCirculationMenuSelectedIndex() == 7 && uiManager.isTemperatureAdjustmentActive()) {
            uiManager.adjustTemperature(-1);
            DEBUG_PRINTLN(F("[INPUT] Rotary CW (HW) / CCW (Physical) - Temperature decrease"));
        } else {
            uiManager.navigateCirculationMenu(-1);
            DEBUG_PRINTLN(F("[INPUT] Rotary CW (HW) / CCW (Physical) - Circulation Menu navigate backward"));
        }
    } else if (inputHandler.wasRotatedCCW()) {
        if (uiManager.getCirculationMenuSelectedIndex() == 7 && uiManager.isTemperatureAdjustmentActive()) {
            uiManager.adjustTemperature(1);
            DEBUG_PRINTLN(F("[INPUT] Rotary CCW (HW) / CW (Physical) - Temperature increase"));
        } else {
            uiManager.navigateCirculationMenu(1);
            DEBUG_PRINTLN(F("[INPUT] Rotary CCW (HW) / CW (Physical) - Circulation Menu navigate forward"));
        }
    }

    if (inputHandler.wasButtonPressed()) {
        uint8_t selectedIndex = uiManager.getCirculationMenuSelectedIndex();
        DEBUG_PRINT(F("[INPUT] Button pressed in Circulation UI - selected index: "));
        DEBUG_PRINTLN(selectedIndex);

        switch (selectedIndex) {
            case 0:
                if (safetySystem.isCirculationStarted()) {
                    DEBUG_PRINTLN(F("[INPUT] Stopping circulation..."));
                    safetySystem.requestCirculationStop();
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
                if (uiManager.isTemperatureAdjustmentActive()) {
                    DEBUG_PRINT(F("[INPUT] Confirming temperature setting: "));
                    DEBUG_PRINT(uiManager.getAdjustedTemperature());
                    DEBUG_PRINTLN(F(" °C"));
                    safetySystem.setUserTargetTemperature(uiManager.getAdjustedTemperature());
                    uiManager.confirmTemperatureSetting();
                } else {
                    DEBUG_PRINTLN(F("[INPUT] Starting temperature adjustment"));
                    uiManager.adjustTemperature(0);
                }
                break;
        }
    }
}

static void handleWarningInput() {
    if (inputHandler.wasButtonPressed()) {
        DEBUG_PRINTLN(F("[INPUT] Button pressed in Warning UI - acknowledging"));

        if (safetySystem.isThermalRunawayAcknowledgmentRequired()) {
            safetySystem.acknowledgeThermalRunaway();
        }

        stateMachine.requestTransition(STATE_FEATURE_ENABLED_BATH);
        uiManager.forceUIState(UI_CIRCULATION);
    }
}

static void handleFaultInput() {
    if (inputHandler.wasButtonPressed() || inputHandler.isButtonHeld()) {
        DEBUG_PRINTLN(F("[INPUT] Fault UI - entering Fault Inspection"));
        uiManager.resetFaultInspectionIndex();
        uiManager.forceUIState(UI_FAULT_INSPECTION);
    }
}

static void handleFaultInspectionInput() {
    if (inputHandler.wasRotatedCW()) {
        uiManager.navigateFaultInspection(-1);
        DEBUG_PRINTLN(F("[INPUT] Fault Inspection navigate backward"));
    } else if (inputHandler.wasRotatedCCW()) {
        uiManager.navigateFaultInspection(1);
        DEBUG_PRINTLN(F("[INPUT] Fault Inspection navigate forward"));
    }

    if (inputHandler.wasButtonPressed() || inputHandler.isButtonHeld()) {
        DEBUG_PRINTLN(F("[INPUT] Fault Inspection - returning to Fault UI"));
        uiManager.forceUIState(UI_FAULT);
    }
}
