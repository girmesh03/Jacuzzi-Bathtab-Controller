# Phase 8: Buzzer and Audible Feedback

## Overview

Implemented non-blocking buzzer controller with priority-based beep patterns for audible feedback on user interactions, feature activations, warnings, and faults.

## Files Created

- `include/BuzzerController.h` — BuzzerController class declaration with priority-based beep API
- `lib/BuzzerController/BuzzerController.cpp` — Non-blocking state machine implementation

## Files Modified

- `src/main.cpp` — Added buzzer include, global instance, begin(), update(), handleBuzzerEvents(), and per-handler beep calls

## Implementation Details

### BuzzerController Class

- **Pin**: D8 (GPIO15) — active-high, boot-safe LOW initialized in setup()
- **Architecture**: Non-blocking state machine (`BEEP_IDLE` → `BEEP_ACTIVE` → `BEEP_SILENCE`)
- **Timing**: All durations from `TimingConfig.h` constants, `millis()`-based, no `delay()` calls

### Priority System

| Priority | Pattern      | On Time | Off Time | Repeats | Preempts          |
|----------|-------------|---------|----------|---------|--------------------|
| 3 (highest) | `alertBeep`   | 1000ms  | 300ms    | 3       | All lower priorities |
| 2           | `warningBeep` | 500ms   | 200ms    | 2       | BRIEF, CONFIRMATION |
| 1           | `confirmationBeep` | 200ms | 0     | 1       | BRIEF              |
| 0 (lowest)  | `briefBeep`   | 50ms    | 0        | 1       | None               |

Higher priority beeps preempt active lower-priority beeps. Same/lower priority requests are ignored while a beep is active.

### Buzzer Event Mapping

| Event                        | Beep Pattern      | Trigger Location                |
|------------------------------|-------------------|----------------------------------|
| Button press (all UI states) | `briefBeep()`     | Each `handle*Input()` handler   |
| Button hold (all UI states)  | `briefBeep()`     | Each `handle*Input()` handler   |
| Feature activation success   | `confirmationBeep()` | Circulation handler case 1-6 |
| Temperature setting confirm  | `confirmationBeep()` | Circulation handler case 7  |
| Circulation started          | `confirmationBeep()` | `handleBuzzerEvents()` transition detection |
| Heater activated             | `confirmationBeep()` | `handleBuzzerEvents()` transition detection |
| Warning state entry          | `warningBeep()`   | `handleBuzzerEvents()` state transition |
| Fault state entry            | `alertBeep()`     | `handleBuzzerEvents()` state transition |
| Shutdown state entry         | `alertBeep()`     | `handleBuzzerEvents()` state transition |

### Transition Detection

`handleBuzzerEvents()` is called every loop iteration and tracks:
- Previous state machine state → detects fault/warning/shutdown entry
- Previous `isCirculationStarted()` state → detects circulation activation
- Previous `isHeaterActive()` state → detects heater activation

This approach handles both user-triggered and auto-started events without double-beeping.

### Integration in main.cpp

- **Setup**: `buzzer.begin()` called after `inputHandler.begin()` (pin already set OUTPUT LOW in boot-strap section)
- **Loop**: `buzzer.update()` called in the update chain; `handleBuzzerEvents()` called after `handleUIInput()`
- **Handlers**: `buzzer.briefBeep()` added to every button press/hold path
- **Feature start**: `buzzer.confirmationBeep()` in else-branch of successful `requestFeatureStart()` calls

## Build Verification

- Platform: ESP8266 (esp12e)
- RAM: 36.0% (29520 bytes from 81920 bytes)
- Flash: 31.4% (328125 bytes from 1044464 bytes)
- Warnings: 0
- Errors: 0
