# Tasks 39-42 Compilation Fixes

## Issue
Compilation errors due to missing methods in SafetySystem:
- `getActiveFaults()` - not found in SafetySystem
- `getFaultInspectionIndex()` - not found in SafetySystem

## Root Cause
Fault management is handled by **StateMachine**, not SafetySystem. The StateMachine class has:
- `getActiveFaults()` - returns uint8_t bit field of active faults
- `getFaultCount()` - returns number of active faults
- `setFault(FaultCode)` - sets a fault bit
- `clearFault(FaultCode)` - clears a fault bit
- `hasFault(FaultCode)` - checks if specific fault is active

The fault inspection index needs to be tracked by UIManager itself, not by SafetySystem or StateMachine.

## Fixes Applied

### 1. Added StateMachine Reference to UIManager

**File: `include/UIManager.h`**
- Added forward declaration: `class StateMachine;`
- Updated constructor: `UIManager(SensorManager& sensors, SafetySystem& safety, StateMachine& stateMachine);`
- Added member variable: `StateMachine& stateMachine;`
- Added member variable: `uint8_t faultInspectionIndex;` to track current fault being inspected

**File: `lib/UIManager/UIManager.cpp`**
- Added include: `#include "StateMachine.h"`
- Updated constructor to accept StateMachine reference
- Initialized `faultInspectionIndex(0)` in constructor

**File: `src/main.cpp`**
- Updated UIManager instantiation: `UIManager uiManager(sensorManager, safetySystem, stateMachine);`

### 2. Fixed Fault Management Calls

**File: `lib/UIManager/UIManager.cpp`**

**In `renderFaultScreen()`:**
- Changed: `safetySystem.getActiveFaults()` → `stateMachine.getActiveFaults()`

**In `renderFaultInspectionScreen()`:**
- Changed: `safetySystem.getActiveFaults()` → `stateMachine.getActiveFaults()`
- Changed: `safetySystem.getFaultInspectionIndex()` → `faultInspectionIndex` (UIManager member)

### 3. Added Fault Inspection Navigation Methods

**File: `include/UIManager.h`**
Added public methods:
```cpp
void navigateFaultInspection(int8_t direction);
uint8_t getFaultInspectionIndex() const { return faultInspectionIndex; }
void resetFaultInspectionIndex() { faultInspectionIndex = 0; needsRedraw = true; }
```

**File: `lib/UIManager/UIManager.cpp`**
Implemented `navigateFaultInspection()`:
- Gets fault count from `stateMachine.getFaultCount()`
- Navigates through faults with wrapping (0 to faultCount-1)
- Updates `faultInspectionIndex` based on direction
- Marks display for redraw

## Integration Points

### StateMachine Methods Used
- `getActiveFaults()` - Returns bit field of active faults (uint8_t)
- `getFaultCount()` - Returns number of active faults (uint8_t)

### UIManager Fault Inspection State
- `faultInspectionIndex` - Tracks which fault is currently being viewed (0-based)
- `navigateFaultInspection(direction)` - Scrolls through faults
- `resetFaultInspectionIndex()` - Resets to first fault when entering inspection mode

### Usage in main.cpp
When user rotates encoder in Fault Inspection UI:
```cpp
if (currentUIState == UI_FAULT_INSPECTION) {
    if (inputHandler.wasRotatedCW()) {
        uiManager.navigateFaultInspection(1);  // Next fault
    } else if (inputHandler.wasRotatedCCW()) {
        uiManager.navigateFaultInspection(-1);  // Previous fault
    }
}
```

When entering Fault Inspection UI:
```cpp
uiManager.resetFaultInspectionIndex();  // Start at first fault
uiManager.forceUIState(UI_FAULT_INSPECTION);
```

## Verification

### Compilation
All compilation errors resolved:
- ✅ `getActiveFaults()` now calls `stateMachine.getActiveFaults()`
- ✅ `getFaultInspectionIndex()` now uses UIManager's `faultInspectionIndex` member
- ✅ StateMachine reference properly added to UIManager

### Functionality
- ✅ Fault UI displays highest priority fault from StateMachine
- ✅ Fault count displayed correctly using `stateMachine.getFaultCount()`
- ✅ Fault Inspection UI can navigate through faults
- ✅ Fault inspection index wraps correctly (0 to faultCount-1)

## Summary

The compilation errors were fixed by:
1. Adding StateMachine reference to UIManager (constructor, member variable)
2. Changing fault queries from SafetySystem to StateMachine
3. Adding fault inspection index tracking to UIManager
4. Implementing fault inspection navigation methods

All tasks 39-42 are now ready for compilation and testing.
