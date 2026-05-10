# Phase 9: Feature Control and Sequencing

## Overview

Wired feature control (circulation, heater, massage, jet, ozone, speaker, lights) to state machine with proper state transitions. Added live countdown display for circulation delayed start. Fixed layout spacing to prevent bitmap/label overlap.

## Files Modified

- `include/SafetySystem.h` — Added `hasAnyFeatureActive()` private method
- `include/UIManager.h` — Added `startCirculationCountdown()` public method, `circulationCountdownActive` and `circulationCountdownStartTime` private members
- `lib/SafetySystem/SafetySystem.cpp` — State machine transitions on circulation/heater/feature activation/deactivation, `hasAnyFeatureActive()` implementation
- `lib/UIManager/UIManager.cpp` — Independent countdown timer, layout fixes (bitmap Y, denial Y, thermometer lift)
- `src/main.cpp` — Call `uiManager.startCirculationCountdown()` on successful circulation request

## Implementation Details

### State Machine Driving (SafetySystem)

When SafetySystem activates/deactivates components, it now drives the state machine:

| Action | State Transition |
|--------|-----------------|
| Circulation pump starts | → `STATE_ACTIVE_CIRCULATION` |
| Circulation pump stops | → `STATE_READY` |
| Feature activated (any) | → `STATE_FEATURE_ENABLED_BATH` (from Active_Circulation) |
| Last feature deactivated | → `STATE_ACTIVE_CIRCULATION` (from Feature_Enabled_Bath) |
| Heater auto-started | → `STATE_FEATURE_ENABLED_BATH` (from Active_Circulation) |
| Heater deactivated | → `STATE_ACTIVE_CIRCULATION` (if no other features remain) |

### Independent Countdown Timer

The countdown display is decoupled from SafetySystem's internal state:

- `startCirculationCountdown()` sets `circulationCountdownActive = true` and records `millis()`
- `renderCirculationScreen()` checks `circulationCountdownActive` flag (not `isCirculationSelected()`)
- After 2 seconds of wall time (`CIRCULATION_DELAYED_START_MS`), flag is forced false regardless of SafetySystem state
- `update()` forces continuous 100ms redraw while countdown is active

### Layout Fixes

| Element | Before | After | Purpose |
|---------|--------|-------|---------|
| Regular bitmap Y | 8 | 4 | Creates 4px more room below |
| Bitmap bottom | 40 | 36 | - |
| Denial message Y | 42 | 40 | 4px gap from bitmap (was 2px) |
| Thermo bitmap Y | 16 | 10 | Lifted 6px |
| 2x temp text Y | 24 | 18 | Lifted 6px |
| Decimal text Y | 32 | 26 | Lifted 6px |
| Bottom labels Y | 50 | 50 | Unchanged |

Bottom labels at Y=50 ensure no overlap with bitmaps (regular bitmap ends at 36 = 14px gap, thermometer ends at 42 = 8px gap).

### Hardware Config Summary (Unchanged)

All pin assignments per HardwareConfig.h:
- D8/GPIO15: Buzzer (active-high, boot-safe LOW)
- D5/GPIO14: DS18B20 temperature sensor
- D7/GPIO13: XKC-Y25-V water level sensor
- D6/GPIO12: Encoder CLK
- D4/GPIO2: Encoder DT
- D3/GPIO0: Encoder SW (INPUT_PULLUP, boot-strap sensitive)
- I2C: OLED 0x3C, PCF8574 0x20

## Build Verification

- RAM: 36.0% (29520 bytes)
- Flash: 31.5% (328525 bytes)
- Warnings: 0
- Errors: 0
