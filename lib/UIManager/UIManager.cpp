#include "UIManager.h"
#include "SafetySystem.h"  // Need full definition, not just forward declaration
#include "StateMachine.h"  // Need full definition for fault management

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
// UI Layout Constants
// ============================================================================
#define UI_STATUS_Y 2

// ============================================================================
// UIManager Implementation
// ============================================================================

// Constructor initializes display with I2C address
UIManager::UIManager(SensorManager& sensors, SafetySystem& safety, StateMachine& sm) 
    : sensorManager(sensors),
      safetySystem(safety),
      stateMachine(sm),
      display(DISPLAY_WIDTH, DISPLAY_HEIGHT, &Wire, DISPLAY_RESET_PIN),
      displayInitialized(false),
      currentUIState(UI_POWER_UP),
      previousUIState(UI_POWER_UP),
      uiStateEntryTime(0),
      previousSystemState(STATE_BOOT),  // Initialize to Boot state
      manualUIStateChange(false),  // No manual change initially
      needsRedraw(true),
      lastUpdateTime(0),
      mainMenuSelectedIndex(0),  // Default to Circulation (index 0)
      circulationMenuSelectedIndex(0),  // Default to Circulation pump (index 0)
      tempAdjustmentValue(0.0f),  // Will be initialized when thermometer selected
      tempAdjustmentActive(false),  // Not adjusting temperature initially
      faultInspectionIndex(0),  // Start at first fault
      currentFaultDisplayIndex(0),  // Start at first fault
      circulationCountdownActive(false),
      circulationCountdownStartTime(0),
      denialMessageEndTime(0) {
    denialMessage[0] = '\0';
    denialBottomLabel[0] = '\0';
}

// ----------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------

bool UIManager::begin() {
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("Initializing UI Manager..."));
    
    // Initialize display with I2C address from HardwareConfig.h
    if (!display.begin(I2C_ADDRESS_OLED, true)) {
        DEBUG_PRINTLN(F("ERROR: OLED display initialization failed"));
        displayInitialized = false;
        return false;
    }
    
    DEBUG_PRINTLN(F("OLED display initialized successfully"));
    
    // Clear display
    display.clearDisplay();
    display.display();
    
    displayInitialized = true;
    lastUpdateTime = millis();
    uiStateEntryTime = millis();
    currentUIState = UI_POWER_UP;
    needsRedraw = true;  // Force initial draw
    
    DEBUG_PRINTLN(F("UI Manager initialization complete"));
    DEBUG_PRINTLN(F(""));
    
    return true;
}

// ----------------------------------------------------------------------------
// Non-Blocking Update
// ----------------------------------------------------------------------------

void UIManager::update(SystemState currentSystemState) {
    if (!displayInitialized) {
        return;
    }
    
    // Update UI state based on system state
    updateUIState(currentSystemState);
    
    // Non-blocking update at rate defined in TimingConfig.h
    unsigned long currentTime = millis();
    
    // Check if denial message expired - force redraw to clear it
    if (denialMessage[0] != '\0' && currentTime >= denialMessageEndTime) {
        denialMessage[0] = '\0';
        denialBottomLabel[0] = '\0';
        needsRedraw = true;
    }
    
    if (currentTime - lastUpdateTime >= DISPLAY_UPDATE_INTERVAL_MS) {
        // Redraw if needed OR if we're in a state that should always show (Boot, Initialization)
        // This ensures splash screens are always visible
        // Also force redraw during circulation countdown for live countdown display
        bool countdownActive = (currentUIState == UI_CIRCULATION &&
                                circulationMenuSelectedIndex == 0 &&
                                circulationCountdownActive);
        bool alwaysShowStates = (currentUIState == UI_POWER_UP || currentUIState == UI_INITIALIZATION) || countdownActive;
        
        if (needsRedraw || alwaysShowStates) {
            // Render current UI screen
            renderCurrentScreen();
            needsRedraw = false;
        }
        
        lastUpdateTime = currentTime;
    }
}

// ----------------------------------------------------------------------------
// UI State Management
// ----------------------------------------------------------------------------

