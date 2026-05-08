#pragma once

// ============================================================================
// Safety System
// ============================================================================
// Manages precondition checking, fault detection, thermal runaway monitoring,
// and safe shutdown coordination.
//
// Features:
// - Precondition checking for circulation pump, heater, and all features
// - Circulation dependency enforcement (ABSOLUTE RULE for heater)
// - Feature sequencing (all features require circulation active)
// - Thermal runaway detection (temperature delta and rate-of-change)
// - Fault detection for all fault types
// - Auto-clear and manual acknowledgment fault handling
// - Delayed start behavior for circulation pump
// - Auto-start behavior for water heater
// - Temperature control loop (default and user-set targets)
//
// Usage:
//   safetySystem.begin();
//   safetySystem.update();  // Call in main loop
//   if (safetySystem.checkCirculationPreconditions()) { ... }
// ============================================================================

#include <Arduino.h>
#include "HardwareConfig.h"
#include "SafetyConfig.h"
#include "TimingConfig.h"
#include "FaultCodes.h"
#include "SensorManager.h"
#include "RelayController.h"
#include "StateMachine.h"

// Forward declaration to avoid circular dependency
class StateMachine;

class SafetySystem {
public:
    // Constructor
    SafetySystem(SensorManager& sensors, RelayController& relays, StateMachine& stateMachine);
    
    // Initialization
    void begin();
    
    // Update (non-blocking)
    void update();
    
    // ------------------------------------------------------------------------
    // Precondition Checking
    // ------------------------------------------------------------------------
    
    /**
     * @brief Check if circulation pump can be activated
     * Preconditions: water level sufficient AND temp sensor operational AND no faults
     * @return true if all preconditions satisfied
     */
    bool checkCirculationPreconditions();
    
    /**
     * @brief Check if water heater can be activated
     * ABSOLUTE RULE: Circulation pump MUST be actively running
     * Preconditions: circulation active AND water level sufficient AND 
     *                temp below warning AND no faults
     * @return true if all preconditions satisfied
     */
    bool checkHeaterPreconditions();
    
    /**
     * @brief Check if feature can be activated (massage, jet, ozone, speaker, lights)
     * Preconditions: circulation pump MUST be active
     * @return true if circulation active
     */
    bool checkFeaturePreconditions();
    
    // ------------------------------------------------------------------------
    // Circulation Pump Control
    // ------------------------------------------------------------------------
    
    /**
     * @brief Request circulation pump activation (starts delayed countdown)
     * @return true if request accepted, false if preconditions not met
     */
    bool requestCirculationStart();
    
    /**
     * @brief Cancel circulation pump activation during countdown
     */
    void cancelCirculationStart();
    
    /**
     * @brief Request circulation pump deactivation
     */
    void requestCirculationStop();
    
    /**
     * @brief Check if circulation is in "selected" state (countdown active)
     * @return true if countdown in progress
     */
    bool isCirculationSelected() const;
    
    /**
     * @brief Check if circulation is in "started" state (pump running)
     * @return true if pump actively running
     */
    bool isCirculationStarted() const;
    
    /**
     * @brief Get remaining countdown time (ms)
     * @return milliseconds remaining, 0 if not in countdown
     */
    unsigned long getCirculationCountdownRemaining() const;
    
    // ------------------------------------------------------------------------
    // Water Heater Control
    // ------------------------------------------------------------------------
    
    /**
     * @brief Request water heater activation
     * @return true if request accepted, false if preconditions not met
     */
    bool requestHeaterStart();
    
    /**
     * @brief Request water heater deactivation
     */
    void requestHeaterStop();
    
    /**
     * @brief Check if heater is active
     * @return true if heater relay ON
     */
    bool isHeaterActive() const;
    
    /**
     * @brief Set user target temperature (overrides default)
     * @param targetTemp Target temperature in °C
     */
    void setUserTargetTemperature(float targetTemp);
    
    /**
     * @brief Clear user target temperature (revert to default)
     */
    void clearUserTargetTemperature();
    
    /**
     * @brief Get current target temperature (user-set or default)
     * @return Target temperature in °C
     */
    float getTargetTemperature() const;
    
