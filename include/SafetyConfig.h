#pragma once

// ============================================================================
// Safety Configuration
// ============================================================================
// This file contains ALL safety-related thresholds, temperature limits,
// and fault detection parameters. NO hardcoded values allowed elsewhere.
// ============================================================================

// ----------------------------------------------------------------------------
// Temperature Thresholds
// ----------------------------------------------------------------------------

// Valid temperature range for sensor readings
#define TEMP_VALID_MIN -10.0f  // Minimum valid temperature (°C)
#define TEMP_VALID_MAX 60.0f   // Maximum valid temperature (°C)

// Operational temperature thresholds
#define TEMP_WARNING_THRESHOLD 42.0f   // Warning threshold (°C) - enters Warning state
#define TEMP_CRITICAL_THRESHOLD 45.0f  // Critical fault threshold (°C) - enters Fault state

// Heater control
#define TEMP_DEFAULT_SETPOINT 38.0f  // Default heater target temperature (°C) when user hasn't set target

// ----------------------------------------------------------------------------
// Thermal Runaway Detection
// ----------------------------------------------------------------------------
// Detects unexpected temperature CHANGE or abnormal rate-of-change
// (not just absolute threshold)

#define TEMP_HISTORY_SIZE 10          // Number of temperature readings to track for runaway detection
#define TEMP_DELTA_THRESHOLD 5.0f     // Maximum temperature delta (°C) over history window
#define TEMP_RATE_THRESHOLD 0.5f      // Maximum rate of change (°C/second)
#define TEMP_HISTORY_TIME_WINDOW 20   // Time window for history analysis (seconds)

// ----------------------------------------------------------------------------
// Sensor Fault Detection
// ----------------------------------------------------------------------------

// Temperature sensor fault thresholds
#define SENSOR_ERROR_THRESHOLD 3     // Consecutive invalid readings before fault
#define SENSOR_RECOVERY_THRESHOLD 3  // Consecutive valid readings to clear fault

// ----------------------------------------------------------------------------
// Water Level Safety
// ----------------------------------------------------------------------------

// Water level fault timing
#define WATER_LEVEL_FAULT_DURATION_MS 10000  // Duration of insufficient water before fault (10 seconds)
#define WATER_LEVEL_STABILIZATION_MS 5000    // Stabilization period after water restored (5 seconds)

// ----------------------------------------------------------------------------
// Safety Notes
// ----------------------------------------------------------------------------
// 1. Thermal runaway requires manual acknowledgment (cannot auto-clear)
// 2. High temperature warning (42°C) is non-fault, allows continued operation
// 3. Critical overtemperature (45°C) is fault, requires safe shutdown
// 4. Water level fault auto-clears after stabilization period
// 5. Temperature sensor fault auto-clears after consecutive valid readings
