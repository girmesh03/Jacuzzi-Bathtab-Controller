# Phase 7: User Interface - Input - Implementation Documentation

## Overview

Phase 7 implements the User Interface Input module for the ESP8266 Jacuzzi Controller. This module manages the rotary encoder (CLK/DT on D6/D4, SW on D3) with rising-edge-only detection for 1:1 response, ezButton-based debouncing for the push button, and context-sensitive action dispatch across all 7 UI states.

**Implementation Date**: May 2026  
**Hardware Validation**: Pending

---

## Pin Validation (Task 46)

### Encoder DT Pin Selection

| Candidate | GPIO | Result | Reason |
|-----------|------|--------|--------|
| A0 | 17 | ❌ Rejected | ADC pin; digital read unreliable for encoder timing |
| D4 | 2 | ✅ Selected | Digital GPIO, not used by I2C (D1/D2) or heartbeat (D0) |
| D0 | 16 | ❌ Rejected | Reserved for heartbeat LED |
| D1/D2 | 5/4 | ❌ Rejected | Reserved for I2C bus |

**Final assignment**:
- Encoder CLK: D6 (GPIO12)
- Encoder DT: D4 (GPIO2)
- Encoder SW: D3 (GPIO0) — boot-strap sensitive, INPUT_PULLUP

### Boot-Strap Safety Order

Per ESP8266 boot constraints:
1. **D8 (GPIO15)** driven LOW first (buzzer pin — must not float HIGH at boot)
2. **D3 (GPIO0)** set `INPUT_PULLUP` second (encoder switch — must not be pulled LOW at boot)
3. All other init follows

These two lines execute before any other initialization in `setup()` (lines 57–61 of `main.cpp`).

---

## Input Handler Design (Task 47)

### Class: `InputHandler` (include/InputHandler.h)

```
InputHandler
├── begin()          — configure encoder pins, attach ezButton
├── update()         — call every loop() cycle (non-blocking)
├── wasRotatedCW()   — rising-edge detected on CLK while DT stable
├── wasRotatedCCW()  — rising-edge detected on CLK while DT stable
├── wasButtonPressed()    — ezButton pressed event
├── isButtonHeld()        — ezButton held longer than BUTTON_HOLD_DURATION_MS
└── consumeRotationEvent()— drain stale rotation events
```

### Encoder Rising-Edge Detection

- **Every physical detent**: One rising edge on CLK
- **Direction**: Read DT state at CLK rising edge
  - DT HIGH → CW (physical CW = forward/increment)
  - DT LOW → CCW (physical CCW = backward/decrement)
- **No step accumulation**: `consumeRotationEvent()` drains stale events
- **Note**: HW CW = physical clockwise = CCW in code; HW CCW = physical counter-clockwise = CW in code (due to encoder signal convention; gating may be inverted)

### Button Debouncing

- **ezButton library**: Internal 50ms debounce
- **Press**: Falling edge (HIGH→LOW) on D3 (GPIO0)
- **Hold**: `isButtonHeld()` returns true after `BUTTON_HOLD_DURATION_MS` (configurable in `InputHandler.cpp`)

---

## Context-Sensitive Actions (Task 47)

| UI State | CW / CCW | Short Press | Long Press |
|----------|----------|-------------|------------|
| READY | — | → Main Menu | — |
| MAIN_MENU | Scroll items | Select → Circulation menu | — |
| CIRCULATION_MENU | Scroll features | Toggle feature on/off | Back to Main Menu |
| FAULT | — | → Fault Inspection | — |
| FAULT_INSPECTION | Scroll faults (multi-fault) | → Back to Fault | — |
| WARNING | — | Acknowledge → Previous state | — |
| STANDBY | — | Wake → Previous state | — |

### Main Menu Navigation

Main Menu lists a single entry: "Start" (Circulation). Selecting it transitions to CIRCULATION_MENU.

### Multi-Fault Browsing (Task 49)

When multiple faults are active, `FAULT_INSPECTION` state supports CW/CCW scrolling through the fault list. `faultInspectionIndex` tracks the current position; rollover wraps at the list boundaries.

### Temperature Adjustment

On the thermometer screen (READY state with heater active), rotation adjusts the setpoint up/down by `TEMP_ADJUSTMENT_INCREMENT` (1.0°C) per detent. The adjustment is clamped to valid range.

### Stale Event Drainage

In Main Menu handler, `inputHandler.consumeRotationEvent()` is called at the start to ensure no accumulated rotation events from the READY→Main Menu transition cause unintended menu scroll.

---

## Implementation Details

### Key Constants

| Constant | Value | Location |
|----------|-------|----------|
| TEMP_ADJUSTMENT_INCREMENT | 1.0°C | SafetyConfig.h |
| BUTTON_HOLD_DURATION_MS | 1000ms | InputHandler.h |
| TEMP_DEFAULT_SETPOINT | 38.0°C | SafetyConfig.h |

### Build Validation

- **Zero warnings, zero errors**
- Flash: 31.3% (327253 bytes)
- RAM: 35.9% (29440 bytes)
- Serial debug: enabled (`ENABLE_SERIAL_DEBUG`)

---

## Files Created/Modified

| File | Change |
|------|--------|
| `include/InputHandler.h` | New — InputHandler class declaration |
| `lib/InputHandler/InputHandler.cpp` | New — InputHandler implementation |
| `include/HardwareConfig.h` | Updated — encoder DT on D4 (GPIO2), boot-strap note |
| `src/main.cpp` | Updated — boot-strap order, input dispatch in loop() |
| `lib/UIManager/UIManager.cpp` | Updated — stale event drain, circulation index reset |
