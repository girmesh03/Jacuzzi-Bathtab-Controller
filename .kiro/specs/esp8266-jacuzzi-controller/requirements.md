# Requirements Document

## Introduction

### Publicly Observable Behavior Only

The ESP8266 Jacuzzi Controller is an **original firmware specification based exclusively on externally observable UI and behavior**. This specification documents only:
- Publicly visible UI elements (display, bitmaps, indicators)
- User interactions (button presses, encoder rotation)
- Observable system responses (relay activation, buzzer sounds, display changes)

**CRITICAL:** No proprietary firmware, source code, hidden communication protocols, or protected assets may be copied or reverse-engineered. Implementation replicates publicly observable controller behavior without copying proprietary code.

### System Overview

The system provides automated control and monitoring for jacuzzi/bathtub operations, managing multiple loads (pumps, heater, ozone generator, lighting, media) through relay control while enforcing strict safety rules. The controller monitors water temperature and level, and provides a bitmap-only user interface through an OLED display and rotary encoder.

### Hardware Pin Mapping with Boot-Safety

**CRITICAL BOOT-STRAP PINS:**
- **Buzzer: D8 (GPIO15)** ⚠️ BOOT-STRAP SENSITIVE - must default LOW/OFF at boot
- **Rotary encoder SW: D3 (GPIO0)** ⚠️ BOOT-STRAP SENSITIVE - switch circuit must not force programming mode

**Sensor Pins:**
- **DS18B20 temperature sensor: D5 (GPIO14)**
- **Water level sensor XKC-Y25-V: D7 (GPIO13)**, active-low

**Rotary Encoder Pins:**
- **Rotary encoder CLK: D6 (GPIO12)**
- **Rotary encoder DT: A0 (ADC0/GPIO17)** ⚠️ NOT NORMAL DIGITAL GPIO - needs validation
- **Rotary encoder SW: D3 (GPIO0)** ⚠️ BOOT-STRAP SENSITIVE

**I2C Devices:**
- **PCF8574 I2C GPIO expander: address 0x20**
- **OLED display: I2C address 0x3C**

**Relay Configuration:**
- **All relays: active-low through PCF8574, HIGH=off, LOW=on**
- **All relay outputs: default OFF (HIGH) at boot**
- **8-channel relay mapping:** Circulation_Pump, Massage_Pump, Jet_Pump, Water_Heater (3kW load, 5V/30A relay), Ozone_Generator, Speaker_Relay, Light_System, spare channel

### Key Architectural Constraints

- **Platform**: ESP8266 (esp12e) with PlatformIO build system
- **Language**: C++ for ESP8266
- **UI Rendering**: Bitmap-only graphics with **ABSOLUTE RULE: all bitmaps scaled down by factor of 2**
- **Bitmap Names**: Exact names with `_bitmap` suffix: `water_drop_bitmap`, `circulation_bitmap`, `massage_bitmap`, `jet_bitmap`, `heater_bitmap`, `speaker_bitmap`, `ozone_bitmap`, `light_bulb_bitmap`, `settings_bitmap`, `thermometer_bitmap`, `low_water_level_error_bitmap`, `high_temperature_error_bitmap`, `sensor_error_bitmap`
- **Numeric Display**: Bitmap-only (bitmap-font strategy or bitmap digit renderer for temperature values)
- **Architecture**: Non-blocking, state-machine-driven, event loop
- **Configuration**: All constants centralized in `include/*` headers (NO hardcoded values anywhere)
- **Integration**: Every development phase integrates with `src/main.cpp`
- **Libraries**: `Adafruit SH110X@^2.1.14` (exact version), rotary encoder support, `ezButton`
- **Testing**: Manual hardware review only (no automated tests)
- **Documentation**: Phase documentation in `docs/phase-<number>-*` format
- **Debug**: `DENABLE_SERIAL_DEBUG` compile flag (exact name with D prefix) for debug output at 115200 baud
- **Safety**: **On boot, unless ALL preconditions AND safety conditions are satisfied, controller MUST remain in fault state**
- **Fault Persistence**: Faults persist from boot until ALL conditions are resolved
- **Fault Handling**: Multiple active faults browsable via left/right encoder rotation in Fault_Inspection state
- **Build Configuration**: NO `flto` (Link Time Optimization), includes `build_src_filter` in `platformio.ini`

### Development Process: Task Execution Protocol

Each phase follows the mandatory 6-step Task Execution Protocol:

1. **Pre-Git**: `git status`, `git branch -vv`, `git fetch origin`, verify clean state, create feature branch
2. **Deep codebase analysis**: Read all `include/*`, `lib/*`, `src/main.cpp`, `docs/*`
3. **Deep previous-phase analysis**: Review phase N-1 documentation
4. **Phase execution**: Implement only current phase, integrate with `src/main.cpp`, NO magic numbers
5. **User review gate**: Summarize changes, user tests on hardware, get approval before proceeding
6. **Post-Git**: Document in `docs/phase-<N>-*`, commit, push, merge, delete branch

**CRITICAL:** AI agent must ask user to run PlatformIO commands (cannot execute directly). No automated tests exist. Hardware review happens at step 5 of each phase.

## Glossary

