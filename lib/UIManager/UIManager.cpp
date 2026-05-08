#include "UIManager.h"

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
// UIManager Implementation
// ============================================================================

// Constructor initializes display with I2C address
UIManager::UIManager(SensorManager& sensors) 
    : sensorManager(sensors),
      display(128, 64, &Wire, -1),  // 128x64 OLED, no reset pin
      displayInitialized(false),
      currentUIState(UI_POWER_UP),
      previousUIState(UI_POWER_UP),
      uiStateEntryTime(0),
      needsRedraw(true),
      lastUpdateTime(0),
      mainMenuSelectedIndex(0),  // Default to Circulation (index 0)
      circulationMenuSelectedIndex(0) {  // Default to Circulation pump (index 0)
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
    
    if (currentTime - lastUpdateTime >= DISPLAY_UPDATE_INTERVAL_MS) {
        // Redraw if needed OR if we're in a state that should always show (Boot, Initialization)
        // This ensures splash screens are always visible
        bool alwaysShowStates = (currentUIState == UI_POWER_UP || currentUIState == UI_INITIALIZATION);
        
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
    UIState newUIState = currentUIState;
    
    // Map system state to UI state
    switch (systemState) {
        case STATE_BOOT:
            newUIState = UI_POWER_UP;
            break;
            
        case STATE_SELF_CHECK:
            newUIState = UI_INITIALIZATION;
            break;
            
        case STATE_READY:
            newUIState = UI_READY;
            break;
            
        case STATE_ACTIVE_CIRCULATION:
        case STATE_FEATURE_ENABLED_BATH:
            newUIState = UI_CIRCULATION;
            break;
            
        case STATE_WARNING:
            newUIState = UI_WARNING;
            break;
            
        case STATE_FAULT:
            newUIState = UI_FAULT;
            break;
            
        case STATE_FAULT_INSPECTION:
            newUIState = UI_FAULT_INSPECTION;
            break;
            
        case STATE_SHUTDOWN:
            // Keep current UI state during shutdown
            break;
    }
    
    // Update UI state if changed
    if (newUIState != currentUIState) {
        previousUIState = currentUIState;
        currentUIState = newUIState;
        uiStateEntryTime = millis();
        needsRedraw = true;  // Force redraw on state change
        
        #ifdef ENABLE_SERIAL_DEBUG
            DEBUG_PRINT(F("[UI] State changed: "));
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
            
        case UI_SETTINGS_AND_ERROR:
            // Future task - Settings And Error UI
            break;
            
        case UI_WARNING:
            // Future task - Warning UI
            break;
            
        case UI_FAULT:
            // Future task - Fault UI
            break;
            
        case UI_FAULT_INSPECTION:
            // Future task - Fault Inspection UI
            break;
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
    // Requirement 9.17: Initialization/Safety UI
    // - settings_bitmap SCALED DOWN by factor of 2 (128x64 → 64x32)
    // - horizontally centered
    // - positioned with space from top and bottom for text
    // - text at bottom at NORMAL SIZE (8x8) for better readability
    
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Initialization screen"));
            firstRender = false;
        }
    #endif
    
    // Scale down settings bitmap by factor of 2
    int16_t sourceWidth = SETTINGS_BMPWIDTH;   // 128 pixels
    int16_t sourceHeight = SETTINGS_BMPHEIGHT; // 64 pixels
    int16_t scaledWidth = sourceWidth / 2;     // 64 pixels (scaled height will be 32 pixels)
    
    // Center horizontally: (128 - 64) / 2 = 32
    int16_t bitmapX = (128 - scaledWidth) / 2;  // 32
    
    // Position bitmap with offset from top to leave space for text at bottom
    // Display is 64 pixels tall, bitmap is 32 pixels tall, text is 8 pixels tall
    // Layout: top margin (10px) + bitmap (32px) + spacing (4px) + text (8px) + bottom margin (10px) = 64px
    int16_t bitmapY = 10;
    
    // Draw settings bitmap SCALED DOWN by factor of 2
    drawScaledBitmap(settings_bitmap, bitmapX, bitmapY, sourceWidth, sourceHeight);
    
    // Display initialization status text (bitmap glyphs at NORMAL SIZE 8x8)
    // Position text at bottom of screen
    // Text Y position: 10 (top margin) + 32 (bitmap) + 4 (spacing) = 46
    int16_t textY = 46;
    
    // "Checking..." is 11 characters
    // At normal size: each char is 8 pixels wide + 2 pixels spacing = 10 pixels per char
    // Total width: 11 × 10 = 110 pixels
    // Center: (128 - 110) / 2 = 9
    const char* text = "Initializing";
    int16_t textWidth = strlen(text) * 10;  // 10 pixels per char (8 + 2 spacing)
    int16_t textX = (128 - textWidth) / 2;
    
    // Draw text at NORMAL SIZE (8x8) using unscaled method
    drawTextUnscaled(text, textX, textY);
}

// ----------------------------------------------------------------------------
// Task 36: Ready UI Screen
// ----------------------------------------------------------------------------

void UIManager::renderReadyScreen() {
    // Requirement 9.18: Ready UI
    // - Two-column layout
    // - Left: thermometer_bitmap scaled by 2 (128x64 → 64x32)
    // - Right: Numeric temperature with degree symbol (bitmap glyphs at normal size 8x8)
    
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Ready screen"));
            firstRender = false;
        }
    #endif
    
    // Get current temperature from sensor manager
    float currentTemp = sensorManager.getTemperature();
    
    // Left column: Thermometer bitmap (scaled by 2)
    int16_t sourceWidth = THERMOMETER_BMPWIDTH;   // 128 pixels
    int16_t sourceHeight = THERMOMETER_BMPHEIGHT; // 64 pixels
    int16_t scaledHeight = sourceHeight / 2;      // 32 pixels (scaled width will be 64 pixels)
    
    // Position thermometer bitmap on LEFT side
    // Left margin: 0 pixels (flush left)
    // Vertically centered: (64 - 32) / 2 = 16
    int16_t bitmapX = 0;
    int16_t bitmapY = (64 - scaledHeight) / 2;  // 16
    
    // Draw thermometer bitmap scaled down by factor of 2
    drawScaledBitmap(thermometer_bitmap, bitmapX, bitmapY, sourceWidth, sourceHeight);
    
    // Right column: Temperature value with degree symbol
    // Format: "38.5°C" at normal size (8x8 pixels)
    
    // Build temperature string manually
    char tempStr[16];
    int idx = 0;
    
    // Handle negative temperatures
    bool isNegative = (currentTemp < 0);
    float absTemp = isNegative ? -currentTemp : currentTemp;
    
    if (isNegative) {
        tempStr[idx++] = '-';
    }
    
    // Convert to integer parts
    int wholePart = (int)absTemp;
    int decimalPart = (int)((absTemp - wholePart) * 10);
    
    // Build string: whole part
    if (wholePart >= 10) {
        tempStr[idx++] = '0' + (wholePart / 10);
    }
    tempStr[idx++] = '0' + (wholePart % 10);
    
    // Decimal point
    tempStr[idx++] = '.';
    
    // Decimal part
    tempStr[idx++] = '0' + decimalPart;
    
    // Degree symbol (°)
    tempStr[idx++] = '\xB0';  // UTF-8 degree symbol
    
    // 'C'
    tempStr[idx++] = 'C';
    
    // Null terminator
    tempStr[idx] = '\0';
    
    // Position text on right side
    // Right column starts after bitmap with small spacing
    // Text X: 48 (small gap after thermometer)
    int16_t textX = 48;
    
    // Vertically center text: (64 - 8) / 2 = 28
    int16_t textY = (64 - 8) / 2;  // 28
    
    // Draw temperature text at NORMAL SIZE (8x8) using unscaled method
    drawTextUnscaled(tempStr, textX, textY);
}

