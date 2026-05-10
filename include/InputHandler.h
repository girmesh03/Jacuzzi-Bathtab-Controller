#pragma once

#include <Arduino.h>
#include <ezButton.h>
#include "HardwareConfig.h"
#include "TimingConfig.h"

// ============================================================================
// InputHandler - Rotary Encoder Input Management
// ============================================================================
// Manages KY-040 rotary encoder input with debouncing.
// Provides rotation detection (CW/CCW) and button press handling.
// Non-blocking polling-based implementation.
//
// Pin Mapping:
// - CLK: D6 (GPIO12)
// - DT:  D4 (GPIO2) - Digital input
// - SW:  D3 (GPIO0) - Boot-strap sensitive, INPUT_PULLUP
//
// Button Handling: Uses ezButton library for reliable debouncing
// Encoder Handling: Manual implementation with step counting
// ============================================================================

class InputHandler {
public:
    // ------------------------------------------------------------------------
    // Constructor
    // ------------------------------------------------------------------------
    InputHandler();
    
    // ------------------------------------------------------------------------
    // Public Methods
    // ------------------------------------------------------------------------
    
    /**
     * @brief Initialize input handler and encoder pins
     * @return true if initialization successful, false otherwise
     */
    bool begin();
    
    /**
     * @brief Non-blocking update - call in main loop
     * Reads encoder state and detects rotation/button events
     */
    void update();
    
    /**
     * @brief Check if encoder was rotated clockwise since last check
     * @return true if CW rotation detected, false otherwise
     * NOTE: This is a one-shot check - returns true once then clears
     */
    bool wasRotatedCW();
    
    /**
     * @brief Check if encoder was rotated counter-clockwise since last check
     * @return true if CCW rotation detected, false otherwise
     * NOTE: This is a one-shot check - returns true once then clears
     */
    bool wasRotatedCCW();
    
    /**
     * @brief Check if button was pressed since last check
     * @return true if button press detected, false otherwise
     * NOTE: This is a one-shot check - returns true once then clears
     */
    bool wasButtonPressed();
    
    /**
     * @brief Check if button was released since last check
     * @return true if button release detected, false otherwise
     * NOTE: This is a one-shot check - returns true once then clears
     */
    bool wasButtonReleased();
    
    /**
     * @brief Get current button state (for hold detection)
     * @return true if button currently pressed, false otherwise
     */
    bool isButtonPressed();
    
    /**
     * @brief Check if button is being held down for long press duration
     * @return true if button held for ENCODER_HOLD_DURATION_MS or longer
     */
    bool isButtonHeld();
    
    /**
     * @brief Get button press count (for multi-press detection)
     * @return Number of presses detected
     */
    uint8_t getButtonPressCount();

private:
    // ------------------------------------------------------------------------
    // Private Members
    // ------------------------------------------------------------------------
    
    // ezButton instance for button handling
    ezButton button;
    
    // Encoder state
    uint8_t lastCLKState;
    uint8_t lastDTState;
    
    // Event flags (one-shot)
    bool rotatedCW;
    bool rotatedCCW;
    
    // Button hold detection
    unsigned long buttonPressStartTime;  // Time when button was first pressed
    bool buttonWasPressed;               // Track previous button state for edge detection
    
    
    // ------------------------------------------------------------------------
    // Private Methods
    // ------------------------------------------------------------------------
    
    /**
     * @brief Read and process encoder rotation
     */
    void readEncoder();
};