- **System**: The complete ESP8266 Jacuzzi Controller firmware
- **Controller**: The ESP8266 microcontroller running the firmware
- **OLED_Display**: 1.3 inch 128x64 I2C OLED display at address 0x3C
- **Rotary_Encoder**: KY-040 rotary encoder for user input (CLK->D6, DT->A0, SW->D3)
- **Temperature_Sensor**: DS18B20 one-wire digital temperature sensor on pin D5 (GPIO14)
- **Water_Level_Sensor**: XKC-Y25-V capacitive water level sensor (active-low) on pin D7 (GPIO13)
- **Relay_Module**: 8-channel relay module controlled via PCF8574 I2C GPIO expander at address 0x20
- **Circulation_Pump**: Primary water circulation pump (safety-critical, foundation for all bath features)
- **Massage_Pump**: Massage jet pump
- **Jet_Pump**: High-pressure jet pump
- **Water_Heater**: Electric water heating element (3kW load, 5V/30A relay)
- **Ozone_Generator**: Water sanitization ozone generator
- **Speaker_Relay**: Relay controlling audio/media system
- **Light_System**: Jacuzzi lighting system
- **Buzzer**: Audible alert device on pin D8 (GPIO15) - boot-strap sensitive pin, on/off only (no tone generation)
- **Boot_State**: Initial system state during hardware initialization
- **Self_Check_State**: System state performing safety and sensor validation
- **Ready_State**: System state where all preconditions and safety conditions are met, system is safe, and controller is accepting user commands
- **Active_Circulation_State**: System state where circulation pump is running (foundation for feature operations)
- **Feature_Enabled_Bath_State**: System state where circulation and additional features (massage, jet, heater, etc.) are active
- **Warning_State**: System state indicating non-critical issues requiring attention (e.g., high temperature warning)
- **Fault_State**: System state indicating one or more safety or operational faults (all loads off)
- **Fault_Inspection_State**: System state allowing user to browse and review active faults using encoder
- **Shutdown_State**: System state during controlled shutdown sequence
- **Thermal_Runaway**: Condition where water temperature exhibits unexpected CHANGE or abnormal rate-of-change (not just absolute threshold), requiring immediate stop and manual acknowledgment before heater can operate again
- **Feature_Sequencing**: Rule requiring circulation pump to run before activating dependent features
- **Relay_Active_Low**: Relay control logic where LOW signal activates relay, HIGH deactivates
- **Non_Blocking_Architecture**: Design pattern avoiding delay() calls in application logic
- **State_Machine**: Formal state management system with explicit states, transitions, and guards
- **Persistent_Fault**: Fault condition that remains active from boot until all preconditions and safety conditions are satisfied
- **Fault_Persistence**: Behavior where controller does NOT progress to normal ready state until all boot conditions are safe
- **Bitmap_UI**: User interface rendered exclusively using pre-defined bitmap graphics with exact names ending in `_bitmap` suffix
- **Bitmap_Scale_Rule**: Absolute rule that all bitmaps MUST be scaled down by factor of 2 when displayed
- **PCF8574**: I2C GPIO expander chip at address 0x20
- **I2C_Bus**: Inter-Integrated Circuit communication bus shared by OLED_Display and PCF8574
- **I2C_Conflict**: Condition where multiple devices attempt simultaneous I2C bus access
- **Event_Loop**: Main program loop processing events without blocking
- **Phase**: Development milestone with specific deliverables and acceptance criteria
- **Task_Execution_Protocol**: Mandatory 6-step process for each development phase (Pre-Git, Deep codebase analysis, Deep previous-phase analysis, Phase execution, User review gate, Post-Git)
- **DENABLE_SERIAL_DEBUG**: Compile-time flag (exact name with D prefix) enabling serial debug output at 115200 baud
- **PROGMEM**: ESP8266 program memory storage for constants to reduce RAM usage
- **Safe_Shutdown**: Controlled deactivation of all loads in proper sequence (heater first, then other loads, circulation last)
- **Sensor_Failure**: Condition where a sensor provides invalid or no data
- **Precondition**: Required condition that must be met before an operation
- **Guard_Condition**: Boolean expression controlling state transition eligibility
- **Interlock**: Safety mechanism preventing unsafe operation combinations
- **Boot_Strap_Pin**: GPIO pin with special boot-time behavior (GPIO0, GPIO15) requiring careful circuit design
- **Publicly_Observable_Behavior**: Externally visible system behavior without access to proprietary internals
- **PlatformIO**: Build system and development platform for embedded systems
- **build_src_filter**: PlatformIO configuration option for selective source compilation
- **src_main_cpp**: Central integration point at `src/main.cpp` for all phase implementations
- **include_directory**: Centralized location (`include/*`) for all configuration constants and headers (NO hardcoded constants anywhere)
- **Fault_Code**: Enumerated identifier for specific fault conditions
- **Multi_Fault_Browsing**: Capability to scroll left/right through multiple active faults using encoder rotation

## Requirements

### Requirement 1: System Initialization and Hardware Configuration

**User Story:** As a system operator, I want the controller to initialize all hardware components correctly on boot with complete pin mapping and boot-safety, so that the system starts in a known safe state.

#### Acceptance Criteria

1. WHEN the Controller boots, THE System SHALL initialize the I2C_Bus before accessing OLED_Display or PCF8574
2. WHEN the Controller boots, THE System SHALL configure all hardware pins according to the complete pin mapping:
   - Buzzer: D8 (GPIO15) ⚠️ BOOT-STRAP SENSITIVE - must default LOW/OFF at boot
   - DS18B20 temperature sensor: D5 (GPIO14)
   - Water level sensor XKC-Y25-V: D7 (GPIO13), active-low
   - Rotary encoder CLK: D6 (GPIO12)
   - Rotary encoder DT: A0 (ADC0/GPIO17) ⚠️ NOT NORMAL DIGITAL GPIO - needs validation
   - Rotary encoder SW: D3 (GPIO0) ⚠️ BOOT-STRAP SENSITIVE - switch circuit must not force programming mode
   - PCF8574 I2C GPIO expander: address 0x20
   - OLED display: I2C address 0x3C
3. WHEN the Controller boots, THE System SHALL set all Relay_Module outputs to HIGH (off state) as the default boot condition
3a. THE System SHALL NOT energize any relay output before boot safety checks complete
4. WHEN the Controller boots, THE System SHALL initialize the Temperature_Sensor on pin D5 (GPIO14) and verify communication
5. WHEN the Controller boots, THE System SHALL initialize the Water_Level_Sensor on pin D7 (GPIO13) with active-low logic and read initial state
6. WHEN the Controller boots, THE System SHALL initialize the Rotary_Encoder with CLK on pin D6 (GPIO12), DT on pin A0 (ADC0/GPIO17), and SW on pin D3 (GPIO0)
7. WHEN the Controller boots, THE System SHALL initialize the Buzzer on pin D8 (GPIO15) in OFF state to ensure boot-safe operation
8. WHEN the Controller boots, THE System SHALL verify OLED_Display at I2C address 0x3C and PCF8574 at I2C address 0x20 share I2C_Bus without conflict
9. **WHEN the Controller boots, IF water level is insufficient OR temperature is unsafe OR any sensor fails initialization, THEN THE System SHALL enter Fault_State and persist fault until ALL conditions are resolved (boot fault persistence)**
10. **WHEN the Controller boots with unsafe conditions, THE System SHALL NOT progress to Ready_State until ALL preconditions AND safety conditions are satisfied**
11. IF any hardware initialization fails, THEN THE System SHALL enter Fault_State and display the specific initialization error
12. THE System SHALL configure pin D3 (GPIO0) and pin D8 (GPIO15) with boot-safe circuit design to prevent unintended programming mode or boot failure
13. THE System SHALL store all configuration constants (pin assignments, I2C addresses, timing values, thresholds) in centralized headers under `include/*` directory
14. THE System SHALL NOT use hardcoded constants anywhere in the codebase
15. WHERE DENABLE_SERIAL_DEBUG is defined, THE System SHALL output boot initialization logs via serial at 115200 baud

**Design Notes to be Finalized in Design Phase:**
- Boot splash screen timing and content
- Exact initialization timeout values (unless safety-critical)

### Requirement 2: Temperature Monitoring and Thermal Safety

**User Story:** As a system operator, I want continuous temperature monitoring with fault detection and thermal runaway protection based on unexpected temperature CHANGE or DELTA, so that I can trust the temperature readings and prevent dangerous conditions.

#### Acceptance Criteria