    /**
     * @brief Check if heater auto-start is enabled
     * @return true if auto-start enabled
     */
    bool isHeaterAutoStartEnabled() const;
    
    /**
     * @brief Enable/disable heater auto-start
     * @param enabled true to enable, false to disable
     */
    void setHeaterAutoStart(bool enabled);
    
    // ------------------------------------------------------------------------
    // Feature Control (Massage, Jet, Ozone, Speaker, Lights)
    // ------------------------------------------------------------------------
    
    /**
     * @brief Request feature activation
     * @param channel Relay channel (RELAY_CHANNEL_MASSAGE, etc.)
     * @return true if request accepted, false if preconditions not met
     */
    bool requestFeatureStart(uint8_t channel);
    
    /**
     * @brief Request feature deactivation
     * @param channel Relay channel
     */
    void requestFeatureStop(uint8_t channel);
    
    /**
     * @brief Check if feature is active
     * @param channel Relay channel
     * @return true if feature relay ON
     */
    bool isFeatureActive(uint8_t channel) const;
    
    // ------------------------------------------------------------------------
    // Thermal Runaway Detection
    // ------------------------------------------------------------------------
    
    /**
     * @brief Check for thermal runaway condition
     * Analyzes temperature history for unexpected delta or rate-of-change
     * @return true if thermal runaway detected
     */
    bool detectThermalRunaway();
    
    /**
     * @brief Acknowledge thermal runaway fault (manual acknowledgment required)
     */
    void acknowledgeThermalRunaway();
    
    /**
     * @brief Check if thermal runaway requires acknowledgment
     * @return true if thermal runaway fault active and not acknowledged
     */
    bool isThermalRunawayAcknowledgmentRequired() const;
    
    // ------------------------------------------------------------------------
    // Fault Detection
    // ------------------------------------------------------------------------
    
    /**
     * @brief Detect all fault types and update fault state
     * Called automatically in update()
     */
    void detectFaults();
    
    /**
     * @brief Check if any fault is active
     * @return true if any fault detected
     */
    bool hasAnyFault() const;
    
private:
    // Module references
    SensorManager& sensorManager;
    RelayController& relayController;
    StateMachine& stateMachine;
    
    // ------------------------------------------------------------------------
    // Circulation Pump State
    // ------------------------------------------------------------------------
    
    bool circulationSelected;              // User has selected circulation (countdown)
    bool circulationStarted;               // Circulation pump actively running
    unsigned long circulationSelectTime;   // Time when user selected circulation
    unsigned long circulationStartTime;    // Time when pump actually started
    
    // ------------------------------------------------------------------------
    // Water Heater State
    // ------------------------------------------------------------------------
    
    bool heaterActive;                     // Heater relay ON
    bool heaterAutoStartEnabled;           // Auto-start enabled flag
    bool heaterAutoStartPending;           // Auto-start countdown active
    unsigned long heaterAutoStartTime;     // Time when auto-start countdown began
    float userTargetTemperature;           // User-set target (0 = use default)
    bool hasUserTargetTemperature;         // Flag indicating user has set target
    
    // ------------------------------------------------------------------------
    // Thermal Runaway Detection
    // ------------------------------------------------------------------------
    
    bool thermalRunawayAcknowledged;       // Manual acknowledgment flag
    unsigned long lastThermalRunawayCheck; // Last check timestamp
    
    // ------------------------------------------------------------------------
    // Private Methods
    // ------------------------------------------------------------------------
    
    // Circulation pump control
    void processCirculationDelayedStart();
    void activateCirculationPump();
    void deactivateCirculationPump();
    void monitorCirculationWaterLevel();
    
    // Water heater control
    void processHeaterAutoStart();
    void processHeaterTemperatureControl();
    void activateHeater();
    void deactivateHeater();
    
    // Feature control
    void deactivateAllDependentFeatures();
    
    // Thermal runaway detection
    float calculateTemperatureDelta();
    float calculateTemperatureRate();
    
    // Fault detection helpers
    void detectLowWaterLevelFault();
    void detectHighTemperatureFault();
    void detectThermalRunawayFault();
    void detectTemperatureSensorFault();
    void detectI2CFault();
    void detectPCF8574Fault();
};

