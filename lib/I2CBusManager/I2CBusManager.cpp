#include "I2CBusManager.h"

// ============================================================================
// Constructor
// ============================================================================
I2CBusManager::I2CBusManager() 
    : locked(false)
    , lockedAddress(0)
    , lockStartTime(0)
    , highPriority(false)
    , oledVerified(false)
    , pcf8574Verified(false)
{
}

// ============================================================================
// Initialization
// ============================================================================
void I2CBusManager::begin() {
    // I2C bus should already be initialized in main.cpp
    // This method verifies I2C devices
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F(""));
        Serial.println(F("Initializing I2C Bus Manager..."));
    #endif
    
    // Verify OLED at 0x3C
    oledVerified = verifyDevice(I2C_ADDRESS_OLED);
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("OLED (0x"));
        Serial.print(I2C_ADDRESS_OLED, HEX);
        Serial.print(F("): "));
        Serial.println(oledVerified ? F("VERIFIED") : F("NOT FOUND"));
    #endif
    
    // Verify PCF8574 at 0x20
    pcf8574Verified = verifyDevice(I2C_ADDRESS_PCF8574);
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.print(F("PCF8574 (0x"));
        Serial.print(I2C_ADDRESS_PCF8574, HEX);
        Serial.print(F("): "));
        Serial.println(pcf8574Verified ? F("VERIFIED") : F("NOT FOUND"));
    #endif
    
    // Initialize lock state
    locked = false;
    lockedAddress = 0;
    lockStartTime = 0;
    highPriority = false;
    
    #ifdef ENABLE_SERIAL_DEBUG
        Serial.println(F("I2C Bus Manager initialized"));
        Serial.println(F(""));
    #endif
}

// ============================================================================
// Lock/Release Mechanism
// ============================================================================

/**
 * @brief Acquire lock for I2C bus access
 * @param address I2C device address
 * @param timeout Maximum wait time in milliseconds
 * @return true if lock acquired, false if timeout
 */
bool I2CBusManager::acquireLock(uint8_t address, unsigned long timeout) {
    unsigned long startTime = millis();
    
    // Wait for lock to be released if currently locked
    while (locked) {
        // Check timeout
        if (millis() - startTime >= timeout) {
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.print(F("[I2C] Lock timeout for address 0x"));
                Serial.println(address, HEX);
            #endif
            return false;
        }
        
        // Check if lock has been held too long (potential deadlock)
        if (millis() - lockStartTime >= I2C_TRANSACTION_TIMEOUT_MS) {
            #ifdef ENABLE_SERIAL_DEBUG
                Serial.print(F("[I2C] WARNING: Lock held too long by 0x"));
                Serial.print(lockedAddress, HEX);
                Serial.println(F(", forcing release"));
            #endif
            releaseLock();
            break;
        }
        
        // Small delay to prevent busy-waiting
        yield();
    }
    
    // Acquire lock
    locked = true;
    lockedAddress = address;
    lockStartTime = millis();
    
    #ifdef ENABLE_SERIAL_DEBUG
        // Only log if debug level is verbose
        // Serial.print(F("[I2C] Lock acquired by 0x"));
        // Serial.println(address, HEX);
    #endif
    
    return true;
}

/**
 * @brief Release I2C bus lock
 */
void I2CBusManager::releaseLock() {
    if (locked) {
        #ifdef ENABLE_SERIAL_DEBUG
            // Only log if debug level is verbose
            // unsigned long duration = millis() - lockStartTime;
            // Serial.print(F("[I2C] Lock released by 0x"));
            // Serial.print(lockedAddress, HEX);
            // Serial.print(F(" (held for "));
            // Serial.print(duration);
            // Serial.println(F(" ms)"));
        #endif
        
        locked = false;
        lockedAddress = 0;
        lockStartTime = 0;
        highPriority = false;
    }
}

// ============================================================================
// Lock Status
// ============================================================================

/**
 * @brief Check if bus is currently locked
 * @return true if locked, false otherwise
 */
bool I2CBusManager::isLocked() const {
    return locked;
}

/**
 * @brief Get address of device that currently holds lock
 * @return I2C address, or 0 if not locked
 */
uint8_t I2CBusManager::getLockedAddress() const {
    return lockedAddress;
}

/**
 * @brief Get duration lock has been held
 * @return Duration in milliseconds, or 0 if not locked
 */
unsigned long I2CBusManager::getLockDuration() const {
    if (locked) {
        return millis() - lockStartTime;
    }
    return 0;
}

// ============================================================================
// Device Verification
// ============================================================================

/**
 * @brief Verify I2C device responds at given address
 * @param address I2C device address
 * @return true if device responds, false otherwise
 */
bool I2CBusManager::verifyDevice(uint8_t address) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();
    return (error == 0);
}

/**
 * @brief Check if device is currently responding
 * @param address I2C device address
 * @return true if device responds, false otherwise
 */
bool I2CBusManager::isDeviceResponding(uint8_t address) {
    // Acquire lock with short timeout
    if (!acquireLock(address, 100)) {
        return false;
    }
    
    // Check device
    bool responding = verifyDevice(address);
    
    // Release lock
    releaseLock();
    
    return responding;
}

// ============================================================================
// Priority Management
// ============================================================================

/**
 * @brief Set priority for current transaction
 * @param highPriority true for high priority (safety-critical), false for normal
 */
void I2CBusManager::setPriority(bool highPriority) {
    this->highPriority = highPriority;
}

/**
 * @brief Check if current transaction is high priority
 * @return true if high priority, false otherwise
 */
bool I2CBusManager::isHighPriority() const {
    return highPriority;
}

// ============================================================================
// Helper Methods
// ============================================================================

/**
 * @brief Wait for lock to be released
 * @param timeout Maximum wait time in milliseconds
 * @return true if lock released, false if timeout
 */
bool I2CBusManager::waitForLockRelease(unsigned long timeout) {
    unsigned long startTime = millis();
    
    while (locked) {
        if (millis() - startTime >= timeout) {
            return false;
        }
        yield();
    }
    
    return true;
}

/**
 * @brief Update lock status (check for stale locks)
 */
void I2CBusManager::updateLockStatus() {
    if (locked && (millis() - lockStartTime >= I2C_TRANSACTION_TIMEOUT_MS)) {
        #ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[I2C] WARNING: Stale lock detected for 0x"));
            Serial.print(lockedAddress, HEX);
            Serial.println(F(", forcing release"));
        #endif
        releaseLock();
    }
}

