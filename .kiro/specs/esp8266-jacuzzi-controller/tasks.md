# Implementation Plan: ESP8266 Jacuzzi Controller

## Overview

This implementation plan breaks down the ESP8266 Jacuzzi Controller firmware into 10 sequential development phases. Each phase follows the mandatory 6-step Task Execution Protocol and integrates with `src/main.cpp`. The system is an embedded C++ firmware for ESP8266 that provides automated control and monitoring for jacuzzi/bathtub operations with strict safety rules.

**Key Implementation Principles:**
- **Language**: C++ for ESP8266 (PlatformIO build system)
- **Architecture**: Non-blocking, state-machine-driven, event loop
- **Configuration**: All constants centralized in `include/*` headers (NO hardcoded values)
- **Safety First**: Boot fault persistence, thermal runaway detection, safe shutdown sequences
- **UI**: Bitmap-only graphics with scale-by-2 rule, exact bitmap names with `_bitmap` suffix
- **Testing**: Manual hardware validation only (no automated tests)
- **Integration**: Every phase integrates with `src/main.cpp`

**Critical Constraints:**
- NO `flto` (Link Time Optimization) in build configuration
- NO hardcoded constants anywhere (absolute rule)
- NO delay() calls in application logic (non-blocking architecture)
- NO text rendering functions (bitmap-only UI)
- Exact library version: `Adafruit SH110X@^2.1.14`

## Tasks

### Phase 1: Hardware Initialization and Configuration

- [x] 1. Set up project structure and centralized configuration headers
  - Create `include/HardwareConfig.h` with complete pin mapping (D8/GPIO15 buzzer, D5/GPIO14 temp sensor, D7/GPIO13 water level, D6/GPIO12 encoder CLK, A0/ADC0/GPIO17 encoder DT, D3/GPIO0 encoder SW, I2C addresses 0x3C OLED and 0x20 PCF8574)
  - Create `include/SafetyConfig.h` with temperature thresholds and safety limits
  - Create `include/TimingConfig.h` with all timing constants
  - Create `include/StateDefinitions.h` with state enumerations
  - Create `include/FaultCodes.h` with fault code enumerations
  - Configure `platformio.ini` with esp12e board, exact library versions (`Adafruit SH110X@^2.1.14`), `DENABLE_SERIAL_DEBUG` flag, `build_src_filter`, NO `flto`
  - _Requirements: 1.2, 1.13, 1.14, 14.2, 14.3, 14.10, 14.11, 14.12, 14.13, 18.1-18.13_

- [x] 2. Implement boot-safe hardware initialization in src/main.cpp
  - Initialize boot-strap sensitive pins FIRST: D8 (GPIO15) buzzer LOW/OFF, D3 (GPIO0) encoder SW with INPUT_PULLUP
  - Initialize I2C bus before accessing any I2C devices (Wire.begin(), Wire.setClock(100000))
  - Initialize PCF8574 with all relays OFF (write 0xFF = all HIGH = all OFF)
  - Configure all sensor pins: D5 (GPIO14) temp sensor, D7 (GPIO13) water level sensor
  - Configure remaining encoder pins: D6 (GPIO12) CLK
  - Verify I2C devices at addresses 0x3C (OLED) and 0x20 (PCF8574)
  - Implement conditional serial debug output framework using `DENABLE_SERIAL_DEBUG` flag at 115200 baud
  - _Requirements: 1.1, 1.2, 1.3, 1.3a, 1.4, 1.5, 1.6, 1.7, 1.8, 1.12, 1.15, 14.8, 14.9, 18.12_

- [x] 3. Manual hardware validation checkpoint
  - User must manually test hardware initialization on actual ESP8266 hardware
  - Verify boot-safe pins (D8 LOW, D3 with pull-up), I2C device detection (0x3C, 0x20), all relays OFF
  - User must report results and provide explicit approval before proceeding to Phase 2

- [x] 4. Validate Implementation and Document Phase 1 results
  - Validate implementation of phase 1 using `phase-1-checklist.md`
  - Create `docs/phase-1-hardware-initialization.md` documenting pin configuration, I2C setup, boot behavior, and any hardware-specific notes
  - Document board variant selection (esp12e or alternative) if finalized during validation
  - Document any pin mapping caveats or circuit requirements discovered

- [x] 5. Phase 1 post-git workflow
  - Execute `git add`, `git commit -m "Phase 1: Hardware initialization and configuration"`
  - Execute `git push origin feature/phase-1-hardware-init`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync

### Phase 2: Sensor Integration

- [-] 6. Implement DS18B20 temperature sensor driver (non-blocking)
  - Create `lib/SensorManager/SensorManager.cpp` and `include/SensorManager.h`
  - Implement non-blocking temperature reading using state machine (IDLE → REQUEST → WAITING → PROCESS)
  - Read temperature every 2 seconds (from TimingConfig.h) using millis()-based timing
  - Validate readings within defined range (from SafetyConfig.h, e.g., -10°C to 60°C)
  - Implement sensor error counter for consecutive invalid readings
  - Detect communication timeouts and treat as sensor failure
  - Maintain temperature history buffer for thermal runaway detection
  - Integrate with `src/main.cpp` event loop
  - _Requirements: 1.4, 2.1, 2.2, 2.3, 2.4, 2.5, 2.13, 2.14, 13.4_

