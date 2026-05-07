#pragma once

#include <Arduino.h>
#include "HardwareConfig.h"
#include "I2CBusManager.h"

// ============================================================================
// Relay Controller
// ============================================================================
// Controls 8-channel relay module through PCF8574 I2C GPIO expander.
//
// Features:
// - 8-channel relay mapping with explicit channel assignments
// - Active-low logic (HIGH = OFF, LOW = ON)
// - Relay state model maintained in memory
// - Synchronization with PCF8574 on every state change
// - Communication verification before each operation
// - Spare channel (bit 7) permanently OFF
//
// Channel Mapping:
//   0: Circulation Pump (foundation for all features)
//   1: Massage Pump (requires circulation)
//   2: Jet Pump (requires circulation)
//   3: Water Heater (3kW load, 5V/30A relay, requires circulation)
//   4: Ozone Generator (requires circulation)
//   5: Speaker Relay (requires circulation)
//   6: Light System (requires circulation)
//   7: Spare (RESERVED, permanently OFF)
//
// Usage:
//   relayController.setRelay(RELAY_CHANNEL_CIRCULATION, true);  // Turn ON
//   relayController.setRelay(RELAY_CHANNEL_CIRCULATION, false); // Turn OFF
//   bool state = relayController.getRelayState(RELAY_CHANNEL_CIRCULATION);
// ============================================================================

class RelayController {
public:
    // Constructor
    RelayController(I2CBusManager& busManager);
    
    // Initialization
    void begin();
    
    // Relay control
    bool setRelay(uint8_t channel, bool state);
    bool getRelayState(uint8_t channel) const;
    
    // Bulk operations
    bool setAllRelays(bool state);
    bool setRelayState(uint8_t relayStateByte);
    uint8_t getRelayStateByte() const;
    
    // Communication verification
    bool verifyPCF8574();
    bool isPCF8574Responding();
    
    // Update (non-blocking)
    void update();
    
    // Safe shutdown
    void startSafeShutdown();
    bool isSafeShutdownInProgress() const;
    bool isSafeShutdownComplete() const;
    
private:
    // I2C bus manager reference
    I2CBusManager& i2cBus;
    
    // Relay state (8-bit, active-low: HIGH = OFF, LOW = ON)
    uint8_t relayState;
    
    // PCF8574 status
    bool pcf8574Operational;
    
    // Safe shutdown state
    bool shutdownInProgress;
    bool shutdownComplete;
    unsigned long shutdownStartTime;
    uint8_t shutdownStep;
    
    // Helper methods
    bool writeRelayState();
    bool readRelayState();
    void validateSpareChannel();
    void processSafeShutdown();
};