1. WHILE the System is running, THE System SHALL read Temperature_Sensor every 2 seconds using non-blocking operations
2. WHEN a temperature reading is obtained, THE System SHALL validate the reading is within a defined valid range
3. IF Temperature_Sensor returns an invalid reading, THEN THE System SHALL increment a sensor error counter
4. IF Temperature_Sensor returns consecutive invalid readings beyond a threshold, THEN THE System SHALL enter Fault_State with sensor_error_bitmap indication
5. WHEN a valid temperature reading is obtained after sensor failure, THE System SHALL reset the sensor error counter
6. THE System SHALL display current water temperature on OLED_Display using thermometer_bitmap and numeric value rendered in bitmap-only format (bitmap digits or bitmap font)
7. WHEN temperature exceeds a warning threshold, THE System SHALL activate high temperature warning indication using high_temperature_error_bitmap and enter Warning_State
8. **THE System SHALL distinguish three temperature conditions:**
   - **High temperature warning: temperature exceeds warning threshold, enters Warning_State, non-fault condition**
   - **Critical overtemperature fault: temperature exceeds critical threshold, enters Fault_State**
   - **Thermal runaway fault: unexpected temperature CHANGE or abnormal rate-of-change, enters Fault_State, requires manual acknowledgment before heater can operate again**
9. **IF temperature exhibits unexpected CHANGE or DELTA indicating Thermal_Runaway, THEN THE System SHALL enter Fault_State, display high_temperature_error_bitmap, execute Safe_Shutdown, and stop all heating operations immediately**
10. IF absolute temperature exceeds a critical fault threshold, THEN THE System SHALL enter Fault_State with critical overtemperature indication and execute Safe_Shutdown
11. **WHEN Thermal_Runaway is detected, THE System SHALL require manual acknowledgment before heater can operate again**
12. THE System SHALL distinguish between high_temperature_error_bitmap (absolute threshold or thermal runaway) and sensor_error_bitmap (sensor failure) in UI
13. THE System SHALL detect Temperature_Sensor communication timeouts and treat as sensor failure
14. THE System SHALL store all temperature thresholds and timing values as centralized constants in `include/*` headers

**Design Notes to be Finalized in Design Phase:**
- Specific temperature ranges (e.g., -10°C to 60°C)
- Warning threshold (e.g., 42°C)
- Fault threshold (e.g., 45°C)
- Timeout duration (e.g., 5s)
- Temperature delta calculation for thermal runaway detection
- Number of consecutive invalid readings (e.g., 3)

### Requirement 3: Water Level Detection and Safety

**User Story:** As a system operator, I want reliable water level detection with safety interlocks and persistent fault handling including multi-fault browsing, so that pumps and heaters never operate without sufficient water.

#### Acceptance Criteria

1. WHILE the System is running, THE System SHALL read Water_Level_Sensor on pin D7 (GPIO13) using non-blocking operations
2. WHEN Water_Level_Sensor reads LOW, THE System SHALL interpret this as sufficient water level (active-low logic)
3. WHEN Water_Level_Sensor reads HIGH, THE System SHALL interpret this as insufficient water level (active-low logic)
4. **IF Water_Level_Sensor indicates insufficient water level at boot, THEN THE System SHALL enter Fault_State and persist fault until water level is sufficient (boot fault persistence)**
5. IF Water_Level_Sensor indicates insufficient water level, THEN THE System SHALL prevent activation of Circulation_Pump, Massage_Pump, Jet_Pump, and Water_Heater
6. IF insufficient water level is detected WHILE any pump or Water_Heater is active, THEN THE System SHALL execute Safe_Shutdown
7. THE System SHALL display water level status on OLED_Display using water_drop_bitmap
8. IF Water_Level_Sensor indicates insufficient water level for an extended duration, THEN THE System SHALL enter Fault_State with low_water_level_error_bitmap indication
9. WHEN sufficient water level is restored, THE System SHALL clear the low water level fault after a stabilization period of stable readings
10. THE System SHALL store all water level timing values as centralized constants in `include/*` headers

**Design Notes to be Finalized in Design Phase:**
- Extended duration threshold (e.g., 10 seconds)
- Stabilization period (e.g., 5 seconds)
- Sensor reading period (e.g., 500ms)

### Requirement 4: Relay Control and Load Management

**User Story:** As a system operator, I want safe and reliable control of all loads through the relay module with explicit 8-channel mapping and boot-safe defaults, so that equipment operates correctly and safely.

#### Acceptance Criteria

1. THE System SHALL control all relays through PCF8574 at I2C address 0x20 using Relay_Active_Low logic (HIGH=off, LOW=on)
2. WHEN activating a relay, THE System SHALL write LOW to the corresponding PCF8574 output pin
3. WHEN deactivating a relay, THE System SHALL write HIGH to the corresponding PCF8574 output pin
4. **THE System SHALL define explicit 8-channel relay mapping: Circulation_Pump, Massage_Pump, Jet_Pump, Water_Heater (3kW load, 5V/30A relay), Ozone_Generator, Speaker_Relay, Light_System, spare channel**
5. **THE System SHALL reserve the spare relay channel (8th channel) as permanently OFF unless explicitly assigned in a future phase**
6. **THE System SHALL NOT energize the spare relay channel during normal operation**
7. **WHEN the Controller boots, THE System SHALL ensure all relay outputs default to OFF state (HIGH signal) before any other operations**
8. THE System SHALL maintain relay state model in memory and synchronize with PCF8574 on every state change
9. THE System SHALL maintain relay state model for UI/hardware synchronization
10. WHEN the System enters Fault_State, THE System SHALL deactivate all relays
11. THE System SHALL implement non-blocking relay operations integrated into the event loop
12. **THE System SHALL prioritize safety-critical relay writes (pump shutdown, heater shutdown) over OLED_Display refresh operations to prevent starvation**
13. THE System SHALL verify PCF8574 communication before each relay state change
14. IF PCF8574 communication fails, THEN THE System SHALL enter Fault_State and attempt no further relay operations until communication is restored
15. THE System SHALL store relay channel assignments and I2C address as centralized constants in `include/*` headers

**Design Notes to be Finalized in Design Phase:**
- Specific relay activation delay values (e.g., 0ms, 1000ms, 2000ms, 3000ms)
- Relay state synchronization strategy details
- I2C transaction retry logic

### Requirement 5: Circulation Pump Control and Safety Interlocks

**User Story:** As a system operator, I want the circulation pump to operate safely with proper preconditions as the foundation for all bath features, so that it provides reliable water circulation and enables feature-enabled bath operations.

#### Acceptance Criteria