- [ ] 7. Implement XKC-Y25-V water level sensor driver
  - Add water level sensor reading to SensorManager
  - Read pin D7 (GPIO13) with active-low logic (LOW = sufficient, HIGH = insufficient)
  - Implement non-blocking polling with read period from TimingConfig.h (e.g., 500ms)
  - Detect insufficient water for extended duration (from TimingConfig.h, e.g., 10 seconds)
  - Implement stabilization period for fault recovery (from TimingConfig.h, e.g., 5 seconds)
  - Integrate with `src/main.cpp` event loop
  - _Requirements: 1.5, 3.1, 3.2, 3.3, 3.8, 3.9, 3.10, 13.4_

- [ ] 8. Manual hardware validation checkpoint
  - User must manually test both sensors on actual hardware
  - Verify temperature readings accuracy (DS18B20), water level detection (XKC-Y25-V active-low logic)
  - Verify sensor fault detection (disconnect sensors, verify fault state entry)
  - Verify sensor-to-fault integration and sensor-to-UI status propagation
  - User must report results and provide explicit approval before proceeding to Phase 3

- [ ] 9. Validate Implementation and Document Phase 2 results
  - Validate implementation of phase 2 using `phase-2-checklist.md`
  - Create `docs/phase-2-sensor-integration.md` documenting sensor behavior, timing constants, fault thresholds, and validation results
  - Document temperature history buffer implementation for thermal runaway detection
  - Document any sensor-specific calibration or circuit requirements

- [ ] 10. Phase 2 post-git workflow
  - Execute `git add`, `git commit -m "Phase 2: Sensor integration"`
  - Execute `git push origin feature/phase-2-sensors`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync

### Phase 3: Relay Control and I2C Bus Management

- [ ] 11. Implement I2C bus manager with locking mechanism
  - Create `lib/I2CBusManager/I2CBusManager.cpp` and `include/I2CBusManager.h`
  - Implement lock/release mechanism for I2C bus access
  - Implement transaction timeout handling (timeout from TimingConfig.h, e.g., 1000ms)
  - Prioritize safety-critical relay updates over OLED display refresh
  - Verify I2C device addresses during initialization (0x3C OLED, 0x20 PCF8574 from HardwareConfig.h)
  - Integrate with `src/main.cpp`
  - _Requirements: 1.1, 1.8, 4.12, 17.1, 17.2, 17.3, 17.4, 17.5, 17.6, 17.7, 17.8, 17.9, 17.10, 17.11, 13.5_

- [ ] 12. Implement PCF8574 relay controller with active-low logic
  - Create `lib/RelayController/RelayController.cpp` and `include/RelayController.h`
  - Implement 8-channel relay mapping: Channel 0 (Circulation), 1 (Massage), 2 (Jet), 3 (Heater 3kW/5V/30A), 4 (Ozone), 5 (Speaker), 6 (Lights), 7 (Spare - permanently OFF)
  - Implement active-low logic: HIGH = OFF, LOW = ON (bit manipulation for relay state)
  - Maintain 8-bit relay state model in memory
  - Synchronize relay state with PCF8574 on every state change using I2C bus manager
  - Verify PCF8574 communication before each relay operation
  - Ensure spare channel (bit 7) remains OFF permanently
  - Integrate with `src/main.cpp` event loop
  - _Requirements: 1.3, 1.3a, 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 4.7, 4.8, 4.9, 4.11, 4.13, 4.14, 4.15, 13.6_

- [ ] 13. Implement safe shutdown sequence
  - Add safe shutdown coordinator to RelayController or SafetySystem
  - Implement priority order: Heater OFF (immediate), Ozone OFF (delay), Jet OFF (delay), Massage OFF (delay), Speaker/Lights OFF (delay), Circulation OFF (last, delay)
  - Use non-blocking timing with delays from TimingConfig.h (e.g., 100ms, 200ms)
  - Verify all relays end in HIGH (off) state after shutdown
  - Override all user actions during shutdown
  - Integrate with `src/main.cpp`
  - _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5, 12.6, 12.7, 12.8, 12.9, 12.10, 12.11, 12.12, 12.13, 12.14, 13.6_

- [ ] 14. Manual hardware validation checkpoint
  - User must manually test relay control and I2C bus management on actual hardware
  - Verify all 8 relay channels (active-low logic: HIGH=off, LOW=on), spare channel remains OFF
  - Verify safe shutdown sequence (heater first, circulation last), measure timing
  - Verify I2C conflict handling (OLED vs PCF8574), verify safety-critical relay priority over display updates
  - User must report results and provide explicit approval before proceeding to Phase 4

- [ ] 15. Validate Implementation and Document Phase 3 results
  - Validate implementation of phase 3 using `phase-3-checklist.md`
  - Create `docs/phase-3-relay-i2c.md` documenting relay channel mapping, shutdown sequence timing, I2C arbitration behavior
  - Document PCF8574 communication verification results
  - Document safe shutdown coordinator ownership and trigger paths

