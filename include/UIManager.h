#pragma once

#include <Arduino.h>
#include <Adafruit_SH110X.h>
#include "HardwareConfig.h"
#include "TimingConfig.h"
#include "StateDefinitions.h"
#include "Bitmaps.h"
#include "SensorManager.h"

// Forward declaration to avoid circular dependency
class SafetySystem;
class StateMachine;

// ============================================================================
// UIManager - Bitmap-Only Display Management
// ============================================================================
// Manages OLED display with bitmap-only rendering (NO text functions).
// All bitmaps scaled down by factor of 2 (absolute rule).
// All text rendered as bitmap glyphs from PROGMEM.
// Non-blocking display updates integrated into event loop.
//
// UI States (corresponding to System States):
// - Power-Up UI: Boot splash screen (water_drop_bitmap, 3+ seconds)
// - Initialization/Safety UI: Self-check progress (settings_bitmap + status text)
// - Ready UI: Temperature display (thermometer_bitmap + numeric value)
// - Main Menu UI: Start circulation
// - Circulation UI: Feature control (scrollable feature list)

// - Warning UI: Warning overlay
// - Fault UI: Fault display (error bitmaps)
// - Fault_Inspection UI: Multi-fault browsing
// ============================================================================

// UI State enumeration (maps to System States)
enum UIState {
    UI_POWER_UP,              // Boot splash screen (STATE_BOOT)
    UI_INITIALIZATION,        // Self-check progress (STATE_SELF_CHECK)
    UI_READY,                 // Ready screen (STATE_READY)
    UI_MAIN_MENU,             // Main menu navigation
    UI_CIRCULATION,           // Feature control (STATE_ACTIVE_CIRCULATION, STATE_FEATURE_ENABLED_BATH)
    UI_SETTINGS_AND_ERROR,    // (reserved, not used)
    UI_WARNING,               // Warning overlay (STATE_WARNING)
    UI_FAULT,                 // Fault display (STATE_FAULT)
    UI_FAULT_INSPECTION       // Multi-fault browsing (STATE_FAULT_INSPECTION)
};

class UIManager {
public:
    // ------------------------------------------------------------------------
    // Constructor
    // ------------------------------------------------------------------------
    UIManager(SensorManager& sensors, SafetySystem& safety, StateMachine& stateMachine);
    
    // ------------------------------------------------------------------------
    // Public Methods
    // ------------------------------------------------------------------------
    
    /**
     * @brief Initialize UI Manager and OLED display
     * @return true if initialization successful, false otherwise
     */
    bool begin();
    
    /**
     * @brief Non-blocking update - call in main loop
     * Updates display at rate defined in TimingConfig.h
     * @param currentSystemState Current system state from StateMachine
     */
    void update(SystemState currentSystemState);
    
    /**
     * @brief Get current UI state
     * @return Current UI state
     */
    UIState getCurrentUIState() const { return currentUIState; }
    
    /**
     * @brief Force display redraw on next update
     * Call this when dynamic content changes (e.g., temperature update)
     */
    void requestRedraw() { needsRedraw = true; }
    
    /**
     * @brief Force UI state change (for testing/debugging only)
     * @param newState New UI state to display
     * 
     * CRITICAL: This bypasses normal state machine logic
     * Use only for Phase 6/7 testing before full state machine integration
     */
    void forceUIState(UIState newState);
    
    /**
     * @brief Get current main menu selected index
     * @return 0 = Circulation
     */
    uint8_t getMainMenuSelectedIndex() const { return mainMenuSelectedIndex; }
    
    /**
     * @brief Set main menu selected index
     * @param index 0 = Circulation
     */
    void setMainMenuSelectedIndex(uint8_t index) {
        if (index == 0) {  // Only 1 menu item (Circulation)
            mainMenuSelectedIndex = index;
            needsRedraw = true;
        }
    }
    
    /**
     * @brief Navigate main menu (left/right)
     * @param direction -1 = left, +1 = right
     */
    void navigateMainMenu(int8_t direction);
    
    /**
     * @brief Select current main menu item
     * Transitions to Circulation UI
     */
    void selectMainMenuItem();
    
    /**
     * @brief Get current circulation menu selected index
     * @return 0-7: circulation, massage, jet, heater, ozone, lights, speaker, thermometer
     */
    uint8_t getCirculationMenuSelectedIndex() const { return circulationMenuSelectedIndex; }
    