void UIManager::updateUIState(SystemState systemState) {
    // CRITICAL FIX: Only auto-map UI state for states that have NO user navigation
    // States WITH user navigation: Ready, Active_Circulation, Feature_Enabled_Bath, Fault
    // States WITHOUT user navigation: Boot, Self_Check, Warning, Shutdown
    //
    // This prevents the system state from overriding user-driven UI navigation
    
    // If UI state was manually changed (via forceUIState or navigation methods),
    // skip auto-mapping for this frame
    if (manualUIStateChange) {
        manualUIStateChange = false;  // Clear flag
        previousSystemState = systemState;  // Update tracking
        return;  // Skip auto-mapping
    }
    
    bool systemStateChanged = (systemState != previousSystemState);
    
    switch (systemState) {
        case STATE_BOOT:
            // Auto-map: No user navigation
            if (currentUIState != UI_POWER_UP) {
                previousUIState = currentUIState;
                currentUIState = UI_POWER_UP;
                uiStateEntryTime = millis();
                needsRedraw = true;
                
                #ifdef ENABLE_SERIAL_DEBUG
                    DEBUG_PRINT(F("[UI] State changed: "));
                    DEBUG_PRINT((int)previousUIState);
                    DEBUG_PRINT(F(" -> "));
                    DEBUG_PRINTLN((int)currentUIState);
                #endif
            }
            break;
            
        case STATE_SELF_CHECK:
            // Auto-map: No user navigation
            if (currentUIState != UI_INITIALIZATION) {
                previousUIState = currentUIState;
                currentUIState = UI_INITIALIZATION;
                uiStateEntryTime = millis();
                needsRedraw = true;
                
                #ifdef ENABLE_SERIAL_DEBUG
                    DEBUG_PRINT(F("[UI] State changed: "));
                    DEBUG_PRINT((int)previousUIState);
                    DEBUG_PRINT(F(" -> "));
                    DEBUG_PRINTLN((int)currentUIState);
                #endif
            }
            break;
            
        case STATE_READY:
            // User navigation: UI_READY ↔ UI_MAIN_MENU
            // Do NOT auto-map - let user input control UI state
            // Only set default if coming from a different system state
            if (systemStateChanged) {
                previousUIState = currentUIState;
                currentUIState = UI_READY;
                uiStateEntryTime = millis();
                needsRedraw = true;
                
                #ifdef ENABLE_SERIAL_DEBUG
                    DEBUG_PRINT(F("[UI] System state changed to Ready, resetting to UI_READY"));
                    DEBUG_PRINT(F(" (was: "));
                    DEBUG_PRINT((int)previousUIState);
                    DEBUG_PRINTLN(F(")"));
                #endif
            }
            // Otherwise, keep current UI state (user is navigating)
            break;
            
        case STATE_ACTIVE_CIRCULATION:
        case STATE_FEATURE_ENABLED_BATH:
            // User navigation: UI_CIRCULATION (scrolling through items)
            // Do NOT auto-map - let user input control menu position
            // Only set if not already in Circulation UI
            if (currentUIState != UI_CIRCULATION) {
                previousUIState = currentUIState;
                currentUIState = UI_CIRCULATION;
                uiStateEntryTime = millis();
                needsRedraw = true;
                
                #ifdef ENABLE_SERIAL_DEBUG
                    DEBUG_PRINT(F("[UI] State changed: "));
                    DEBUG_PRINT((int)previousUIState);
                    DEBUG_PRINT(F(" -> "));
                    DEBUG_PRINTLN((int)currentUIState);
                #endif
            }
            break;
            
        case STATE_WARNING:
            // Auto-map: No user navigation (warning overlay)
            if (currentUIState != UI_WARNING) {
                previousUIState = currentUIState;
                currentUIState = UI_WARNING;
                uiStateEntryTime = millis();
                needsRedraw = true;
                
                #ifdef ENABLE_SERIAL_DEBUG
                    DEBUG_PRINT(F("[UI] State changed: "));
                    DEBUG_PRINT((int)previousUIState);
                    DEBUG_PRINT(F(" -> "));
                    DEBUG_PRINTLN((int)currentUIState);
                #endif
            }
            break;
            
        case STATE_FAULT:
            // User navigation: UI_FAULT ↔ UI_FAULT_INSPECTION
            // Do NOT auto-map - let user input control
            // Only set default if coming from a different system state
            if (systemStateChanged && 
                previousSystemState != STATE_FAULT_INSPECTION) {
                previousUIState = currentUIState;
                currentUIState = UI_FAULT;
                uiStateEntryTime = millis();
                needsRedraw = true;
                
                #ifdef ENABLE_SERIAL_DEBUG
                    DEBUG_PRINT(F("[UI] System state changed to Fault, setting UI_FAULT"));
                    DEBUG_PRINT(F(" (was: "));
                    DEBUG_PRINT((int)previousUIState);
                    DEBUG_PRINTLN(F(")"));
                #endif
            }
            break;
            
        case STATE_FAULT_INSPECTION:
            // User navigation: Browsing faults
            if (currentUIState != UI_FAULT_INSPECTION) {
                previousUIState = currentUIState;
                currentUIState = UI_FAULT_INSPECTION;
                uiStateEntryTime = millis();
                needsRedraw = true;
                
                #ifdef ENABLE_SERIAL_DEBUG
                    DEBUG_PRINT(F("[UI] State changed: "));
                    DEBUG_PRINT((int)previousUIState);
                    DEBUG_PRINT(F(" -> "));
                    DEBUG_PRINTLN((int)currentUIState);
                #endif
            }
            break;
            
        case STATE_SHUTDOWN:
            // Keep current UI state during shutdown
            break;
    }
    
    // Update previous system state for next frame
    previousSystemState = systemState;
}

// ----------------------------------------------------------------------------
// Force UI State (Testing/Debugging Only)
// ----------------------------------------------------------------------------

void UIManager::forceUIState(UIState newState) {
    if (newState != currentUIState) {
        previousUIState = currentUIState;
        currentUIState = newState;
        uiStateEntryTime = millis();
        needsRedraw = true;
        manualUIStateChange = true;  // Mark as manual change
        
        #ifdef ENABLE_SERIAL_DEBUG
            DEBUG_PRINT(F("[UI] FORCED state change: "));
            DEBUG_PRINT((int)previousUIState);
            DEBUG_PRINT(F(" -> "));
            DEBUG_PRINTLN((int)currentUIState);
        #endif
    }
}

void UIManager::renderCurrentScreen() {
    // Clear display buffer
    clearDisplay();
    
    #ifdef ENABLE_SERIAL_DEBUG
        static UIState lastRenderedState = (UIState)-1;
        if (currentUIState != lastRenderedState) {
            DEBUG_PRINT(F("[UI] Rendering screen for state: "));
            DEBUG_PRINTLN((int)currentUIState);
            lastRenderedState = currentUIState;
        }
    #endif
    
    // Render screen based on current UI state
    switch (currentUIState) {
        case UI_POWER_UP:
            renderPowerUpScreen();
            break;
            
        case UI_INITIALIZATION:
            renderInitializationScreen();
            break;
            
        case UI_READY:
            renderReadyScreen();
            break;
            
        case UI_MAIN_MENU:
            renderMainMenuScreen();
            break;
            
        case UI_CIRCULATION:
            // Task 38 - Circulation UI
            renderCirculationScreen();
            break;
            
        case UI_WARNING:
            // Task 40 - Warning UI
            renderWarningScreen();
            break;
            
        case UI_FAULT:
            // Task 41 - Fault UI
            renderFaultScreen();
            break;
            
        case UI_FAULT_INSPECTION:
            // Task 42 - Fault Inspection UI
            renderFaultInspectionScreen();
            break;
            
        case UI_SETTINGS_AND_ERROR:
            // Settings removed from Main Menu - this state is no longer reachable
            break;
    }
    
    // Draw denial message if active (in gap between bitmap and bottom label)
    if (denialMessage[0] != '\0') {
        int16_t msgWidth = strlen(denialMessage) * UI_GLYPH_SPACING;
        int16_t msgX = (DISPLAY_WIDTH - msgWidth) / 2;
        int16_t msgY = 40;
        drawTextUnscaled(denialMessage, msgX, msgY);
    }
    
    // Commit display buffer to screen
    displayBuffer();
}

// ----------------------------------------------------------------------------
// Task 34: Power-Up UI Screen
// ----------------------------------------------------------------------------
// NOTE: Original requirement specified scale-by-2, but for better visibility
// on the 128x64 OLED display, we're displaying at full size (1:1).
// This provides a better user experience for the boot splash screen.

void UIManager::renderPowerUpScreen() {
    // Requirement 9.16: Power-Up UI
    // - water_drop_bitmap at FULL SIZE (128x64) - NO SCALING for better visibility
    // - horizontally centered
    // - vertically centered
    // - visible for at least 3 seconds
    // - NO text or labels
    
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Power-Up screen (water drop)"));
            firstRender = false;
        }
    #endif
    
    // Display at full size for maximum visibility
    int16_t fullWidth = WATER_DROP_BMPWIDTH;   // 128 pixels
    int16_t fullHeight = WATER_DROP_BMPHEIGHT; // 64 pixels
    
    // Display is 128x64
    // Center horizontally: (128 - 128) / 2 = 0
    int16_t x = (128 - fullWidth) / 2;  // 0 (full width)
    
    // Center vertically: (64 - 64) / 2 = 0
    int16_t y = (64 - fullHeight) / 2;  // 0 (full height)
    
    // Draw water drop bitmap at FULL SIZE (no scaling), centered
    display.drawBitmap(x, y, water_drop_bitmap, fullWidth, fullHeight, SH110X_WHITE);
    
    // NO text or labels on this screen (bitmap-only)
    
    // Note: 3-second minimum duration is enforced by State Machine
    // (Boot state lasts at least 3 seconds before transitioning to Self_Check)
}