- [ ] 16. Phase 3 post-git workflow
  - Execute `git add`, `git commit -m "Phase 3: Relay control and I2C bus management"`
  - Execute `git push origin feature/phase-3-relay-i2c`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync

### Phase 4: State Machine Implementation

- [ ] 17. Implement state machine with all 9 states
  - Create `lib/StateMachine/StateMachine.cpp` and `include/StateMachine.h`
  - Define states: Boot, Self_Check, Ready, Active_Circulation, Feature_Enabled_Bath, Warning, Fault, Fault_Inspection, Shutdown
  - Implement state transition logic with guard conditions
  - Implement entry actions for each state (e.g., Fault entry: execute safe shutdown, display fault bitmap)
  - Implement exit actions for each state
  - Integrate with `src/main.cpp` event loop
  - _Requirements: 8.1, 8.2, 8.13, 8.14, 13.1_

- [ ] 18. Implement boot fault persistence logic
  - In Self_Check state, verify ALL preconditions: water level sufficient, temperature sensor operational, temperature within safe range, I2C devices responding, all relays OFF
  - IF ANY precondition fails, transition to Fault state and persist until ALL conditions resolved
  - System does NOT progress to Ready state until ALL preconditions AND safety conditions satisfied
  - Implement state transitions: Boot → Self_Check (on init complete), Self_Check → Ready (all conditions satisfied), Self_Check → Fault (any condition fails)
  - Add state transition logging when `DENABLE_SERIAL_DEBUG` defined
  - _Requirements: 1.9, 1.10, 8.3, 8.4, 8.5, 8.6, 8.7, 8.15, 11.2, 11.3, 11.5_

- [ ] 19. Implement operational state transitions
  - Implement transitions: Ready → Active_Circulation (user activates circulation), Active_Circulation → Feature_Enabled_Bath (user activates features), Feature_Enabled_Bath → Warning (non-critical warning), Any state → Fault (safety condition fails)
  - Implement Fault → Fault_Inspection (user requests fault review with multiple faults), Fault_Inspection → Fault (user exits inspection)
  - Implement guard conditions for all transitions (e.g., Ready → Active_Circulation requires water level sufficient AND temp sensor operational AND no faults)
  - _Requirements: 8.8, 8.9, 8.10, 8.11, 8.12, 8.13_

- [ ] 20. Implement state-driven UI updates
  - Connect state transitions to UI screen changes
  - Ensure each state displays the correct UI mode (Power-Up, Initialization, Ready, Main Menu, Circulation, Fault, Fault_Inspection, etc.)
  - Integrate state transition logging with `DENABLE_SERIAL_DEBUG`

- [ ] 21. Manual hardware validation checkpoint
  - User must manually test state machine on actual hardware
  - Verify all 9 state transitions with guard conditions
  - Verify boot fault persistence (system stays in Fault until ALL conditions resolved)
  - Verify state entry/exit actions, verify Shutdown state behavior
  - User must report results and provide explicit approval before proceeding to Phase 5

- [ ] 22. Validate Implementation and Document Phase 4 results
  - Validate implementation of phase 4 using `phase-4-checklist.md`
  - Create `docs/phase-4-state-machine.md` documenting state definitions, transitions, guard conditions, entry/exit actions
  - Document boot fault persistence behavior observed on hardware
  - Document Shutdown state trigger and exit conditions

- [ ] 23. Phase 4 post-git workflow
  - Execute `git add`, `git commit -m "Phase 4: State machine implementation"`
  - Execute `git push origin feature/phase-4-state-machine`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync

### Phase 5: Safety System

- [ ] 24. Implement precondition checking for circulation pump
  - Create `lib/SafetySystem/SafetySystem.cpp` and `include/SafetySystem.h`
  - Implement circulation pump precondition check: water level sufficient AND temperature sensor operational AND no active faults
  - Deny activation if ANY precondition fails, display error indication
  - Implement delayed start behavior: activate pump after delay (from TimingConfig.h, e.g., 2 seconds) after user selection
  - Distinguish "circulation selected" state from "circulation started" state
  - Continuously monitor water level while circulation active, execute safe shutdown if water loss detected
  - Integrate with `src/main.cpp`
  - _Requirements: 5.1, 5.2, 5.3, 5.9, 5.11_

- [ ] 25. Implement precondition checking for water heater with absolute circulation dependency
  - Implement heater precondition check: Circulation pump MUST be actively running (ABSOLUTE RULE) AND water level sufficient AND temperature below warning threshold AND no active faults
  - Deny activation if circulation not active, display error message
  - Deny activation if ANY precondition fails
  - Deactivate heater immediately when circulation pump deactivated
  - Implement delayed auto-start: WHEN circulation starts, automatically activate heater after delay (from TimingConfig.h, e.g., 5 seconds) IF all preconditions remain satisfied
  - Implement default set temperature (from SafetyConfig.h, e.g., 38°C) - used when user has not explicitly set target temperature
  - Allow user-adjusted target temperature to override default set temperature
  - Deactivate heater automatically when current temperature reaches or exceeds target temperature (default or user-set)
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.8_

