# Navigation and Temperature Adjustment - Debug Guide

## Current Implementation

### Navigation Order Array
```cpp
const uint8_t navigationOrder[] = {0, 1, 2, 3, 4, 6, 5, 7};
```

### Expected Behavior

#### CW Navigation (direction = +1)
Starting from any position, rotating CW should follow this sequence:
1. Circulation (0)
2. Massage (1)
3. Jet (2)
4. Heater (3)
5. Ozone (4)
6. Speaker (6)  ← Note: 6 before 5
7. Light (5)
8. Thermometer (7)
9. Back to Circulation (0)

#### CCW Navigation (direction = -1)
Reverse of CW:
1. Thermometer (7)
2. Light (5)
3. Speaker (6)
4. Ozone (4)
5. Heater (3)
6. Jet (2)
7. Massage (1)
8. Circulation (0)
9. Back to Thermometer (7)

### Temperature Adjustment

#### CW Rotation (direction = +1)
```cpp
if (direction > 0) {
    tempAdjustmentValue += TEMP_ADJUSTMENT_INCREMENT;  // Increases by 0.5°C
}
```

#### CCW Rotation (direction = -1)
```cpp
if (direction < 0) {
    tempAdjustmentValue -= TEMP_ADJUSTMENT_INCREMENT;  // Decreases by 0.5°C
}
```

## Debug Steps

### Step 1: Verify Code Was Uploaded
1. Check serial monitor for debug output
2. Look for: `[INPUT] Rotary CW - Circulation Menu navigate forward`
3. Look for: `[UI] Circulation Menu navigate: index = X`

### Step 2: Test Navigation
1. Start at Circulation (index 0)
2. Rotate CW once
3. **Expected serial output**: `[UI] Circulation Menu navigate: index = 1` (Massage)
4. Rotate CW again
5. **Expected**: `index = 2` (Jet)
6. Continue and verify sequence: 3, 4, 6, 5, 7, 0

### Step 3: Test Temperature
1. Navigate to Thermometer (index 7)
2. Press button to enter adjustment mode
3. **Expected serial output**: `[UI] Temperature adjusted: X.X °C`
4. Rotate CW
5. **Expected**: Temperature value increases
6. Rotate CCW
7. **Expected**: Temperature value decreases

## Possible Issues

### Issue 1: Code Not Uploaded
**Symptom**: Old behavior persists
**Solution**: Run `pio run -t upload` again

### Issue 2: Encoder Wiring Swapped
**Symptom**: CW/CCW reversed from expected
**Solution**: Swap CLK and DT pins in hardware OR swap in InputHandler

### Issue 3: Navigation Array Wrong
**Symptom**: Wrong sequence (e.g., Light before Speaker)
**Current array**: `{0, 1, 2, 3, 4, 6, 5, 7}`
**Verify**: Position 5 = Speaker(6), Position 6 = Light(5)

## Serial Monitor Commands

Enable debug output and watch for these messages:

### Navigation Debug
```
[INPUT] Rotary CW - Circulation Menu navigate forward
[UI] Circulation Menu navigate: index = X
```

### Temperature Debug
```
[INPUT] Rotary CW - Temperature increase
[UI] Temperature adjusted: XX.X °C
```

## Quick Fix if Encoder is Reversed

If the encoder CLK/DT are physically swapped, you can fix in software by swapping the direction values in `src/main.cpp`:

```cpp
// Change this:
if (inputHandler.wasRotatedCW()) {
    uiManager.navigateCirculationMenu(1);  // Forward
}

// To this:
if (inputHandler.wasRotatedCW()) {
    uiManager.navigateCirculationMenu(-1);  // Backward (compensate for swapped wiring)
}
```

## Verification Checklist

- [ ] Code compiled without errors
- [ ] Code uploaded to ESP8266
- [ ] Serial monitor shows debug output
- [ ] Navigation follows correct sequence
- [ ] Temperature increases on CW
- [ ] Temperature decreases on CCW
- [ ] "Set" label appears/disappears correctly