// ----------------------------------------------------------------------------
// Task 35: Initialization/Safety UI Screen
// ----------------------------------------------------------------------------
// NOTE: Original requirement specified scale-by-2, but for better visibility
// on the 128x64 OLED display, we're displaying at full size (1:1).
// Text is positioned at the bottom for maximum visibility.

void UIManager::renderInitializationScreen() {
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Initialization screen"));
            firstRender = false;
        }
    #endif
    
    int16_t sourceWidth = SETTINGS_BMPWIDTH;
    int16_t sourceHeight = SETTINGS_BMPHEIGHT;
    int16_t scaledWidth = sourceWidth / 2;
    
    int16_t bitmapX = (DISPLAY_WIDTH - scaledWidth) / 2;
    int16_t bitmapY = 10;
    
    drawScaledBitmap(settings_bitmap, bitmapX, bitmapY, sourceWidth, sourceHeight);
    
    const char* text = "Initializing";
    int16_t textWidth = strlen(text) * UI_GLYPH_SPACING;
    int16_t textX = (DISPLAY_WIDTH - textWidth) / 2;
    int16_t textY = bitmapY + (sourceHeight / 2) + 4;
    
    drawTextUnscaled(text, textX, textY);
}

// ----------------------------------------------------------------------------
// Task 36: Ready UI Screen
// ----------------------------------------------------------------------------

void UIManager::renderReadyScreen() {
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Ready screen"));
            firstRender = false;
        }
    #endif

    float currentTemp = sensorManager.getTemperature();

    // Left: Thermometer bitmap (scaled 64x32, flush left, vertically centered)
    int16_t thermoY = (DISPLAY_HEIGHT - (THERMOMETER_BMPHEIGHT / 2)) / 2;
    drawScaledBitmap(thermometer_bitmap, 0, thermoY,
                     THERMOMETER_BMPWIDTH, THERMOMETER_BMPHEIGHT);

    // Right: Temperature - integer part in 2x, decimal+unit unscaled
    bool isNegative = (currentTemp < 0);
    float absTemp = isNegative ? -currentTemp : currentTemp;
    int wholePart = (int)absTemp;
    int decimalPart = (int)((absTemp - wholePart) * 10);

    char intStr[4];
    int pi = 0;
    if (isNegative) intStr[pi++] = '-';
    if (wholePart >= 10) intStr[pi++] = '0' + (wholePart / 10);
    intStr[pi++] = '0' + (wholePart % 10);
    intStr[pi] = '\0';

    char decUnitStr[6];
    int di = 0;
    decUnitStr[di++] = '.';
    decUnitStr[di++] = '0' + decimalPart;
    decUnitStr[di++] = '\xB0';
    decUnitStr[di++] = 'C';
    decUnitStr[di] = '\0';

    // 2x text partially overlaps right edge of scaled thermometer (which is mostly empty there)
    int16_t intX = 50;
    int16_t intY = 24;
    drawText2x(intStr, intX, intY);

    // Unscaled text right after 2x text
    int16_t decX = intX + (strlen(intStr) * UI_GLYPH_2X_SPACING);
    int16_t decY = intY + (GLYPH_HEIGHT / 2);
    drawTextUnscaled(decUnitStr, decX, decY);
}

// ----------------------------------------------------------------------------
// Task 37: Main Menu UI Screen
// ----------------------------------------------------------------------------

void UIManager::renderMainMenuScreen() {
    // Requirement 9.19: Main Menu UI
    // - One item visible: circulation_bitmap with "Start" label
    // - Scaled by 2 (128x64 → 64x32)
    // - Bottom-centered bitmap-glyph labels
    // - Rotary left/right scrolls (Phase 7 - Input)
    // - Button confirms selection (Phase 7 - Input)
    
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Main Menu screen"));
            firstRender = false;
        }
    #endif
    
    // Only one item: Circulation pump (Start)
    const unsigned char* menuBitmap = circulation_bitmap;
    const char* menuLabel = "Start";
    int16_t sourceWidth = CIRCULATION_BMPWIDTH;   // 128 pixels
    
    // Scale down bitmap by factor of 2
    int16_t scaledWidth = sourceWidth / 2;   // 64 pixels
    
    int16_t bitmapX = (DISPLAY_WIDTH - scaledWidth) / 2;
    int16_t bitmapY = 8;
    
    drawScaledBitmap(menuBitmap, bitmapX, bitmapY, sourceWidth, CIRCULATION_BMPHEIGHT);
    
    int16_t labelWidth = strlen(menuLabel) * UI_GLYPH_SPACING;
    int16_t labelX = (DISPLAY_WIDTH - labelWidth) / 2;
    int16_t labelY = DISPLAY_HEIGHT - GLYPH_HEIGHT - 4;
    
    drawTextUnscaled(menuLabel, labelX, labelY);
}

// ----------------------------------------------------------------------------
// Task 38: Circulation UI Screen
// ----------------------------------------------------------------------------