// ----------------------------------------------------------------------------
// Task 37: Main Menu UI Screen
// ----------------------------------------------------------------------------

void UIManager::renderMainMenuScreen() {
    // Requirement 9.19: Main Menu UI
    // - One item visible at a time (circulation_bitmap or settings_bitmap)
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
    
    // Determine which menu item to display based on selection
    // mainMenuSelectedIndex: 0 = Circulation, 1 = Settings
    const unsigned char* menuBitmap;
    const char* menuLabel;
    int16_t sourceWidth;
    int16_t sourceHeight;
    
    if (mainMenuSelectedIndex == 0) {
        // Circulation menu item
        menuBitmap = circulation_bitmap;
        menuLabel = "Start";
        sourceWidth = CIRCULATION_BMPWIDTH;   // 128 pixels
        sourceHeight = CIRCULATION_BMPHEIGHT; // 64 pixels
    } else {
        // Settings menu item
        menuBitmap = settings_bitmap;
        menuLabel = "Settings";
        sourceWidth = SETTINGS_BMPWIDTH;   // 128 pixels
        sourceHeight = SETTINGS_BMPHEIGHT; // 64 pixels
    }
    
    // Scale down bitmap by factor of 2
    int16_t scaledWidth = sourceWidth / 2;   // 64 pixels
    // Note: scaledHeight = sourceHeight / 2 = 32 pixels (not stored, calculated in drawScaledBitmap)
    
    // Position bitmap
    // Horizontally centered: (128 - 64) / 2 = 32
    int16_t bitmapX = (128 - scaledWidth) / 2;  // 32
    
    // Vertically positioned with space at bottom for label
    // Top offset: 8 pixels from top
    int16_t bitmapY = 8;
    
    // Draw menu bitmap scaled down by factor of 2
    drawScaledBitmap(menuBitmap, bitmapX, bitmapY, sourceWidth, sourceHeight);
    
    // Display label at bottom center (bitmap glyphs at normal size 8x8)
    // Calculate label width for centering
    // At normal size: each char is 8 pixels wide + 2 pixels spacing = 10 pixels per char
    int16_t labelWidth = strlen(menuLabel) * 10;
    
    // Position label at bottom center
    // Label X: (128 - labelWidth) / 2 (centered)
    int16_t labelX = (128 - labelWidth) / 2;
    
    // Label Y: 64 - 8 - 4 = 52 (4 pixels from bottom)
    int16_t labelY = 64 - 8 - 4;  // 52
    
    // Draw label text at NORMAL SIZE (8x8) using unscaled method
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
    
    // Define menu items (8 total)
    // Index 0: Circulation pump
    // Index 1: Massage pump
    // Index 2: Jet pump
    // Index 3: Water heater
    // Index 4: Ozone generator
    // Index 5: Light system
    // Index 6: Speaker relay
    // Index 7: Temperature display
    
    const unsigned char* itemBitmap;
    const char* itemLabel;
    int16_t sourceWidth;
    int16_t sourceHeight;
    
    switch (circulationMenuSelectedIndex) {
        case 0:
            // Circulation pump
            itemBitmap = circulation_bitmap;
            itemLabel = "Circulation";
            sourceWidth = CIRCULATION_BMPWIDTH;
            sourceHeight = CIRCULATION_BMPHEIGHT;
            break;
            
        case 1:
            // Massage pump
            itemBitmap = massage_bitmap;
            itemLabel = "Massage";
            sourceWidth = MASSAGE_BMPWIDTH;
            sourceHeight = MASSAGE_BMPHEIGHT;
            break;
            
        case 2:
            // Jet pump
            itemBitmap = jet_bitmap;
            itemLabel = "Jet";
            sourceWidth = JET_BMPWIDTH;
            sourceHeight = JET_BMPHEIGHT;
            break;
            
        case 3:
            // Water heater
            itemBitmap = heater_bitmap;
            itemLabel = "Heater";
            sourceWidth = HEATER_BMPWIDTH;
            sourceHeight = HEATER_BMPHEIGHT;
            break;
            
        case 4:
            // Ozone generator
            itemBitmap = ozone_bitmap;
            itemLabel = "Ozone";
            sourceWidth = OZONE_BMPWIDTH;
            sourceHeight = OZONE_BMPHEIGHT;
            break;
            
        case 5:
            // Light system
            itemBitmap = light_bulb_bitmap;
            itemLabel = "Lights";
            sourceWidth = LIGHTBULB_BMPWIDTH;
            sourceHeight = LIGHTBULB_BMPHEIGHT;
            break;
            
        case 6:
            // Speaker relay
            itemBitmap = speaker_bitmap;
            itemLabel = "Speaker";
            sourceWidth = SPEAKER_BMPWIDTH;
            sourceHeight = SPEAKER_BMPHEIGHT;
            break;
            
        case 7:
            // Temperature display
            itemBitmap = thermometer_bitmap;
            itemLabel = "Temperature";
            sourceWidth = THERMOMETER_BMPWIDTH;
            sourceHeight = THERMOMETER_BMPHEIGHT;
            break;
            
        default:
            // Should never happen, default to circulation
            itemBitmap = circulation_bitmap;
            itemLabel = "Circulation";
            sourceWidth = CIRCULATION_BMPWIDTH;
            sourceHeight = CIRCULATION_BMPHEIGHT;
            break;
    }
    
    // Scale down bitmap by factor of 2
    int16_t scaledWidth = sourceWidth / 2;   // 64 pixels
    // Note: scaledHeight = sourceHeight / 2 = 32 pixels (calculated in drawScaledBitmap)
    
    // Position bitmap
    // Horizontally centered: (128 - 64) / 2 = 32
    int16_t bitmapX = (128 - scaledWidth) / 2;  // 32
    
    // Vertically positioned with space at bottom for label
    // Top offset: 8 pixels from top
    int16_t bitmapY = 8;
    
    // Draw item bitmap scaled down by factor of 2
    drawScaledBitmap(itemBitmap, bitmapX, bitmapY, sourceWidth, sourceHeight);
    
    // Display label at bottom center (bitmap glyphs at normal size 8x8)
    // Calculate label width for centering
    // At normal size: each char is 8 pixels wide + 2 pixels spacing = 10 pixels per char
    int16_t labelWidth = strlen(itemLabel) * 10;
    
    // Position label at bottom center
    // Label X: (128 - labelWidth) / 2 (centered)
    int16_t labelX = (128 - labelWidth) / 2;
    
    // Label Y: 64 - 8 - 4 = 52 (4 pixels from bottom)
    int16_t labelY = 64 - 8 - 4;  // 52
    
    // Draw label text at NORMAL SIZE (8x8) using unscaled method
    drawTextUnscaled(itemLabel, labelX, labelY);
    
    // TODO (Phase 9): Display on/off status indicator for each feature
    // TODO (Phase 9): Display countdown for circulation delayed start
    // TODO (Phase 9): Display temperature value when thermometer selected
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
        cursorX += (GLYPH_WIDTH / 2) + 1;  // 4 pixels + 1 spacing = 5 pixels per char
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
        
        // Advance cursor (8 pixels + 2 pixels spacing)
        cursorX += GLYPH_WIDTH + 2;  // 10 pixels per char
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
        
        // Advance cursor (16 pixels + 4 pixels spacing)
        cursorX += (GLYPH_WIDTH * 2) + 4;  // 20 pixels per char
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

