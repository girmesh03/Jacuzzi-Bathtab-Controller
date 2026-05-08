#pragma once

// ============================================================================
// Timing Configuration
// ============================================================================
// This file contains ALL timing constants for sensors, display, input,
// features, and shutdown sequences. NO hardcoded values allowed elsewhere.
// All values in milliseconds unless otherwise noted.
// ============================================================================

// ----------------------------------------------------------------------------
// Sensor Timing
// ----------------------------------------------------------------------------

// Temperature sensor (DS18B20)
#define TEMP_SENSOR_READ_INTERVAL_MS 2000    // Read temperature every 2 seconds
#define TEMP_SENSOR_CONVERSION_TIME_MS 750   // DS18B20 conversion time (~750ms for 12-bit)

// Water level sensor (XKC-Y25-V)
#define WATER_LEVEL_READ_INTERVAL_MS 500  // Read water level every 500ms (fast safety response)

// ----------------------------------------------------------------------------
// Display Timing
// ----------------------------------------------------------------------------

#define DISPLAY_UPDATE_INTERVAL_MS 100  // Display refresh rate (100ms = 10Hz maximum)

// ----------------------------------------------------------------------------
// State Machine Timing
// ----------------------------------------------------------------------------

#define BOOT_STATE_MINIMUM_DURATION_MS 3000       // Boot state minimum duration (3 seconds for Power-Up UI display)
#define SELF_CHECK_MINIMUM_DURATION_MS 2000       // Self-check minimum duration (2 seconds for Initialization UI visibility)

// ----------------------------------------------------------------------------
// Input Timing
// ----------------------------------------------------------------------------

// Rotary encoder
#define ENCODER_DEBOUNCE_MS 50          // Button debounce time
#define ENCODER_HOLD_DURATION_MS 1000   // Long press duration

// ----------------------------------------------------------------------------
// Feature Timing
// ----------------------------------------------------------------------------

// Circulation pump
#define CIRCULATION_DELAYED_START_MS 2000  // Countdown before circulation pump starts after user selection

// Water heater
#define HEATER_AUTO_START_DELAY_MS 5000  // Auto-start delay after circulation starts

// ----------------------------------------------------------------------------
// Safe Shutdown Sequence Timing
// ----------------------------------------------------------------------------
// Priority order: Heater first (immediate), then other loads, Circulation last
// All delays are non-blocking using millis()

#define SHUTDOWN_HEATER_DELAY_MS 0           // Heater OFF immediately (highest priority)
#define SHUTDOWN_OZONE_DELAY_MS 100          // Ozone OFF after 100ms
#define SHUTDOWN_JET_DELAY_MS 100            // Jet OFF after 100ms
#define SHUTDOWN_MASSAGE_DELAY_MS 100        // Massage OFF after 100ms
#define SHUTDOWN_SPEAKER_LIGHTS_DELAY_MS 100 // Speaker and Lights OFF after 100ms
#define SHUTDOWN_CIRCULATION_DELAY_MS 200    // Circulation OFF last (after 200ms)

// ----------------------------------------------------------------------------
// I2C Timing
// ----------------------------------------------------------------------------

#define I2C_TRANSACTION_TIMEOUT_MS 1000  // I2C transaction timeout

// ----------------------------------------------------------------------------
// Buzzer Timing
// ----------------------------------------------------------------------------

#define BUZZER_BRIEF_MS 50           // Brief feedback beep
#define BUZZER_CONFIRMATION_MS 200   // Confirmation beep (feature activation)
#define BUZZER_WARNING_MS 500        // Warning alert
#define BUZZER_ALERT_MS 1000         // Fault alert

// ----------------------------------------------------------------------------
// UI Feedback Timing
// ----------------------------------------------------------------------------

#define UI_FEEDBACK_DELAY_MS 100  // Visual feedback response time for encoder interactions

// ----------------------------------------------------------------------------
// Timing Notes
// ----------------------------------------------------------------------------
// 1. All timing uses millis()-based non-blocking architecture
// 2. NO delay() calls allowed in application logic
// 3. Safe shutdown sequence ensures heater stops first, circulation last
// 4. Display update rate limited to 10Hz to prevent I2C bus saturation
// 5. Water level read interval fast (500ms) for safety-critical monitoring