1. **WHEN the user requests Circulation_Pump activation, THE System SHALL verify ALL required preconditions AND safety conditions are satisfied: Water_Level_Sensor indicates sufficient water AND Temperature_Sensor is operational AND no active faults exist**
2. **THE System SHALL NOT allow Circulation_Pump to run unless ALL preconditions AND safety conditions are satisfied**
3. **IF ANY precondition or safety condition fails, THEN THE System SHALL deny Circulation_Pump activation and display appropriate error indication**
4. WHEN Circulation_Pump is activated, THE System SHALL transition to Active_Circulation_State
5. **WHEN Circulation_Pump is active, THE System SHALL establish the foundation state for Feature_Enabled_Bath_State operations (massage, jet, heater, ozone, etc.)**
6. WHEN Circulation_Pump is deactivated, THE System SHALL deactivate all dependent features (Massage_Pump, Jet_Pump, Water_Heater, Ozone_Generator)
7. THE System SHALL display Circulation_Pump status on OLED_Display using circulation_bitmap
8. WHILE Circulation_Pump is active, THE System SHALL continuously monitor Water_Level_Sensor for changes
9. IF Water_Level_Sensor indicates water loss WHILE Circulation_Pump is active, THEN THE System SHALL execute Safe_Shutdown
10. THE System SHALL allow manual Circulation_Pump deactivation at any time through user interface
11. THE System SHALL store all circulation pump timing and threshold values as centralized constants in `include/*` headers
12. **THE System SHALL support delayed start behavior: WHEN user selects circulation in UI, THE System SHALL activate Circulation_Pump after a delay (from TimingConfig.h) to allow user confirmation**
13. **THE System SHALL distinguish between "circulation selected" state and "circulation started" state in the UI**

**Design Notes to be Finalized in Design Phase:**
- Buzzer confirmation on activation
- Delayed start timing after user selection (e.g., 2 seconds)
- Exact timing for dependent feature deactivation
- Water level monitoring frequency
- Whether circulation is blocked on temperature sensor failure alone (without water level or other faults) is a design decision to be finalized based on safety policy

### Requirement 6: Water Heater Control with Safety Interlocks

**User Story:** As a system operator, I want the water heater to operate only under safe conditions with absolute dependency on circulation, so that it heats water effectively without safety risks.

#### Acceptance Criteria

1. **THE System SHALL NEVER allow Water_Heater activation unless Circulation_Pump is actively running (absolute rule)**
2. **WHEN the user requests Water_Heater activation, THE System SHALL verify ALL preconditions are satisfied: Circulation_Pump is active AND Water_Level_Sensor indicates sufficient water AND temperature is below warning threshold AND no active faults exist**
3. **IF Circulation_Pump is not active, THEN THE System SHALL deny Water_Heater activation and display an error message**
4. **IF ANY precondition or safety condition fails, THEN THE System SHALL deny Water_Heater activation**
5. WHILE Water_Heater is active, THE System SHALL monitor temperature continuously using non-blocking operations
6. **IF temperature exhibits unexpected CHANGE or DELTA indicating Thermal_Runaway WHILE Water_Heater is active, THEN THE System SHALL deactivate Water_Heater immediately and enter Fault_State**
7. IF temperature exceeds a critical threshold WHILE Water_Heater is active, THEN THE System SHALL deactivate Water_Heater immediately
8. WHEN Circulation_Pump is deactivated, THE System SHALL deactivate Water_Heater immediately
9. THE System SHALL display Water_Heater status on OLED_Display using heater_bitmap
10. **THE System SHALL control Water_Heater through a relay rated for 3kW load using 5V/30A relay specification (hardware requirement)**
11. THE System SHALL integrate Water_Heater control into non-blocking state management
12. THE System SHALL store all heater thresholds and timing values as centralized constants in `include/*` headers
13. **THE System SHALL support delayed auto-start behavior: WHEN Circulation_Pump starts AND heater auto-start is enabled, THE System SHALL automatically activate Water_Heater after a delay (from TimingConfig.h) IF all heater preconditions remain satisfied**
14. **THE System SHALL use a default set temperature for heater control IF the user has not explicitly set a target temperature**
15. **THE System SHALL allow user-adjusted target temperature to override the default set temperature**
16. **THE System SHALL deactivate Water_Heater automatically WHEN current temperature reaches or exceeds the target temperature (default or user-set)**

**Design Notes to be Finalized in Design Phase:**
- Specific temperature thresholds (e.g., 42°C warning, 43°C deactivation, 45°C fault)
- Default set temperature value (e.g., 38°C)
- Delayed auto-start timing after circulation starts (e.g., 5 seconds)
- Exact deactivation timing
- Temperature monitoring frequency

### Requirement 7: Feature Sequencing and Dependent Load Control

**User Story:** As a system operator, I want massage pump, jet pump, ozone generator, speaker, and lights to be INHIBITED until circulation is active, so that features operate in the correct sequence and only in appropriate bath states.

#### Acceptance Criteria

1. **THE System SHALL INHIBIT activation of Massage_Pump, Jet_Pump, Ozone_Generator, Speaker_Relay, and Light_System until Circulation_Pump is active**
2. WHEN the user requests Massage_Pump activation, THE System SHALL verify Circulation_Pump is active before allowing activation
3. WHEN the user requests Jet_Pump activation, THE System SHALL verify Circulation_Pump is active before allowing activation
4. WHEN the user requests Ozone_Generator activation, THE System SHALL verify Circulation_Pump is active before allowing activation
5. WHEN the user requests Speaker_Relay activation, THE System SHALL verify Circulation_Pump is active before allowing activation
6. WHEN the user requests Light_System activation, THE System SHALL verify Circulation_Pump is active before allowing activation
7. IF Circulation_Pump is not active, THEN THE System SHALL deny activation of dependent features and display an error message
8. WHEN Circulation_Pump is deactivated, THE System SHALL deactivate Massage_Pump, Jet_Pump, Ozone_Generator, Speaker_Relay, and Light_System
9. THE System SHALL allow feature activation only in Active_Circulation_State or Feature_Enabled_Bath_State
10. **THE System SHALL display status of all features on OLED_Display using exact bitmap names: massage_bitmap, jet_bitmap, ozone_bitmap, speaker_bitmap, light_bulb_bitmap**
11. THE System SHALL store all feature sequencing timing values as centralized constants in `include/*` headers

**Design Notes to be Finalized in Design Phase:**
- Specific activation delay values (e.g., 2 seconds for massage, 3 seconds for jet)
- Feature deactivation timing sequence

### Requirement 8: State Machine Implementation

**User Story:** As a developer, I want a formal state machine with explicit states including Boot, Self_Check, Idle, Active_Circulation, Feature_Enabled_Bath, Warning, Fault, Fault_Inspection, and Shutdown, so that system behavior is predictable and maintainable.

#### Acceptance Criteria