    /**
     * @brief Set circulation menu selected index
     * @param index 0-7: circulation, massage, jet, heater, ozone, lights, speaker, thermometer
     */
    void setCirculationMenuSelectedIndex(uint8_t index) {
        if (index <= 7) {  // 8 menu items (0-7)
            circulationMenuSelectedIndex = index;
            needsRedraw = true;
        }
    }
    
    /**
     * @brief Navigate circulation menu (left/right)
     * @param direction -1 = left/up, +1 = right/down
     */
    void navigateCirculationMenu(int8_t direction);
    
    /**
     * @brief Adjust temperature on thermometer screen
     * @param direction -1 = decrease, +1 = increase
     * 
     * Only active when thermometer item (index 7) is selected
     * Adjusts temperature in 0.5°C increments
     */
    void adjustTemperature(int8_t direction);
    
    /**
     * @brief Confirm temperature setting on thermometer screen
     * 
     * Sets the adjusted temperature as the new target temperature
     * Only active when thermometer item (index 7) is selected
     */
    void confirmTemperatureSetting();
    
    /**
     * @brief Get adjusted temperature value
     * @return Current temperature being adjusted (only valid when tempAdjustmentActive)
     */
    float getAdjustedTemperature() const { return tempAdjustmentValue; }
    
    /**
     * @brief Check if temperature adjustment is active
     * @return true if user is currently adjusting temperature
     */
    bool isTemperatureAdjustmentActive() const { return tempAdjustmentActive; }
    
    /**
     * @brief Show a denial message overlay on screen
     * @param message Brief text shown between bitmap and label (e.g., "Start")
     * @param bottomLabel Optional replacement text for bottom feature label (e.g., "Circulation")
     * Message(s) appear for DENIAL_MESSAGE_DURATION_MS then clear
     */
    void showDenialMessage(const char* message, const char* bottomLabel = nullptr);
    
    /**
     * @brief Toggle current circulation menu item on/off
     * Phase 9 will implement actual relay control
     * For now, provides visual feedback only
     */
    void toggleCirculationMenuItem();
    
    /**
     * @brief Navigate fault inspection (left/right)
     * @param direction -1 = left/previous, +1 = right/next
     * 
     * Scrolls through active faults in Fault Inspection UI
     */
    void navigateFaultInspection(int8_t direction);
    
    /**
     * @brief Get current fault inspection index
     * @return Current fault being inspected (0-based)
     */
    uint8_t getFaultInspectionIndex() const { return faultInspectionIndex; }
    
    /**
     * @brief Reset fault inspection index to 0
     * Called when entering Fault Inspection UI
     */
    void resetFaultInspectionIndex() { faultInspectionIndex = 0; needsRedraw = true; }
    

    
    /**
     * @brief Draw bitmap scaled down by factor of 2
     * @param bitmap Pointer to bitmap data in PROGMEM
     * @param x X position on display
     * @param y Y position on display
     * @param w Source bitmap width (before scaling)
     * @param h Source bitmap height (before scaling)
     * 
     * CRITICAL: All bitmaps MUST be scaled down by factor of 2 (absolute rule)
     * Source bitmaps designed at 2x resolution, displayed at 1x
     */
    void drawScaledBitmap(const unsigned char* bitmap, int16_t x, int16_t y,
                          int16_t w, int16_t h);
    
    /**
     * @brief Draw text using bitmap glyphs
     * @param text Text string to render
     * @param x X position on display
     * @param y Y position on display
     * 
     * CRITICAL: NO native text rendering functions allowed
     * All text rendered as bitmap glyphs from PROGMEM
     */
    void drawText(const char* text, int16_t x, int16_t y);
    
    /**
     * @brief Draw text using bitmap glyphs at ORIGINAL SIZE (8x8, no scaling)
     * @param text Text string to render
     * @param x X position on display
     * @param y Y position on display
     * 
     * Use this for larger, more readable text (e.g., splash screens)
     * Glyphs rendered at 8x8 pixels instead of scaled 4x4
     */
    void drawTextUnscaled(const char* text, int16_t x, int16_t y);
    
    /**
     * @brief Draw text using bitmap glyphs at 2X SIZE (16x16 pixels)
     * @param text Text string to render
     * @param x X position on display
     * @param y Y position on display
     * 
     * Use this for large, prominent text (e.g., temperature display)
     * Glyphs rendered at 16x16 pixels (2x scale from 8x8)
     */
    void drawText2x(const char* text, int16_t x, int16_t y);
    