- [ ] 26. Implement feature sequencing and dependent load control
  - Implement precondition checks for massage, jet, ozone, speaker, lights: Circulation pump MUST be active before allowing activation
  - INHIBIT all features until circulation active
  - Deny feature activation if circulation not active, display error message
  - Deactivate all dependent features when circulation deactivated
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.11_

- [ ] 27. Implement thermal runaway detection algorithm
  - Maintain temperature history buffer (size from SafetyConfig.h)
  - Calculate temperature delta over time window
  - Calculate rate of change (°C/second)
  - Detect thermal runaway: IF delta exceeds threshold OR rate exceeds threshold (thresholds from SafetyConfig.h)
  - On thermal runaway detection: enter Fault state, display high_temperature_error_bitmap, execute safe shutdown, stop all heating immediately
  - Require manual acknowledgment before heater can operate again (separate from auto-clear faults)
  - Distinguish thermal runaway (manual ack required) from high temperature warning (auto-clear)
  - _Requirements: 2.8, 2.9, 2.11, 6.6, 11.17, 19.5, 19.7_

- [ ] 28. Implement fault detection for all fault types
  - Implement fault bit field storage for multiple simultaneous faults
  - Implement fault detection: low water level (extended duration), high temperature warning (enters Warning state), critical overtemperature (enters Fault state), thermal runaway (manual ack), temperature sensor failure (consecutive errors), I2C communication failure, PCF8574 failure
  - On fault detection: enter Fault state, execute safe shutdown, store fault in bit field, display fault with specific error bitmap
  - Enable Fault_Inspection browsing when multiple faults active
  - Implement auto-clear logic: low water level (after stabilization), high temp warning (when temp drops), sensor communication (after valid readings), I2C/PCF8574 (when restored)
  - Implement manual acknowledgment requirement for thermal runaway
  - _Requirements: 2.4, 2.7, 2.10, 3.4, 3.5, 3.6, 3.8, 11.1, 11.4, 11.6, 11.7, 11.8, 11.9, 11.14, 11.16, 11.17, 11.18, 19.1, 19.2, 19.3, 19.4, 19.6_

- [ ] 29. Manual hardware validation checkpoint
  - User must manually test safety system on actual hardware
  - Verify circulation preconditions, verify heater absolute circulation dependency (ABSOLUTE RULE)
  - Verify feature inhibition until circulation active
  - Verify thermal runaway detection with simulated temperature changes
  - Verify fault detection for all fault types, verify multi-fault bit field storage
  - User must report results and provide explicit approval before proceeding to Phase 6

- [ ] 30. Validate Implementation and Document Phase 5 results
  - Validate implementation of phase 5 using `phase-5-checklist.md`
  - Create `docs/phase-5-safety-system.md` documenting precondition logic, circulation countdown behavior, heater auto-start timing and temperature target policy
  - Document thermal runaway detection thresholds and manual acknowledgment requirement
  - Document fault detection and auto-clear vs manual acknowledgment behavior

- [ ] 31. Phase 5 post-git workflow
  - Execute `git add`, `git commit -m "Phase 5: Safety system"`
  - Execute `git push origin feature/phase-5-safety`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync

### Phase 6: User Interface - Display

- [ ] 32. Create bitmap definitions with exact names and scale-by-2 rule
  - Create `include/Bitmaps.h` with all bitmap data in PROGMEM
  - Define 13 bitmaps with exact names ending in `_bitmap` suffix: water_drop_bitmap, circulation_bitmap, massage_bitmap, jet_bitmap, heater_bitmap, speaker_bitmap, ozone_bitmap, light_bulb_bitmap, settings_bitmap, thermometer_bitmap, low_water_level_error_bitmap, high_temperature_error_bitmap, sensor_error_bitmap
  - Design all bitmaps at 2x resolution for scale-by-2 display rule
  - Create bitmap glyph set for text rendering: alphanumeric (A-Z, a-z, 0-9), degree symbol (°), common punctuation (.,!?:-/), space
  - Create glyph lookup table structure in PROGMEM
  - **CRITICAL**: Every visible word, digit, degree symbol, label, and fault caption MUST be rendered as bitmap glyphs - NO native text rendering functions allowed
  - _Requirements: 9.1, 9.4, 9.14, 14.1_

- [ ] 33. Implement bitmap rendering engine with scale-by-2 rule
  - Create `lib/UIManager/UIManager.cpp` and `include/UIManager.h`
  - Implement bitmap scaling function: scale down by factor of 2, sample every other pixel from source
  - Implement bitmap-glyph text rendering: lookup glyphs from PROGMEM table, render using scaled bitmaps, NO native font functions (e.g., NO display.print(), NO display.drawChar())
  - Implement temperature display using thermometer_bitmap and bitmap digits (NO text rendering)
  - Implement non-blocking display updates integrated into event loop (update rate from TimingConfig.h, e.g., 100ms = 10Hz)
  - Integrate with `src/main.cpp`
  - _Requirements: 9.2, 9.3, 9.5, 9.6, 9.12, 13.9_

- [ ] 34. Implement Power-Up UI screen
  - Display water_drop_bitmap scaled down by factor of 2
  - Position: horizontally centered, top offset y = -10
  - Duration: visible for at least 3 seconds
  - NO text or labels on this screen (bitmap-only)
  - Purpose: Boot splash screen during hardware initialization
  - _Requirements: 9.7_