void UIManager::renderCirculationScreen() {
    // Requirement 9.7, 9.8: Circulation UI
    // - One item visible at a time, scrollable list
    // - Items: circulation, massage, jet, heater, ozone, lights, speaker, thermometer
    // - All bitmaps scaled down by factor of 2
    // - Rotary left/right scrolls through items (Phase 7 - Input)
    // - Button press toggles selected item on/off (Phase 9 - Feature Control)
    // - ON/OFF status displayed in top right corner
    // - Thermometer screen shows temperature adjustment interface
    
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        static uint8_t lastSelectedIndex = 255;
        if (firstRender || circulationMenuSelectedIndex != lastSelectedIndex) {
            DEBUG_PRINT(F("[UI] Rendering Circulation screen, item: "));
            DEBUG_PRINTLN(circulationMenuSelectedIndex);
            firstRender = false;
            lastSelectedIndex = circulationMenuSelectedIndex;
        }
    #endif
    
    // Special handling for thermometer screen (index 7)
    if (circulationMenuSelectedIndex == 7) {
        float displayTemp;
        if (tempAdjustmentActive) {
            displayTemp = tempAdjustmentValue;
        } else {
            displayTemp = safetySystem.getTargetTemperature();
        }

        // Left: Thermometer bitmap, lifted up 6px from original center
        int16_t thermoY = (DISPLAY_HEIGHT - (THERMOMETER_BMPHEIGHT / 2)) / 2 - 6;
        drawScaledBitmap(thermometer_bitmap, 0, thermoY,
                         THERMOMETER_BMPWIDTH, THERMOMETER_BMPHEIGHT);

        // Right: Temperature - integer part in 2x, decimal+unit unscaled
        bool isNegative = (displayTemp < 0);
        float absTemp = isNegative ? -displayTemp : displayTemp;
        int wholePart = (int)absTemp;
        int decimalPart = (int)((absTemp - wholePart) * 10);

        char intStr[4];
        int pi = 0;
        if (isNegative) intStr[pi++] = '-';
        if (wholePart >= 10) intStr[pi++] = '0' + (wholePart / 10);
        intStr[pi++] = '0' + (wholePart % 10);
        intStr[pi] = '\0';

        char decUnitStr[6];
        int di = 0;
        decUnitStr[di++] = '.';
        decUnitStr[di++] = '0' + decimalPart;
        decUnitStr[di++] = '\xB0';
        decUnitStr[di++] = 'C';
        decUnitStr[di] = '\0';

        int16_t intX = 50;
        int16_t intY = 18;
        drawText2x(intStr, intX, intY);

        int16_t decX = intX + (strlen(intStr) * UI_GLYPH_2X_SPACING);
        int16_t decY = intY + (GLYPH_HEIGHT / 2);
        drawTextUnscaled(decUnitStr, decX, decY);

        // "Set" label at top right when adjusting
        if (tempAdjustmentActive) {
            const char* setLabel = "Set";
            int16_t setWidth = strlen(setLabel) * UI_GLYPH_SPACING;
            int16_t setX = DISPLAY_WIDTH - setWidth - 2;
            int16_t setY = 2;
            drawTextUnscaled(setLabel, setX, setY);
        }

        // "Temperature" label at bottom center
        const char* tempLabel = "Temperature";
        int16_t tempLabelWidth = strlen(tempLabel) * UI_GLYPH_SPACING;
        int16_t tempLabelX = (DISPLAY_WIDTH - tempLabelWidth) / 2;
        int16_t tempLabelY = DISPLAY_HEIGHT - GLYPH_HEIGHT - 6;
        drawTextUnscaled(tempLabel, tempLabelX, tempLabelY);

        return;
    }
    
    // Regular feature items (0-6): circulation, massage, jet, heater, ozone, lights, speaker
    
    // Define menu items (8 total)
    const unsigned char* itemBitmap;
    const char* itemLabel;
    int16_t sourceWidth;
    int16_t sourceHeight;
    bool itemIsOn = false;  // Track if item is currently ON
    
    switch (circulationMenuSelectedIndex) {
        case 0:
            // Circulation pump
            itemBitmap = circulation_bitmap;
            itemLabel = "Circulation";
            sourceWidth = CIRCULATION_BMPWIDTH;
            sourceHeight = CIRCULATION_BMPHEIGHT;
            // CRITICAL FIX: Show ON if EITHER selected (countdown) OR started (running)
            itemIsOn = safetySystem.isCirculationSelected() || safetySystem.isCirculationStarted();
            break;
            
        case 1:
            // Massage pump
            itemBitmap = massage_bitmap;
            itemLabel = "Massage";
            sourceWidth = MASSAGE_BMPWIDTH;
            sourceHeight = MASSAGE_BMPHEIGHT;
            itemIsOn = safetySystem.isFeatureActive(RELAY_CHANNEL_MASSAGE);
            break;
            
        case 2:
            // Jet pump
            itemBitmap = jet_bitmap;
            itemLabel = "Jet";
            sourceWidth = JET_BMPWIDTH;
            sourceHeight = JET_BMPHEIGHT;
            itemIsOn = safetySystem.isFeatureActive(RELAY_CHANNEL_JET);
            break;
            
        case 3:
            // Water heater
            itemBitmap = heater_bitmap;
            itemLabel = "Heater";
            sourceWidth = HEATER_BMPWIDTH;
            sourceHeight = HEATER_BMPHEIGHT;
            itemIsOn = safetySystem.isHeaterActive();
            break;
            
        case 4:
            // Ozone generator
            itemBitmap = ozone_bitmap;
            itemLabel = "Ozone";
            sourceWidth = OZONE_BMPWIDTH;
            sourceHeight = OZONE_BMPHEIGHT;
            itemIsOn = safetySystem.isFeatureActive(RELAY_CHANNEL_OZONE);
            break;
            
        case 5:
            // Speaker relay
            itemBitmap = speaker_bitmap;
            itemLabel = "Speaker";
            sourceWidth = SPEAKER_BMPWIDTH;
            sourceHeight = SPEAKER_BMPHEIGHT;
            itemIsOn = safetySystem.isFeatureActive(RELAY_CHANNEL_SPEAKER);
            break;
            
        case 6:
            // Light system
            itemBitmap = light_bulb_bitmap;
            itemLabel = "Lights";
            sourceWidth = LIGHTBULB_BMPWIDTH;
            sourceHeight = LIGHTBULB_BMPHEIGHT;
            itemIsOn = safetySystem.isFeatureActive(RELAY_CHANNEL_LIGHTS);
            break;
            
        default:
            // Should never happen, default to circulation
            itemBitmap = circulation_bitmap;
            itemLabel = "Circulation";
            sourceWidth = CIRCULATION_BMPWIDTH;
            sourceHeight = CIRCULATION_BMPHEIGHT;
            itemIsOn = safetySystem.isCirculationStarted();
            break;
    }
    
    // Scale down bitmap by factor of 2
    int16_t scaledWidth = sourceWidth / 2;
    int16_t bitmapX = (DISPLAY_WIDTH - scaledWidth) / 2;
    int16_t bitmapY = 4;

    // Draw item bitmap scaled down by factor of 2
    drawScaledBitmap(itemBitmap, bitmapX, bitmapY, sourceWidth, sourceHeight);

    // Show countdown at top-left using independent timer (not tied to SafetySystem state)
    if (circulationMenuSelectedIndex == 0 && circulationCountdownActive) {
        unsigned long elapsed = millis() - circulationCountdownStartTime;
        if (elapsed < CIRCULATION_DELAYED_START_MS) {
            unsigned long remaining = CIRCULATION_DELAYED_START_MS - elapsed;
            int secondsRemaining = (int)((remaining + 999) / 1000);

            char countdownStr[4];
            int ci = 0;
            if (secondsRemaining >= 10) countdownStr[ci++] = '0' + (secondsRemaining / 10);
            countdownStr[ci++] = '0' + (secondsRemaining % 10);
            countdownStr[ci++] = 's';
            countdownStr[ci] = '\0';

            drawTextUnscaled(countdownStr, 2, 2);
        } else {
            circulationCountdownActive = false;
        }
    }

    // Display ON/OFF status in top right corner
    const char* statusText = itemIsOn ? "ON" : "OFF";
    int16_t statusWidth = strlen(statusText) * UI_GLYPH_SPACING;
    int16_t statusX = DISPLAY_WIDTH - statusWidth - 2;
    int16_t statusY = UI_STATUS_Y;
    drawTextUnscaled(statusText, statusX, statusY);

    // Display label at bottom - use denial replacement if active
    const char* displayLabel = (denialBottomLabel[0] != '\0') ? denialBottomLabel : itemLabel;
    int16_t labelWidth = strlen(displayLabel) * UI_GLYPH_SPACING;
    int16_t labelX = (DISPLAY_WIDTH - labelWidth) / 2;
    int16_t labelY = DISPLAY_HEIGHT - GLYPH_HEIGHT - 6;
    drawTextUnscaled(displayLabel, labelX, labelY);
}