    /**
     * @brief Draw temperature display using thermometer bitmap and digits
     * @param temperature Temperature value in °C
     * @param x X position on display
     * @param y Y position on display
     * 
     * Renders thermometer_bitmap and numeric value with degree symbol
     * All rendered as bitmaps (NO text functions)
     */
    void drawTemperature(float temperature, int16_t x, int16_t y);
    
    /**
     * @brief Clear display buffer
     */
    void clearDisplay();
    
    /**
     * @brief Commit display buffer to screen
     */
    void displayBuffer();
    
    /**
     * @brief Check if display is initialized
     * @return true if display ready, false otherwise
     */
    bool isDisplayReady() const { return displayInitialized; }

private:
    // ------------------------------------------------------------------------
    // Private Members
    // ------------------------------------------------------------------------
    
    // Module references
    SensorManager& sensorManager;
    SafetySystem& safetySystem;
    StateMachine& stateMachine;
    
    // Display instance
    Adafruit_SH1106G display;
    
    // Display state
    bool displayInitialized;
    
    // UI state tracking
    UIState currentUIState;
    UIState previousUIState;
    unsigned long uiStateEntryTime;
    
    // System state tracking (for detecting system state changes)
    SystemState previousSystemState;
    
    // Manual UI state change tracking
    bool manualUIStateChange;
    
    // Display refresh control
    bool needsRedraw;
    
    // Timing for non-blocking updates
    unsigned long lastUpdateTime;
    
    // Main Menu UI state
    uint8_t mainMenuSelectedIndex;  // 0 = Circulation
    
    // Circulation UI state
    uint8_t circulationMenuSelectedIndex;  // 0-7: circulation, massage, jet, heater, ozone, lights, speaker, thermometer
    
    // Temperature adjustment state (for thermometer screen)
    float tempAdjustmentValue;  // Current temperature being adjusted
    bool tempAdjustmentActive;  // True when user is adjusting temperature
    
    // Fault inspection state
    uint8_t faultInspectionIndex;  // Current fault being inspected (0-based)
    
    // Fault display state
    uint8_t currentFaultDisplayIndex;  // Current fault being displayed on Fault UI (0-based)
    
    // Denial message overlay state
    char denialMessage[24];      // Message buffer for denial feedback (shown between bitmap and label)
    char denialBottomLabel[24];  // Replacement bottom label during denial (e.g., "Circulation")
    unsigned long denialMessageEndTime;  // millis() when message expires
    
    // ------------------------------------------------------------------------
    // Private Methods
    // ------------------------------------------------------------------------
    
    /**
     * @brief Find glyph bitmap for character
     * @param c Character to find
     * @return Pointer to glyph bitmap in PROGMEM, or nullptr if not found
     */
    const unsigned char* findGlyph(char c);
    
    /**
     * @brief Update UI state based on system state
     * @param systemState Current system state
     */
    void updateUIState(SystemState systemState);
    
    /**
     * @brief Render current UI screen
     */
    void renderCurrentScreen();
    
    /**
     * @brief Render Power-Up UI screen (Task 34)
     * water_drop_bitmap scaled by 2, centered, y=-10, 3+ seconds
     */
    void renderPowerUpScreen();
    
    /**
     * @brief Render Initialization/Safety UI screen (Task 35)
     * settings_bitmap + status text
     */
    void renderInitializationScreen();
    
    /**
     * @brief Render Ready UI screen (Task 36)
     * thermometer_bitmap + temperature value
     */
    void renderReadyScreen();
    
    /**
     * @brief Render Main Menu UI screen (Task 37)
     * circulation_bitmap with "Start" label
     */
    void renderMainMenuScreen();
    
    /**
     * @brief Render Circulation UI screen (Task 38)
     * Scrollable list of features: circulation, massage, jet, heater, ozone, lights, speaker, thermometer
     */
    void renderCirculationScreen();
    
    /**
     * @brief Render Warning UI overlay (Task 40)
     * Warning indicator with high_temperature_error_bitmap
     * Non-critical warnings that don't require shutdown
     */
    void renderWarningScreen();
    
    /**
     * @brief Render Fault UI screen (Task 41)
     * Fault indicator with specific error bitmap
     * Displays highest priority fault with fault count if multiple
     */
    void renderFaultScreen();
    
    /**
     * @brief Render Fault Inspection UI screen (Task 42)
     * Browse through multiple active faults
     * Rotary left/right scrolls, displays fault index and total count
     */
    void renderFaultInspectionScreen();
};

