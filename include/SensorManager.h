#pragma once

// ============================================================================
// Sensor Manager
// ============================================================================
// This module manages DS18B20 temperature sensor and XKC-Y25-V water level
// sensor with non-blocking operation, fault detection, and history tracking.
// ============================================================================

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "HardwareConfig.h"
#include "SafetyConfig.h"
#include "TimingConfig.h"

// ----------------------------------------------------------------------------
// Temperature Sensor State Machine
// ----------------------------------------------------------------------------

enum TempSensorState {
    TEMP_IDLE,      // Waiting for next read interval
    TEMP_WAITING,   // Waiting for conversion to complete
    TEMP_PROCESS    // Processing temperature reading
};

// ----------------------------------------------------------------------------
// Sensor Manager Class
// ============================================================================

class SensorManager {
public:
    // ------------------------------------------------------------------------
    // Public Methods
    // ------------------------------------------------------------------------
    
    /**
     * @brief Initialize sensors (DS18B20 and water level)
     * Call once in setup() after hardware initialization
     */
    void begin();
    
    /**
     * @brief Non-blocking update for all sensors
     * Call repeatedly in loop()
     */
    void update();
    
    // Temperature sensor getters
    float getTemperature();                      // Get current temperature (°C)
    bool isTemperatureSensorOperational();       // Sensor operational status
    uint8_t getTemperatureSensorErrorCount();    // Consecutive error count
    bool hasTemperatureSensorFault();            // Fault status
    
    // Water level sensor getters
    bool isWaterLevelSufficient();               // Current water level status
    bool hasWaterLevelFault();                   // Water level fault status
    unsigned long getWaterLevelFaultDuration();  // Time in fault (ms)

private:
    // ------------------------------------------------------------------------
    // Temperature Sensor Members
    // ------------------------------------------------------------------------
    
    OneWire oneWire;                             // OneWire instance
    DallasTemperature sensors;                   // Dallas Temperature instance
    TempSensorState tempState;                   // Current state machine state
    float currentTemperature;                    // Latest valid reading (°C)
    unsigned long lastTempReadTime;              // Last read timestamp
    unsigned long tempRequestTime;               // Conversion request timestamp
    uint8_t tempErrorCount;                      // Consecutive error count
    uint8_t tempValidCount;                      // Consecutive valid count
    bool tempSensorOperational;                  // Sensor operational flag
    
    // Temperature history buffer (for thermal runaway detection in Phase 5)
    float temperatureHistory[TEMP_HISTORY_SIZE]; // Circular buffer
    uint8_t historyIndex;                        // Current write index
    uint8_t historyCount;                        // Number of readings stored
    
    // ------------------------------------------------------------------------
    // Water Level Sensor Members
    // ------------------------------------------------------------------------
    
    unsigned long lastWaterLevelReadTime;        // Last read timestamp
    bool waterLevelSufficient;                   // Current status
    unsigned long waterLevelFaultStartTime;      // Fault start timestamp
    unsigned long waterLevelRecoveryStartTime;   // Recovery start timestamp
    bool waterLevelFaultActive;                  // Fault active flag
    
    // ------------------------------------------------------------------------
    // Private Methods
    // ------------------------------------------------------------------------
    
    void updateTemperatureSensor();              // Temperature state machine
    void updateWaterLevelSensor();               // Water level polling
    bool validateTemperature(float temp);        // Range validation
    void addToHistory(float temp);               // History buffer management
};

