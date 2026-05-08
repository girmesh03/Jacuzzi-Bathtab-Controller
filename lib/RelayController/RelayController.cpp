#include "RelayController.h"
#include <Wire.h>

// ============================================================================
// Constructor
// ============================================================================
RelayController::RelayController(I2CBusManager& busManager)
    : i2cBus(busManager)
    , relayState(RELAY_ALL_OFF)  // All relays OFF at initialization
    , pcf8574Operational(false)
    , shutdownInProgress(false)
    , shutdownComplete(false)
    , shutdownStartTime(0)
    , shutdownStep(0)
{
}

// ============================================================================
// Initialization
// ============================================================================
void RelayController::begin() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F(""));
        Serial.println(F("Initializing Relay Controller..."));
    #endif
    
    // Initialize relay state to all OFF
    relayState = RELAY_ALL_OFF;
    
    // Verify PCF8574 communication
    pcf8574Operational = verifyPCF8574();
    
    if (pcf8574Operational) {
        // Write initial state (all OFF)
        if (writeRelayState()) {
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("All relays initialized to OFF (0xFF)"));
                Serial.println(F("Active-low logic: HIGH = OFF, LOW = ON"));
                Serial.println(F(""));
                Serial.println(F("Relay Channel Mapping:"));
                Serial.println(F("  0: Circulation Pump"));
                Serial.println(F("  1: Massage Pump"));
                Serial.println(F("  2: Jet Pump"));
                Serial.println(F("  3: Water Heater (3kW, 5V/30A)"));
                Serial.println(F("  4: Ozone Generator"));
                Serial.println(F("  5: Speaker Relay"));
                Serial.println(F("  6: Light System"));
                Serial.println(F("  7: Spare (RESERVED, OFF)"));
            #endif
        } else {
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("ERROR: Failed to write initial relay state"));
            #endif
        }
    } else {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("WARNING: PCF8574 not responding"));
        #endif
    }
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("Relay Controller initialized"));
        Serial.println(F(""));
    #endif
}

// ============================================================================
// Relay Control
// ============================================================================

/**
 * @brief Set relay state (ON or OFF)
 * @param channel Relay channel (0-7)
 * @param state true = ON, false = OFF
 * @return true if successful, false if failed
 */
bool RelayController::setRelay(uint8_t channel, bool state) {
    // Validate channel
    if (channel > 7) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[RELAY] ERROR: Invalid channel "));
            Serial.println(channel);
        #endif
        return false;
    }
    
    // CRITICAL: Spare channel (7) must remain OFF permanently
    if (channel == RELAY_CHANNEL_SPARE) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[RELAY] WARNING: Spare channel (7) is reserved, ignoring request"));
        #endif
        return false;
    }
    
    // Update relay state (active-low logic)
    if (state) {
        // Turn ON: Clear bit (LOW = ON)
        relayState &= ~(1 << channel);
    } else {
        // Turn OFF: Set bit (HIGH = OFF)
        relayState |= (1 << channel);
    }
    
    // Ensure spare channel remains OFF
    validateSpareChannel();
    
    // Write to PCF8574
    bool success = writeRelayState();
    
    #ifdef ENABLE_SERIAL_DEBUG
        if (success) {
            Serial.print(F("[RELAY] Channel "));
            Serial.print(channel);
            Serial.print(F(": "));
            Serial.println(state ? F("ON") : F("OFF"));
        } else {
            Serial.print(F("[RELAY] ERROR: Failed to set channel "));
            Serial.println(channel);
        }
    #endif
    
    return success;
}

/**
 * @brief Get relay state
 * @param channel Relay channel (0-7)
 * @return true if ON, false if OFF
 */
bool RelayController::getRelayState(uint8_t channel) const {
    if (channel > 7) {
        return false;
    }
    
    // Active-low logic: bit LOW = relay ON
    return !(relayState & (1 << channel));
}

// ============================================================================
// Bulk Operations
// ============================================================================

/**
 * @brief Set all relays to same state
 * @param state true = all ON, false = all OFF
 * @return true if successful, false if failed
 */
bool RelayController::setAllRelays(bool state) {
    if (state) {
        // All ON (except spare channel)
        relayState = 0x00;  // All bits LOW = all ON
    } else {
        // All OFF
        relayState = RELAY_ALL_OFF;  // All bits HIGH = all OFF
    }
    
    // Ensure spare channel remains OFF
    validateSpareChannel();
    
    // Write to PCF8574
    bool success = writeRelayState();
    
    #ifdef ENABLE_SERIAL_DEBUG
        if (success) {
            Serial.print(F("[RELAY] All relays: "));
            Serial.println(state ? F("ON") : F("OFF"));
        } else {
            Serial.println(F("[RELAY] ERROR: Failed to set all relays"));
        }
    #endif
    
    return success;
}