- [ ] 35. Implement Initialization/Safety UI screen
  - Display settings_bitmap scaled down by factor of 2
  - Position: horizontally centered, top offset y = -10
  - Display bitmap-glyph text for each initialization and safety check:
    - "Checking I2C..." (rendered as bitmap glyphs)
    - "Checking sensors..." (rendered as bitmap glyphs)
    - "Checking relays..." (rendered as bitmap glyphs)
    - "Checking water level..." (rendered as bitmap glyphs)
    - "Checking temperature..." (rendered as bitmap glyphs)
  - Purpose: Show progress during Self_Check state
  - Transition: To Ready UI when all checks pass, to Fault UI if any check fails
  - _Requirements: 9.7_

- [ ] 36. Implement Ready UI screen
  - Layout: Two-column
    - Left column: thermometer_bitmap scaled to fit
    - Right column: Numeric temperature with degree symbol (rendered as bitmap glyphs, e.g., "38°C")
  - Purpose: System ready, awaiting user input
  - Interaction: Encoder button press transitions to Main Menu UI
  - _Requirements: 9.7, 9.8_

- [ ] 37. Implement Main Menu UI screen
  - Display: One item visible at a time
    - circulation_bitmap with bottom-centered label "Start" (bitmap glyphs)
    - settings_bitmap with bottom-centered label "Settings" (bitmap glyphs)
  - All bitmaps scaled down by factor of 2
  - Interaction:
    - Rotary left/right: Scroll between "Start" and "Settings"
    - Button press: Confirm selection
      - "Start" → Circulation UI
      - "Settings" → Settings And Error UI
  - Purpose: Top-level menu navigation
  - _Requirements: 9.7, 9.8_

- [ ] 38. Implement Circulation UI screen
  - Display: One item visible at a time, scrollable list:
    1. circulation_bitmap - Circulation pump control
    2. massage_bitmap - Massage pump control
    3. jet_bitmap - Jet pump control
    4. heater_bitmap - Water heater control
    5. ozone_bitmap - Ozone generator control
    6. light_bulb_bitmap - Light system control
    7. speaker_bitmap - Speaker relay control
    8. thermometer_bitmap - Temperature display
  - All bitmaps scaled down by factor of 2
  - Interaction:
    - Rotary left/right: Scroll through items
    - Button press: Toggle selected item on/off
    - Special behavior for circulation:
      - "Selected" state: User has selected circulation, delayed start countdown begins (show countdown using bitmap glyphs)
      - "Started" state: Circulation pump actively running after delay
      - Stop circulation: Deactivates all dependent features, returns to Main Menu UI
  - Purpose: Feature control during active bath operation
  - _Requirements: 9.7, 9.8_

- [ ] 39. Implement Settings And Error UI screen
  - Layout: Same interaction style and constraints as Circulation UI
  - Display: One item visible at a time, scrollable list
  - Items: Configuration options, error history, system information (rendered as bitmap glyphs)
  - Interaction: Same as Circulation UI (rotary left/right scrolls, button confirms/toggles)
  - Purpose: System configuration and error review
  - _Requirements: 9.7, 9.8_

- [ ] 40. Implement Warning UI overlay
  - Display: Warning indicator with high_temperature_error_bitmap or appropriate warning bitmap
  - Overlay: Warning displayed over current operational UI
  - Interaction: System continues operation, user can acknowledge warning
  - Purpose: Non-critical warnings that don't require shutdown
  - _Requirements: 9.9_

- [ ] 41. Implement Fault UI screen
  - Display: Fault indicator with specific error bitmap
    - low_water_level_error_bitmap for water level faults
    - high_temperature_error_bitmap for temperature faults
    - sensor_error_bitmap for sensor failures
  - Text: Bitmap-glyph fault description (e.g., "Low Water Level", "Sensor Error")
  - Fault count: Displayed if multiple faults active (e.g., "Fault 1/3" using bitmap glyphs)
  - Interaction: Button press transitions to Fault_Inspection UI if multiple faults
  - Purpose: Display active fault condition
  - _Requirements: 9.9, 9.10, 9.11_

- [ ] 42. Implement Fault_Inspection UI screen
  - Display: Current fault with specific error bitmap
  - Fault navigation: Rotary left/right scrolls through active faults
  - Fault index: Current fault number and total count (bitmap glyphs, e.g., "Fault 2/3")
  - Each fault: Displays with its specific error bitmap and description (bitmap glyphs)
  - Interaction: Button press returns to Fault UI or Ready UI (if faults cleared)
  - Purpose: Browse and review multiple active faults
  - _Requirements: 9.10, 9.11, 11.10, 11.11, 11.13_

- [ ] 43. Manual hardware validation checkpoint
  - User must manually test all UI screens on actual hardware with OLED display
  - Verify all bitmaps render correctly scaled by 2
  - Verify NO native text rendering used (all text as bitmap glyphs)
  - Verify temperature display using bitmap digits
  - Verify all 13 bitmaps display correctly
  - Verify screen-by-screen navigation flow matches UI brief
  - Verify fault count and fault index display with bitmap glyphs
  - Verify multi-fault browsing with multiple simultaneous faults
  - User must report results and provide explicit approval before proceeding to Phase 7

