# Phase 6-7 Minimal Encoder Integration

## Overview

This document describes the minimal rotary encoder input implementation added to enable Phase 6 UI testing. This is a **temporary solution** to unblock Phase 6 hardware validation, as the full Phase 7 implementation requires Phase 6 to be complete first.

## Problem Statement

**Critical Issue**: Phase 6 (User Interface - Display) tasks 37-38 were marked complete, but **cannot be tested** because Phase 7 (Rotary Encoder Input) is not implemented. There is no way to:
- Navigate between UI screens
- Scroll through menu items
- Select/toggle features
- Interact with the controller

**User's Key Insight**: "The implementation of phase 6 completely requires the implementation of phase 7, am not able to interact with the controller and test what implemented"

## Solution: Minimal Encoder Support

A minimal rotary encoder input handler was implemented to enable Phase 6 testing. This provides just enough functionality to:
1. Navigate the Main Menu (left/right)
2. Navigate the Circulation Menu (up/down through 8 items)
3. Select menu items (button press)
4. Toggle features on/off (button press)

## Files Created

### 1. `include/InputHandler.h`
- **Purpose**: Public API for rotary encoder input handling
- **Features**:
  - Non-blocking encoder reading
  - Debounced button press detection
  - Rotation direction detection (CW/CCW)
  - Event-based interface (wasRotatedCW(), wasRotatedCCW(), wasButtonPressed())

### 2. `lib/InputHandler/InputHandler.cpp`
- **Purpose**: Implementation of rotary encoder input logic
- **Features**:
  - State machine for encoder CLK/DT pin reading
  - Button debouncing using ezButton library
  - Non-blocking update() method for event loop integration
  - Debug output when ENABLE_SERIAL_DEBUG defined

### 3. `src/main.cpp` (Modified)
- **Changes**:
  - Added `#include "InputHandler.h"`
  - Declared global instance: `InputHandler inputHandler;`
  - Called `inputHandler.begin()` in `setup()`
  - Called `inputHandler.update()` in `loop()` before UI update
  - Added encoder input handling logic for:
    - Ready UI → Main Menu (button press)
    - Main Menu navigation (CW/CCW rotation)
    - Main Menu selection (button press)
    - Circulation UI navigation (CW/CCW rotation)
    - Circulation UI item toggle (button press)

### 4. `include/UIManager.h` (Modified)
- **Changes**:
  - Added `forceUIState(UIState newState)` method for testing
  - **CRITICAL**: This method bypasses normal state machine logic
  - **Purpose**: Enable manual UI state transitions for Phase 6 testing
  - **Note**: This is a temporary testing method, will be removed in full Phase 7 implementation

## Hardware Configuration

### Rotary Encoder Pins
- **CLK**: D6 (GPIO12) - configured with INPUT_PULLUP
- **DT**: A0 (GPIO17/ADC0) - configured with INPUT
- **SW**: D3 (GPIO0) - configured with INPUT_PULLUP (boot-strap sensitive)

### Boot-Strap Sensitivity
- **D3 (GPIO0)**: Boot-strap sensitive pin, must have pull-up resistor
- **Circuit requirement**: External 10kΩ pull-up resistor recommended
- **Boot behavior**: If D3 is LOW at boot, ESP8266 enters programming mode

## Encoder Input Behavior

### Ready UI
- **Button Press**: Transitions to Main Menu UI
- **Rotation**: No action (not used in Ready UI)

### Main Menu UI
- **Rotation CW**: Navigate right (Circulation → Settings)
- **Rotation CCW**: Navigate left (Settings → Circulation)
- **Button Press**: Confirm selection
  - "Start" (index 0) → Request circulation start
  - "Settings" (index 1) → Not yet implemented

### Circulation UI
- **Rotation CW**: Navigate down through 8 items (0→1→2→...→7→0)
- **Rotation CCW**: Navigate up through 8 items (7→6→5→...→0→7)
- **Button Press**: Toggle selected item on/off
  - Index 0: Circulation pump (start/stop)
  - Index 1: Massage pump (on/off)
  - Index 2: Jet pump (on/off)
  - Index 3: Water heater (on/off)
  - Index 4: Ozone generator (on/off)
  - Index 5: Light system (on/off)
  - Index 6: Speaker relay (on/off)
  - Index 7: Temperature display (no action)

## Integration with Safety System

All encoder actions are routed through the SafetySystem for precondition checking:
- **Circulation start**: `safetySystem.requestCirculationStart()`
- **Feature start**: `safetySystem.requestFeatureStart(channel)`
- **Feature stop**: `safetySystem.requestFeatureStop(channel)`
- **Heater start**: `safetySystem.requestHeaterStart()`

This ensures all safety rules are enforced:
- Water level sufficient
- Temperature sensor operational
- Circulation active before features
- Heater requires circulation (ABSOLUTE RULE)

## Debug Output

When `ENABLE_SERIAL_DEBUG` is defined, the following debug messages are output:

### InputHandler Debug
```
[INPUT] Encoder initialized
[INPUT] Encoder CLK: D6 (GPIO12)
[INPUT] Encoder DT: A0 (GPIO17)
[INPUT] Encoder SW: D3 (GPIO0)
[INPUT] Rotated CW
[INPUT] Rotated CCW
[INPUT] Button pressed
```

