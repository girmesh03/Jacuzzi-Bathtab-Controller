# Phase 6B: UI Screens Fix - Implementation Plan

## Overview

**Goal**: Fix Tasks 39-42 UI screen implementations to fully comply with requirements and design specifications.

**Scope**: 
- Task 39: Complete Settings And Error UI with scrollable menu
- Task 40: Fix Warning UI to be an overlay (not full-screen replacement)
- Task 41: Fix Fault UI priority display logic
- Task 42: Validate Fault Inspection UI (appears correct)

**Requirements**: 9.13-9.21, 11.9-11.13
**Design**: UI Manager component, UI States section
**Integration**: `lib/UIManager/UIManager.cpp`, `include/UIManager.h`

---

## Pre-Implementation Analysis

### Current State Assessment

**Task 39 - Settings And Error UI**:
- ❌ Only placeholder implementation (static "Settings" bitmap)
- ❌ No scrollable menu items
- ❌ No configuration options
- ❌ No error history display
- ❌ No system information display
- ❌ No navigation support
- ❌ Violates Requirement 9.21

**Task 40 - Warning UI**:
- ⚠️ Full-screen implementation (should be overlay)
- ✅ Shows high_temperature_error_bitmap correctly
- ✅ Shows temperature value correctly
- ❌ Violates Requirement 9.9 (overlay requirement)

**Task 41 - Fault UI**:
- ✅ Shows specific error bitmaps correctly
- ✅ Shows fault labels correctly
- ✅ Shows fault count for multiple faults
- ⚠️ Always shows "1/3" instead of actual current fault index
- ⚠️ Should track which fault is currently displayed

**Task 42 - Fault Inspection UI**:
- ✅ Implementation appears correct
- ✅ Shows current fault with specific bitmap
- ✅ Shows fault index (e.g., "2/3")
- ✅ Supports navigation

---

## Task 39: Complete Settings And Error UI

### Requirements Analysis

**Requirement 9.21**: Settings And Error UI
- Same interaction style and layout constraints as Circulation UI
- One item visible at a time, scrollable list
- Items: Configuration options, error history, system information
- Rotary left/right scrolls, button confirms/toggles
- Purpose: System configuration and error review

**Design Specification** (from design.md):
- Layout: Same as Circulation UI (one item at a time)
- Display: Scrollable list
- Items: Configuration options, error history, system information
- Interaction: Rotary left/right scrolls, button confirms/toggles

### Implementation Checklist

#### 39.1 Add Settings Menu State Tracking to UIManager.h

- [ ] Add private member: `uint8_t settingsMenuSelectedIndex`
- [ ] Initialize in constructor: `settingsMenuSelectedIndex(0)`
- [ ] Add getter method: `uint8_t getSettingsMenuSelectedIndex() const`
- [ ] Add setter method: `void setSettingsMenuSelectedIndex(uint8_t index)`
- [ ] Add navigation method: `void navigateSettingsMenu(int8_t direction)`
- [ ] Add selection method: `void selectSettingsMenuItem()`

**File**: `include/UIManager.h`

**Code to add**:
```cpp
// In private members section:
uint8_t settingsMenuSelectedIndex;  // 0-2: Configuration, Error History, System Info

// In public methods section:
uint8_t getSettingsMenuSelectedIndex() const { return settingsMenuSelectedIndex; }
void setSettingsMenuSelectedIndex(uint8_t index) {
    if (index <= 2) {  // 3 menu items (0-2)
        settingsMenuSelectedIndex = index;
        needsRedraw = true;
    }
}
void navigateSettingsMenu(int8_t direction);
void selectSettingsMenuItem();
```

#### 39.2 Initialize Settings Menu State in Constructor

- [ ] Add initialization in UIManager constructor
- [ ] Set `settingsMenuSelectedIndex(0)` in initializer list

**File**: `lib/UIManager/UIManager.cpp`

**Code to add** (in constructor initializer list):
```cpp
settingsMenuSelectedIndex(0),  // Default to Configuration (index 0)
```

#### 39.3 Implement navigateSettingsMenu Method

- [ ] Create method with wrapping navigation
- [ ] Support 3 menu items: Configuration (0), Error History (1), System Info (2)
- [ ] Navigate with wrapping (0 → 1 → 2 → 0)
- [ ] Set `needsRedraw = true`
- [ ] Set `manualUIStateChange = true`
- [ ] Add debug logging

