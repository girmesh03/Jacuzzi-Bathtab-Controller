#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "HardwareConfig.h"
#include "TimingConfig.h"

// ============================================================================
// I2C Bus Manager
// ============================================================================
// Manages I2C bus access with lock/release mechanism to prevent conflicts
// between OLED display (0x3C) and PCF8574 relay controller (0x20).
//
// Features:
// - Transaction serialization (lock/release mechanism)
// - Timeout handling for all transactions
// - Priority management (safety-critical relay updates over display refresh)
// - Device address verification during initialization
//
// Usage:
//   if (i2cBus.acquireLock(I2C_ADDRESS_PCF8574, I2C_TRANSACTION_TIMEOUT_MS)) {
//       // Perform I2C transaction
//       i2cBus.releaseLock();
//   }
// ============================================================================

class I2CBusManager {
public:
    // Constructor
    I2CBusManager();
    
    // Initialization
    void begin();
    
    // Lock/Release mechanism
    bool acquireLock(uint8_t address, unsigned long timeout);
    void releaseLock();
    
    // Lock status
    bool isLocked() const;
    uint8_t getLockedAddress() const;
    unsigned long getLockDuration() const;
    
    // Device verification
    bool verifyDevice(uint8_t address);
    bool isDeviceResponding(uint8_t address);
    
    // Update (non-blocking) - call from main loop to handle stale lock detection
    void update();

    // Priority management
    void setPriority(bool highPriority);
    bool isHighPriority() const;
    
private:
    // Lock state
    bool locked;
    uint8_t lockedAddress;
    unsigned long lockStartTime;
    bool highPriority;
    
    // Device status
    bool oledVerified;
    bool pcf8574Verified;
    
    // Helper methods
    bool waitForLockRelease(unsigned long timeout);
    void updateLockStatus();
};