// ----------------------------------------------------------------------------
// Bitmap Rendering with Scale-by-2 Rule
// ----------------------------------------------------------------------------

void UIManager::drawScaledBitmap(const unsigned char* bitmap, int16_t x, int16_t y,
                                  int16_t w, int16_t h) {
    if (!displayInitialized || bitmap == nullptr) {
        return;
    }
    
    // CRITICAL: Scale down by factor of 2 (absolute rule)
    // Source bitmaps designed at 2x resolution (e.g., 128x64)
    // Displayed at 1x resolution (e.g., 64x32)
    int16_t scaledW = w / 2;
    int16_t scaledH = h / 2;
    
    // Render scaled bitmap by sampling every other pixel
    for (int16_t j = 0; j < scaledH; j++) {
        for (int16_t i = 0; i < scaledW; i++) {
            // Sample every other pixel from source bitmap
            int16_t srcX = i * 2;
            int16_t srcY = j * 2;
            
            // Calculate byte and bit position in source bitmap
            // Bitmap format: horizontal bytes, MSB first
            int16_t srcIndex = srcY * w + srcX;
            int16_t byteIndex = srcIndex / 8;
            int16_t bitIndex = 7 - (srcIndex % 8);
            
            // Read pixel from PROGMEM
            uint8_t byte = pgm_read_byte(&bitmap[byteIndex]);
            uint8_t bit = (byte >> bitIndex) & 1;
            
            // Draw pixel if set
            if (bit) {
                display.drawPixel(x + i, y + j, SH110X_WHITE);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Bitmap-Glyph Text Rendering
// ----------------------------------------------------------------------------

void UIManager::drawText(const char* text, int16_t x, int16_t y) {
    if (!displayInitialized || text == nullptr) {
        return;
    }
    
    int16_t cursorX = x;
    
    // Render each character as bitmap glyph
    for (size_t i = 0; i < strlen(text); i++) {
        char c = text[i];
        
        // Find glyph bitmap for character
        const unsigned char* glyphBitmap = findGlyph(c);
        if (glyphBitmap == nullptr) {
            // Character not found, skip
            continue;
        }
        
        // All glyphs are 8x8 pixels at 2x resolution (defined in Bitmaps.h)
        // Scale down to 4x4 pixels when displayed
        // Use GLYPH_WIDTH and GLYPH_HEIGHT macros from Bitmaps.h
        
        // Draw glyph bitmap scaled down by factor of 2
        drawScaledBitmap(glyphBitmap, cursorX, y, GLYPH_WIDTH, GLYPH_HEIGHT);
        
        // Advance cursor (scaled width + 1 pixel spacing)
        cursorX += (UI_GLYPH_SPACING / 2);  // 5 pixels per char (scaled 8x8 → 4x4 + 1 spacing)
    }
}

void UIManager::drawTextUnscaled(const char* text, int16_t x, int16_t y) {
    if (!displayInitialized || text == nullptr) {
        return;
    }
    
    int16_t cursorX = x;
    
    // Render each character as bitmap glyph at ORIGINAL SIZE (8x8)
    for (size_t i = 0; i < strlen(text); i++) {
        char c = text[i];
        
        // Find glyph bitmap for character
        const unsigned char* glyphBitmap = findGlyph(c);
        if (glyphBitmap == nullptr) {
            // Character not found, skip
            continue;
        }
        
        // Draw glyph at original 8x8 size (no scaling)
        // Glyphs are stored as 8 bytes, one per row
        for (int16_t row = 0; row < GLYPH_HEIGHT; row++) {
            // Read row byte from PROGMEM
            uint8_t rowByte = pgm_read_byte(&glyphBitmap[row]);
            
            // Draw each pixel in the row
            for (int16_t col = 0; col < GLYPH_WIDTH; col++) {
                // Check if bit is set (MSB first)
                uint8_t bit = (rowByte >> (7 - col)) & 1;
                
                if (bit) {
                    display.drawPixel(cursorX + col, y + row, SH110X_WHITE);
                }
            }
        }
        
        cursorX += UI_GLYPH_SPACING;
    }
}

void UIManager::drawText2x(const char* text, int16_t x, int16_t y) {
    if (!displayInitialized || text == nullptr) {
        return;
    }
    
    int16_t cursorX = x;
    
    // Render each character as bitmap glyph at 2X SIZE (16x16)
    for (size_t i = 0; i < strlen(text); i++) {
        char c = text[i];
        
        // Find glyph bitmap for character
        const unsigned char* glyphBitmap = findGlyph(c);
        if (glyphBitmap == nullptr) {
            // Character not found, skip
            continue;
        }
        
        // Draw glyph at 2x size (8x8 → 16x16)
        // Each pixel becomes a 2x2 block
        for (int16_t row = 0; row < GLYPH_HEIGHT; row++) {
            // Read row byte from PROGMEM
            uint8_t rowByte = pgm_read_byte(&glyphBitmap[row]);
            
            // Draw each pixel in the row at 2x scale
            for (int16_t col = 0; col < GLYPH_WIDTH; col++) {
                // Check if bit is set (MSB first)
                uint8_t bit = (rowByte >> (7 - col)) & 1;
                
                if (bit) {
                    // Draw 2x2 pixel block
                    display.drawPixel(cursorX + col * 2, y + row * 2, SH110X_WHITE);
                    display.drawPixel(cursorX + col * 2 + 1, y + row * 2, SH110X_WHITE);
                    display.drawPixel(cursorX + col * 2, y + row * 2 + 1, SH110X_WHITE);
                    display.drawPixel(cursorX + col * 2 + 1, y + row * 2 + 1, SH110X_WHITE);
                }
            }
        }
        
        cursorX += UI_GLYPH_2X_SPACING;
    }
}

// ----------------------------------------------------------------------------
// Temperature Display
// ----------------------------------------------------------------------------

void UIManager::drawTemperature(float temperature, int16_t x, int16_t y) {
    if (!displayInitialized) {
        return;
    }
    
    // Draw thermometer bitmap (scaled down by factor of 2)
    // thermometer_bitmap is 128x64 at 2x, displays as 64x32
    drawScaledBitmap(thermometer_bitmap, x, y, 
                     THERMOMETER_BMPWIDTH, THERMOMETER_BMPHEIGHT);
    
    // Format temperature as string with degree symbol
    // Example: "38.5°C"
    char tempStr[16];
    
    // Convert temperature to string with 1 decimal place
    int wholePart = (int)temperature;
    int decimalPart = (int)((temperature - wholePart) * 10);
    
    // Build string manually to avoid sprintf (saves memory)
    int idx = 0;
    
    // Handle negative temperatures
    if (temperature < 0) {
        tempStr[idx++] = '-';
        wholePart = -wholePart;
        decimalPart = -decimalPart;
    }
    
    // Convert whole part to string
    if (wholePart >= 10) {
        tempStr[idx++] = '0' + (wholePart / 10);
    }
    tempStr[idx++] = '0' + (wholePart % 10);
    
    // Decimal point
    tempStr[idx++] = '.';
    
    // Decimal part
    tempStr[idx++] = '0' + decimalPart;
    
    // Degree symbol (UTF-8: 0xB0)
    tempStr[idx++] = '\xB0';
    
    // 'C'
    tempStr[idx++] = 'C';
    
    // Null terminator
    tempStr[idx] = '\0';
    
    // Draw temperature text next to thermometer bitmap
    // Position to the right of thermometer
    int16_t textX = x + (THERMOMETER_BMPWIDTH / 2) + 4;  // 4 pixels spacing
    int16_t textY = y + (THERMOMETER_BMPHEIGHT / 2) / 2 - 2;  // Vertically centered
    
    drawText(tempStr, textX, textY);
}

// ----------------------------------------------------------------------------
// Display Buffer Management
// ----------------------------------------------------------------------------

void UIManager::clearDisplay() {
    if (displayInitialized) {
        display.clearDisplay();
    }
}

void UIManager::displayBuffer() {
    if (displayInitialized) {
        display.display();
    }
}

// ----------------------------------------------------------------------------
// Navigation Methods
// ----------------------------------------------------------------------------

void UIManager::navigateMainMenu(int8_t direction) {
    // Only 1 item (Circulation), rotation has no effect
    // Keep index at 0 always
    mainMenuSelectedIndex = 0;
}

void UIManager::selectMainMenuItem() {
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINTLN(F("[UI] Main Menu select: Starting Circulation"));
    #endif
    
    // Reset to Circulation item (index 0) when entering Circulation UI
    circulationMenuSelectedIndex = 0;
    
    // Only one item: Circulation → Circulation UI
    // System state transition is handled by main.cpp via SafetySystem
    forceUIState(UI_CIRCULATION);
}

void UIManager::navigateCirculationMenu(int8_t direction) {
    // Circulation menu has 8 items in SEQUENTIAL order
    // CW order: Circulation(0) → Massage(1) → Jet(2) → Heater(3) → Ozone(4) → Speaker(5) → Light(6) → Thermometer(7)
    // CCW order: Reverse of CW
    
    // Sequential navigation (0-7)
    if (direction > 0) {
        // Navigate CW (forward)
        circulationMenuSelectedIndex = (circulationMenuSelectedIndex + 1) % 8;
    } else if (direction < 0) {
        // Navigate CCW (backward)
        if (circulationMenuSelectedIndex == 0) {
            circulationMenuSelectedIndex = 7;  // Wrap to last
        } else {
            circulationMenuSelectedIndex--;
        }
    }
    
    needsRedraw = true;
    manualUIStateChange = true;  // Mark as manual change (menu navigation)
    
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINT(F("[UI] Circulation Menu navigate: index = "));
        DEBUG_PRINTLN(circulationMenuSelectedIndex);
    #endif
}

void UIManager::startCirculationCountdown() {
    circulationCountdownActive = true;
    circulationCountdownStartTime = millis();
    needsRedraw = true;
}

void UIManager::showDenialMessage(const char* message, const char* bottomLabel) {
    if (message == nullptr) return;
    
    // Safe string copy into fixed buffer
    uint8_t i = 0;
    while (message[i] != '\0' && i < sizeof(denialMessage) - 1) {
        denialMessage[i] = message[i];
        i++;
    }
    denialMessage[i] = '\0';
    
    // Copy optional bottom label replacement
    denialBottomLabel[0] = '\0';
    if (bottomLabel != nullptr) {
        i = 0;
        while (bottomLabel[i] != '\0' && i < sizeof(denialBottomLabel) - 1) {
            denialBottomLabel[i] = bottomLabel[i];
            i++;
        }
        denialBottomLabel[i] = '\0';
    }
    
    denialMessageEndTime = millis() + DENIAL_MESSAGE_DURATION_MS;
    needsRedraw = true;
    
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINT(F("[UI] Denial message: "));
        DEBUG_PRINT(denialMessage);
        if (denialBottomLabel[0] != '\0') {
            DEBUG_PRINT(F(", bottom label: "));
            DEBUG_PRINT(denialBottomLabel);
        }
        DEBUG_PRINTLN(F(""));
    #endif
}

void UIManager::toggleCirculationMenuItem() {
    // Toggle current circulation menu item on/off
    // Phase 9 will implement actual relay control
    // For now, provides visual feedback only
    
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINT(F("[UI] Circulation Menu toggle: index = "));
        DEBUG_PRINTLN(circulationMenuSelectedIndex);
    #endif
    
    // Note: Actual toggle logic is handled by main.cpp via SafetySystem
    // This method is called from main.cpp when button is pressed
    // The main.cpp code routes the toggle request to SafetySystem
    
    // Request redraw to show updated state
    needsRedraw = true;
    manualUIStateChange = true;  // Mark as manual change
}

// ----------------------------------------------------------------------------
// Glyph Lookup
// ----------------------------------------------------------------------------

const unsigned char* UIManager::findGlyph(char c) {
    // Search glyph table for character
    for (size_t i = 0; i < GLYPH_TABLE_SIZE; i++) {
        // Read character from PROGMEM
        char glyphChar = pgm_read_byte(&glyphTable[i].character);
        
        if (glyphChar == c) {
            // Read bitmap pointer from PROGMEM
            const unsigned char* glyphBitmap = 
                (const unsigned char*)pgm_read_ptr(&glyphTable[i].bitmap);
            return glyphBitmap;
        }
    }
    
    // Character not found
    return nullptr;
}


void UIManager::adjustTemperature(int8_t direction) {
    // Adjust temperature on thermometer screen
    // Only active when thermometer item (index 7) is selected
    
    if (circulationMenuSelectedIndex != 7) {
        return;  // Not on thermometer screen
    }
    
    // Initialize adjustment value if not already adjusting
    if (!tempAdjustmentActive) {
        tempAdjustmentActive = true;
        tempAdjustmentValue = safetySystem.getTargetTemperature();
    }
    
    // Adjust temperature using configurable increment from SafetyConfig.h
    if (direction > 0) {
        // Increase temperature
        tempAdjustmentValue += TEMP_ADJUSTMENT_INCREMENT;
        
        // Limit to maximum safe temperature (from SafetyConfig.h)
        if (tempAdjustmentValue > TEMP_WARNING_THRESHOLD) {
            tempAdjustmentValue = TEMP_WARNING_THRESHOLD;
        }
    } else if (direction < 0) {
        // Decrease temperature
        tempAdjustmentValue -= TEMP_ADJUSTMENT_INCREMENT;
        
        if (tempAdjustmentValue < TEMP_ADJUSTMENT_MIN) {
            tempAdjustmentValue = TEMP_ADJUSTMENT_MIN;
        }
    }
    
    needsRedraw = true;
    manualUIStateChange = true;
    
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINT(F("[UI] Temperature adjusted: "));
        DEBUG_PRINT(tempAdjustmentValue);
        DEBUG_PRINTLN(F(" °C"));
    #endif
}

void UIManager::confirmTemperatureSetting() {
    // Confirm temperature setting on thermometer screen
    // Sets the adjusted temperature as the new target temperature
    
    if (circulationMenuSelectedIndex != 7 || !tempAdjustmentActive) {
        return;  // Not on thermometer screen or not adjusting
    }
    
    // Note: Actual temperature setting is handled by main.cpp via SafetySystem
    // This method signals that the user wants to confirm the setting
    // The main.cpp code will call safetySystem.setTargetTemperature()
    
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINT(F("[UI] Temperature setting confirmed: "));
        DEBUG_PRINT(tempAdjustmentValue);
        DEBUG_PRINTLN(F(" °C"));
    #endif
    
    // Reset adjustment state
    tempAdjustmentActive = false;
    needsRedraw = true;
    manualUIStateChange = true;
}

void UIManager::navigateFaultInspection(int8_t direction) {
    // Navigate through active faults in Fault Inspection UI
    // Get fault count from state machine
    uint8_t faultCount = stateMachine.getFaultCount();
    
    if (faultCount == 0) {
        return;  // No faults to navigate
    }
    
    // Navigate with wrapping
    if (direction > 0) {
        // Navigate right/next
        faultInspectionIndex = (faultInspectionIndex + 1) % faultCount;
    } else if (direction < 0) {
        // Navigate left/previous
        if (faultInspectionIndex == 0) {
            faultInspectionIndex = faultCount - 1;  // Wrap to last
        } else {
            faultInspectionIndex--;
        }
    }
    
    needsRedraw = true;
    manualUIStateChange = true;
    
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINT(F("[UI] Fault Inspection navigate: index = "));
        DEBUG_PRINT(faultInspectionIndex);
        DEBUG_PRINT(F(" / "));
        DEBUG_PRINTLN(faultCount);
    #endif
}





// ----------------------------------------------------------------------------
// Task 40: Warning UI Screen
// ----------------------------------------------------------------------------

void UIManager::renderWarningScreen() {
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Warning screen"));
            firstRender = false;
        }
    #endif

    // Draw error bitmap at FULL SIZE (128x64) covering entire screen
    // No scaling - bitmaps are visible at native resolution
    display.drawBitmap(0, 0, high_temperature_error_bitmap,
                       HIGH_TEMPERATURE_ERROR_BMPWIDTH,
                       HIGH_TEMPERATURE_ERROR_BMPHEIGHT,
                       SH110X_WHITE);

    // Overlay "WARNING" text centered on screen
    const char* warningLabel = "WARNING";
    int16_t warningWidth = strlen(warningLabel) * UI_GLYPH_SPACING;
    int16_t warningX = (DISPLAY_WIDTH - warningWidth) / 2;
    int16_t warningY = 16;
    drawTextUnscaled(warningLabel, warningX, warningY);

    // Overlay temperature value centered below WARNING
    float currentTemp = sensorManager.getTemperature();

    char tempStr[16];
    int idx = 0;

    bool isNegative = (currentTemp < 0);
    float absTemp = isNegative ? -currentTemp : currentTemp;

    if (isNegative) {
        tempStr[idx++] = '-';
    }

    int wholePart = (int)absTemp;
    int decimalPart = (int)((absTemp - wholePart) * 10);

    if (wholePart >= 10) {
        tempStr[idx++] = '0' + (wholePart / 10);
    }
    tempStr[idx++] = '0' + (wholePart % 10);

    tempStr[idx++] = '.';
    tempStr[idx++] = '0' + decimalPart;

    tempStr[idx++] = '\xB0';
    tempStr[idx++] = 'C';
    tempStr[idx] = '\0';

    int16_t tempWidth = strlen(tempStr) * UI_GLYPH_SPACING;
    int16_t tempX = (DISPLAY_WIDTH - tempWidth) / 2;
    int16_t tempY = warningY + UI_GLYPH_SPACING;
    drawTextUnscaled(tempStr, tempX, tempY);

    // Draw condition text at bottom
    const char* conditionText = "High Temperature";
    int16_t condWidth = strlen(conditionText) * UI_GLYPH_SPACING;
    int16_t condX = (DISPLAY_WIDTH - condWidth) / 2;
    int16_t condY = DISPLAY_HEIGHT - UI_GLYPH_SPACING - 4;
    drawTextUnscaled(conditionText, condX, condY);
}

// ----------------------------------------------------------------------------
// Task 41: Fault UI Screen
// ----------------------------------------------------------------------------

void UIManager::renderFaultScreen() {
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Fault screen"));
            firstRender = false;
        }
    #endif
    
    uint8_t activeFaults = stateMachine.getActiveFaults();
    
    uint8_t faultCount = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (activeFaults & (1 << i)) {
            faultCount++;
        }
    }
    
    const char* faultLabel;
    uint8_t currentFault = 0;
    
    if (activeFaults & FAULT_LOW_WATER_LEVEL) {
        faultLabel = "Low Water";
        currentFault = FAULT_LOW_WATER_LEVEL;
    } else if (activeFaults & FAULT_HIGH_TEMPERATURE) {
        faultLabel = "High Temp";
        currentFault = FAULT_HIGH_TEMPERATURE;
    } else if (activeFaults & FAULT_THERMAL_RUNAWAY) {
        faultLabel = "Thermal Runaway";
        currentFault = FAULT_THERMAL_RUNAWAY;
    } else if (activeFaults & FAULT_TEMPERATURE_SENSOR) {
        faultLabel = "Temp Sensor";
        currentFault = FAULT_TEMPERATURE_SENSOR;
    } else if (activeFaults & FAULT_I2C_FAILURE) {
        faultLabel = "I2C Failure";
        currentFault = FAULT_I2C_FAILURE;
    } else if (activeFaults & FAULT_PCF8574_FAILURE) {
        faultLabel = "Relay Failure";
        currentFault = FAULT_PCF8574_FAILURE;
    } else {
        faultLabel = "Unknown Fault";
        currentFault = 0;
    }
    
    // Calculate current fault index
    uint8_t currentFaultIndex = 0;
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t faultBit = (1 << i);
        if (activeFaults & faultBit) {
            if (faultBit == currentFault) break;
            currentFaultIndex++;
        }
    }
    currentFaultDisplayIndex = currentFaultIndex;
    
    // Draw error bitmap at FULL SIZE (fills entire screen)
    if (currentFault == FAULT_LOW_WATER_LEVEL) {
        display.drawBitmap(0, 0, low_water_level_error_bitmap,
                           LOW_WATER_LEVEL_ERROR_BMPWIDTH,
                           LOW_WATER_LEVEL_ERROR_BMPHEIGHT, SH110X_WHITE);
    } else if (currentFault == FAULT_HIGH_TEMPERATURE || currentFault == FAULT_THERMAL_RUNAWAY) {
        display.drawBitmap(0, 0, high_temperature_error_bitmap,
                           HIGH_TEMPERATURE_ERROR_BMPWIDTH,
                           HIGH_TEMPERATURE_ERROR_BMPHEIGHT, SH110X_WHITE);
    } else {
        display.drawBitmap(0, 0, sensor_error_bitmap,
                           SENSOR_ERROR_BMPWIDTH,
                           SENSOR_ERROR_BMPHEIGHT, SH110X_WHITE);
    }
    
    // Overlay fault label at bottom center
    int16_t labelWidth = strlen(faultLabel) * UI_GLYPH_SPACING;
    int16_t labelX = (DISPLAY_WIDTH - labelWidth) / 2;
    int16_t labelY = DISPLAY_HEIGHT - GLYPH_HEIGHT - 6;
    drawTextUnscaled(faultLabel, labelX, labelY);
    
    // Overlay fault count at top right if multiple faults
    if (faultCount > 1) {
        char countStr[8];
        countStr[0] = '0' + (currentFaultIndex + 1);
        countStr[1] = '/';
        countStr[2] = '0' + faultCount;
        countStr[3] = '\0';
        
        int16_t countWidth = strlen(countStr) * UI_GLYPH_SPACING;
        int16_t countX = DISPLAY_WIDTH - countWidth - 2;
        int16_t countY = 2;
        drawTextUnscaled(countStr, countX, countY);
    }
}