1. **THE System SHALL implement states: Boot, Self_Check, Ready, Active_Circulation, Feature_Enabled_Bath, Warning, Fault, Fault_Inspection, Shutdown**
2. WHEN the Controller boots, THE System SHALL enter Boot state
3. WHEN hardware initialization completes successfully, THE System SHALL transition from Boot to Self_Check state
4. WHEN Self_Check completes with no faults AND all preconditions and safety conditions are satisfied, THE System SHALL transition to Ready state
5. **IF Self_Check detects any fault OR any precondition is not satisfied, THEN THE System SHALL transition to Fault state and persist fault until ALL conditions are resolved (boot fault persistence)**
6. **WHEN the Controller boots with unsafe conditions (insufficient water, sensor failure, temperature out of range), THE System SHALL enter Fault state after Self_Check and persist fault until ALL conditions are safe**
7. **THE System SHALL NOT progress to Ready state from Self_Check unless ALL preconditions AND safety conditions are satisfied**
8. WHEN Circulation_Pump is activated from Ready state, THE System SHALL transition to Active_Circulation state
9. WHEN additional features (massage, jet, heater, ozone, speaker, lights) are activated beyond circulation, THE System SHALL transition to Feature_Enabled_Bath state
10. WHEN any safety fault occurs, THE System SHALL transition to Fault state from any other state
11. **WHEN in Fault state with multiple active faults, THE System SHALL allow transition to Fault_Inspection state for error review and browsing**
12. WHEN in Warning state, THE System SHALL allow continued operation with visible warning indication
13. THE System SHALL define Guard_Condition functions for all state transitions
14. THE System SHALL execute entry actions when entering each state and exit actions when leaving each state
15. WHERE DENABLE_SERIAL_DEBUG is defined, THE System SHALL log all state transitions via serial output

**Design Notes to be Finalized in Design Phase:**
- State transition timing details
- State entry/exit action implementation details

### Requirement 9: User Interface and Display Management

**User Story:** As a user, I want a clear bitmap-only interface with exact bitmap names and scale-by-2 rule showing system status and allowing control with multi-fault browsing, so that I can operate the jacuzzi effectively.

#### Acceptance Criteria

1. **THE System SHALL render all UI elements exclusively using pre-defined bitmaps stored in PROGMEM (bitmap-only UI)**
2. **THE System SHALL render all visible characters, digits, status messages, and fault information using bitmaps or bitmap glyphs exclusively**
3. **THE System SHALL NOT use any text rendering functions or character-based display output**
4. **THE System SHALL implement bitmaps with exact names ending in `_bitmap` suffix: water_drop_bitmap, circulation_bitmap, massage_bitmap, jet_bitmap, heater_bitmap, speaker_bitmap, ozone_bitmap, light_bulb_bitmap, settings_bitmap, thermometer_bitmap, low_water_level_error_bitmap, high_temperature_error_bitmap, sensor_error_bitmap**
5. **WHEN displaying bitmaps, THE System SHALL scale all bitmaps down by factor of 2 (absolute rule)**
6. **THE System SHALL display current temperature using thermometer_bitmap and numeric value rendered in bitmap-only format (bitmap digits or bitmap font strategy)**
7. THE System SHALL display water level status using water_drop_bitmap
8. THE System SHALL display active features with corresponding bitmaps indicated as active in bitmap-only system
9. WHEN in Fault state, THE System SHALL display fault indicators using error bitmaps: low_water_level_error_bitmap, high_temperature_error_bitmap, sensor_error_bitmap
10. **WHEN multiple faults are active, THE System SHALL display each fault with its specific error bitmap during browsing (see Requirement 11 for browsing behavior)**
11. **THE System SHALL display fault count on main screen when faults are active**
12. THE System SHALL update OLED_Display using non-blocking operations
13. THE System SHALL define explicit UI states corresponding to controller states: Power-Up UI, Initialization/Safety UI, Ready UI, Main Menu UI, Circulation UI, Settings And Error UI, Warning UI, Fault UI, Fault_Inspection UI
14. THE System SHALL store all bitmap data in PROGMEM with centralized bitmap definitions in `include/*` headers
15. **THE System SHALL render all text, labels, and status words using bitmap glyphs (no native font/text rendering functions allowed)**
16. **THE System SHALL implement Power-Up UI: water_drop_bitmap scaled by 2, horizontally centered, top offset y=-10, visible for at least 3 seconds, no text**
17. **THE System SHALL implement Initialization/Safety UI: settings_bitmap scaled by 2, horizontally centered, top offset y=-10, with bitmap-glyph text for each initialization and safety check**
18. **THE System SHALL implement Ready UI: two-column layout with thermometer_bitmap (left, scaled to fit) and numeric temperature with degree symbol (right, bitmap glyphs)**
19. **THE System SHALL implement Main Menu UI: one item visible at a time (circulation_bitmap or settings_bitmap), scaled by 2, with bottom-centered bitmap-glyph labels, rotary left/right scrolls, button confirms selection**
20. **THE System SHALL implement Circulation UI: one item visible at a time (circulation, massage, jet, heater, ozone, light, speaker, thermometer), rotary left/right scrolls, button toggles selected item**
21. **THE System SHALL implement Settings And Error UI: same interaction style and layout constraints as Circulation UI**

**Design Notes to be Finalized in Design Phase:**
- Display update rate (e.g., 10 Hz maximum)
- Bitmap highlighting/indication method for active vs inactive items
- Exact positioning coordinates for centered layouts

### Requirement 10: Rotary Encoder Input Handling

**User Story:** As a user, I want intuitive rotary encoder control with exact pin mapping and context-sensitive actions including multi-fault browsing, so that I can easily interact with the system.

#### Acceptance Criteria

1. **THE System SHALL connect Rotary_Encoder with exact pin mapping: CLK on pin D6 (GPIO12), DT on pin A0 (ADC0/GPIO17), SW on pin D3 (GPIO0)**
2. THE System SHALL handle Rotary_Encoder rotation using interrupt-driven or polling input appropriate for pin capabilities
3. **THE System SHALL validate that pin A0 (ADC0/GPIO17) can function correctly as rotary encoder DT input**
4. **IF pin A0 cannot behave correctly as encoder DT input, THEN the pin mapping SHALL be revised to use a valid digital-capable GPIO pin before implementation proceeds**
5. **THE System SHALL ensure pin D3 (GPIO0) switch circuit does not force programming mode during boot (boot-strap safety requirement)**
6. WHEN Rotary_Encoder is rotated clockwise, THE System SHALL increment menu position or value
7. WHEN Rotary_Encoder is rotated counter-clockwise, THE System SHALL decrement menu position or value
8. **WHEN Rotary_Encoder button is pressed, THE System SHALL perform context-sensitive action: select/deselect in menu, on/off for features, back/ok in dialogs, acknowledge in fault state**
9. **WHILE in Fault_Inspection state with multiple active faults, THE System SHALL allow Rotary_Encoder left/right rotation to scroll through faults**
10. THE System SHALL debounce Rotary_Encoder button presses to prevent spurious inputs
11. THE System SHALL provide visual feedback for all Rotary_Encoder interactions
12. THE System SHALL store encoder pin assignments and debounce timing as centralized constants in `include/*` headers

