# Phase 2: Sensor Integration

## Overview

Phase 2 implements non-blocking DS18B20 temperature sensor and XKC-Y25-V water level sensor drivers with fault detection, recovery logic, and full integration with the event loop. Both sensors operate independently using non-blocking state machines and polling, with comprehensive error handling and history tracking for future thermal runaway detection.

## Implementation Date

Completed: 2026-05-08

## Sensor Implementation

### DS18B20 Temperature Sensor

**Hardware Configuration:**
- Pin: D5 (GPIO14)
- Protocol: OneWire
- Resolution: 12-bit (~750ms conversion time)
- Initial Status: Operational (1 sensor found)

**Non-Blocking State Machine:**
- **TEMP_IDLE**: Wait for read interval (2 seconds)
- **TEMP_WAITING**: Wait for DS18B20 conversion (~750ms)
- **TEMP_PROCESS**: Read and validate temperature

**Timing:**
- Read interval: 2000ms (2 seconds)
- Conversion time: 750ms
- Total cycle: ~2.75 seconds per reading

**Validation:**
- Valid range: -10°C to 60°C (from SafetyConfig.h)
- Disconnected sensor detection: -127.0°C (DEVICE_DISCONNECTED_C)
- Range check: TEMP_VALID_MIN to TEMP_VALID_MAX

**Fault Detection:**
- Error threshold: 3 consecutive invalid readings
- Recovery threshold: 3 consecutive valid readings
- Error counter: Increments on invalid reading, resets on valid reading
- Valid counter: Increments on valid reading, resets on invalid reading

**Temperature History Buffer:**
- Size: 10 readings (TEMP_HISTORY_SIZE)
- Implementation: Circular buffer
- Purpose: Thermal runaway detection (Phase 5)
- Storage: float array with index and count tracking

### XKC-Y25-V Water Level Sensor

**Hardware Configuration:**
- Pin: D7 (GPIO13)
- Logic: Active-low (LOW = sufficient, HIGH = insufficient)
- Pull-up: INPUT_PULLUP enabled
- Initial Status: Sufficient

**Non-Blocking Polling:**
- Read interval: 500ms (fast safety response)
- Active-low interpretation: pinState == LOW → sufficient

**Fault Detection:**
- Fault duration: 10 seconds (WATER_LEVEL_FAULT_DURATION_MS)
- Stabilization period: 5 seconds (WATER_LEVEL_STABILIZATION_MS)
- Fault timing: Tracks start time, checks duration
- Recovery timing: Tracks recovery start, checks stabilization

**Fault Logic:**
- Insufficient water detected → start fault timer
- Fault timer exceeds 10 seconds → set fault active
- Sufficient water detected → start recovery timer
- Recovery timer exceeds 5 seconds → clear fault

## Hardware Validation Results

### Test Environment
- Board: ESP8266 esp12e
- Room temperature: 22.4-22.5°C
- Test date: 2026-05-08

### Temperature Sensor Tests

**Normal Operation:**
✅ Temperature readings: 22.4-22.5°C (reasonable room temperature)
✅ Read interval: Every 2 seconds (confirmed)
✅ Non-blocking operation: No delays observed
✅ Conversion timing: ~750ms respected

**Fault Detection Test:**
✅ Sensor disconnected → Invalid readings (-127.0°C)
✅ Error counter incremented (observed up to 14 errors)
✅ "SENSOR FAULT" status displayed after 3 errors
✅ Sensor reconnected → Valid readings resumed (22.4°C)
✅ "[TEMP] Sensor recovered" message after 3 valid readings
✅ Normal operation restored

**Observations:**
- Sensor fault detection is immediate (within 3 read cycles = ~6 seconds)
- Recovery is equally fast (3 valid readings = ~6 seconds)
- Error counter continues incrementing while sensor disconnected
- Temperature history buffer populated correctly

### Water Level Sensor Tests

**Normal Operation:**
✅ Initial status: Sufficient
✅ Active-low logic: LOW = sufficient, HIGH = insufficient
✅ Read interval: Every 500ms (confirmed)
✅ Non-blocking operation: No delays observed

**Fault Detection Test:**
✅ Insufficient water simulated → "Insufficient" status
✅ Fault triggered after 10+ seconds → "WATER LEVEL FAULT"
✅ Fault persisted during insufficient condition
✅ Sufficient water restored → Fault remained active
✅ "[WATER] Fault cleared (stabilized)" after 5 seconds
✅ Normal operation restored

**Observations:**
- Fault detection timing accurate (10 seconds)
- Stabilization period accurate (5 seconds)
- Fault persists correctly during insufficient water
- Recovery requires sustained sufficient water reading

### Integration Tests

**Both Sensors Operating:**
✅ Temperature and water level update independently
✅ No blocking behavior observed
✅ Periodic status output every 5 seconds working correctly
✅ Debug output clear and informative

**Event Loop Performance:**
✅ Non-blocking architecture confirmed
✅ NO delay() calls anywhere
✅ Smooth operation with both sensors active

## Configuration Constants Used

### From SafetyConfig.h
- `TEMP_VALID_MIN`: -10.0°C
- `TEMP_VALID_MAX`: 60.0°C
- `TEMP_HISTORY_SIZE`: 10 readings
- `SENSOR_ERROR_THRESHOLD`: 3 consecutive errors
- `SENSOR_RECOVERY_THRESHOLD`: 3 consecutive valid readings
- `WATER_LEVEL_FAULT_DURATION_MS`: 10000ms (10 seconds)
- `WATER_LEVEL_STABILIZATION_MS`: 5000ms (5 seconds)