// ----------------------------------------------------------------------------
// Task 42: Fault Inspection UI Screen
// ----------------------------------------------------------------------------

void UIManager::renderFaultInspectionScreen() {
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Fault Inspection screen"));
            firstRender = false;
        }
    #endif
    
    uint8_t activeFaults = stateMachine.getActiveFaults();
    
    uint8_t faultList[8];
    uint8_t faultCount = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (activeFaults & (1 << i)) {
            faultList[faultCount++] = (1 << i);
        }
    }
    
    if (faultCount == 0) {
        const char* label = "No Faults";
        int16_t labelWidth = strlen(label) * UI_GLYPH_SPACING;
        int16_t labelX = (DISPLAY_WIDTH - labelWidth) / 2;
        int16_t labelY = (DISPLAY_HEIGHT - GLYPH_HEIGHT) / 2;
        drawTextUnscaled(label, labelX, labelY);
        return;
    }
    
    uint8_t currentFaultIndex = faultInspectionIndex;
    if (currentFaultIndex >= faultCount) {
        currentFaultIndex = 0;
    }
    
    uint8_t currentFault = faultList[currentFaultIndex];
    
    const unsigned char* faultBitmap;
    const char* faultLabel;
    
    if (currentFault == FAULT_LOW_WATER_LEVEL) {
        faultBitmap = low_water_level_error_bitmap;
        faultLabel = "Low Water";
    } else if (currentFault == FAULT_HIGH_TEMPERATURE) {
        faultBitmap = high_temperature_error_bitmap;
        faultLabel = "High Temp";
    } else if (currentFault == FAULT_THERMAL_RUNAWAY) {
        faultBitmap = high_temperature_error_bitmap;
        faultLabel = "Thermal Runaway";
    } else if (currentFault == FAULT_TEMPERATURE_SENSOR) {
        faultBitmap = sensor_error_bitmap;
        faultLabel = "Temp Sensor";
    } else if (currentFault == FAULT_I2C_FAILURE) {
        faultBitmap = sensor_error_bitmap;
        faultLabel = "I2C Failure";
    } else if (currentFault == FAULT_PCF8574_FAILURE) {
        faultBitmap = sensor_error_bitmap;
        faultLabel = "Relay Failure";
    } else {
        faultBitmap = sensor_error_bitmap;
        faultLabel = "Unknown Fault";
    }
    
    // Draw error bitmap at FULL SIZE (fills entire screen)
    display.drawBitmap(0, 0, faultBitmap, DISPLAY_WIDTH, DISPLAY_HEIGHT, SH110X_WHITE);
    
    // Overlay fault label at bottom center
    int16_t labelWidth = strlen(faultLabel) * UI_GLYPH_SPACING;
    int16_t labelX = (DISPLAY_WIDTH - labelWidth) / 2;
    int16_t labelY = DISPLAY_HEIGHT - GLYPH_HEIGHT - 6;
    drawTextUnscaled(faultLabel, labelX, labelY);
    
    // Overlay fault index at top right (e.g., "2/3")
    char indexStr[8];
    indexStr[0] = '0' + (currentFaultIndex + 1);
    indexStr[1] = '/';
    indexStr[2] = '0' + faultCount;
    indexStr[3] = '\0';
    
    int16_t indexWidth = strlen(indexStr) * UI_GLYPH_SPACING;
    int16_t indexX = DISPLAY_WIDTH - indexWidth - 2;
    int16_t indexY = 2;
    drawTextUnscaled(indexStr, indexX, indexY);
}

