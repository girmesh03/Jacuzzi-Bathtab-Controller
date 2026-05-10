# Circulation UI Enhancements

## Overview

Three enhancements have been implemented for the Circulation UI to improve usability and functionality:

1. **ON/OFF Status Display**: Each feature shows its current status in the top right corner
2. **Circulation Stop Returns to Ready UI**: Stopping circulation turns off everything and returns to Ready screen
3. **Temperature Adjustment on Thermometer Screen**: User can adjust target temperature using encoder rotation

## Enhancement 1: ON/OFF Status Display

### Implementation

**File**: `lib/UIManager/UIManager.cpp` - `renderCirculationScreen()`

Each feature item (0-6) now displays its current ON/OFF status in the top right corner of the screen.

**Status Detection**:
- Circulation (index 0): `safetySystem.isCirculationStarted()`
- Massage (index 1): `safetySystem.isFeatureActive(RELAY_CHANNEL_MASSAGE)`
- Jet (index 2): `safetySystem.isFeatureActive(RELAY_CHANNEL_JET)`
- Heater (index 3): `safetySystem.isHeaterActive()`
- Ozone (index 4): `safetySystem.isFeatureActive(RELAY_CHANNEL_OZONE)`
- Lights (index 5): `safetySystem.isFeatureActive(RELAY_CHANNEL_LIGHTS)`
- Speaker (index 6): `safetySystem.isFeatureActive(RELAY_CHANNEL_SPEAKER)`

**Display Position**:
- Top right corner: X = 128 - statusWidth - 2, Y = 2
- Text: "ON" or "OFF" in bitmap glyphs (normal size 8x8)

### User Experience

When navigating through features in Circulation UI:
- User can immediately see if a feature is currently ON or OFF
- No need to remember which features are active
- Clear visual feedback for toggle operations

## Enhancement 2: Circulation Stop Returns to Ready UI

### Implementation

**File**: `src/main.cpp` - Circulation UI button handler (case 0)

When the user stops circulation:
1. `safetySystem.requestCirculationStop()` is called
2. SafetySystem automatically stops all dependent features (massage, jet, heater, ozone, speaker, lights)
3. UI transitions back to Ready screen: `uiManager.forceUIState(UI_READY)`

**Code**:
```cpp
case 0:
    // Circulation pump toggle
    if (safetySystem.isCirculationStarted()) {
        DEBUG_PRINTLN(F("[INPUT] Stopping circulation..."));
        safetySystem.requestCirculationStop();
        
        // CRITICAL: When circulation stops, return to Ready UI
        // This will also stop all features (handled by SafetySystem)
        DEBUG_PRINTLN(F("[INPUT] Returning to Ready UI"));
        uiManager.forceUIState(UI_READY);
    } else {
        DEBUG_PRINTLN(F("[INPUT] Starting circulation..."));
        safetySystem.requestCirculationStart();
    }
    break;
```

### User Experience

**Before**:
- User stops circulation
- UI stays on Circulation UI
- All features stop but user is still in feature menu
- Confusing state

**After**:
- User stops circulation
- All features stop automatically (safety requirement)
- UI returns to Ready screen
- Clear indication that system is back to idle state
- User must go through Main Menu → Start → Circulation UI to restart

## Enhancement 3: Temperature Adjustment on Thermometer Screen

### Implementation

**Files Modified**:
- `include/UIManager.h` - Added temperature adjustment methods and members
- `lib/UIManager/UIManager.cpp` - Implemented temperature adjustment logic
- `src/main.cpp` - Added encoder rotation and button handling for temperature adjustment

### New UIManager Members

**Private Members**:
```cpp
float tempAdjustmentValue;   // Current temperature being adjusted
bool tempAdjustmentActive;   // True when user is adjusting temperature
```

**Public Methods**:
```cpp
void adjustTemperature(int8_t direction);      // Adjust temp by ±0.5°C
void confirmTemperatureSetting();              // Confirm and set new target
float getAdjustedTemperature() const;          // Get adjusted value
bool isTemperatureAdjustmentActive() const;    // Check if adjusting
```

### Thermometer Screen Behavior

**Display Layout** (same as Ready UI):
- Left: Thermometer bitmap (scaled by 2)
- Right: Temperature value with degree symbol (bitmap glyphs)
- Bottom: "Adjusting" indicator when in adjustment mode

**Temperature Source**:
- If NOT adjusting: Shows current target temperature from `safetySystem.getTargetTemperature()`
- If adjusting: Shows `tempAdjustmentValue` being adjusted

**User Interaction**:

1. **Navigate to thermometer** (item 7 in Circulation UI)
2. **Press button** → Enters adjustment mode
   - Initializes `tempAdjustmentValue` with current target temperature
   - Sets `tempAdjustmentActive = true`
   - Shows "Adjusting" indicator at bottom