- [ ] 44. Validate Implementation and Document Phase 6 results
  - Validate implementation of phase 6 using `phase-1-checklist.md`
  - Create `docs/phase-6-ui-display.md` documenting all UI screens, bitmap scaling verification, glyph rendering approach
  - Document screen layout definitions for each UI state
  - Document bitmap memory usage and display refresh rate measurements
  - Include screenshots or photos of each UI screen from actual hardware

- [ ] 45. Phase 6 post-git workflow
  - Execute `git add`, `git commit -m "Phase 6: User interface - Display"`
  - Execute `git push origin feature/phase-6-ui-display`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync

### Phase 7: User Interface - Input

- [ ] 46. Validate pin A0 (ADC0/GPIO17) for rotary encoder DT input
  - Test if pin A0 (ADC0/GPIO17) can function correctly as digital input for encoder DT
  - IF A0 cannot work correctly, select alternative digital GPIO pin (e.g., D0/GPIO16, D4/GPIO2)
  - Update HardwareConfig.h with validated pin assignment
  - Document pin validation results and any circuit requirements
  - _Requirements: 10.1, 10.3, 10.4_

- [ ] 47. Implement rotary encoder driver with debouncing
  - Create `lib/InputHandler/InputHandler.cpp` and `include/InputHandler.h`
  - Implement encoder rotation detection: CLK on D6 (GPIO12), DT on validated pin (A0 or alternative), SW on D3 (GPIO0)
  - Use interrupt-driven or polling approach appropriate for pin capabilities
  - Implement button debouncing with timing from TimingConfig.h (e.g., 50ms)
  - Detect clockwise rotation (increment) and counter-clockwise rotation (decrement)
  - Detect button press and button hold (hold duration from TimingConfig.h)
  - Integrate with `src/main.cpp` event loop
  - _Requirements: 10.1, 10.2, 10.6, 10.7, 10.10, 10.12, 13.10_

- [ ] 48. Verify boot-strap safety for D3 (GPIO0) encoder switch
  - Test boot behavior with encoder button pressed to verify no programming mode entry
  - Ensure switch circuit includes proper pull-up resistor
  - Document circuit requirements for boot-safe operation
  - _Requirements: 1.12, 10.5_

- [ ] 49. Implement context-sensitive encoder actions and multi-fault browsing
  - Implement context-sensitive button press actions: select/deselect in menu, on/off toggle for features, back/ok in dialogs, acknowledge in fault state
  - Implement multi-fault browsing: WHILE in Fault_Inspection state, rotary left/right scrolls through fault list
  - Connect encoder press behavior to screen flow defined in Phase 6
  - Provide visual feedback for all encoder interactions (feedback timing from TimingConfig.h, e.g., within 100ms)
  - _Requirements: 10.8, 10.9, 10.11_

- [ ] 50. Manual hardware validation checkpoint
  - User must manually test encoder input on actual hardware
  - Verify rotation detection (CW/CCW), verify button debouncing
  - Verify pin A0 functions correctly as DT input (or alternative pin if A0 failed validation)
  - Verify boot-strap safety for D3 (GPIO0) - test boot with button pressed
  - Verify multi-fault browsing (left/right scrolling through faults)
  - Verify context-sensitive actions in all UI states
  - User must report results and provide explicit approval before proceeding to Phase 8

- [ ] 51. Validate Implementation and Document Phase 7 results
  - Validate implementation of phase 7 using `phase-7-checklist.md`
  - Create `docs/phase-7-ui-input.md` documenting final validated pin mapping for encoder (including A0 validation result)
  - Document debounce and hold-duration constants used
  - Document fault-browsing user flow from actual hardware testing
  - Document D3 boot-strap safety test results and circuit requirements

- [ ] 52. Phase 7 post-git workflow
  - Execute `git add`, `git commit -m "Phase 7: User interface - Input"`
  - Execute `git push origin feature/phase-7-ui-input`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync

### Phase 8: Buzzer and Audible Feedback

- [ ] 53. Implement non-blocking buzzer controller
  - Create `lib/BuzzerController/BuzzerController.cpp` and `include/BuzzerController.h` (or add to existing component)
  - Implement on/off buzzer control on pin D8 (GPIO15) with boot-safe LOW/OFF default
  - Implement non-blocking state machine for buzzer timing (OFF, ON, TIMING states)
  - Use millis() for duration tracking, NO delay() calls
  - Integrate with `src/main.cpp` event loop
  - _Requirements: 1.7, 20.1, 20.2, 20.3, 13.7_

- [ ] 54. Implement buzzer event patterns
  - Implement buzzer activation for circulation pump activation (brief confirmation, duration from TimingConfig.h)
  - Implement buzzer activation for fault detection (alert duration, duration from TimingConfig.h)
  - Implement buzzer activation for user confirmation via encoder button (brief feedback, duration from TimingConfig.h)
  - Implement buzzer activation for temperature warning (warning duration, duration from TimingConfig.h)
  - Implement buzzer activation for safe shutdown (alert duration, duration from TimingConfig.h)
  - Implement overlap prevention: manage alert requests to prevent buzzer overlap
  - Implement priority handling: fault alerts prioritized over confirmation beeps
  - _Requirements: 20.4, 20.5, 20.6, 20.7, 20.8, 20.9, 20.10, 20.11_