**Design Notes to be Finalized in Design Phase:**
- Specific debounce timing (e.g., 50 milliseconds)
- Visual feedback timing (e.g., within 100 milliseconds)
- Button hold duration for special actions (e.g., 1 second hold)
- Interrupt vs polling implementation based on pin capabilities

### Requirement 11: Fault Detection, Persistence, and Inspection

**User Story:** As a system operator, I want comprehensive fault detection with boot fault persistence, multi-fault browsing, and distinct auto-clear vs manual acknowledgment behavior, so that I can diagnose and resolve issues.

#### Acceptance Criteria

1. THE System SHALL detect fault conditions: low water level, high temperature, Thermal_Runaway, sensor failure, I2C communication failure, PCF8574 failure
2. **WHEN the Controller boots, IF ANY precondition is not satisfied OR ANY safety condition fails, THEN THE System SHALL enter Fault_State and persist fault until ALL conditions are resolved (boot fault persistence)**
3. **THE System SHALL implement boot fault persistence: on boot, if preconditions/safety not satisfied, remain in Fault_State until resolved**
4. **THE System MAY implement cross-power-cycle fault history storage as a design decision (not required by specification)**
5. **THE System SHALL NOT progress from Self_Check to Ready_State unless ALL preconditions AND safety conditions are satisfied**
6. WHEN a fault is detected during operation, THE System SHALL enter Fault_State
7. WHEN entering Fault_State, THE System SHALL execute Safe_Shutdown of all loads
8. THE System SHALL store active faults in a fault buffer with Fault_Code identifiers
9. **WHEN the System enters Fault_State with multiple active faults, THE System SHALL automatically enable fault inspection browsing mode**
10. **WHEN multiple faults are active, THE System SHALL allow user to scroll left/right through faults using Rotary_Encoder rotation from Fault_Inspection state**
11. **THE System SHALL display each fault with its specific error bitmap during browsing**
12. THE System SHALL allow user to enter Fault_Inspection state to review active faults
13. **WHILE in Fault_Inspection state, THE System SHALL display fault details using corresponding error bitmaps: low_water_level_error_bitmap, high_temperature_error_bitmap, sensor_error_bitmap**
14. THE System SHALL clear faults only when underlying conditions are resolved
15. **THE System SHALL display fault count on main screen when faults are active**
16. **THE System SHALL distinguish between auto-clear faults (cleared when condition resolves) and manual acknowledgment faults (require user action)**
17. **WHEN Thermal_Runaway is detected, THE System SHALL require manual acknowledgment beyond automatic recovery before heater can operate again**
18. THE System SHALL store fault codes and fault handling logic as centralized constants in `include/*` headers

**Design Notes to be Finalized in Design Phase:**
- Fault timestamp storage mechanism
- Persistent storage mechanism (EEPROM, flash) for cross-power-cycle fault history (optional design decision)
- Fault acknowledgment UI flow
- Fault entry timing (e.g., within 500 milliseconds)

### Requirement 12: Safe Shutdown Procedure

**User Story:** As a system operator, I want a defined safe shutdown procedure with heater-first priority, so that all loads are deactivated in the correct sequence during faults or user-initiated shutdown.

#### Acceptance Criteria

1. **WHEN Safe_Shutdown is initiated, THE System SHALL deactivate all loads safely and non-blockingly**
2. **THE System SHALL prioritize Water_Heater deactivation (highest priority)**
3. **THE System SHALL deactivate Circulation_Pump after all dependent loads**
4. **THE System SHALL ensure all relays end in HIGH (off) state**
5. WHEN Safe_Shutdown is initiated, THE System SHALL deactivate Ozone_Generator
6. WHEN Safe_Shutdown is initiated, THE System SHALL deactivate Jet_Pump
7. WHEN Safe_Shutdown is initiated, THE System SHALL deactivate Massage_Pump
8. WHEN Safe_Shutdown is initiated, THE System SHALL deactivate Speaker_Relay and Light_System
9. WHEN Safe_Shutdown completes, THE System SHALL verify all relays are in HIGH (off) state
10. THE System SHALL implement Safe_Shutdown using non-blocking operations
11. THE System SHALL ensure Safe_Shutdown overrides all user actions
12. THE System SHALL distinguish between fault-driven shutdown and user-initiated shutdown
13. IF any relay fails to deactivate, THEN THE System SHALL log the failure and set a relay control fault
14. THE System SHALL store shutdown sequence timing as centralized constants in `include/*` headers

**Design Notes to be Finalized in Design Phase:**
- Specific shutdown timing sequence (100ms, 200ms, etc.) is a design decision
- Buzzer activation during shutdown
- Relay verification method

### Requirement 13: Non-Blocking Architecture and Event Loop

**User Story:** As a developer, I want a non-blocking event-driven architecture throughout the ENTIRE project, so that the system remains responsive and can handle multiple concurrent operations.

#### Acceptance Criteria

1. THE System SHALL implement an Event_Loop that processes events without blocking
2. THE System SHALL NOT use delay() function calls in application logic
3. THE System SHALL use millis() based timing for all time-dependent operations
4. THE System SHALL implement non-blocking sensor reading (Temperature_Sensor, Water_Level_Sensor) with state tracking
5. THE System SHALL implement non-blocking I2C_Bus transactions (OLED_Display, PCF8574) with timeout handling
6. THE System SHALL implement non-blocking relay operations
7. THE System SHALL implement non-blocking buzzer operations
8. THE System SHALL implement non-blocking fault processing
9. THE System SHALL implement non-blocking display rendering
10. **THE System SHALL process Rotary_Encoder inputs using interrupt service routines or non-blocking polling appropriate for pin capabilities (considering DT on A0 may not support same interrupt approach as normal digital pins)**
11. THE System SHALL maintain separate timing contexts for: sensor reads, display updates, relay operations, user input debouncing
12. THE System SHALL ensure Event_Loop remains responsive even with multiple active features or faults
13. THE System SHALL store all timing context values as centralized constants in `include/*` headers

**Design Notes to be Finalized in Design Phase:**
- Event loop iteration time target (e.g., less than 50 milliseconds)
- Priority ordering of operations within event loop

### Requirement 14: Memory Optimization and Resource Management

**User Story:** As a developer, I want efficient memory usage with centralized configuration optimized for ESP8266 constraints, so that the system runs reliably within available resources.

#### Acceptance Criteria

