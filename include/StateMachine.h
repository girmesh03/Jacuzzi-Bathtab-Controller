#pragma once

#include <Arduino.h>
#include "StateDefinitions.h"
#include "FaultCodes.h"
#include "SensorManager.h"
#include "RelayController.h"
#include "I2CBusManager.h"

// ============================================================================
// State Machine
// ============================================================================
// Manages system state transitions with guard conditions, entry/exit actions,
// and boot fault persistence.
//
// Features:
// - 9 states: Boot, Self_Check, Ready, Active_Circulation, Feature_Enabled_Bath,
//   Warning, Fault, Fault_Inspection, Shutdown
// - Guard conditions for all transitions
// - Entry and exit actions for each state
// - Boot fault persistence (system stays in Fault until ALL conditions safe)
// - Multiple simultaneous faults support (bit field)
// - State transition logging (when ENABLE_SERIAL_DEBUG defined)
//
// Usage:
//   stateMachine.begin();
//   stateMachine.update();  // Call in main loop
//   SystemState state = stateMachine.getCurrentState();
// ============================================================================

class StateMachine {
public:
    // Constructor
    StateMachine(SensorManager& sensors, RelayController& relays, I2CBusManager& i2c);
    
    // Initialization
    void begin();
    
    // Update (non-blocking)
    void update();
    
    // State queries
    SystemState getCurrentState() const;
    SystemState getPreviousState() const;
    const char* getStateName(SystemState state) const;
    
    // Fault management
    uint8_t getActiveFaults() const;
    bool hasFault(FaultCode fault) const;
    void setFault(FaultCode fault);
    void clearFault(FaultCode fault);
    void clearAllFaults();
    uint8_t getFaultCount() const;
    
    // State transition requests
    bool requestTransition(SystemState newState);
    
    // Precondition checks (for guard conditions)
    bool checkAllPreconditions() const;
    bool checkCirculationPreconditions() const;
    bool checkHeaterPreconditions() const;
    
private:
    // Module references
    SensorManager& sensorManager;
    RelayController& relayController;
    I2CBusManager& i2cBus;
    
    // State tracking
    SystemState currentState;
    SystemState previousState;
    unsigned long stateEntryTime;
    
    // Fault tracking
    uint8_t activeFaults;  // Bit field for multiple simultaneous faults
    
    // Initialization tracking
    bool hardwareInitComplete;
    bool selfCheckComplete;
    
    // State transition logic
    bool canTransition(SystemState from, SystemState to) const;
    void executeTransition(SystemState newState);
    
    // Entry actions
    void onEnterBoot();
    void onEnterSelfCheck();
    void onEnterReady();
    void onEnterActiveCirculation();
    void onEnterFeatureEnabledBath();
    void onEnterWarning();
    void onEnterFault();
    void onEnterFaultInspection();
    void onEnterShutdown();
    
    // Exit actions
    void onExitBoot();
    void onExitSelfCheck();
    void onExitReady();
    void onExitActiveCirculation();
    void onExitFeatureEnabledBath();
    void onExitWarning();
    void onExitFault();
    void onExitFaultInspection();
    void onExitShutdown();
    
    // Guard conditions
    bool guardBootToSelfCheck() const;
    bool guardSelfCheckToReady() const;
    bool guardSelfCheckToFault() const;
    bool guardReadyToActiveCirculation() const;
    bool guardActiveCirculationToFeatureEnabledBath() const;
    bool guardToFault() const;
    bool guardFaultToFaultInspection() const;
    bool guardFaultToSelfCheck() const;
    
    // Helper methods
    void performSelfCheck();
    void logStateTransition(SystemState from, SystemState to) const;
};