- [ ] 55. Manual hardware validation checkpoint
  - User must manually test buzzer operation on actual hardware
  - Verify on/off control (no tone generation), verify boot-safe D8 initialization (LOW/OFF at boot)
  - Verify non-blocking timing for all durations
  - Verify confirmation beep on feature activation, alert sounds on faults
  - Verify no buzzer overlap issues, verify priority handling (fault alerts over confirmation beeps)
  - User must report results and provide explicit approval before proceeding to Phase 9

- [ ] 56. Validate Implementation and Document Phase 8 results
  - Validate implementation of phase 8 using `phase-8-checklist.md`
  - Create `docs/phase-8-buzzer.md` documenting alert priorities, buzzer duration constants, boot-safe D8 behavior
  - Document hardware polarity choice (active-high or active-low) if finalized during validation

- [ ] 57. Phase 8 post-git workflow
  - Execute `git add`, `git commit -m "Phase 8: Buzzer and audible feedback"`
  - Execute `git push origin feature/phase-8-buzzer`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync

### Phase 9: Feature Control and Sequencing

- [ ] 58. Implement circulation pump control with preconditions and delayed start
  - Wire circulation pump control to UI and state machine
  - Verify ALL preconditions before activation: water level sufficient, temp sensor operational, no faults
  - Implement delayed start: activate pump after delay (from TimingConfig.h, e.g., 2 seconds) after user selection
  - Distinguish "circulation selected" state from "circulation started" state in UI (show countdown using bitmap glyphs)
  - Allow user cancellation during countdown
  - Transition to Active_Circulation state when pump starts
  - Deactivate all dependent features when circulation deactivated
  - Return to Main Menu UI when circulation stopped
  - Display circulation status using circulation_bitmap
  - _Requirements: 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.10_

- [ ] 59. Implement water heater control with absolute circulation dependency and auto-start
  - Wire heater control to UI and state machine
  - Verify ABSOLUTE RULE: circulation pump MUST be actively running before allowing heater activation
  - Verify all heater preconditions: circulation active, water level sufficient, temperature below warning threshold, no faults
  - Deny activation if circulation not active, display error message
  - Implement auto-start: WHEN circulation starts, automatically activate heater after delay (from TimingConfig.h, e.g., 5 seconds) IF all preconditions remain satisfied
  - Implement temperature control loop: use default set temperature (from SafetyConfig.h, e.g., 38°C) when user has not explicitly set target, allow user-adjusted target to override default
  - Deactivate heater automatically when current temperature reaches or exceeds target temperature (default or user-set)
  - Monitor temperature continuously, deactivate immediately on thermal runaway or critical overtemperature
  - Deactivate immediately when circulation deactivated
  - Display heater status using heater_bitmap
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6, 6.7, 6.8, 6.9, 6.12_

- [ ] 60. Implement massage, jet, ozone, speaker, and lights control with feature sequencing
  - Wire all feature controls to UI and state machine
  - INHIBIT all features until circulation pump active
  - Verify circulation active before allowing any feature activation
  - Deny activation if circulation not active, display error message
  - Deactivate all features when circulation deactivated
  - Transition to Feature_Enabled_Bath state when features activated beyond circulation
  - Display feature status using corresponding bitmaps: massage_bitmap, jet_bitmap, ozone_bitmap, speaker_bitmap, light_bulb_bitmap
  - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 7.6, 7.7, 7.8, 7.9, 7.10_

- [ ] 61. Manual hardware validation checkpoint
  - User must manually test feature control and sequencing on actual hardware
  - Verify circulation preconditions and delayed start with countdown display
  - Verify heater ABSOLUTE circulation dependency (attempt heater without circulation, verify denial)
  - Verify heater auto-start after circulation starts
  - Verify feature inhibition until circulation active
  - Verify all features deactivate when circulation stops, verify return to Main Menu
  - User must report results and provide explicit approval before proceeding to Phase 10

- [ ] 62. Validate Implementation and Document Phase 9 results
  - Validate implementation of phase 9 using `phase-9-checklist.md`
  - Create `docs/phase-9-feature-control.md` documenting circulation countdown/start behavior, heater auto-start timing, feature on/off sequencing
  - Document return-to-main-menu behavior when circulation stopped
  - Document "selected" vs "started" circulation distinction observed on hardware

- [ ] 63. Phase 9 post-git workflow
  - Execute `git add`, `git commit -m "Phase 9: Feature control and sequencing"`
  - Execute `git push origin feature/phase-9-features`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync

### Phase 10: Integration Testing and Validation

