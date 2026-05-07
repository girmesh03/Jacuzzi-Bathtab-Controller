#pragma once

// ============================================================================
// Fault Codes
// ============================================================================
// This file contains fault code enumerations as bit field for supporting
// multiple simultaneous faults. NO hardcoded fault values allowed elsewhere.
// ============================================================================

// ----------------------------------------------------------------------------
// Fault Code Bit Field
// ----------------------------------------------------------------------------
// Each fault is a unique bit, allowing multiple faults to be active
// simultaneously. Use bitwise OR to set faults, bitwise AND to check.

enum FaultCode {
    FAULT_NONE = 0,                              // No faults active
    FAULT_LOW_WATER_LEVEL = (1 << 0),            // Bit 0: Insufficient water level
    FAULT_HIGH_TEMPERATURE = (1 << 1),           // Bit 1: Critical overtemperature
    FAULT_THERMAL_RUNAWAY = (1 << 2),            // Bit 2: Unexpected temperature change/rate
    FAULT_TEMPERATURE_SENSOR = (1 << 3),         // Bit 3: Temperature sensor failure
    FAULT_I2C_FAILURE = (1 << 4),                // Bit 4: I2C bus communication failure
    FAULT_PCF8574_FAILURE = (1 << 5)             // Bit 5: PCF8574 relay controller failure
};

// ----------------------------------------------------------------------------
// Fault Descriptions
// ----------------------------------------------------------------------------
// FAULT_LOW_WATER_LEVEL:
//   - Trigger: Water level sensor indicates insufficient water for extended duration
//   - Display: low_water_level_error_bitmap
//   - Auto-clear: Yes, after stabilization period of sufficient water
//   - Action: Prevent pump/heater activation, execute safe shutdown if active
//
// FAULT_HIGH_TEMPERATURE:
//   - Trigger: Temperature exceeds critical fault threshold (45°C)
//   - Display: high_temperature_error_bitmap
//   - Auto-clear: Yes, when temperature drops below threshold
//   - Action: Deactivate heater immediately, execute safe shutdown
//
// FAULT_THERMAL_RUNAWAY:
//   - Trigger: Unexpected temperature CHANGE or abnormal rate-of-change
//   - Display: high_temperature_error_bitmap
//   - Auto-clear: NO - requires manual acknowledgment
//   - Action: Deactivate heater immediately, execute safe shutdown
//   - CRITICAL: Manual acknowledgment required before heater can operate again
//
// FAULT_TEMPERATURE_SENSOR:
//   - Trigger: Consecutive invalid readings beyond threshold or communication timeout
//   - Display: sensor_error_bitmap
//   - Auto-clear: Yes, after consecutive valid readings
//   - Action: Prevent heater activation, may prevent circulation depending on policy
//
// FAULT_I2C_FAILURE:
//   - Trigger: I2C bus transaction timeout
//   - Display: sensor_error_bitmap (or specific I2C error bitmap if available)
//   - Auto-clear: Yes, when communication restored
//   - Action: Enter fault state, attempt no further I2C operations until restored
//
// FAULT_PCF8574_FAILURE:
//   - Trigger: PCF8574 communication fails
//   - Display: sensor_error_bitmap (or specific relay error bitmap if available)
//   - Auto-clear: Yes, when communication restored
//   - Action: Enter fault state, attempt no further relay operations until restored

// ----------------------------------------------------------------------------
// Fault Handling Notes
// ----------------------------------------------------------------------------
// 1. Multiple faults can be active simultaneously (bit field)
// 2. Fault_Inspection state allows browsing through active faults
// 3. Each fault displays with specific error bitmap
// 4. Auto-clear faults resolve when underlying condition fixed
// 5. Manual acknowledgment faults (thermal runaway) require user action
// 6. Boot fault persistence: system stays in Fault until ALL conditions safe
//
// Usage Examples:
//   Set fault:   activeFaults |= FAULT_LOW_WATER_LEVEL;
//   Clear fault: activeFaults &= ~FAULT_LOW_WATER_LEVEL;
//   Check fault: if (activeFaults & FAULT_LOW_WATER_LEVEL) { ... }
//   Count faults: __builtin_popcount(activeFaults)