**File**: `lib/UIManager/UIManager.cpp`

**Code to add**:
```cpp
void UIManager::navigateSettingsMenu(int8_t direction) {
    // Settings menu has 3 items: 0 = Configuration, 1 = Error History, 2 = System Info
    // Navigate with wrapping
    
    if (direction > 0) {
        // Navigate right/down
        settingsMenuSelectedIndex = (settingsMenuSelectedIndex + 1) % 3;
    } else if (direction < 0) {
        // Navigate left/up
        if (settingsMenuSelectedIndex == 0) {
            settingsMenuSelectedIndex = 2;  // Wrap to last item
        } else {
            settingsMenuSelectedIndex--;
        }
    }
    
    needsRedraw = true;
    manualUIStateChange = true;
    
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINT(F("[UI] Settings Menu navigate: index = "));
        DEBUG_PRINTLN(settingsMenuSelectedIndex);
    #endif
}
```

#### 39.4 Implement selectSettingsMenuItem Method

- [ ] Create method for item selection
- [ ] Handle Configuration item (index 0) - placeholder for now
- [ ] Handle Error History item (index 1) - placeholder for now
- [ ] Handle System Info item (index 2) - placeholder for now
- [ ] Add debug logging

**File**: `lib/UIManager/UIManager.cpp`

**Code to add**:
```cpp
void UIManager::selectSettingsMenuItem() {
    // Confirm selection and perform action
    
    #ifdef ENABLE_SERIAL_DEBUG
        DEBUG_PRINT(F("[UI] Settings Menu select: index = "));
        DEBUG_PRINTLN(settingsMenuSelectedIndex);
    #endif
    
    // Note: Actual settings actions will be implemented in Phase 9
    // For now, just log the selection
    
    switch (settingsMenuSelectedIndex) {
        case 0:
            DEBUG_PRINTLN(F("[UI] Configuration selected (not yet implemented)"));
            break;
        case 1:
            DEBUG_PRINTLN(F("[UI] Error History selected (not yet implemented)"));
            break;
        case 2:
            DEBUG_PRINTLN(F("[UI] System Info selected (not yet implemented)"));
            break;
    }
    
    needsRedraw = true;
    manualUIStateChange = true;
}
```

#### 39.5 Implement Complete renderSettingsAndErrorScreen Method

- [ ] Replace placeholder implementation
- [ ] Implement scrollable menu with 3 items
- [ ] Display one item at a time (same layout as Circulation UI)
- [ ] Show item bitmap scaled by 2
- [ ] Show item label at bottom center
- [ ] Use appropriate bitmaps for each item:
  - Configuration: settings_bitmap
  - Error History: sensor_error_bitmap (or create dedicated bitmap)
  - System Info: thermometer_bitmap (or create dedicated bitmap)

**File**: `lib/UIManager/UIManager.cpp`