- [ ] 64. Perform complete system integration testing
  - Verify all requirements on actual hardware
  - Test boot sequence: verify boot-safe pins (D3, D8), I2C initialization, relay default OFF state
  - Test boot fault persistence: verify system stays in Fault state until ALL conditions resolved (insufficient water, sensor failure, temperature out of range)
  - Test sensor readings: temperature accuracy, water level detection (active-low logic), sensor fault detection
  - Test relay control: all 8 channels, active-low logic (HIGH=off, LOW=on), spare channel remains OFF
  - Test state machine: all state transitions, guard conditions, entry/exit actions
  - Test safety system: all precondition checks, circulation dependency for heater (ABSOLUTE RULE), feature sequencing, thermal runaway detection
  - Test safe shutdown: priority order (heater first, circulation last), timing, all relays OFF at end
  - Test UI: all bitmaps render correctly scaled by 2, NO text rendering used, temperature display using bitmap digits, all 13 bitmaps displayed
  - Test screen-by-screen UI flow: Power-Up, Initialization, Ready, Main Menu, Circulation, Settings, Warning, Fault, Fault_Inspection
  - Test multi-fault browsing: multiple simultaneous faults, left/right scrolling, fault count display, specific error bitmaps for each fault
  - Test encoder input: rotation detection, button debouncing, context-sensitive actions, boot-strap safety for D3
  - Test buzzer: all event patterns, non-blocking timing, boot-safe D8 initialization
  - Test feature control: circulation delayed start with countdown, heater auto-start, feature inhibition until circulation active, return to Main Menu when circulation stops
  - Measure display refresh rate, input response time, event loop iteration time
  - Verify memory usage within limits (heap, stack, PROGMEM)
  - Verify final A0 encoder DT pin decision documented
  - _Requirements: All requirements 1-20_

- [ ] 65. Create comprehensive documentation
  - Create `docs/phase-10-integration-testing.md` documenting all hardware test procedures and results
  - Document any pin assignment changes (e.g., if A0 replaced for encoder DT)
  - Document circuit requirements for boot-safe operation (D3, D8)
  - Document performance measurements (timing, memory usage)
  - Document known limitations and future enhancements
  - Create user operation guide with bitmap-only UI navigation (screen-by-screen flow)
  - Include screenshots or photos of all UI screens from actual hardware
  - _Requirements: 16.7_

- [ ] 66. Final manual hardware validation checkpoint
  - User must perform final complete system validation on actual hardware
  - User must verify all 20 requirements are satisfied
  - User must test all UI screens match the UI brief exactly
  - User must provide explicit final approval before git closure

- [ ] 67. Phase 10 post-git workflow and final closure
  - Execute `git add`, `git commit -m "Phase 10: Integration testing and validation"`
  - Execute `git push origin feature/phase-10-integration`
  - Merge feature branch to main
  - Delete feature branch
  - Verify repository sync
  - Confirm all 10 phases complete and merged

## Notes

- **NO property-based testing**: This is embedded firmware with hardware integration, not suitable for property-based testing (design explicitly omits Correctness Properties section)
- **Manual hardware testing only**: All validation through manual testing on actual hardware at each phase (no automated test framework) - user must test and approve at each checkpoint
- **Task Execution Protocol**: Each phase follows mandatory 6-step protocol: Pre-Git (git status, branch creation) → Deep codebase analysis (read include/*, lib/*, src/main.cpp, docs/*) → Deep previous-phase analysis → Phase execution (integrate with src/main.cpp, NO hardcoded constants) → User review gate (manual hardware testing, user approval) → Post-Git (documentation in docs/phase-<N>-*, commit, push, merge, cleanup)
- **Documentation required for every phase**: Each phase must create `docs/phase-<N>-*.md` documenting implementation, validation results, and any hardware-specific notes
- **Post-git workflow required for every phase**: Each phase must complete git workflow (commit, push, merge, delete branch, verify sync) before proceeding to next phase
- **AI agent limitations**: AI agent MUST ask user to run PlatformIO commands (build, upload, monitor) - cannot execute directly
- **Centralized configuration**: All constants from include/* headers, NO hardcoded values anywhere (absolute rule)
- **Non-blocking architecture**: NO delay() calls in application logic, all timing uses millis()-based state machines
- **Bitmap-only UI**: NO text rendering functions (e.g., NO display.print(), NO display.drawChar()), all UI elements use bitmaps with exact names ending in _bitmap suffix, all visible text rendered as bitmap glyphs, scale-by-2 rule (absolute)
- **Screen-by-screen UI implementation**: Phase 6 implements exact UI screens from UI brief: Power-Up, Initialization/Safety, Ready, Main Menu, Circulation, Settings And Error, Warning, Fault, Fault_Inspection
- **Boot-strap safety**: D8 (GPIO15) buzzer MUST be LOW at boot, D3 (GPIO0) encoder SW circuit must not force programming mode
- **Build configuration**: NO flto, includes build_src_filter, exact library version Adafruit SH110X@^2.1.14, DENABLE_SERIAL_DEBUG flag
- **Integration**: Every phase integrates with src/main.cpp, incremental development with validation at each step
- **Heater auto-start**: Heater automatically starts after circulation starts (after delay from TimingConfig.h) if all preconditions satisfied - uses default set temperature when user has not set target
- **Checkpoints**: Manual hardware validation checkpoints with explicit user approval required before proceeding to next phase
- **Total tasks**: 67 tasks across 10 phases, each phase includes implementation, manual validation checkpoint, documentation, and post-git workflow