1. THE System SHALL store all constant data (bitmaps, strings, configuration) in PROGMEM
2. **THE System SHALL define all configuration constants in centralized headers under `include/*` directory (NO hardcoded constants anywhere)**
3. **THE System SHALL NOT use hardcoded constants anywhere in the codebase (absolute rule)**
4. THE System SHALL import centralized configuration from one source of truth in all implementation files
5. THE System SHALL use static allocation for all data structures to avoid heap fragmentation
6. THE System SHALL minimize dynamic memory allocation to essential operations only
7. THE System SHALL implement string operations using F() macro for flash string storage
8. **WHERE DENABLE_SERIAL_DEBUG is defined (exact name with D prefix), THE System SHALL include debug logging code**
9. WHERE DENABLE_SERIAL_DEBUG is not defined, THE System SHALL exclude debug logging code from compilation
10. **THE System SHALL configure build flags in `platformio.ini` including DENABLE_SERIAL_DEBUG**
11. **THE System SHALL NOT use `flto` (Link Time Optimization) in build configuration (absolute rule)**
12. **THE System SHALL include `build_src_filter` in `platformio.ini` configuration**
13. **THE System SHALL include required libraries in `platformio.ini`: rotary encoder support, `ezButton`, `Adafruit SH110X@^2.1.14` (exact version)**
14. THE System SHALL use bit fields for boolean state flags to minimize memory usage
15. THE System SHALL reuse display buffers for rendering operations

**Design Notes to be Finalized in Design Phase:**
- Heap monitoring threshold (e.g., 8 KB warning)
- Specific memory allocation strategies

### Requirement 15: Timing and Deterministic Behavior

**User Story:** As a system operator, I want deterministic timing for all operations, so that the system behaves predictably and safely.

#### Acceptance Criteria

1. THE System SHALL implement deterministic timing for all time-dependent operations
2. THE System SHALL read Temperature_Sensor with consistent period using non-blocking timing
3. THE System SHALL read Water_Level_Sensor with consistent period using non-blocking timing
4. THE System SHALL update OLED_Display with consistent period using non-blocking timing
5. THE System SHALL enforce feature activation sequencing delays using non-blocking timing
6. THE System SHALL debounce Rotary_Encoder button using non-blocking timing
7. THE System SHALL execute Safe_Shutdown with deterministic timing behavior
8. THE System SHALL detect I2C_Bus communication timeouts using deterministic timing
9. **THE System SHALL define all timing values as centralized constants in `include/*` headers (NO hardcoded timing values anywhere)**
10. THE System SHALL ensure bitmap scaling (factor of 2) and UI responsiveness are linked to display timing

**Design Notes to be Finalized in Design Phase:**
- Specific timing values (e.g., 2000ms for temperature, 500ms for water level, 100ms for display)
- Specific tolerance values (e.g., ±100ms, ±50ms, ±20ms)
- Specific delay values (e.g., 2000ms for massage, 3000ms for jet, 1000ms for heater)
- Specific debounce timing (e.g., 50ms)
- Specific shutdown timing (e.g., 500ms)
- Specific timeout values (e.g., 1000ms for I2C)

### Requirement 16: Development Process and Phase Management

**User Story:** As a developer, I want a structured phase-based development process with mandatory Task Execution Protocol, so that the system is built incrementally with validation at each step.

#### Acceptance Criteria

1. THE System SHALL be developed in sequential phases with defined deliverables
2. **WHEN starting each phase, THE System SHALL follow the mandatory Task Execution Protocol Step 1 (Pre-Git): execute `git status`, `git branch -vv`, `git fetch origin`, verify clean working state, create feature branch**
3. **WHEN starting each phase, THE System SHALL follow the mandatory Task Execution Protocol Step 2 (Deep codebase analysis): analyze all files in `include/*`, `lib/*`, `src/main.cpp`, and `docs/*` directories**
4. **WHEN starting each phase, THE System SHALL follow the mandatory Task Execution Protocol Step 3 (Deep previous-phase analysis): analyze previous phase (N-1) documentation and implementation**
5. **WHEN executing each phase, THE System SHALL follow the mandatory Task Execution Protocol Step 4 (Phase execution): integrate all phase work with `src/main.cpp`, NO magic numbers, all constants from `include/*`**
6. **WHEN completing each phase, THE System SHALL follow the mandatory Task Execution Protocol Step 5 (User review gate): require manual hardware testing approval with no automated tests before proceeding**
7. **WHEN completing each phase, THE System SHALL follow the mandatory Task Execution Protocol Step 6 (Post-Git): create documentation in `docs/phase-<number>-*` format, execute `git commit`, `git push`, merge branch, cleanup branch**
8. **THE System SHALL require AI agent to ask user to run PlatformIO commands (build, upload, monitor) rather than executing them directly**
9. THE System SHALL ensure each phase is meaningful and incrementally integrated with `src/main.cpp`
10. **THE System SHALL use manual hardware review at Step 5 with no automated test framework**

**Design Notes to be Finalized in Design Phase:**
- Specific phase breakdown (e.g., hardware initialization, sensor integration, relay control, state machine, UI implementation, safety systems, feature control, testing)
- Git branch naming strategy
- Commit message format

### Requirement 17: I2C Bus Management and Conflict Prevention

**User Story:** As a developer, I want safe I2C bus management preventing conflicts between devices with priority handling, so that OLED display and relay control operate reliably.

#### Acceptance Criteria

1. THE System SHALL initialize I2C_Bus before accessing any I2C devices
2. THE System SHALL serialize all I2C_Bus transactions to prevent concurrent access
3. WHEN accessing OLED_Display at address 0x3C, THE System SHALL acquire I2C_Bus lock before transaction
4. WHEN accessing PCF8574 at address 0x20, THE System SHALL acquire I2C_Bus lock before transaction
5. THE System SHALL release I2C_Bus lock immediately after completing each transaction
6. THE System SHALL implement I2C_Bus transaction timeout handling
7. THE System SHALL prioritize safety-critical relay updates over OLED_Display refresh operations to prevent starvation
8. THE System SHALL implement transaction ordering to avoid starvation of critical operations
9. IF I2C_Bus transaction times out, THEN THE System SHALL release the lock and log an error
10. THE System SHALL verify I2C device addresses (OLED_Display: 0x3C, PCF8574: 0x20) during initialization as centralized constants from `include/*`
11. IF I2C address conflict is detected, THEN THE System SHALL enter Fault_State and persist fault until conflict is resolved

**Design Notes to be Finalized in Design Phase:**
- Specific timeout value (e.g., 1000 milliseconds)
- I2C lock implementation mechanism
- Transaction priority algorithm

### Requirement 18: Configuration Management and Build System

**User Story:** As a developer, I want centralized configuration with proper PlatformIO build system setup, so that the system is maintainable and builds correctly.

#### Acceptance Criteria