**Code to replace** (entire renderSettingsAndErrorScreen method):
```cpp
void UIManager::renderSettingsAndErrorScreen() {
    // Requirement 9.21: Settings And Error UI
    // - Same interaction style and layout constraints as Circulation UI
    // - One item visible at a time, scrollable list
    // - Items: Configuration options, error history, system information
    // - Rotary left/right scrolls, button confirms/toggles
    // - Purpose: System configuration and error review
    
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        static uint8_t lastSelectedIndex = 255;
        if (firstRender || settingsMenuSelectedIndex != lastSelectedIndex) {
            DEBUG_PRINT(F("[UI] Rendering Settings And Error screen, item: "));
            DEBUG_PRINTLN(settingsMenuSelectedIndex);
            firstRender = false;
            lastSelectedIndex = settingsMenuSelectedIndex;
        }
    #endif
    
    // Define menu items (3 total)
    const unsigned char* itemBitmap;
    const char* itemLabel;
    int16_t sourceWidth;
    int16_t sourceHeight;
    
    switch (settingsMenuSelectedIndex) {
        case 0:
            // Configuration
            itemBitmap = settings_bitmap;
            itemLabel = "Configuration";
            sourceWidth = SETTINGS_BMPWIDTH;
            sourceHeight = SETTINGS_BMPHEIGHT;
            break;
            
        case 1:
            // Error History
            itemBitmap = sensor_error_bitmap;  // Use sensor error bitmap for error history
            itemLabel = "Error History";
            sourceWidth = SENSOR_ERROR_BMPWIDTH;
            sourceHeight = SENSOR_ERROR_BMPHEIGHT;
            break;
            
        case 2:
            // System Information
            itemBitmap = thermometer_bitmap;  // Use thermometer bitmap for system info
            itemLabel = "System Info";
            sourceWidth = THERMOMETER_BMPWIDTH;
            sourceHeight = THERMOMETER_BMPHEIGHT;
            break;
            
        default:
            // Should never happen, default to configuration
            itemBitmap = settings_bitmap;
            itemLabel = "Configuration";
            sourceWidth = SETTINGS_BMPWIDTH;
            sourceHeight = SETTINGS_BMPHEIGHT;
            break;
    }
    
    // Scale down bitmap by factor of 2
    int16_t scaledWidth = sourceWidth / 2;   // 64 pixels
    
    // Position bitmap
    // Horizontally centered: (128 - 64) / 2 = 32
    int16_t bitmapX = (128 - scaledWidth) / 2;  // 32
    
    // Vertically positioned with space at bottom for label
    // Top offset: 8 pixels from top
    int16_t bitmapY = 8;
    
    // Draw item bitmap scaled down by factor of 2
    drawScaledBitmap(itemBitmap, bitmapX, bitmapY, sourceWidth, sourceHeight);
    
    // Display label at bottom center (bitmap glyphs at normal size 8x8)
    int16_t labelWidth = strlen(itemLabel) * 10;
    int16_t labelX = (128 - labelWidth) / 2;
    int16_t labelY = 64 - 8;  // 56
    
    // Draw label text at NORMAL SIZE (8x8)
    drawTextUnscaled(itemLabel, labelX, labelY);
    
    // Note: Actual settings functionality will be implemented in Phase 9
    // This provides the complete UI structure with scrollable menu
}
```

#### 39.6 Update InputHandler Integration (Phase 7)

- [ ] Verify InputHandler calls `navigateSettingsMenu()` on encoder rotation
- [ ] Verify InputHandler calls `selectSettingsMenuItem()` on button press
- [ ] Test navigation between all 3 menu items
- [ ] Test wrapping (2 → 0 and 0 → 2)

**File**: `lib/InputHandler/InputHandler.cpp` (Phase 7 integration)

**Note**: This will be verified during Phase 7 integration testing

---

## Task 40: Fix Warning UI to be Overlay

### Requirements Analysis

**Requirement 9.9**: Warning UI
- Display: Warning indicator with high_temperature_error_bitmap or appropriate warning bitmap
- **Overlay: Warning displayed over current operational UI**
- Interaction: System continues operation, user can acknowledge warning
- Purpose: Non-critical warnings that don't require shutdown

**Design Specification** (from design.md):
- Display: Warning indicator with `high_temperature_error_bitmap`
- **Overlay: Warning displayed over current operational UI**
- Interaction: System continues operation
- Purpose: Non-critical warnings

### Implementation Checklist

#### 40.1 Analyze Current Operational UI State

- [ ] Determine which UI state is active when warning occurs
- [ ] Most likely: UI_CIRCULATION (Active_Circulation or Feature_Enabled_Bath states)
- [ ] Could also be: UI_READY (if warning occurs before circulation starts)

**Analysis**:
- Warning state (STATE_WARNING) typically occurs during operation
- System continues running with warning indication
- Most common: High temperature warning during active bath operation

#### 40.2 Implement Overlay Rendering Strategy

- [ ] Render underlying operational UI first
- [ ] Then overlay warning indicator on top
- [ ] Use semi-transparent or bordered warning box
- [ ] Position warning indicator prominently but not obscuring critical info

**Strategy**:
1. Render current operational screen (circulation/ready)
2. Overlay warning indicator (bitmap + text)
3. Position at top or center with clear visibility

#### 40.3 Reimplement renderWarningScreen Method

- [ ] Replace full-screen implementation with overlay
- [ ] Render underlying UI state first
- [ ] Overlay warning indicator on top
- [ ] Show warning bitmap (scaled by 2)
- [ ] Show temperature value
- [ ] Show "Warning" label