### From TimingConfig.h
- `TEMP_SENSOR_READ_INTERVAL_MS`: 2000ms (2 seconds)
- `TEMP_SENSOR_CONVERSION_TIME_MS`: 750ms
- `WATER_LEVEL_READ_INTERVAL_MS`: 500ms

### From HardwareConfig.h
- `PIN_TEMP_SENSOR`: 14 (D5/GPIO14)
- `PIN_WATER_LEVEL`: 13 (D7/GPIO13)

## Implementation Details

### Files Created

**include/SensorManager.h:**
- Complete sensor manager interface
- Temperature sensor state machine enum (3 states)
- Public methods for sensor status and readings
- Private members for state tracking and history
- NO hardcoded constants

**lib/SensorManager/SensorManager.cpp:**
- Full implementation of both sensor drivers
- Non-blocking state machine for temperature
- Non-blocking polling for water level
- Temperature validation and history management
- Fault detection and recovery logic
- Debug output at key points

### Files Modified

**src/main.cpp:**
- Added SensorManager include
- Declared global SensorManager instance
- Called sensorManager.begin() in setup()
- Called sensorManager.update() in loop()
- Added periodic debug output (every 5 seconds)

## Sensor-Specific Notes

### DS18B20 Temperature Sensor

**Circuit Requirements:**
- OneWire protocol requires 4.7kΩ pull-up resistor on data line
- Sensor powered by 3.3V or 5V (both work)
- Data line connected to D5 (GPIO14)

**Calibration:**
- No calibration required
- Factory calibrated to ±0.5°C accuracy
- 12-bit resolution provides 0.0625°C precision

**Known Behavior:**
- Returns -127.0°C when disconnected (DEVICE_DISCONNECTED_C)
- Conversion time ~750ms for 12-bit resolution
- First reading after power-on may be invalid

### XKC-Y25-V Water Level Sensor

**Circuit Requirements:**
- Active-low output (LOW = water detected, HIGH = no water)
- INPUT_PULLUP enabled on D7 (GPIO13)
- Sensor powered by 5V
- Output connected to D7 (GPIO13)

**Calibration:**
- Sensitivity adjustable via potentiometer on sensor module
- Adjust for reliable detection at desired water level
- Test with actual water to verify threshold

**Known Behavior:**
- Capacitive sensing (non-contact)
- May be affected by container material and thickness
- Requires stable mounting for consistent readings

## Troubleshooting

### Temperature Sensor Issues

**Symptom:** "SENSOR FAULT" immediately on boot
- **Cause:** Sensor not connected or wiring issue
- **Solution:** Check OneWire connection, verify 4.7kΩ pull-up resistor

**Symptom:** Temperature readings fluctuate wildly
- **Cause:** Poor connection or electrical noise
- **Solution:** Check wiring, add capacitor near sensor (0.1µF)

**Symptom:** Readings always -127.0°C
- **Cause:** Sensor disconnected or failed
- **Solution:** Replace sensor, check power supply

### Water Level Sensor Issues

**Symptom:** Always reads "Insufficient"
- **Cause:** Sensor sensitivity too high or not detecting water
- **Solution:** Adjust potentiometer, verify sensor placement

**Symptom:** Always reads "Sufficient"
- **Cause:** Sensor sensitivity too low or stuck
- **Solution:** Adjust potentiometer, check sensor for damage

**Symptom:** Readings unstable
- **Cause:** Sensor mounting loose or water level fluctuating
- **Solution:** Secure sensor mounting, verify water level stable

## Memory Usage

**Temperature History Buffer:**
- Size: 10 floats × 4 bytes = 40 bytes
- Plus index and count: 2 bytes
- Total: 42 bytes

**Sensor Manager State:**
- Temperature sensor: ~60 bytes (state, timers, counters, history)
- Water level sensor: ~20 bytes (state, timers, flags)
- Total: ~80 bytes

**PROGMEM Usage:**
- Debug strings stored in flash memory using F() macro
- Minimal RAM impact from debug output

## Next Steps

**Phase 3: Relay Control and I2C Bus Management**
- Implement I2C bus manager with locking mechanism
- Implement PCF8574 relay controller with active-low logic
- Implement safe shutdown sequence
- Integrate with src/main.cpp event loop

## Lessons Learned

1. **State machine simplification** - Removed unused TEMP_REQUEST state, simplified to 3 states
2. **INPUT_PULLUP required** - Water level sensor needs pull-up for reliable active-low operation
3. **Error counter persistence** - Error counter continues incrementing while sensor disconnected (useful for diagnostics)
4. **Stabilization period critical** - 5-second stabilization prevents false fault clears from transient water level changes
5. **Debug output invaluable** - Detailed debug messages made hardware validation straightforward

## Files Created/Modified

### Created:
- `include/SensorManager.h`
- `lib/SensorManager/SensorManager.cpp`
- `docs/phase-2-sensor-integration.md`

### Modified:
- `src/main.cpp` (added SensorManager integration)
- `include/SensorManager.h` (removed unused TEMP_REQUEST state)

## Approval

**User Approval:** ✅ Granted on 2026-05-08
**Hardware Validation:** ✅ All tests passed
**Ready for Phase 3:** ✅ Yes