### Main Loop Debug
```
[INPUT] Button pressed in Ready UI - transitioning to Main Menu
[INPUT] Rotary CW - Main Menu navigate right
[INPUT] Rotary CCW - Main Menu navigate left
[INPUT] Button pressed in Main Menu - selected index: 0
[INPUT] Requesting circulation start...
[INPUT] Circulation start request ACCEPTED
[INPUT] Rotary CW - Circulation Menu navigate down
[INPUT] Rotary CCW - Circulation Menu navigate up
[INPUT] Button pressed in Circulation UI - selected index: 1
[INPUT] Starting massage...
```

## Testing Instructions

### Build and Upload
```bash
pio run -t upload
```

### Monitor Serial Output
```bash
pio device monitor
```

### Test Sequence

#### 1. Power-Up and Boot
- **Expected**: Water drop bitmap displayed for 3+ seconds
- **Expected**: Transitions to Initialization UI (settings bitmap + "Initializing")
- **Expected**: Transitions to Ready UI (thermometer + temperature)

#### 2. Ready UI → Main Menu
- **Action**: Press encoder button
- **Expected**: Transitions to Main Menu UI
- **Expected**: "Start" displayed with circulation bitmap

#### 3. Main Menu Navigation
- **Action**: Rotate encoder CW
- **Expected**: Displays "Settings" with settings bitmap
- **Action**: Rotate encoder CCW
- **Expected**: Displays "Start" with circulation bitmap

#### 4. Main Menu Selection
- **Action**: Press encoder button (with "Start" selected)
- **Expected**: Circulation start request sent
- **Expected**: Countdown begins (2 seconds)
- **Expected**: Circulation pump activates after countdown
- **Expected**: Transitions to Circulation UI

#### 5. Circulation UI Navigation
- **Action**: Rotate encoder CW
- **Expected**: Scrolls through items: Circulation → Massage → Jet → Heater → Ozone → Lights → Speaker → Temperature → Circulation
- **Action**: Rotate encoder CCW
- **Expected**: Scrolls in reverse order

#### 6. Circulation UI Item Toggle
- **Action**: Navigate to Massage (index 1)
- **Action**: Press encoder button
- **Expected**: Massage pump activates (if preconditions met)
- **Expected**: Serial output: "[INPUT] Starting massage..."
- **Action**: Press encoder button again
- **Expected**: Massage pump deactivates
- **Expected**: Serial output: "[INPUT] Stopping massage..."

#### 7. Feature Precondition Testing
- **Action**: Navigate to Heater (index 3)
- **Action**: Press encoder button
- **Expected**: Heater activates (if circulation active and preconditions met)
- **Expected**: Serial output: "[INPUT] Starting heater..."
- **Action**: Stop circulation (navigate to index 0, press button)
- **Expected**: Heater automatically deactivates (circulation dependency)
- **Expected**: All features deactivate

#### 8. Temperature Display
- **Action**: Navigate to Temperature (index 7)
- **Action**: Press encoder button
- **Expected**: No action (temperature display is read-only)
- **Expected**: Serial output: "[INPUT] Temperature display selected (no action)"

## Known Limitations

### 1. A0 Pin Validation Required
- **Issue**: A0 (GPIO17/ADC0) used for encoder DT input
- **Concern**: A0 is typically an analog input pin
- **Status**: Validation required in Phase 7
- **Workaround**: If A0 doesn't work, may need to use different pin

### 2. Main Menu UI Not Mapped to System State
- **Issue**: UI_MAIN_MENU is not mapped to any SystemState
- **Impact**: Main Menu can only be accessed via forceUIState() method
- **Status**: Will be resolved in full Phase 7 implementation
- **Workaround**: Using forceUIState() for testing

### 3. Settings UI Not Implemented
- **Issue**: "Settings" menu item has no implementation
- **Impact**: Selecting "Settings" does nothing
- **Status**: Task 39 (Settings And Error UI) not yet implemented
- **Workaround**: Only test "Start" menu item

### 4. No Long Press Detection
- **Issue**: Only short button presses detected
- **Impact**: Cannot distinguish short vs long press
- **Status**: Will be added in full Phase 7 implementation
- **Workaround**: Use short presses only

### 5. No Encoder Acceleration
- **Issue**: Rotation speed is constant
- **Impact**: Slow navigation through long lists
- **Status**: Will be added in full Phase 7 implementation
- **Workaround**: Rotate slowly for precise control

## Next Steps

### Immediate (Phase 6 Completion)
1. **User must test** minimal encoder support on hardware
2. **User must verify** all UI screens render correctly
3. **User must verify** encoder navigation works as expected
4. **User must verify** feature toggle works with safety system
5. **User must provide approval** to proceed with remaining Phase 6 tasks

### Phase 6 Remaining Tasks
- [ ] Task 39: Settings And Error UI screen
- [ ] Task 40: Warning UI overlay
- [ ] Task 41: Fault UI screen
- [ ] Task 42: Fault_Inspection UI screen
- [ ] Task 43: Manual hardware validation checkpoint
- [ ] Task 44: Document Phase 6 results
- [ ] Task 45: Phase 6 post-git workflow

### Phase 7 Full Implementation
After Phase 6 is complete, Phase 7 will implement:
- Full encoder input validation (A0 pin testing)
- Long press detection (1 second hold)
- Encoder acceleration (faster rotation = faster navigation)
- Button hold actions (e.g., hold to exit menu)
- Input debouncing improvements
- State machine integration (remove forceUIState() workaround)

## Conclusion

This minimal encoder implementation provides just enough functionality to enable Phase 6 UI testing. It is a **temporary solution** that will be replaced by the full Phase 7 implementation once Phase 6 is validated and complete.

**CRITICAL**: User must test this implementation on hardware and provide feedback before proceeding with remaining Phase 6 tasks.