**File**: `lib/UIManager/UIManager.cpp`

**Code to replace** (entire renderWarningScreen method):
```cpp
void UIManager::renderWarningScreen() {
    // Requirement 9.9: Warning UI
    // - Display: Warning indicator with high_temperature_error_bitmap
    // - **OVERLAY: Warning displayed over current operational UI**
    // - Interaction: System continues operation, user can acknowledge warning
    // - Purpose: Non-critical warnings that don't require shutdown
    
    #ifdef ENABLE_SERIAL_DEBUG
        static bool firstRender = true;
        if (firstRender) {
            DEBUG_PRINTLN(F("[UI] Rendering Warning screen (overlay)"));
            firstRender = false;
        }
    #endif
    
    // CRITICAL: Render underlying operational UI first
    // Determine which UI state was active before warning
    // Most likely: Circulation UI (during active operation)
    // Could also be: Ready UI (if warning occurs before circulation)
    
    // For now, render the most common case: Circulation UI underneath
    // This shows the current operational state with warning overlay
    renderCirculationScreen();
    
    // Now overlay warning indicator on top
    // Position warning at top center with semi-transparent background effect
    
    // Draw warning bitmap (smaller, scaled by 2)
    // Use smaller size for overlay: 32x16 (from 64x32 scaled)
    int16_t warningBitmapWidth = HIGH_TEMPERATURE_ERROR_BMPWIDTH / 4;   // 32 pixels
    int16_t warningBitmapHeight = HIGH_TEMPERATURE_ERROR_BMPHEIGHT / 4; // 16 pixels
    
    // Position at top center
    int16_t warningX = (128 - warningBitmapWidth) / 2;  // Centered
    int16_t warningY = 2;  // 2 pixels from top
    
    // Draw warning bitmap at smaller size (scaled by 4 instead of 2 for overlay)
    // Note: We'll draw a simple warning indicator instead of full bitmap
    // Draw a bordered box with warning text
    
    // Draw warning box background (filled rectangle)
    // Box dimensions: 80 pixels wide, 12 pixels tall
    int16_t boxX = (128 - 80) / 2;  // 24
    int16_t boxY = 2;
    int16_t boxW = 80;
    int16_t boxH = 12;
    
    // Draw box border (white rectangle)
    display.drawRect(boxX, boxY, boxW, boxH, SH110X_WHITE);
    
    // Draw "WARNING" text inside box
    const char* warningText = "WARNING";
    int16_t textWidth = strlen(warningText) * 10;  // 10 pixels per char at normal size
    int16_t textX = (128 - textWidth) / 2;  // Centered
    int16_t textY = boxY + 2;  // 2 pixels from top of box
    
    // Draw warning text at NORMAL SIZE (8x8)
    drawTextUnscaled(warningText, textX, textY);
    
    // Optionally show temperature value at top right (outside box)
    float currentTemp = sensorManager.getTemperature();
    
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
    tempStr[idx++] = '\xB0';
    
    // 'C'
    tempStr[idx++] = 'C';
    
    // Null terminator
    tempStr[idx] = '\0';
    
    // Position temperature below warning box
    int16_t tempWidth = strlen(tempStr) * 10;
    int16_t tempX = (128 - tempWidth) / 2;  // Centered
    int16_t tempY = boxY + boxH + 2;  // Below warning box
    
    // Draw temperature text at NORMAL SIZE (8x8)
    drawTextUnscaled(tempStr, tempX, tempY);
    
    // Note: Underlying operational UI remains visible
    // Warning overlay is non-intrusive and allows continued operation
}
```

#### 40.4 Test Warning Overlay Visibility

- [ ] Verify underlying UI remains visible
- [ ] Verify warning indicator is prominent
- [ ] Verify temperature value is readable
- [ ] Verify overlay doesn't obscure critical operational info
- [ ] Test with different underlying UI states (Ready, Circulation)

---

## Task 41: Fix Fault UI Priority Display

### Requirements Analysis

**Requirement 11**: Fault UI
- Display: Fault indicator with specific error bitmap
- Text: Bitmap-glyph fault description
- **Fault count: Displayed if multiple faults active (e.g., "Fault 1/3")**
- Interaction: Button press transitions to Fault_Inspection UI if multiple faults
- Purpose: Display active fault condition

