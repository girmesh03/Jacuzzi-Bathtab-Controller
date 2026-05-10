#pragma once

// ============================================================================
// Hardware Configuration
// ============================================================================
// This file contains ALL hardware pin assignments, I2C addresses, and
// hardware-specific constants. NO hardcoded values allowed elsewhere.
//
// CRITICAL BOOT-STRAP PINS:
// - GPIO15 (D8): MUST be LOW at boot (buzzer pin)
// - GPIO0 (D3): MUST NOT be forced LOW at boot (encoder switch)
// ============================================================================

// ----------------------------------------------------------------------------
// Pin Assignments
// ----------------------------------------------------------------------------

// ⚠️ BOOT-STRAP SENSITIVE - must be LOW at boot
#define PIN_BUZZER 15  // D8/GPIO15 - Buzzer output (active-high)

// Temperature sensor
#define PIN_TEMP_SENSOR 14  // D5/GPIO14 - DS18B20 OneWire

// Water level sensor
#define PIN_WATER_LEVEL 13  // D7/GPIO13 - XKC-Y25-V (active-low: LOW=sufficient, HIGH=insufficient)

// Rotary encoder pins
#define PIN_ENCODER_CLK 12  // D6/GPIO12 - Encoder CLK
#define PIN_ENCODER_DT 2    // D4/GPIO2 - Encoder DT (D0 reserved for heartbeat LED, D1/D2 are I2C)
#define PIN_ENCODER_SW 0    // D3/GPIO0 - Encoder SW ⚠️ BOOT-STRAP SENSITIVE - needs pull-up

// ----------------------------------------------------------------------------
// I2C Configuration
// ----------------------------------------------------------------------------

// I2C device addresses
#define I2C_ADDRESS_OLED 0x3C     // SH110X OLED display
#define I2C_ADDRESS_PCF8574 0x20  // PCF8574 GPIO expander for relay control

// I2C bus speed
#define I2C_CLOCK_SPEED 100000  // 100kHz (standard mode for reliability)

// ----------------------------------------------------------------------------
// Relay Channel Mapping (PCF8574 GPIO Expander)
// ----------------------------------------------------------------------------
// Active-low logic: HIGH = OFF, LOW = ON
// All relays controlled via single byte write to PCF8574

#define RELAY_CHANNEL_CIRCULATION 0  // Channel 0 (Bit 0) - Circulation pump (foundation for all features)
#define RELAY_CHANNEL_MASSAGE 1      // Channel 1 (Bit 1) - Massage pump (requires circulation)
#define RELAY_CHANNEL_JET 2          // Channel 2 (Bit 2) - Jet pump (requires circulation)
#define RELAY_CHANNEL_HEATER 3       // Channel 3 (Bit 3) - Water heater (3kW load, 5V/30A relay, requires circulation)
#define RELAY_CHANNEL_OZONE 4        // Channel 4 (Bit 4) - Ozone generator (requires circulation)
#define RELAY_CHANNEL_SPEAKER 5      // Channel 5 (Bit 5) - Speaker relay (requires circulation)
#define RELAY_CHANNEL_LIGHTS 6       // Channel 6 (Bit 6) - Light system (requires circulation)
#define RELAY_CHANNEL_SPARE 7        // Channel 7 (Bit 7) - Spare channel (RESERVED, permanently OFF)

// Relay control constants
#define RELAY_ALL_OFF 0xFF  // All relays OFF (active-low: all bits HIGH)
#define RELAY_ALL_ON 0x00   // All relays ON (active-low: all bits LOW) - NEVER USE except for testing

// ----------------------------------------------------------------------------
// Display Configuration
// ----------------------------------------------------------------------------

#define DISPLAY_WIDTH 128        // OLED display width in pixels
#define DISPLAY_HEIGHT 64        // OLED display height in pixels
#define DISPLAY_RESET_PIN -1     // No reset pin connected (uses I2C)

// ----------------------------------------------------------------------------
// Serial Debug Configuration
// ----------------------------------------------------------------------------

#define SERIAL_BAUD_RATE 115200  // Serial debug baud rate
#define SERIAL_WAIT_TIMEOUT_MS 3000  // Maximum wait time for serial connection (ms)

// ----------------------------------------------------------------------------
// Debug Timing
// ----------------------------------------------------------------------------

#define DEBUG_PRINT_INTERVAL_MS 5000  // Periodic debug output interval (ms)

// ----------------------------------------------------------------------------
// Hardware Notes
// ----------------------------------------------------------------------------
// 1. Boot-strap pins (GPIO0, GPIO15) require careful circuit design
// 2. GPIO15 (D8) MUST be LOW at boot - initialize FIRST in setup()
// 3. GPIO0 (D3) circuit must include pull-up resistor
// 4. D4 (GPIO2) selected as encoder DT after A0 (ADC0/GPIO17) validation determined unreliable
// 5. All relays use active-low logic through PCF8574
// 6. Spare relay channel (bit 7) reserved for future use
