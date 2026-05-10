# Phase 10: Production Hardening

## Overview

Final production hardening pass — fixed all known bugs, eliminated dead code, implemented placeholder fault detection, added watchdog protection, created production build variant, and verified flash/RAM budget.

## Files Modified

- `include/I2CBusManager.h` — Added `update()` public method for stale lock detection
- `include/InputHandler.h` — Removed unused `lastEncoderReadTime` member
- `include/SafetySystem.h` — Added `I2CBusManager& i2cBusManager` reference, extended constructor signature
- `lib/I2CBusManager/I2CBusManager.cpp` — Implemented `update()` method delegating to `updateLockStatus()`
- `lib/InputHandler/InputHandler.cpp` — Removed `lastEncoderReadTime(0)` from initializer list
- `lib/SafetySystem/SafetySystem.cpp` — Implemented `detectI2CFault()` with rate-limited health check; updated constructor
- `lib/UIManager/UIManager.cpp` — Fixed countdown null-termination bug (missing `ci++`)
- `src/main.cpp` — Added `yield()` watchdog feed at loop start, `i2cBus.update()` call, updated SafetySystem constructor
- `platformio.ini` — Added `env:esp12e-prod` production build target without `-DENABLE_SERIAL_DEBUG`
- `README.md` — Added build/flash instructions for both debug and production

## Fix Details

### P0 — Critical Bugs

| # | Issue | File | Fix |
|---|-------|------|-----|
| 1 | Countdown displays bare number without `"s"` suffix | `lib/UIManager/UIManager.cpp:732` | Added missing `ci++` before `'s'` write: `countdownStr[ci++] = 's'` |
| 2 | Stale I2C locks never cleared (private `updateLockStatus()` never called) | `include/I2CBusManager.h`, `I2CBusManager.cpp` | Added public `update()` method calling `updateLockStatus()`, invoked from main loop |
| 3 | No watchdog protection — crash could hang system | `src/main.cpp` | Added `yield()` at start of `loop()` to feed ESP8266 watchdog |
| 4 | No production build variant | `platformio.ini` | Added `env:esp12e-prod` without `-DENABLE_SERIAL_DEBUG` |

### P1 — Robustness

| # | Issue | File | Fix |
|---|-------|------|-----|
| 5 | `detectI2CFault()` was empty placeholder | `lib/SafetySystem/SafetySystem.cpp` | Implemented rate-limited (5s) check: probes both OLED and PCF8574, sets `FAULT_I2C_FAILURE` if neither responds |
| 6 | Unused `lastEncoderReadTime` in InputHandler | `include/InputHandler.h`, `InputHandler.cpp` | Removed dead member variable and initializer |

### SafetySystem I2C Fault Detection

```cpp
void SafetySystem::detectI2CFault() {
    // Rate-limited: checks every 5 seconds
    // Sets FAULT_I2C_FAILURE if NEITHER OLED nor PCF8574 responds
    // (single device failure is handled by individual fault detectors)
}
```

The implementation distinguishes between:
- **I2C bus failure** (both devices silent → `FAULT_I2C_FAILURE`)
- **Individual device failure** (PCF8574 silent → `FAULT_PCF8574_FAILURE`, handled separately)

### Constructor Change

SafetySystem now takes an additional `I2CBusManager&` parameter:

```cpp
// Before:
SafetySystem(SensorManager&, RelayController&, StateMachine&);

// After:
SafetySystem(SensorManager&, RelayController&, StateMachine&, I2CBusManager&);
```

## Build Verification

### Debug (`env:esp12e`)
- RAM: 36.0% (29528 / 81920 bytes)
- Flash: 31.5% (328833 / 1044464 bytes)
- Warnings: 0
- Errors: 0

### Production (`env:esp12e-prod`)
- RAM: 35.8% (29304 / 81920 bytes)
- Flash: 29.3% (305733 / 1044464 bytes)
- Warnings: 0
- Errors: 0

Production build saves ~23KB flash by omitting `Serial.print` / `DEBUG_PRINT*` calls.

## Usage

```bash
# Debug build (serial output enabled)
pio run -e esp12e
pio run -e esp12e -t upload

# Production build (no serial, smaller)
pio run -e esp12e-prod
pio run -e esp12e-prod -t upload
```