**Current Issue**:
- Always shows "1/3" instead of actual current fault index
- Should track which fault is currently displayed (highest priority)

### Implementation Checklist

#### 41.1 Add Current Fault Index Tracking

- [ ] Add private member to track current fault being displayed
- [ ] Initialize to 0 in constructor
- [ ] Update when fault state is entered

**File**: `include/UIManager.h`

**Code to add** (in private members):
```cpp
uint8_t currentFaultDisplayIndex;  // Current fault being displayed on Fault UI (0-based)
```

**File**: `lib/UIManager/UIManager.cpp` (constructor):
```cpp
currentFaultDisplayIndex(0),  // Start at first fault
```

#### 41.2 Update renderFaultScreen to Track Current Fault

- [ ] Calculate which fault is being displayed (highest priority)
- [ ] Track the index of that fault in the fault list
- [ ] Display correct fault index in "X/Y" format

**File**: `lib/UIManager/UIManager.cpp`

**Code to update** (in renderFaultScreen method):
```cpp
// After building fault list and determining current fault...

// Calculate current fault index (position in priority order)
uint8_t currentFaultIndex = 0;
for (uint8_t i = 0; i < 8; i++) {
    uint8_t faultBit = (1 << i);
    if (activeFaults & faultBit) {
        if (faultBit == currentFault) {
            // Found current fault position
            break;
        }
        currentFaultIndex++;
    }
}

// Store for reference
currentFaultDisplayIndex = currentFaultIndex;

// If multiple faults, display fault count at top right
if (faultCount > 1) {
    // Format: "X/Y" (showing current fault X of Y total)
    char countStr[8];
    countStr[0] = '0' + (currentFaultIndex + 1);  // 1-indexed for user
    countStr[1] = '/';
    countStr[2] = '0' + faultCount;
    countStr[3] = '\0';
    
    // Position at top right
    int16_t countWidth = strlen(countStr) * 10;
    int16_t countX = 128 - countWidth - 2;  // 2 pixels from right edge
    int16_t countY = 2;  // 2 pixels from top
    
    // Draw count text at NORMAL SIZE (8x8)
    drawTextUnscaled(countStr, countX, countY);
}
```

#### 41.3 Test Fault Priority Display

- [ ] Test with single fault (no count displayed)
- [ ] Test with multiple faults (correct index displayed)
- [ ] Verify highest priority fault is shown first
- [ ] Verify fault index updates correctly

---

## Task 42: Validate Fault Inspection UI

### Requirements Analysis

**Requirement 11**: Fault_Inspection UI
- Display: Current fault with specific error bitmap
- Fault navigation: Rotary left/right scrolls through active faults
- Fault index: Current fault number and total count (bitmap glyphs)
- Each fault: Displays with its specific error bitmap and description
- Interaction: Button press returns to Fault UI or Ready UI (if faults cleared)
- Purpose: Browse and review multiple active faults

**Current State**: Implementation appears correct

### Validation Checklist

#### 42.1 Review Current Implementation

- [x] Method exists: `renderFaultInspectionScreen()`
- [x] Builds fault list from active faults
- [x] Displays current fault based on `faultInspectionIndex`
- [x] Shows fault index (e.g., "2/3")
- [x] Shows specific error bitmap for each fault
- [x] Shows fault label for each fault

#### 42.2 Review Navigation Implementation

- [x] Method exists: `navigateFaultInspection(int8_t direction)`
- [x] Supports left/right navigation
- [x] Wraps at boundaries (0 ↔ faultCount-1)
- [x] Updates `faultInspectionIndex`
- [x] Requests redraw

#### 42.3 Test Fault Inspection UI

- [ ] Test with 2 faults (navigate between them)
- [ ] Test with 3+ faults (navigate through all)
- [ ] Verify wrapping works (last → first, first → last)
- [ ] Verify each fault displays correct bitmap
- [ ] Verify each fault displays correct label
- [ ] Verify fault index updates correctly (e.g., "1/3", "2/3", "3/3")

#### 42.4 Test Fault Inspection State Transitions

- [ ] Test transition from Fault UI to Fault_Inspection UI (button press)
- [ ] Test transition from Fault_Inspection UI back to Fault UI (button press)
- [ ] Test transition from Fault_Inspection UI to Ready UI (when faults cleared)
- [ ] Verify `faultInspectionIndex` resets when entering Fault_Inspection state