3. **Rotate encoder left/right** → Adjusts temperature
   - CW (right): Increase by 0.5°C
   - CCW (left): Decrease by 0.5°C
   - Range: 20.0°C to TEMP_WARNING_THRESHOLD (e.g., 42°C)
4. **Press button again** → Confirms setting
   - Calls `safetySystem.setUserTargetTemperature(tempAdjustmentValue)`
   - Sets `tempAdjustmentActive = false`
   - New target temperature is saved

### Encoder Routing in main.cpp

**Circulation UI encoder handling**:
```cpp
if (inputHandler.wasRotatedCW()) {
    // Check if on thermometer screen (index 7)
    if (uiManager.getCirculationMenuSelectedIndex() == 7 && 
        uiManager.isTemperatureAdjustmentActive()) {
        // Adjusting temperature - increase
        uiManager.adjustTemperature(1);
    } else {
        // Normal menu navigation
        uiManager.navigateCirculationMenu(1);
    }
}
```

**Button handling for thermometer**:
```cpp
case 7:
    // Temperature display - button press behavior depends on adjustment state
    if (uiManager.isTemperatureAdjustmentActive()) {
        // Confirm temperature setting
        safetySystem.setUserTargetTemperature(uiManager.getAdjustedTemperature());
        uiManager.confirmTemperatureSetting();
    } else {
        // Start temperature adjustment
        uiManager.adjustTemperature(0);  // Initialize adjustment mode
    }
    break;
```

### User Experience

**Temperature Adjustment Flow**:
1. User navigates to thermometer screen (item 7)
2. Screen shows current target temperature (or default if not set)
3. User presses button → "Adjusting" appears at bottom
4. User rotates encoder to adjust temperature in 0.5°C steps
5. User presses button → Temperature is set, "Adjusting" disappears
6. Heater will now target the new temperature

**Safety Limits**:
- Minimum: 20.0°C (reasonable lower bound)
- Maximum: TEMP_WARNING_THRESHOLD (e.g., 42°C) - prevents setting dangerous temperatures

## Dependencies Added

### UIManager Constructor

**Before**:
```cpp
UIManager::UIManager(SensorManager& sensors)
```

**After**:
```cpp
UIManager::UIManager(SensorManager& sensors, SafetySystem& safety)
```

**Reason**: UIManager needs access to SafetySystem to:
- Check feature ON/OFF status for display
- Get current target temperature for thermometer screen

### main.cpp Instantiation

**Before**:
```cpp
UIManager uiManager(sensorManager);
```

**After**:
```cpp
UIManager uiManager(sensorManager, safetySystem);
```

## Testing Instructions

### Test 1: ON/OFF Status Display

1. Upload code and open serial monitor
2. Navigate to Circulation UI
3. Start circulation (item 0) → Should show "ON" in top right
4. Navigate to other items → Should show "OFF" initially
5. Toggle features on → Should show "ON" when active
6. Toggle features off → Should show "OFF" when inactive

### Test 2: Circulation Stop Returns to Ready

1. Start circulation from Circulation UI
2. Start some features (massage, heater, etc.)
3. Navigate back to circulation (item 0)
4. Press button to stop circulation
5. **Expected**: All features stop, UI returns to Ready screen
6. **Verify**: Must go through Main Menu → Start to access Circulation UI again

### Test 3: Temperature Adjustment

1. Navigate to thermometer screen (item 7)
2. Observe current target temperature displayed
3. Press button → "Adjusting" should appear at bottom
4. Rotate encoder CW → Temperature should increase by 0.5°C
5. Rotate encoder CCW → Temperature should decrease by 0.5°C
6. Press button → "Adjusting" disappears, temperature is set
7. **Verify**: Heater targets the new temperature (check serial monitor)

## Files Modified

1. `include/UIManager.h`
   - Added SafetySystem forward declaration
   - Updated constructor signature
   - Added temperature adjustment methods
   - Added temperature adjustment member variables

2. `lib/UIManager/UIManager.cpp`
   - Updated constructor to accept SafetySystem reference
   - Enhanced `renderCirculationScreen()` to show ON/OFF status
   - Implemented thermometer screen with temperature display
   - Added `adjustTemperature()` method
   - Added `confirmTemperatureSetting()` method

3. `src/main.cpp`
   - Updated UIManager instantiation with SafetySystem reference
   - Enhanced Circulation UI encoder handling for temperature adjustment
   - Added circulation stop → Ready UI transition
   - Added thermometer button handling for adjustment mode

## Summary

These three enhancements significantly improve the Circulation UI usability:

1. **Status visibility**: Users can see at a glance which features are ON/OFF
2. **Clear shutdown flow**: Stopping circulation cleanly returns to Ready state
3. **Temperature control**: Users can easily adjust target temperature with visual feedback

All enhancements maintain the bitmap-only UI requirement and integrate seamlessly with the existing SafetySystem architecture.