/**
 * @brief Set relay state using raw byte
 * @param relayStateByte 8-bit relay state (active-low)
 * @return true if successful, false if failed
 */
bool RelayController::setRelayState(uint8_t relayStateByte) {
    relayState = relayStateByte;
    
    // Ensure spare channel remains OFF
    validateSpareChannel();
    
    return writeRelayState();
}

/**
 * @brief Get current relay state byte
 * @return 8-bit relay state (active-low)
 */
uint8_t RelayController::getRelayStateByte() const {
    return relayState;
}

// ============================================================================
// Communication Verification
// ============================================================================

/**
 * @brief Verify PCF8574 communication
 * @return true if responding, false otherwise
 */
bool RelayController::verifyPCF8574() {
    // Acquire I2C bus lock
    if (!i2cBus.acquireLock(I2C_ADDRESS_PCF8574, I2C_TRANSACTION_TIMEOUT_MS)) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[RELAY] ERROR: Failed to acquire I2C lock"));
        #endif
        return false;
    }
    
    // Verify device
    Wire.beginTransmission(I2C_ADDRESS_PCF8574);
    uint8_t error = Wire.endTransmission();
    
    // Release I2C bus lock
    i2cBus.releaseLock();
    
    bool responding = (error == 0);
    pcf8574Operational = responding;
    
    return responding;
}

/**
 * @brief Check if PCF8574 is currently responding
 * @return true if responding, false otherwise
 */
bool RelayController::isPCF8574Responding() {
    return verifyPCF8574();
}

// ============================================================================
// Update (Non-Blocking)
// ============================================================================

/**
 * @brief Update relay controller (non-blocking)
 * Called from main event loop
 */
void RelayController::update() {
    // Process safe shutdown if in progress
    if (shutdownInProgress) {
        processSafeShutdown();
    }
    
    // Periodic verification of PCF8574 communication
    // This is a placeholder for future enhancements
    // Currently, verification happens on each write operation
}

// ============================================================================
// Safe Shutdown
// ============================================================================

/**
 * @brief Start safe shutdown sequence
 * Priority order: Heater first (immediate), then other loads, Circulation last
 */
void RelayController::startSafeShutdown() {
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F(""));
        Serial.println(F("[SHUTDOWN] Starting safe shutdown sequence..."));
    #endif
    
    shutdownInProgress = true;
    shutdownComplete = false;
    shutdownStartTime = millis();
    shutdownStep = 0;
    
    // Start shutdown sequence
    processSafeShutdown();
}

/**
 * @brief Check if safe shutdown is in progress
 * @return true if shutdown in progress, false otherwise
 */
bool RelayController::isSafeShutdownInProgress() const {
    return shutdownInProgress;
}

/**
 * @brief Check if safe shutdown is complete
 * @return true if shutdown complete, false otherwise
 */
bool RelayController::isSafeShutdownComplete() const {
    return shutdownComplete;
}

/**
 * @brief Process safe shutdown sequence (non-blocking)
 * Called from update() when shutdown is in progress
 */