**Status**: Implementation appears correct, validation testing required

---

## Integration and Testing

### Integration Checklist

#### Update InputHandler (Phase 7)

- [ ] Verify encoder rotation calls `navigateSettingsMenu()` in UI_SETTINGS_AND_ERROR state
- [ ] Verify button press calls `selectSettingsMenuItem()` in UI_SETTINGS_AND_ERROR state
- [ ] Test all navigation paths

#### Update main.cpp

- [ ] Verify UI state transitions work correctly
- [ ] Verify warning overlay displays during STATE_WARNING
- [ ] Verify fault UI displays during STATE_FAULT
- [ ] Verify fault inspection displays during STATE_FAULT_INSPECTION

### Testing Checklist

#### Task 39 Testing

- [ ] Navigate through all 3 settings menu items
- [ ] Verify wrapping works (2 → 0 and 0 → 2)
- [ ] Verify each item displays correct bitmap
- [ ] Verify each item displays correct label
- [ ] Verify button press logs selection (placeholder)

#### Task 40 Testing

- [ ] Trigger warning state (high temperature)
- [ ] Verify underlying operational UI remains visible
- [ ] Verify warning overlay is prominent
- [ ] Verify temperature value is displayed
- [ ] Verify system continues operation

#### Task 41 Testing

- [ ] Trigger single fault
- [ ] Verify no fault count displayed
- [ ] Trigger multiple faults
- [ ] Verify correct fault index displayed (e.g., "1/3", not always "1")
- [ ] Verify highest priority fault is shown

#### Task 42 Testing

- [ ] Trigger multiple faults
- [ ] Enter fault inspection mode
- [ ] Navigate through all faults
- [ ] Verify each fault displays correctly
- [ ] Verify fault index updates correctly
- [ ] Verify wrapping works

---

## Acceptance Criteria

### Task 39: Settings And Error UI

- [ ] Settings menu has 3 scrollable items
- [ ] Navigation works with wrapping
- [ ] Each item displays correct bitmap and label
- [ ] Button press logs selection (placeholder for Phase 9)
- [ ] Layout matches Circulation UI style
- [ ] Complies with Requirement 9.21

### Task 40: Warning UI

- [ ] Warning is displayed as overlay
- [ ] Underlying operational UI remains visible
- [ ] Warning indicator is prominent
- [ ] Temperature value is displayed
- [ ] System continues operation
- [ ] Complies with Requirement 9.9

### Task 41: Fault UI

- [ ] Displays highest priority fault
- [ ] Shows correct fault index when multiple faults active
- [ ] Fault count format is "X/Y" (not always "1/Y")
- [ ] Single fault shows no count
- [ ] Multiple faults show correct count

### Task 42: Fault Inspection UI

- [ ] Displays current fault with specific bitmap
- [ ] Shows fault index (e.g., "2/3")
- [ ] Navigation works (left/right)
- [ ] Wrapping works at boundaries
- [ ] Each fault displays correctly
- [ ] Implementation is correct

---

## Documentation

### Files to Update

- [ ] `docs/phase-6b-ui-screens-fix-implementation-plan.md` (this file)
- [ ] `docs/phase-6b-ui-screens-fix-results.md` (after implementation)
- [ ] Update `docs/phase-6-ui-display.md` with corrections

### Documentation Checklist

- [ ] Document all changes made
- [ ] Document testing results
- [ ] Document any issues encountered
- [ ] Document validation results
- [ ] Update phase 6 documentation with corrections

---

## Summary

This implementation plan provides exhaustive checklists for fixing Tasks 39-42:

1. **Task 39**: Complete Settings And Error UI with scrollable menu (3 items)
2. **Task 40**: Fix Warning UI to be overlay (not full-screen)
3. **Task 41**: Fix Fault UI priority display logic
4. **Task 42**: Validate Fault Inspection UI (appears correct)

All changes maintain:
- Bitmap-only UI (no text rendering)
- Scale-by-2 rule for all bitmaps
- Non-blocking architecture
- Centralized configuration
- Integration with existing code

Next: Proceed with Phase 2 implementation plan for complete error handling system.
