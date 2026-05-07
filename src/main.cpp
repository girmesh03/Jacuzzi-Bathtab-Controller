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
    
    // Test safe shutdown sequence (for Phase 3 validation)
    // This will be removed in later phases when integrated with state machine
    #ifdef ENABLE_SERIAL_DEBUG
        static bool shutdownTested = false;
        static unsigned long testStartTime = millis();
        
        // Wait 10 seconds after boot, then test shutdown once
        if (!shutdownTested && (millis() - testStartTime >= 10000)) {
            Serial.println(F(""));
            Serial.println(F("=== TESTING SAFE SHUTDOWN SEQUENCE ==="));
            
            // Turn on some relays first
            Serial.println(F("[TEST] Activating relays for shutdown test..."));
            relayController.setRelay(RELAY_CHANNEL_CIRCULATION, true);
            relayController.setRelay(RELAY_CHANNEL_HEATER, true);
            relayController.setRelay(RELAY_CHANNEL_MASSAGE, true);
            relayController.setRelay(RELAY_CHANNEL_JET, true);
            relayController.setRelay(RELAY_CHANNEL_OZONE, true);
            relayController.setRelay(RELAY_CHANNEL_SPEAKER, true);
            relayController.setRelay(RELAY_CHANNEL_LIGHTS, true);
            
            delay(1000);  // Brief delay to see relays activate
            
            // Start safe shutdown
            relayController.startSafeShutdown();
            shutdownTested = true;
        }
    #endif
    
    // Periodic debug output (every 5 seconds)
    #ifdef ENABLE_SERIAL_DEBUG
        static unsigned long lastDebugTime = 0;
        unsigned long currentTime = millis();
        
        if (currentTime - lastDebugTime >= 5000) {
            Serial.println(F(""));
            Serial.println(F("--- Sensor Status ---"));
            
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
