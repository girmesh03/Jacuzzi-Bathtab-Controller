#include "InputHandler.h"

// ============================================================================
// Debug Macros
// ============================================================================
#ifdef ENABLE_SERIAL_DEBUG
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
#endif

// ============================================================================
// InputHandler Implementation
// ============================================================================

InputHandler::InputHandler()
    : button(PIN_ENCODER_SW),  // Initialize ezButton with encoder switch pin
      lastCLKState(HIGH),
      lastDTState(HIGH),
      rotatedCW(false),
      rotatedCCW(false),
      buttonPressStartTime(0),
      buttonWasPressed(false),
      lastEncoderReadTime(0) {
}

// ----------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------

bool InputHandler::begin() {
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("Initializing Input Handler..."));
    
    // Configure encoder pins
    // CLK and DT with pull-ups for reliable reading
    pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
    pinMode(PIN_ENCODER_DT, INPUT);  // D4 as digital input
    
    // SW already configured in main.cpp as boot-strap sensitive pin
    // ezButton will use the existing pin configuration
    
    // Configure ezButton
    button.setDebounceTime(ENCODER_DEBOUNCE_MS);  // Use debounce time from TimingConfig.h
    
    // Read initial encoder states
    lastCLKState = digitalRead(PIN_ENCODER_CLK);
    lastDTState = digitalRead(PIN_ENCODER_DT);
    
    DEBUG_PRINTLN(F("Input Handler initialization complete"));
    DEBUG_PRINT(F("  CLK initial state: "));
    DEBUG_PRINTLN(lastCLKState);
    DEBUG_PRINT(F("  DT initial state: "));
    DEBUG_PRINTLN(lastDTState);
    DEBUG_PRINT(F("  Button debounce time: "));
    DEBUG_PRINT(ENCODER_DEBOUNCE_MS);
    DEBUG_PRINTLN(F(" ms"));
    DEBUG_PRINTLN(F(""));
    
    return true;
}

// ----------------------------------------------------------------------------
// Non-Blocking Update
// ----------------------------------------------------------------------------

void InputHandler::update() {
    // Update ezButton state (must be called in loop)
    button.loop();
    
    // Track button press time for hold detection
    bool currentButtonState = (button.getState() == LOW);  // Active LOW
    
    if (currentButtonState && !buttonWasPressed) {
        // Button just pressed - record start time
        buttonPressStartTime = millis();
    } else if (!currentButtonState && buttonWasPressed) {
        // Button just released - reset
        buttonPressStartTime = 0;
    }
    
    buttonWasPressed = currentButtonState;
    
    // Read encoder rotation
    readEncoder();
}

// ----------------------------------------------------------------------------
// Event Getters (One-Shot)
// ----------------------------------------------------------------------------

bool InputHandler::wasRotatedCW() {
    if (rotatedCW) {
        rotatedCW = false;  // Clear flag after reading
        return true;
    }
    return false;
}

bool InputHandler::wasRotatedCCW() {
    if (rotatedCCW) {
        rotatedCCW = false;  // Clear flag after reading
        return true;
    }
    return false;
}

bool InputHandler::wasButtonPressed() {
    if (button.isPressed()) {
        #ifdef ENABLE_SERIAL_DEBUG
            DEBUG_PRINTLN(F("[Input] Button: PRESSED"));
        #endif
        return true;
    }
    return false;
}

bool InputHandler::wasButtonReleased() {
    if (button.isReleased()) {
        #ifdef ENABLE_SERIAL_DEBUG
            DEBUG_PRINTLN(F("[Input] Button: RELEASED"));
        #endif
        return true;
    }
    return false;
}

bool InputHandler::isButtonPressed() {
    return (button.getState() == LOW);  // Active LOW
}

bool InputHandler::isButtonHeld() {
    // Check if button is currently pressed
    if (button.getState() == LOW && buttonPressStartTime > 0) {
        // Calculate how long it's been pressed
        unsigned long pressDuration = millis() - buttonPressStartTime;
        
        if (pressDuration >= ENCODER_HOLD_DURATION_MS) {
            #ifdef ENABLE_SERIAL_DEBUG
                DEBUG_PRINT(F("[Input] Button: HELD ("));
                DEBUG_PRINT(pressDuration);
                DEBUG_PRINTLN(F(" ms)"));
            #endif
            return true;
        }
    }
    return false;
}

uint8_t InputHandler::getButtonPressCount() {
    return button.getCount();
}

// ----------------------------------------------------------------------------
// Encoder Reading
// ----------------------------------------------------------------------------

void InputHandler::readEncoder() {
    uint8_t currentCLKState = digitalRead(PIN_ENCODER_CLK);
    
    // Trigger only on RISING edge of CLK for smooth 1:1 per-detent response
    // (every CLK transition = 2 events/detent → overshoots; rising-only = 1 event/detent)
    if (currentCLKState == HIGH && lastCLKState == LOW) {
        uint8_t currentDTState = digitalRead(PIN_ENCODER_DT);
        
        // INVERTED LOGIC (hardware CLK/DT physically reversed):
        // DT same as CLK → CW, DT different → CCW
        if (currentDTState == currentCLKState) {
            rotatedCW = true;
        } else {
            rotatedCCW = true;
        }
    }
    
    lastCLKState = currentCLKState;
}