void RelayController::processSafeShutdown() {
    unsigned long elapsed = millis() - shutdownStartTime;
    
    switch (shutdownStep) {
        case 0:
            // Step 0: Heater OFF immediately (highest priority)
            if (elapsed >= SHUTDOWN_HEATER_DELAY_MS) {
                setRelay(RELAY_CHANNEL_HEATER, false);
                #ifdef ENABLE_SERIAL_DEBUG
                    Serial.println(F("[SHUTDOWN] Step 0: Heater OFF"));
                #endif
                shutdownStep = 1;
            }
            break;
            
        case 1:
            // Step 1: Ozone OFF after delay
            if (elapsed >= SHUTDOWN_OZONE_DELAY_MS) {
                setRelay(RELAY_CHANNEL_OZONE, false);
                #ifdef ENABLE_SERIAL_DEBUG
                    Serial.println(F("[SHUTDOWN] Step 1: Ozone OFF"));
                #endif
                shutdownStep = 2;
            }
            break;
            
        case 2:
            // Step 2: Jet OFF after delay
            if (elapsed >= SHUTDOWN_JET_DELAY_MS) {
                setRelay(RELAY_CHANNEL_JET, false);
                #ifdef ENABLE_SERIAL_DEBUG
                    Serial.println(F("[SHUTDOWN] Step 2: Jet OFF"));
                #endif
                shutdownStep = 3;
            }
            break;
            
        case 3:
            // Step 3: Massage OFF after delay
            if (elapsed >= SHUTDOWN_MASSAGE_DELAY_MS) {
                setRelay(RELAY_CHANNEL_MASSAGE, false);
                #ifdef ENABLE_SERIAL_DEBUG
                    Serial.println(F("[SHUTDOWN] Step 3: Massage OFF"));
                #endif
                shutdownStep = 4;
            }
            break;
            
        case 4:
            // Step 4: Speaker and Lights OFF after delay
            if (elapsed >= SHUTDOWN_SPEAKER_LIGHTS_DELAY_MS) {
                setRelay(RELAY_CHANNEL_SPEAKER, false);
                setRelay(RELAY_CHANNEL_LIGHTS, false);
                #ifdef ENABLE_SERIAL_DEBUG
                    Serial.println(F("[SHUTDOWN] Step 4: Speaker and Lights OFF"));
                #endif
                shutdownStep = 5;
            }
            break;
            
        case 5:
            // Step 5: Circulation OFF last (after all other loads)
            if (elapsed >= SHUTDOWN_CIRCULATION_DELAY_MS) {
                setRelay(RELAY_CHANNEL_CIRCULATION, false);
                #ifdef ENABLE_SERIAL_DEBUG
                    Serial.println(F("[SHUTDOWN] Step 5: Circulation OFF (last)"));
                #endif
                shutdownStep = 6;
            }
            break;
            
        case 6:
            // Shutdown complete
            shutdownInProgress = false;
            shutdownComplete = true;
            
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.println(F("[SHUTDOWN] Safe shutdown sequence complete"));
                Serial.print(F("[SHUTDOWN] Total time: "));
                Serial.print(elapsed);
                Serial.println(F(" ms"));
                Serial.print(F("[SHUTDOWN] Final relay state: 0x"));
                Serial.println(relayState, HEX);
                Serial.println(F(""));
            #endif
            break;
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

/**
 * @brief Write relay state to PCF8574
 * @return true if successful, false if failed
 */
bool RelayController::writeRelayState() {
    // Verify PCF8574 is operational
    if (!pcf8574Operational) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[RELAY] ERROR: PCF8574 not operational"));
        #endif
        return false;
    }
    
    // Acquire I2C bus lock with high priority (safety-critical)
    i2cBus.setPriority(true);
    if (!i2cBus.acquireLock(I2C_ADDRESS_PCF8574, I2C_TRANSACTION_TIMEOUT_MS)) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.println(F("[RELAY] ERROR: Failed to acquire I2C lock"));
        #endif
        i2cBus.setPriority(false);
        return false;
    }
    
    // Write relay state
    Wire.beginTransmission(I2C_ADDRESS_PCF8574);
    Wire.write(relayState);
    uint8_t error = Wire.endTransmission();
    
    // Release I2C bus lock
    i2cBus.releaseLock();
    i2cBus.setPriority(false);
    
    if (error != 0) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[RELAY] ERROR: PCF8574 write failed with code "));
            Serial.println(error);
        #endif
        pcf8574Operational = false;
        return false;
    }
    
    return true;
}

/**
 * @brief Read relay state from PCF8574
 * @return true if successful, false if failed
 */
bool RelayController::readRelayState() {
    // Acquire I2C bus lock
    if (!i2cBus.acquireLock(I2C_ADDRESS_PCF8574, I2C_TRANSACTION_TIMEOUT_MS)) {
        return false;
    }
    
    // Request 1 byte from PCF8574
    // Use explicit uint8_t for both parameters to avoid ambiguity
    uint8_t bytesRead = Wire.requestFrom((uint8_t)I2C_ADDRESS_PCF8574, (uint8_t)1);
    
    if (bytesRead == 1) {
        relayState = Wire.read();
        i2cBus.releaseLock();
        return true;
    }
    
    i2cBus.releaseLock();
    return false;
}

/**
 * @brief Ensure spare channel (bit 7) remains OFF
 */
void RelayController::validateSpareChannel() {
    // Spare channel (bit 7) must always be HIGH (OFF)
    relayState |= (1 << RELAY_CHANNEL_SPARE);
}