1. **THE System SHALL define all hardware pin assignments in a centralized header file in `include/*` directory**
2. **THE System SHALL define all timing constants in a centralized header file in `include/*` directory**
3. **THE System SHALL define all I2C addresses in a centralized header file in `include/*` directory**
4. **THE System SHALL define all safety thresholds in a centralized header file in `include/*` directory**
5. **THE System SHALL NOT use hardcoded constants anywhere in the codebase (all constants must be imported from centralized `include/*` headers) - absolute rule**
6. THE System SHALL use PlatformIO as the exclusive build system
7. **THE System SHALL target ESP8266 platform in PlatformIO configuration**
8. **THE System SHALL configure `build_flags` in `platformio.ini` including DENABLE_SERIAL_DEBUG**
9. **THE System SHALL include `build_src_filter` in `platformio.ini` (required)**
10. **THE System SHALL NOT use `flto` (Link Time Optimization) in build flags (absolute rule)**
11. **THE System SHALL include required libraries in `platformio.ini`: rotary encoder support, `ezButton`, `Adafruit SH110X@^2.1.14` (exact version)**
12. **WHERE DENABLE_SERIAL_DEBUG is defined (exact name with D prefix), THE System SHALL enable serial debug output at 115200 baud**
13. **THE System SHALL organize code into: `include/*` (headers with all constants), `lib/*` (libraries), `src/*` (implementation with `src/main.cpp` integration), `docs/*` (phase documentation in `docs/phase-<number>-*` format)**
14. **THE System SHALL require AI agent to ask user to manually execute PlatformIO commands (build, upload, monitor) - agent cannot execute directly**

**Design Notes to be Finalized in Design Phase:**
- Specific header file organization within `include/*`
- Library version pinning strategy
- Specific board variant (e.g., esp12e, nodemcuv2) to be selected based on actual hardware during implementation

### Requirement 19: Error Recovery and Resilience

**User Story:** As a system operator, I want the system to recover from transient errors when possible, so that temporary issues don't require manual intervention.

#### Acceptance Criteria

1. WHEN Temperature_Sensor communication is restored after failure, THE System SHALL resume normal operation after consecutive valid readings
2. WHEN Water_Level_Sensor indicates sufficient water after low water fault, THE System SHALL clear the fault after a stabilization period of stable readings
3. WHEN temperature drops below warning threshold after high temperature warning, THE System SHALL clear the warning and allow Water_Heater reactivation
4. WHEN I2C_Bus communication is restored after failure, THE System SHALL attempt to resume normal operation
5. **IF Thermal_Runaway fault occurs, THEN THE System SHALL require more severe handling than simple overtemperature, including manual acknowledgment before heater can operate again**
6. THE System SHALL implement fault persistence: faults persist until conditions are resolved or explicit recovery logic executes
7. **THE System SHALL distinguish Thermal_Runaway (more severe, requires manual acknowledgment) from high temperature warning (automatic recovery when temperature drops)**

**Design Notes to be Finalized in Design Phase:**
- Specific consecutive valid reading count (e.g., 3 readings)
- Specific stabilization period (e.g., 5 seconds)
- Exponential backoff strategy (e.g., 100ms, 200ms, 400ms, 800ms)
- Session fault history tracking (e.g., 3 faults within 5 minutes)
- Rapid fault cycling prevention heuristics

### Requirement 20: Buzzer Feedback and Audible Alerts

**User Story:** As a user, I want audible feedback for important events and alerts, so that I'm aware of system state changes without watching the display.

#### Acceptance Criteria

1. **THE System SHALL connect Buzzer to pin D8 (GPIO15) with boot-safe default OFF state (boot-strap sensitive pin)**
2. **THE System SHALL implement Buzzer as on/off notification only (no tone generation)**
3. THE System SHALL implement non-blocking Buzzer control using timing state machine
4. WHEN Circulation_Pump is activated, THE System SHALL activate Buzzer for a brief confirmation
5. WHEN any fault is detected, THE System SHALL activate Buzzer for an alert duration
6. WHEN user confirms a selection via Rotary_Encoder button, THE System SHALL activate Buzzer for a brief feedback
7. WHEN temperature exceeds warning threshold, THE System SHALL activate Buzzer for a warning duration
8. WHEN Safe_Shutdown is initiated, THE System SHALL activate Buzzer for an alert duration
9. THE System SHALL prevent Buzzer overlap by managing alert requests appropriately
10. THE System SHALL ensure fault alerts are prioritized appropriately
11. THE System SHALL store buzzer timing values as centralized constants in `include/*` headers

**Design Notes to be Finalized in Design Phase:**
- Specific buzzer durations (e.g., 50ms, 200ms, 500ms, 1000ms)
- User preference for disabling non-critical alerts
- Buzzer queuing vs overlap prevention strategy
- Buzzer pin polarity (active-high or active-low) to be determined by hardware circuit design

## Requirements Summary

This requirements document defines 20 major requirements covering:

- **Hardware Integration** (Requirements 1, 17, 18): Complete pin mapping with boot-safety, I2C management, centralized configuration
- **Sensor Systems** (Requirements 2, 3): Temperature and water level monitoring with fault detection and boot fault persistence
- **Load Control** (Requirements 4, 5, 6, 7): Relay management with explicit 8-channel mapping, pump control, heater safety with absolute circulation dependency, feature sequencing
- **System Architecture** (Requirements 8, 13, 15): State machine with boot fault persistence, non-blocking design, deterministic timing
- **User Interface** (Requirements 9, 10, 20): Bitmap-only display with exact names and scale-by-2 rule, multi-fault browsing, rotary encoder with exact pin mapping, audible feedback
- **Safety Systems** (Requirements 11, 12, 19): Fault detection with boot persistence and multi-fault browsing, safe shutdown with heater-first priority, error recovery with thermal runaway manual acknowledgment
- **Development Process** (Requirements 14, 16, 18): Memory optimization with NO hardcoded constants, mandatory 6-step Task Execution Protocol, build system with exact library versions

**Key Differentiators:**
- **Publicly Observable Behavior Only**: Original specification based on externally visible UI/behavior without copying proprietary code
- **Boot Fault Persistence**: System remains in fault state from boot until ALL preconditions and safety conditions are satisfied
- **Thermal Runaway**: Defined as unexpected temperature CHANGE or DELTA (not just absolute threshold), requires manual acknowledgment
- **Bitmap-Only UI**: Exact bitmap names with `_bitmap` suffix, absolute scale-by-2 rule, bitmap-font for numeric display
- **NO Hardcoded Constants**: All constants centralized in `include/*` headers (absolute rule)
- **Multi-Fault Browsing**: Left/right encoder rotation to scroll through multiple active faults with specific error bitmaps
- **Exact Hardware Specification**: Complete pin mapping with boot-strap safety notes, exact library versions, NO `flto`, `build_src_filter` required

All requirements follow EARS patterns and comply with INCOSE quality rules for clarity, testability, and completeness. The requirements are solution-free, focusing on what the system must do rather than how it should be implemented, with implementation details clearly marked as "Design Notes to be Finalized in Design Phase."
