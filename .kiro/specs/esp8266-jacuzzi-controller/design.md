# Design Document: ESP8266 Jacuzzi Controller

## Overview

The ESP8266 Jacuzzi Controller is an embedded firmware system that provides automated control and monitoring for jacuzzi/bathtub operations. The system manages multiple loads (circulation pump, massage pump, jet pump, water heater, ozone generator, speaker relay, and lighting) through relay control while enforcing strict safety rules based on water level and temperature monitoring.

### Design Philosophy

This design implements **publicly observable behavior only** - replicating the externally visible UI and system responses without copying any proprietary firmware or protected assets. The implementation is original, based solely on observable controller behavior.

### Key Design Principles

1. **Safety First**: All operations prioritize safety through precondition checking, fault persistence, and safe shutdown sequences
2. **Non-Blocking Architecture**: Event-driven design using millis()-based timing throughout the entire system
3. **Centralized Configuration**: All constants defined in `include/*` headers with NO hardcoded values anywhere
4. **Bitmap-Only UI**: Complete visual interface using pre-defined bitmaps with exact naming and scale-by-2 rule
5. **State Machine Driven**: Formal state management with explicit states, transitions, and guard conditions
6. **Boot Fault Persistence**: System remains in fault state from boot until ALL preconditions and safety conditions are satisfied
7. **Hardware Integration**: Direct integration with `src/main.cpp` following the 6-step Task Execution Protocol

### System Constraints

- **Platform**: ESP8266 (esp12e) with 80MHz CPU, 80KB RAM, 4MB flash
- **Real-Time Requirements**: Non-blocking event loop with deterministic timing
- **Memory Optimization**: PROGMEM for constants, static allocation, minimal heap usage
- **Build System**: PlatformIO with NO `flto`, includes `build_src_filter`
- **Testing**: Manual hardware validation only (no automated test framework)
- **Development**: Phase-based with mandatory 6-step Task Execution Protocol


## Architecture

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Main Event Loop                          │
│                        (src/main.cpp)                            │
└────────────┬────────────────────────────────────────────────────┘
             │
             ├──> State Machine Manager
             │    ├─> Boot State
             │    ├─> Self_Check State
             │    ├─> Ready State
             │    ├─> Active_Circulation State
             │    ├─> Feature_Enabled_Bath State
             │    ├─> Warning State
             │    ├─> Fault State
             │    ├─> Fault_Inspection State
             │    └─> Shutdown State
             │
             ├──> Hardware Abstraction Layer
             │    ├─> I2C Bus Manager (OLED + PCF8574)
             │    ├─> Sensor Manager (DS18B20 + Water Level)
             │    ├─> Relay Controller (8-channel via PCF8574)
             │    ├─> Input Handler (Rotary Encoder)
             │    └─> Buzzer Controller
             │
             ├──> Safety System
             │    ├─> Precondition Checker
             │    ├─> Fault Detector
             │    ├─> Thermal Runaway Monitor
             │    └─> Safe Shutdown Coordinator
             │
             └──> UI Manager
                  ├─> Display Renderer (Bitmap-only)
                  ├─> UI State Manager
                  └─> Multi-Fault Browser
```

### Layered Architecture

**Layer 1: Hardware Abstraction**
- Direct hardware interface (GPIO, I2C, OneWire)
- Non-blocking I/O operations
- Boot-safe pin initialization
- Centralized pin mapping from `include/HardwareConfig.h`

**Layer 2: Device Drivers**
- Temperature sensor driver (DS18B20)
- Water level sensor driver (XKC-Y25-V)
- Relay controller (PCF8574)
- Display driver (SH110X OLED)
- Rotary encoder driver
- Buzzer driver

**Layer 3: Safety and Control Logic**
- State machine implementation
- Safety interlock enforcement
- Fault detection and persistence
- Feature sequencing logic
- Thermal runaway detection

**Layer 4: User Interface**
- Bitmap rendering engine
- UI state management
- Multi-fault browsing
- User input processing

**Layer 5: Application Integration**
- Main event loop coordination
- Timing management
- Debug logging (conditional)

### Module Organization

```
include/
├── HardwareConfig.h      # Pin assignments, I2C addresses
├── SafetyConfig.h        # Temperature thresholds, safety limits
├── TimingConfig.h        # All timing constants
├── Bitmaps.h             # All bitmap definitions (PROGMEM)
├── StateDefinitions.h    # State machine states and transitions
├── FaultCodes.h          # Fault code enumerations
├── I2CBusManager.h       # I2C bus management interface
├── SensorManager.h       # Sensor reading and validation interface
├── RelayController.h     # Relay control interface
├── StateMachine.h        # State machine interface
├── SafetySystem.h        # Safety system interface
├── UIManager.h           # UI management interface
└── InputHandler.h        # Input handling interface

lib/
├── I2CBusManager/
│   └── I2CBusManager.cpp
├── SensorManager/
│   └── SensorManager.cpp
├── RelayController/
│   └── RelayController.cpp
├── StateMachine/
│   └── StateMachine.cpp
├── SafetySystem/
│   └── SafetySystem.cpp
├── UIManager/
│   └── UIManager.cpp
└── InputHandler/
    └── InputHandler.cpp

src/
└── main.cpp              # Event loop integration point
```


## Components and Interfaces

### 1. I2C Bus Manager
Serializes I2C transactions, prevents bus conflicts, prioritizes safety-critical operations.

**Initialization** (Requirement 17.1):
- Initialize I2C bus BEFORE accessing any I2C devices
- Called first in hardware initialization sequence
- Configure I2C speed: 100kHz for reliability (standard mode)

**Transaction Serialization** (Requirements 17.2, 17.3, 17.4, 17.5):
- **Serialize ALL I2C bus transactions** to prevent concurrent access
- Lock/release mechanism for bus access
- **WHEN accessing OLED (0x3C)**: Acquire lock before transaction
- **WHEN accessing PCF8574 (0x20)**: Acquire lock before transaction
- **Release lock immediately** after completing transaction

**Timeout Handling** (Requirements 17.6, 17.9):
- Implement timeout for all I2C transactions
- Timeout value from TimingConfig.h (e.g., 1000ms)
- **IF transaction times out**: Release lock, log error, may enter Fault state

**Priority Management** (Requirements 17.7, 17.8):
- **Safety-critical relay updates prioritized** over OLED display refresh
- Prevents display operations from starving critical safety operations
- Transaction ordering ensures critical operations not delayed

**Address Verification** (Requirements 17.10, 17.11):
- Verify I2C device addresses during initialization
- OLED Display: 0x3C (from HardwareConfig.h)
- PCF8574: 0x20 (from HardwareConfig.h)
- **IF address conflict detected**: Enter Fault state, persist until resolved

**Shared Bus Devices**:
- OLED Display (SH110X) at 0x3C - High bandwidth (display buffer updates)
- PCF8574 GPIO Expander at 0x20 - Low bandwidth (single byte writes)

**Implementation Pattern**:
```cpp
bool acquireLock(uint8_t address, unsigned long timeout);
void releaseLock();
bool isLocked();
uint8_t getLockedAddress();
```

### 2. Sensor Manager
Non-blocking sensor reading with validation and fault detection for DS18B20 temperature sensor and XKC-Y25-V water level sensor.

**Temperature Sensor (DS18B20)** (Requirements 2.1-2.6, 2.12, 2.13):

**Pin Assignment**:
- DS18B20 on pin D5 (GPIO14)
- OneWire protocol

**Reading Strategy** (Requirement 2.1):
- Non-blocking operation using state machine
- Read every 2 seconds (from TimingConfig.h)
- States: IDLE → REQUEST → WAITING → PROCESS → IDLE
- Conversion time: ~750ms for DS18B20

**Validation** (Requirement 2.2):
- Validate reading within defined valid range (from SafetyConfig.h)
- Example range: -10°C to 60°C

**Fault Detection** (Requirements 2.3, 2.4, 2.5, 2.13):
- **IF invalid reading**: Increment sensor error counter
- **IF consecutive invalid readings exceed threshold**: Enter Fault state with `sensor_error_bitmap`
- **IF communication timeout**: Treat as sensor failure
- Threshold from SafetyConfig.h (e.g., 3 consecutive errors)

**Fault Recovery** (Requirement 2.5):
- **WHEN valid reading obtained after failure**: Reset sensor error counter
- Auto-clear fault after consecutive valid readings (from SafetyConfig.h)

**Temperature Display** (Requirement 2.6):
- Display current temperature on OLED using `thermometer_bitmap` and bitmap digits
- Bitmap-only numeric rendering (no text functions)

**Temperature Thresholds** (Requirements 2.7, 2.8, 2.10):
- **Warning threshold**: Temperature exceeds warning level, enters Warning state (from SafetyConfig.h, e.g., 42°C)
- **Critical threshold**: Temperature exceeds critical level, enters Fault state (from SafetyConfig.h, e.g., 45°C)
- **Thermal runaway**: Unexpected temperature CHANGE/DELTA, enters Fault state, requires manual acknowledgment

**Water Level Sensor (XKC-Y25-V)** (Requirements 3.1-3.10):

**Pin Assignment**:
- XKC-Y25-V on pin D7 (GPIO13)
- Digital input with active-low logic

**Active-Low Logic** (Requirements 3.2, 3.3):
- **LOW signal = Sufficient water level**
- **HIGH signal = Insufficient water level**

**Reading Strategy** (Requirement 3.1):
- Non-blocking polling
- Read period from TimingConfig.h (e.g., 500ms for fast safety response)

**Boot Fault Persistence** (Requirement 3.4):
- **IF insufficient water at boot**: Enter Fault state and persist until water level sufficient
- System does NOT progress to Ready until water level safe

**Safety Interlocks** (Requirements 3.5, 3.6):
- **IF insufficient water**: Prevent activation of Circulation, Massage, Jet, Heater
- **IF insufficient water WHILE pumps/heater active**: Execute Safe_Shutdown immediately

**Display** (Requirement 3.7):
- Display water level status using `water_drop_bitmap`

**Fault Detection** (Requirement 3.8):
- **IF insufficient water for extended duration**: Enter Fault state with `low_water_level_error_bitmap`
- Extended duration from TimingConfig.h (e.g., 10 seconds)

**Fault Recovery** (Requirement 3.9):
- **WHEN sufficient water restored**: Clear fault after stabilization period
- Stabilization period from TimingConfig.h (e.g., 5 seconds of stable readings)

**Centralized Configuration** (Requirements 2.14, 3.10):
- All temperature thresholds in SafetyConfig.h
- All timing values in TimingConfig.h
- All pin assignments in HardwareConfig.h

### 3. Relay Controller
Safe relay control with active-low logic, boot-safe defaults, explicit 8-channel mapping through PCF8574.

**I2C Control** (Requirement 4.1):
- All relays controlled through PCF8574 at I2C address 0x20
- Active-low logic: HIGH = OFF, LOW = ON
- Single byte write controls all 8 channels

**Active-Low Logic** (Requirements 4.2, 4.3):
- **Activate relay**: Write LOW to corresponding PCF8574 output pin (clear bit)
- **Deactivate relay**: Write HIGH to corresponding PCF8574 output pin (set bit)
- Bit manipulation: `relayState &= ~(1 << channel)` for ON, `relayState |= (1 << channel)` for OFF

**8-Channel Relay Mapping** (Requirements 4.4, 4.5, 4.6):
- **Channel 0 (Bit 0): Circulation Pump** - Foundation for all bath features
- **Channel 1 (Bit 1): Massage Pump** - Requires circulation active
- **Channel 2 (Bit 2): Jet Pump** - Requires circulation active
- **Channel 3 (Bit 3): Water Heater** - 3kW load, 5V/30A relay, requires circulation active
- **Channel 4 (Bit 4): Ozone Generator** - Requires circulation active
- **Channel 5 (Bit 5): Speaker Relay** - Requires circulation active
- **Channel 6 (Bit 6): Light System** - Requires circulation active
- **Channel 7 (Bit 7): Spare** - **Reserved, permanently OFF**, never activated

**Boot-Safe Defaults** (Requirements 1.3, 4.7):
- **WHEN Controller boots**: All relay outputs default to OFF state (HIGH signal)
- **MUST NOT energize any relay** before boot safety checks complete
- PCF8574 initialized with 0xFF (all bits HIGH = all relays OFF)

**Relay State Model** (Requirements 4.8, 4.9):
- Maintain 8-bit relay state in memory
- Synchronize with PCF8574 on every state change
- Bit field represents current relay states
- UI/hardware synchronization for display updates

**Fault Handling** (Requirement 4.10):
- **WHEN System enters Fault state**: Deactivate ALL relays
- Execute Safe_Shutdown sequence (heater first, circulation last)

**Non-Blocking Operations** (Requirement 4.11):
- All relay operations integrated into event loop
- No delay() calls for relay timing
- Use millis()-based timing for activation delays

**I2C Priority** (Requirement 4.12):
- **Safety-critical relay writes prioritized** over OLED display refresh
- Prevents starvation of safety operations by display updates
- I2C bus manager enforces priority

**Communication Verification** (Requirements 4.13, 4.14):
- **Verify PCF8574 communication** before each relay state change
- **IF PCF8574 communication fails**: Enter Fault state, attempt no further relay operations
- Fault persists until communication restored

**Centralized Configuration** (Requirement 4.15):
- Relay channel assignments in HardwareConfig.h
- I2C address (0x20) in HardwareConfig.h
- Activation delays in TimingConfig.h

### 4. State Machine
Formal state management with 9 states: Boot, Self_Check, Ready, Active_Circulation, Feature_Enabled_Bath, Warning, Fault, Fault_Inspection, Shutdown.

**State Definitions** (Requirement 8.1):

1. **Boot State** (Requirement 8.2):
   - Initial system state during hardware initialization
   - Entry: Power-on or reset
   - Actions: Initialize I2C bus, configure pins, set relays to OFF
   - Exit: When hardware initialization completes successfully
   - Transition: Boot → Self_Check (on successful initialization)
   - Transition: Boot → Fault (on initialization failure)

2. **Self_Check State** (Requirements 8.3, 8.4, 8.5, 8.6, 8.7):
   - System performs safety and sensor validation
   - **CRITICAL**: Implements boot fault persistence
   - Actions: Verify all preconditions and safety conditions
     - Check water level sufficient
     - Check temperature sensor operational and reading valid
     - Check temperature within safe range
     - Check I2C devices responding
     - Check all relays in OFF state
   - **IF ALL conditions satisfied**: Transition to Ready
   - **IF ANY condition fails**: Transition to Fault and persist until ALL resolved
   - **Boot Fault Persistence**: System does NOT progress to Ready until ALL conditions safe
   - Transition: Self_Check → Ready (all preconditions satisfied)
   - Transition: Self_Check → Fault (any precondition fails, persist until resolved)

3. **Ready State** (Requirements 8.4, 8.7):
   - System safe and accepting user commands
   - All preconditions and safety conditions satisfied
   - No active faults
   - Awaiting user input to activate circulation or features
   - Transition: Ready → Active_Circulation (user activates circulation pump)
   - Transition: Ready → Fault (safety condition fails)

4. **Active_Circulation State** (Requirements 5.4, 5.5, 8.8):
   - Circulation pump running (foundation for all features)
   - Enables Feature_Enabled_Bath operations
   - Continuously monitors water level and temperature
   - Transition: Active_Circulation → Feature_Enabled_Bath (user activates additional features)
   - Transition: Active_Circulation → Ready (user deactivates circulation)
   - Transition: Active_Circulation → Fault (safety condition fails)

5. **Feature_Enabled_Bath State** (Requirements 7.9, 8.9):
   - Circulation active PLUS additional features (massage, jet, heater, ozone, speaker, lights)
   - Multiple features can be active simultaneously
   - All features depend on circulation remaining active
   - Transition: Feature_Enabled_Bath → Active_Circulation (all features deactivated except circulation)
   - Transition: Feature_Enabled_Bath → Ready (circulation deactivated, all features stop)
   - Transition: Feature_Enabled_Bath → Warning (non-critical warning condition)
   - Transition: Feature_Enabled_Bath → Fault (safety condition fails)

6. **Warning State** (Requirements 2.7, 8.12):
   - Non-critical issue requiring attention
   - Example: High temperature warning (below critical threshold)
   - System continues operation with visible warning indication
   - Transition: Warning → Feature_Enabled_Bath (warning condition clears)
   - Transition: Warning → Fault (warning escalates to fault)

7. **Fault State** (Requirements 1.11, 8.10, 11.1, 11.6, 11.7):
   - One or more safety or operational faults active
   - All loads deactivated via Safe_Shutdown
   - Fault persists until underlying conditions resolved
   - Displays fault indicators using error bitmaps
   - Transition: Fault → Fault_Inspection (user requests fault review, multiple faults active)
   - Transition: Fault → Self_Check (all faults cleared, system re-validates)
   - Transition: Fault → Ready (all faults cleared and conditions safe)

8. **Fault_Inspection State** (Requirements 8.11, 11.9, 11.10, 11.12, 11.13):
   - User browsing and reviewing active faults
   - Rotary encoder left/right scrolls through fault list
   - Each fault displays with specific error bitmap
   - Fault index and total count shown
   - Transition: Fault_Inspection → Fault (user exits inspection mode)
   - Transition: Fault_Inspection → Self_Check (all faults cleared during inspection)

9. **Shutdown State**:
   - Controlled shutdown sequence in progress
   - Safe_Shutdown executing (heater first, then features, circulation last)
   - Non-blocking timing for shutdown sequence
   - Transition: Shutdown → Fault (shutdown completes, fault persists)
   - Transition: Shutdown → Ready (shutdown completes, no faults)

**Guard Conditions** (Requirement 8.13):
- All state transitions protected by guard condition functions
- Guards verify preconditions before allowing transition
- Examples:
  - Ready → Active_Circulation: Water level sufficient AND temperature sensor operational AND no faults
  - Active_Circulation → Feature_Enabled_Bath: Circulation active AND preconditions met
  - Any state → Fault: Safety condition violated

**Entry/Exit Actions** (Requirement 8.14):
- Each state defines entry actions (executed when entering state)
- Each state defines exit actions (executed when leaving state)
- Examples:
  - Fault entry: Execute Safe_Shutdown, display fault bitmap, log fault
  - Ready entry: Clear warnings, reset fault counters
  - Active_Circulation entry: Activate circulation relay, start monitoring

**State Transition Logging** (Requirement 8.15):
- WHERE DENABLE_SERIAL_DEBUG defined, log all state transitions
- Format: "State transition: [OLD_STATE] → [NEW_STATE] (reason)"

### 5. Safety System
Precondition checking, fault detection, thermal runaway monitoring, safe shutdown coordination.

**Precondition Checking** (Requirements 5.1, 5.2, 5.3, 6.1, 6.2, 6.3, 6.4, 7.1-7.7):

**Circulation Pump Preconditions** (Requirement 5.1, 5.2):
- Water level sensor indicates sufficient water
- Temperature sensor operational
- No active faults exist
- **IF ANY precondition fails**: Deny activation, display error

**Water Heater Preconditions** (Requirements 6.1, 6.2, 6.3, 6.4, 6.13, 6.14, 6.15, 6.16):
- **ABSOLUTE RULE**: Circulation pump MUST be actively running
- Water level sensor indicates sufficient water
- Temperature below warning threshold
- No active faults exist
- **IF circulation not active**: Deny activation, display error message
- **IF ANY precondition fails**: Deny activation

**Water Heater Auto-Start Behavior** (Requirements 6.13, 6.14, 6.15, 6.16):
- **Delayed Auto-Start**: WHEN circulation starts AND heater auto-start enabled, automatically activate heater after delay (from TimingConfig.h, e.g., 5 seconds) IF all preconditions remain satisfied
- **Default Set Temperature**: Use default set temperature (from SafetyConfig.h, e.g., 38°C) IF user has not explicitly set target temperature
- **User-Adjusted Temperature**: User-set target temperature overrides default set temperature
- **Auto-Deactivation**: Deactivate heater automatically WHEN current temperature reaches or exceeds target temperature (default or user-set)
- **Temperature Control Loop**: Continuously compare current temperature to target, activate/deactivate heater to maintain target

**Circulation Pump Delayed Start** (Requirements 5.12, 5.13):
- **Delayed Start**: WHEN user selects circulation in UI, activate pump after delay (from TimingConfig.h, e.g., 2 seconds) to allow user confirmation
- **State Distinction**: UI distinguishes between "circulation selected" (countdown) and "circulation started" (pump running)
- **Cancellation**: User can cancel during countdown by deselecting circulation

**Feature Preconditions** (Requirements 7.1-7.7):
- **ALL features INHIBITED until circulation active**: Massage, Jet, Ozone, Speaker, Lights
- Circulation pump must be running before any feature activation
- **IF circulation not active**: Deny feature activation, display error message

**Fault Detection** (Requirement 11.1):

**Fault Types**:
1. **Low Water Level** (Requirement 3.4, 3.8):
   - Water level sensor indicates insufficient water for extended duration
   - Fault code: FAULT_LOW_WATER_LEVEL
   - Display: `low_water_level_error_bitmap`
   - Auto-clear: Yes, after stabilization period of sufficient water

2. **High Temperature Warning** (Requirement 2.7):
   - Temperature exceeds warning threshold
   - Enters Warning state (non-fault)
   - Display: `high_temperature_error_bitmap`
   - Auto-clear: Yes, when temperature drops below threshold

3. **Critical Overtemperature** (Requirement 2.10):
   - Temperature exceeds critical fault threshold
   - Fault code: FAULT_HIGH_TEMPERATURE
   - Display: `high_temperature_error_bitmap`
   - Auto-clear: Yes, when temperature drops below threshold

4. **Thermal Runaway** (Requirements 2.8, 2.9, 2.11):
   - **Unexpected temperature CHANGE or abnormal rate-of-change**
   - NOT just absolute threshold
   - Fault code: FAULT_THERMAL_RUNAWAY
   - Display: `high_temperature_error_bitmap`
   - **Manual acknowledgment required** before heater can operate again
   - Auto-clear: No, requires user acknowledgment

5. **Temperature Sensor Failure** (Requirements 2.3, 2.4, 2.13):
   - Consecutive invalid readings beyond threshold
   - Communication timeout
   - Fault code: FAULT_TEMPERATURE_SENSOR
   - Display: `sensor_error_bitmap`
   - Auto-clear: Yes, after consecutive valid readings

6. **I2C Communication Failure** (Requirement 17.9):
   - I2C bus transaction timeout
   - Fault code: FAULT_I2C_FAILURE
   - Auto-clear: Yes, when communication restored

7. **PCF8574 Failure** (Requirement 4.14):
   - PCF8574 communication fails
   - Fault code: FAULT_PCF8574_FAILURE
   - Auto-clear: Yes, when communication restored

**Boot Fault Persistence** (Requirements 1.9, 1.10, 8.5, 8.6, 8.7, 11.2, 11.3):
- **CRITICAL**: On boot, if ANY precondition or safety condition fails, enter Fault state
- System remains in Fault state until ALL conditions resolved
- System does NOT progress to Ready state until ALL preconditions AND safety conditions satisfied
- This is **ABSOLUTE RULE** for safety-first operation

**Thermal Runaway Detection** (Requirements 2.8, 2.9, 2.11):
- Maintain temperature history buffer (recent readings)
- Calculate temperature delta over time window
- Calculate rate of change (°C/second)
- **IF delta exceeds threshold OR rate exceeds threshold**: Thermal runaway detected
- Immediate heater deactivation and Fault state entry
- **Manual acknowledgment required** before heater can operate again
- Distinguishes from normal high temperature (which auto-clears)

**Multi-Fault Management** (Requirements 11.8, 11.9, 11.10, 11.11):
- Fault bit field stores multiple simultaneous faults
- Each fault has unique Fault_Code bit
- When multiple faults active, enable Fault_Inspection browsing
- User can scroll through faults using encoder rotation
- Each fault displays with specific error bitmap

**Fault Clearing** (Requirements 11.14, 11.16, 11.17):
- **Auto-clear faults**: Clear when underlying condition resolves
  - Low water level (after stabilization)
  - High temperature warning (when temp drops)
  - Sensor communication (after valid readings)
  - I2C/PCF8574 communication (when restored)
- **Manual acknowledgment faults**: Require user action
  - Thermal runaway (requires explicit acknowledgment before heater can operate)

**Safe Shutdown** (Requirements 12.1-12.14):
- **Priority Order** (heater first, circulation last):
  1. Water Heater OFF (highest priority, immediate)
  2. Ozone Generator OFF (delay from TimingConfig.h)
  3. Jet Pump OFF (delay from TimingConfig.h)
  4. Massage Pump OFF (delay from TimingConfig.h)
  5. Speaker and Lights OFF (delay from TimingConfig.h)
  6. Circulation Pump OFF (last, delay from TimingConfig.h)
  7. Verify all relays HIGH (off state)
- Non-blocking timing using millis()
- Overrides all user actions
- Executed on any fault condition
- Timing values from TimingConfig.h (e.g., 100ms, 200ms delays)

### 6. UI Manager
Bitmap-only rendering with scale-by-2 rule, multi-fault browsing, temperature display using bitmap digits.

**Bitmap-Only UI Requirement** (Requirements 9.1, 9.2, 9.3):
- **ALL UI elements rendered exclusively using pre-defined bitmaps** stored in PROGMEM
- **NO text rendering functions** or character-based display output allowed
- All visible characters, digits, status messages, and fault information use bitmaps or bitmap glyphs
- This is an **ABSOLUTE RULE** - no exceptions

**Exact Bitmap Names** (Requirement 9.4):
All bitmaps MUST have exact names ending in `_bitmap` suffix:
1. `water_drop_bitmap` - Water level status indicator
2. `circulation_bitmap` - Circulation pump status
3. `massage_bitmap` - Massage pump status
4. `jet_bitmap` - Jet pump status
5. `heater_bitmap` - Water heater status
6. `speaker_bitmap` - Speaker relay status
7. `ozone_bitmap` - Ozone generator status
8. `light_bulb_bitmap` - Light system status
9. `settings_bitmap` - Settings/menu indicator
10. `thermometer_bitmap` - Temperature display indicator
11. `low_water_level_error_bitmap` - Low water level fault
12. `high_temperature_error_bitmap` - High temperature/thermal runaway fault
13. `sensor_error_bitmap` - Sensor failure fault

**Scale-by-2 Rule** (Requirement 9.5):
- **ABSOLUTE RULE**: All bitmaps MUST be scaled down by factor of 2 when displayed
- Bitmaps designed at 2x resolution, displayed at 1x
- Scaling algorithm samples every other pixel from source
- Ensures consistent visual appearance and predictable memory usage

**Temperature Display** (Requirement 9.6):
- Current temperature displayed using `thermometer_bitmap` indicator
- Numeric value rendered in bitmap-only format
- Use bitmap digits or bitmap font strategy (no text rendering)

**Water Level Display** (Requirement 9.7):
- Water level status displayed using `water_drop_bitmap`
- Bitmap indicates sufficient/insufficient water state

**Feature Status Display** (Requirement 9.8):
- Active features indicated with corresponding bitmaps
- Bitmap-only system for active/inactive indication (e.g., highlighted, dimmed, or positioned differently)

**Fault Display** (Requirement 9.9):
- Fault indicators using error bitmaps:
  - `low_water_level_error_bitmap` for water level faults
  - `high_temperature_error_bitmap` for temperature faults and thermal runaway
  - `sensor_error_bitmap` for sensor failures

**Multi-Fault Display** (Requirements 9.10, 9.11):
- When multiple faults active, display each fault with its specific error bitmap during browsing
- Display fault count on main screen when faults are active
- Fault index and total count shown (e.g., "Fault 2/3" using bitmap digits)

**Non-Blocking Display Updates** (Requirement 9.12):
- All OLED display updates use non-blocking operations
- Display refresh integrated into event loop
- Update rate from TimingConfig.h (e.g., 100ms = 10Hz maximum)

**UI States** (Requirements 9.13, 9.15-9.21):

**1. Power-Up UI** (Requirement 9.16):
- Display: `water_drop_bitmap` scaled down by factor of 2
- Position: Horizontally centered, top offset y = -10
- Duration: Visible for at least 3 seconds
- Text: None (bitmap-only)
- Purpose: Boot splash screen

**2. Initialization/Safety UI** (Requirement 9.17):
- Display: `settings_bitmap` scaled down by factor of 2
- Position: Horizontally centered, top offset y = -10
- Text: Bitmap-glyph text for each initialization and safety check
  - "Checking I2C..."
  - "Checking sensors..."
  - "Checking relays..."
  - "Checking water level..."
  - "Checking temperature..."
- Purpose: Show progress during Self_Check state
- Transition: To Ready UI when all checks pass, to Fault UI if any check fails

**3. Ready UI** (Requirement 9.18):
- Layout: Two-column
  - Left column: `thermometer_bitmap` scaled to fit
  - Right column: Numeric temperature with degree symbol (bitmap glyphs)
- Purpose: System ready, awaiting user input
- Interaction: Encoder button press transitions to Main Menu UI

**4. Main Menu UI** (Requirement 9.19):
- Display: One item visible at a time
  - `circulation_bitmap` with bottom-centered label "Start" (bitmap glyphs)
  - `settings_bitmap` with bottom-centered label "Settings" (bitmap glyphs)
- Scaling: All bitmaps scaled down by factor of 2
- Interaction:
  - Rotary left/right: Scroll between menu items
  - Button press: Confirm selection
    - "Start" → Circulation UI
    - "Settings" → Settings And Error UI
- Purpose: Top-level menu navigation

**5. Circulation UI** (Requirement 9.20):
- Display: One item visible at a time, scrollable list:
  1. `circulation_bitmap` - Circulation pump control
  2. `massage_bitmap` - Massage pump control
  3. `jet_bitmap` - Jet pump control
  4. `heater_bitmap` - Water heater control
  5. `ozone_bitmap` - Ozone generator control
  6. `light_bulb_bitmap` - Light system control
  7. `speaker_bitmap` - Speaker relay control
  8. `thermometer_bitmap` - Temperature display
- Scaling: All bitmaps scaled down by factor of 2
- Interaction:
  - Rotary left/right: Scroll through items
  - Button press: Toggle selected item on/off
  - Special behavior for circulation:
    - "Selected" state: User has selected circulation, delayed start countdown begins
    - "Started" state: Circulation pump actively running after delay
    - Stop circulation: Deactivates all dependent features, returns to Main Menu UI
- Delayed Start: Circulation activates after delay (from TimingConfig.h, e.g., 2 seconds) after selection
- Auto-Start Heater: If circulation active and heater auto-start enabled, heater activates after delay (from TimingConfig.h, e.g., 5 seconds) if preconditions satisfied
- Purpose: Feature control during active bath operation

**6. Settings And Error UI** (Requirement 9.21):
- Layout: Same interaction style and constraints as Circulation UI
- Display: One item visible at a time, scrollable list
- Items: Configuration options, error history, system information
- Interaction: Same as Circulation UI (rotary left/right scrolls, button confirms/toggles)
- Purpose: System configuration and error review

**7. Warning UI**:
- Display: Warning indicator with `high_temperature_error_bitmap` or appropriate warning bitmap
- Overlay: Warning displayed over current operational UI
- Interaction: System continues operation, user can acknowledge warning
- Purpose: Non-critical warnings that don't require shutdown

**8. Fault UI**:
- Display: Fault indicator with specific error bitmap
  - `low_water_level_error_bitmap` for water level faults
  - `high_temperature_error_bitmap` for temperature faults
  - `sensor_error_bitmap` for sensor failures
- Text: Bitmap-glyph fault description
- Fault count: Displayed if multiple faults active (e.g., "Fault 1/3" using bitmap glyphs)
- Interaction: Button press transitions to Fault_Inspection UI if multiple faults
- Purpose: Display active fault condition

**9. Fault_Inspection UI**:
- Display: Current fault with specific error bitmap
- Fault navigation: Rotary left/right scrolls through active faults
- Fault index: Current fault number and total count (bitmap glyphs)
- Each fault: Displays with its specific error bitmap and description
- Interaction: Button press returns to Fault UI or Ready UI (if faults cleared)
- Purpose: Browse and review multiple active faults

**Bitmap-Glyph Text Rendering** (Requirement 9.15):
- All text, labels, and status words rendered using bitmap glyphs
- No native font/text rendering functions allowed
- Bitmap glyph set includes: alphanumeric characters, degree symbol, common punctuation
- Stored in PROGMEM with other bitmaps
- Accessed via bitmap glyph lookup table

**Bitmap Storage** (Requirement 9.14):
- All bitmap data stored in PROGMEM
- Centralized bitmap definitions in `include/Bitmaps.h`
- Includes: icon bitmaps (13 defined) + bitmap glyph set for text rendering
- Accessed using pgm_read_byte() for memory efficiency

### 7. Input Handler
Rotary encoder processing with debouncing, context-sensitive actions, multi-fault browsing support.

**Pin Mapping** (from Requirement 10):
- CLK: D6 (GPIO12)
- DT: A0 (ADC0/GPIO17) ⚠️ **CRITICAL VALIDATION REQUIRED** - A0 is NOT a standard digital GPIO pin
- SW: D3 (GPIO0) ⚠️ Boot-strap sensitive - circuit must not force programming mode

**Pin A0 Validation Requirement** (Requirement 10.3, 10.4):
- A0 (ADC0/GPIO17) is the analog input pin, NOT a normal digital GPIO
- **MUST validate during Phase 7** that A0 can function correctly as encoder DT input
- **IF A0 cannot work correctly**, select alternative digital GPIO pin (e.g., D0/GPIO16, D4/GPIO2)
- Update hardware mapping before proceeding with implementation
- This is a **MANDATORY validation step** before Phase 7 completion

**Boot-Strap Safety for D3** (Requirement 10.5):
- D3 (GPIO0) is boot-strap sensitive - pulling LOW during boot forces programming mode
- Switch circuit **MUST** include proper pull-up resistor
- **MUST** test boot behavior with button pressed to verify safety
- Document circuit requirements clearly in Phase 7

**Context-Sensitive Actions** (Requirement 10.6, 10.7, 10.8):
- Rotation clockwise: Increment menu position or value
- Rotation counter-clockwise: Decrement menu position or value
- Button press actions (context-dependent):
  - In menu: select/deselect
  - In feature control: on/off toggle
  - In dialogs: back/ok
  - In Fault state: acknowledge
  - In Fault_Inspection state: (rotation) scroll through faults

**Multi-Fault Browsing** (Requirement 10.9):
- WHILE in Fault_Inspection state with multiple active faults
- Rotary encoder left/right rotation scrolls through fault list
- Each fault displays with its specific error bitmap
- Fault index and total count shown on display

**Debouncing** (Requirement 10.10, 10.12):
- Button debounce timing: from TimingConfig.h (e.g., 50ms)
- Rotation debounce: Implicit through state change detection
- Non-blocking timing using millis()
- All timing constants centralized in `include/*` headers

**Visual Feedback** (Requirement 10.11):
- All rotary encoder interactions provide visual feedback
- Feedback timing: from TimingConfig.h (e.g., within 100ms)
- Bitmap-based feedback indicators

**Implementation Strategy**:
- Use interrupt-driven rotation detection if pins support it (depends on A0 validation)
- Fall back to polling if A0 doesn't support interrupts
- Button hold detection for special actions (duration from TimingConfig.h)
- Clear rotation delta after reading to prevent double-processing

### 8. Buzzer Controller
Non-blocking on/off buzzer control on boot-safe pin D8 (GPIO15).

**Pin Assignment** (Requirement 20.1):
- Buzzer: D8 (GPIO15) ⚠️ **BOOT-STRAP SENSITIVE PIN**
- **MUST** default to LOW/OFF state at boot
- GPIO15 must be LOW during boot for normal operation (not programming mode)
- Initialize as OUTPUT LOW before any other operations in Phase 1

**Buzzer Type** (Requirement 20.2):
- On/off notification only (NO tone generation)
- Simple digital HIGH/LOW control
- Active buzzer module (generates its own tone when powered)

**Non-Blocking Control** (Requirement 20.3):
- State machine-based timing for all buzzer operations
- Uses millis() for duration tracking
- No delay() calls
- Integrated into main event loop

**Buzzer Events** (Requirements 20.4-20.8):
- Circulation pump activation: Brief confirmation beep
- Fault detection: Alert duration beep
- User confirmation (encoder button): Brief feedback beep
- Temperature warning: Warning duration beep
- Safe shutdown: Alert duration beep

**Buzzer Timing** (Requirement 20.11):
- All durations defined in TimingConfig.h
- Example values: 50ms (brief), 200ms (confirmation), 500ms (warning), 1000ms (alert)
- Centralized configuration in `include/*` headers

**Overlap Prevention** (Requirement 20.9):
- Manage alert requests to prevent buzzer overlap
- Queue or priority-based system for multiple simultaneous requests
- Current alert completes before next begins

**Priority Handling** (Requirement 20.10):
- Fault alerts prioritized over confirmation beeps
- Safety-critical alerts take precedence
- User feedback beeps lowest priority

**Implementation Notes**:
- Buzzer state: OFF (default), ON (active), TIMING (counting down)
- Track buzzer start time and duration
- Auto-off when duration expires
- Polarity determined by hardware circuit (active-high or active-low)


## Data Models

### Configuration Headers

All configuration constants are centralized in `include/*` headers:

- **HardwareConfig.h**: Pin assignments, I2C addresses, relay channel mapping
- **SafetyConfig.h**: Temperature thresholds, sensor fault limits, thermal runaway parameters
- **TimingConfig.h**: All timing constants for sensors, display, input, buzzer, features
- **StateDefinitions.h**: State enumerations, transition definitions
- **FaultCodes.h**: Fault code enumerations and descriptions
- **Bitmaps.h**: All bitmap data in PROGMEM with exact names ending in `_bitmap`, plus bitmap glyph set for text rendering
  - Icon bitmaps (13): water_drop_bitmap, circulation_bitmap, massage_bitmap, jet_bitmap, heater_bitmap, speaker_bitmap, ozone_bitmap, light_bulb_bitmap, settings_bitmap, thermometer_bitmap, low_water_level_error_bitmap, high_temperature_error_bitmap, sensor_error_bitmap
  - Bitmap glyph set: Alphanumeric characters (A-Z, a-z, 0-9), degree symbol (°), common punctuation (.,!?:-/), space
  - All bitmaps scaled down by factor of 2 when displayed
  - Glyph lookup table for text rendering without native font functions

### Hardware Pin Mapping (HardwareConfig.h)

**Complete Pin Assignment** (from Requirement 1.2):

**Boot-Strap Sensitive Pins** (CRITICAL - must be configured correctly for boot):
- **Buzzer: D8 (GPIO15)** ⚠️ MUST be LOW at boot (initialize FIRST)
- **Rotary Encoder SW: D3 (GPIO0)** ⚠️ Circuit must not force programming mode

**Sensor Pins**:
- **DS18B20 Temperature Sensor: D5 (GPIO14)** - OneWire protocol
- **Water Level Sensor XKC-Y25-V: D7 (GPIO13)** - Digital input, active-low logic

**Rotary Encoder Pins**:
- **Rotary Encoder CLK: D6 (GPIO12)** - Digital input
- **Rotary Encoder DT: A0 (ADC0/GPIO17)** ⚠️ **VALIDATION REQUIRED** - May not support digital input
- **Rotary Encoder SW: D3 (GPIO0)** ⚠️ Boot-strap sensitive

**I2C Bus** (shared by multiple devices):
- **SDA: D2 (GPIO4)** - I2C data line
- **SCL: D1 (GPIO5)** - I2C clock line

**I2C Device Addresses**:
- **PCF8574 I2C GPIO Expander: 0x20** - Controls 8-channel relay module
- **OLED Display (SH110X): 0x3C** - 128x64 pixel display

**Relay Channel Mapping** (through PCF8574, active-low logic):
- **Channel 0 (Bit 0): Circulation Pump** - Foundation for all features
- **Channel 1 (Bit 1): Massage Pump** - Requires circulation active
- **Channel 2 (Bit 2): Jet Pump** - Requires circulation active
- **Channel 3 (Bit 3): Water Heater** - 3kW load, 5V/30A relay, requires circulation active
- **Channel 4 (Bit 4): Ozone Generator** - Requires circulation active
- **Channel 5 (Bit 5): Speaker Relay** - Requires circulation active
- **Channel 6 (Bit 6): Light System** - Requires circulation active
- **Channel 7 (Bit 7): Spare** - Reserved, permanently OFF

**Relay Logic** (Requirement 4.1):
- Active-low: HIGH = OFF, LOW = ON
- Boot default: All channels HIGH (all relays OFF)
- Spare channel (7) never activated

**Pin Validation Notes**:
- A0 (ADC0/GPIO17) validation required in Phase 7 (Requirement 10.3, 10.4)
- If A0 cannot function as encoder DT, select alternative digital GPIO
- D3 (GPIO0) circuit must include proper pull-up to prevent boot issues
- D8 (GPIO15) must be initialized LOW before any other operations

### State Machine Data

```cpp
enum SystemState {
    STATE_BOOT,
    STATE_SELF_CHECK,
    STATE_READY,
    STATE_ACTIVE_CIRCULATION,
    STATE_FEATURE_ENABLED_BATH,
    STATE_WARNING,
    STATE_FAULT,
    STATE_FAULT_INSPECTION,
    STATE_SHUTDOWN
};
```

### Fault Management Data

```cpp
enum FaultCode {
    FAULT_NONE = 0,
    FAULT_LOW_WATER_LEVEL = (1 << 0),
    FAULT_HIGH_TEMPERATURE = (1 << 1),
    FAULT_THERMAL_RUNAWAY = (1 << 2),
    FAULT_TEMPERATURE_SENSOR = (1 << 3),
    FAULT_I2C_FAILURE = (1 << 4),
    FAULT_PCF8574_FAILURE = (1 << 5)
};
```

Faults are stored as bit fields allowing multiple simultaneous faults.

### Relay State Data

8-bit field representing relay states (active-low logic):
- Bit 0: Circulation Pump
- Bit 1: Massage Pump
- Bit 2: Jet Pump
- Bit 3: Water Heater
- Bit 4: Ozone Generator
- Bit 5: Speaker
- Bit 6: Lights
- Bit 7: Spare (permanently OFF)


## Error Handling

### Fault Detection Strategy

**Boot Fault Persistence**: On boot, if ANY precondition or safety condition fails, the system enters FAULT state and persists there until ALL conditions are resolved. The system does NOT progress to READY state until safe.

**Fault Categories**:

1. **Sensor Faults**
   - Temperature sensor communication failure (consecutive errors)
   - Water level sensor indicating insufficient water
   - Invalid sensor readings outside defined ranges

2. **Safety Faults**
   - High temperature warning (non-fault, enters WARNING state)
   - Critical overtemperature (enters FAULT state)
   - Thermal runaway (unexpected temperature change/delta, requires manual acknowledgment)
   - Low water level persisting beyond threshold duration

3. **Hardware Faults**
   - I2C bus communication timeout
   - PCF8574 communication failure
   - Relay verification failure

### Fault Handling Flow

```
Fault Detected
    ↓
Enter FAULT State
    ↓
Execute Safe Shutdown (heater first, features, then circulation)
    ↓
Store fault in bit field
    ↓
Display fault on UI with specific error bitmap
    ↓
If multiple faults: Enable Fault_Inspection browsing
    ↓
Monitor for condition resolution
    ↓
Auto-clear OR require manual acknowledgment
    ↓
When ALL faults cleared: Transition to READY
```

### Safe Shutdown Sequence

Priority order (non-blocking with timing):
1. **Water Heater OFF** (highest priority, immediate)
2. **Ozone Generator OFF** (100ms delay)
3. **Jet Pump OFF** (100ms delay)
4. **Massage Pump OFF** (100ms delay)
5. **Speaker and Lights OFF** (100ms delay)
6. **Circulation Pump OFF** (last, 200ms delay)
7. **Verify all relays HIGH** (off state)

### Error Recovery

**Auto-Clear Faults**:
- Low water level: Clears after stabilization period of sufficient water
- High temperature warning: Clears when temperature drops below threshold
- Sensor communication: Clears after consecutive valid readings

**Manual Acknowledgment Required**:
- Thermal runaway: Requires explicit user acknowledgment before heater can operate again
- Critical hardware faults: May require power cycle

### Multi-Fault Browsing

When multiple faults are active:
- Display fault count on main screen
- Enter FAULT_INSPECTION state
- Rotary encoder left/right rotation scrolls through faults
- Each fault displays with its specific error bitmap
- Fault index and total count shown


## Testing Strategy

### Why Property-Based Testing Does NOT Apply

This project is **NOT suitable for property-based testing** because:

1. **Embedded Firmware with Hardware Integration**: The system is tightly coupled to physical hardware (sensors, relays, I2C devices, display). Behavior depends on actual hardware state, not pure functions with universal properties.

2. **Side-Effect Driven**: Most operations are side-effect driven (relay activation, display updates, sensor reads, I2C transactions). There are no pure functions with input/output behavior suitable for property-based testing.

3. **State Machine with Hardware Dependencies**: While the state machine is deterministic, state transitions depend on hardware sensor readings and physical conditions that cannot be meaningfully randomized.

4. **Manual Hardware Testing Required**: The requirements explicitly specify manual hardware testing only, with no automated test framework.

5. **No Universal Properties**: There are no meaningful "for all inputs X, property P(X) holds" statements that can be tested. The system responds to specific hardware conditions, not arbitrary input spaces.

**Therefore, the Correctness Properties section is omitted from this design document.**

### Testing Approach

**Unit Testing Strategy**:
- Test individual component logic with mocked hardware interfaces
- Verify state machine transitions with simulated conditions
- Test safety precondition checking logic
- Test fault detection algorithms
- Test bitmap rendering logic (without actual display)
- Test timing calculations and non-blocking state machines

**Integration Testing Strategy**:
- Test I2C bus manager with actual OLED and PCF8574
- Test sensor manager with actual DS18B20 and water level sensor
- Test relay controller with actual relay module
- Test complete state machine with hardware in the loop
- Test safe shutdown sequence timing
- Test multi-fault browsing with simulated multiple faults

**Manual Hardware Validation** (Required at each phase):
- Verify boot sequence and initialization
- Verify sensor readings accuracy
- Verify relay activation/deactivation
- Verify display rendering and bitmap scaling
- Verify rotary encoder input handling
- Verify buzzer operation
- Verify fault detection and persistence
- Verify safe shutdown sequence
- Verify feature sequencing and interlocks
- Verify thermal runaway detection
- Verify multi-fault browsing

**Test Coverage Goals**:
- All state transitions exercised
- All fault conditions triggered
- All safety interlocks verified
- All precondition checks validated
- All bitmap rendering paths tested
- Boot-safe pin behavior verified

### Debug and Validation Tools

**Serial Debug Output** (when `DENABLE_SERIAL_DEBUG` defined):
- State transitions logged
- Sensor readings logged
- Fault detection events logged
- Relay state changes logged
- I2C transaction status logged
- Timing violations logged

**Hardware Test Procedures**:
- Boot sequence checklist
- Sensor calibration verification
- Relay timing measurement
- Display refresh rate measurement
- Input response time measurement
- Safe shutdown timing verification


## Implementation Guidance

### Non-Blocking Architecture Patterns

**Timing Pattern**:
```cpp
class NonBlockingTimer {
private:
    unsigned long lastTriggerTime;
    unsigned long interval;
    
public:
    bool check() {
        unsigned long currentTime = millis();
        if (currentTime - lastTriggerTime >= interval) {
            lastTriggerTime = currentTime;
            return true;
        }
        return false;
    }
};
```

**State Machine Pattern**:
```cpp
void SensorManager::update() {
    switch (tempSensorState) {
        case TEMP_IDLE:
            if (tempReadTimer.check()) {
                sensors.requestTemperatures();
                tempSensorState = TEMP_WAITING;
                tempRequestTime = millis();
            }
            break;
            
        case TEMP_WAITING:
            if (millis() - tempRequestTime >= TEMP_CONVERSION_TIME_MS) {
                float temp = sensors.getTempCByIndex(0);
                processTemperature(temp);
                tempSensorState = TEMP_IDLE;
            }
            break;
    }
}
```

### I2C Bus Management Pattern

```cpp
bool RelayController::setRelay(RelayChannel channel, bool active) {
    if (!i2cBus.acquireLock(I2C_ADDRESS_PCF8574, I2C_TIMEOUT_MS)) {
        return false;  // Timeout
    }
    
    // Update relay state
    if (active) {
        relayState &= ~(1 << channel);  // Active-low: clear bit
    } else {
        relayState |= (1 << channel);   // Active-low: set bit
    }
    
    // Write to hardware
    bool success = writeToHardware();
    
    i2cBus.releaseLock();
    return success;
}
```

### Bitmap Rendering with Scale-by-2

```cpp
void UIManager::drawScaledBitmap(const uint8_t* bitmap, int16_t x, int16_t y,
                                  int16_t w, int16_t h) {
    // Scale down by factor of 2
    int16_t scaledW = w / 2;
    int16_t scaledH = h / 2;
    
    // Render scaled bitmap
    for (int16_t j = 0; j < scaledH; j++) {
        for (int16_t i = 0; i < scaledW; i++) {
            // Sample every other pixel from source bitmap
            int16_t srcX = i * 2;
            int16_t srcY = j * 2;
            
            // Get pixel from source
            uint8_t byte = pgm_read_byte(&bitmap[(srcY * w + srcX) / 8]);
            uint8_t bit = (byte >> (7 - ((srcY * w + srcX) % 8))) & 1;
            
            if (bit) {
                display.drawPixel(x + i, y + j, SH110X_WHITE);
            }
        }
    }
}
```

### Bitmap-Glyph Text Rendering

```cpp
// Bitmap glyph structure
struct BitmapGlyph {
    char character;
    uint8_t width;
    uint8_t height;
    const uint8_t* bitmap;  // PROGMEM
};

// Glyph lookup table in PROGMEM
const BitmapGlyph glyphTable[] PROGMEM = {
    {'A', 8, 12, glyph_A_bitmap},
    {'B', 8, 12, glyph_B_bitmap},
    // ... all alphanumeric characters
    {'0', 8, 12, glyph_0_bitmap},
    {'1', 8, 12, glyph_1_bitmap},
    // ... all digits
    {176, 6, 6, glyph_degree_bitmap},  // degree symbol °
    {' ', 4, 12, glyph_space_bitmap},
    // ... punctuation
};

void UIManager::drawText(const char* text, int16_t x, int16_t y) {
    int16_t cursorX = x;
    
    for (size_t i = 0; i < strlen(text); i++) {
        char c = text[i];
        
        // Find glyph in lookup table
        const BitmapGlyph* glyph = findGlyph(c);
        if (glyph == nullptr) continue;
        
        // Read glyph properties from PROGMEM
        uint8_t glyphWidth = pgm_read_byte(&glyph->width);
        uint8_t glyphHeight = pgm_read_byte(&glyph->height);
        const uint8_t* glyphBitmap = (const uint8_t*)pgm_read_ptr(&glyph->bitmap);
        
        // Draw glyph bitmap (already scaled by 2 in source)
        drawScaledBitmap(glyphBitmap, cursorX, y, glyphWidth, glyphHeight);
        
        // Advance cursor
        cursorX += (glyphWidth / 2) + 1;  // 1 pixel spacing
    }
}

const BitmapGlyph* UIManager::findGlyph(char c) {
    for (size_t i = 0; i < sizeof(glyphTable) / sizeof(BitmapGlyph); i++) {
        char glyphChar = pgm_read_byte(&glyphTable[i].character);
        if (glyphChar == c) {
            return &glyphTable[i];
        }
    }
    return nullptr;  // Glyph not found
}
```

**Key Points for Bitmap-Glyph Text**:
- No native font rendering functions (e.g., `display.print()`, `display.drawChar()`)
- All text rendered as bitmap glyphs from PROGMEM
- Glyph bitmaps designed at 2x resolution, scaled down by factor of 2
- Glyph lookup table for character-to-bitmap mapping
- Supports alphanumeric, degree symbol, common punctuation
- Horizontal spacing between glyphs for readability

### Thermal Runaway Detection

```cpp
bool SafetySystem::detectThermalRunaway() {
    // Check if we have enough history
    if (historyCount < TEMP_HISTORY_SIZE) {
        return false;
    }
    
    // Calculate temperature delta
    float oldestTemp = temperatureHistory[0];
    float newestTemp = temperatureHistory[TEMP_HISTORY_SIZE - 1];
    float delta = newestTemp - oldestTemp;
    
    // Calculate rate of change (°C/second)
    float timeSpan = (TEMP_HISTORY_SIZE - 1) * (TEMP_SENSOR_READ_PERIOD_MS / 1000.0f);
    float rate = delta / timeSpan;
    
    // Check thresholds
    if (fabs(delta) > TEMP_DELTA_THRESHOLD || fabs(rate) > TEMP_RATE_THRESHOLD) {
        return true;  // Thermal runaway detected
    }
    
    return false;
}
```

### Boot-Safe Pin Initialization

```cpp
void initializeHardware() {
    // Initialize boot-strap sensitive pins FIRST
    // D8 (GPIO15) - Buzzer: MUST be LOW at boot
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    
    // D3 (GPIO0) - Encoder SW: Configure with pull-up
    // Circuit design must ensure this doesn't force programming mode
    pinMode(PIN_ENCODER_SW, INPUT_PULLUP);
    
    // Initialize I2C bus before any I2C devices
    Wire.begin();
    Wire.setClock(100000);  // 100kHz for reliability
    
    // Initialize PCF8574 with all relays OFF (HIGH)
    Wire.beginTransmission(I2C_ADDRESS_PCF8574);
    Wire.write(0xFF);  // All HIGH = all OFF
    Wire.endTransmission();
    
    // Continue with other pins...
}
```

### Feature Sequencing Logic

```cpp
bool SafetySystem::checkHeaterPreconditions() {
    // ABSOLUTE RULE: Heater requires circulation pump active
    if (!relayController.getRelayState(RELAY_CIRCULATION_PUMP)) {
        return false;  // Circulation MUST be active
    }
    
    // Check water level
    if (!sensorManager.isWaterLevelSufficient()) {
        return false;
    }
    
    // Check temperature below warning threshold
    if (sensorManager.getTemperature() >= TEMP_WARNING_THRESHOLD) {
        return false;
    }
    
    // Check no active faults
    if (hasActiveFaults()) {
        return false;
    }
    
    // Check thermal runaway acknowledgment
    if (!thermalRunawayAcknowledged && hadThermalRunaway) {
        return false;  // Requires manual acknowledgment
    }
    
    return true;  // All preconditions satisfied
}
```


## Development Phases

### Phase 1: Hardware Initialization and Configuration
**Deliverables**:
- Complete pin initialization with boot-safety
- I2C bus initialization and device verification
- Centralized configuration headers created
- All relays default to OFF state
- Serial debug output framework
- Integration with `src/main.cpp`

**Acceptance Criteria**:
- All pins configured according to hardware mapping
- Boot-strap sensitive pins (D3, D8) verified safe
- I2C devices (OLED at 0x3C, PCF8574 at 0x20) detected
- All relays verified in OFF state at boot
- Debug output shows initialization sequence

### Phase 2: Sensor Integration
**Deliverables**:
- DS18B20 temperature sensor driver (non-blocking)
- XKC-Y25-V water level sensor driver
- Sensor validation and fault detection
- Temperature history buffer for thermal runaway detection
- Integration with `src/main.cpp` event loop

**Acceptance Criteria**:
- Temperature readings accurate and non-blocking
- Water level sensor reads correctly (active-low logic)
- Invalid sensor readings detected and counted
- Sensor faults trigger after threshold exceeded
- Temperature history maintained for delta calculation

### Phase 3: Relay Control and I2C Bus Management
**Deliverables**:
- PCF8574 relay controller with active-low logic
- I2C bus manager with locking mechanism
- 8-channel relay mapping implementation
- Relay state synchronization
- Safe shutdown sequence implementation

**Acceptance Criteria**:
- All 8 relays controllable individually
- Active-low logic verified (HIGH=off, LOW=on)
- I2C bus conflicts prevented
- Safe shutdown executes in correct priority order
- Spare channel (8th) remains OFF

### Phase 4: State Machine Implementation
**Deliverables**:
- State machine with all 9 states
- State transition logic with guard conditions
- Entry/exit actions for each state
- Boot fault persistence logic
- State transition logging

**Acceptance Criteria**:
- All state transitions work correctly
- Guard conditions prevent invalid transitions
- Boot fault persistence: system stays in FAULT until conditions resolved
- State entry/exit actions execute properly
- Debug logging shows state transitions

### Phase 5: Safety System
**Deliverables**:
- Precondition checking for all features
- Fault detection for all fault types
- Thermal runaway detection algorithm
- Safe shutdown coordinator
- Fault bit field management
- Manual acknowledgment logic for thermal runaway

**Acceptance Criteria**:
- All preconditions enforced correctly
- Circulation pump required for heater (absolute rule)
- Thermal runaway detected based on temperature delta/rate
- Multiple simultaneous faults supported
- Manual acknowledgment required for thermal runaway
- Safe shutdown prioritizes heater deactivation

### Phase 6: User Interface - Display
**Deliverables**:
- Bitmap rendering engine with scale-by-2 rule
- UI state management for all system states
- Temperature display using bitmap digits
- Feature status display with bitmaps
- Fault display with error bitmaps
- Multi-fault browsing UI

**Acceptance Criteria**:
- All bitmaps render correctly scaled by 2
- NO text rendering used (bitmap-only)
- Temperature displayed using bitmap digits
- All 13 bitmaps integrated from `include/Bitmaps.h`
- Fault count displayed on main screen
- Multi-fault browsing shows each fault with specific bitmap

### Phase 7: User Interface - Input
**Deliverables**:
- Rotary encoder driver with debouncing
- Pin A0 (ADC0/GPIO17) validation for DT input
- Context-sensitive action handling
- Multi-fault browsing control
- Button press and hold detection

**Acceptance Criteria**:
- Encoder rotation detected correctly (CW/CCW)
- Pin A0 functions correctly as DT input (or alternative pin selected)
- Button presses debounced properly
- Boot-strap safety verified for D3 (GPIO0)
- In FAULT_INSPECTION: left/right scrolls through faults
- Context-sensitive actions work in all states

### Phase 8: Buzzer and Audible Feedback
**Deliverables**:
- Non-blocking buzzer controller
- Confirmation, warning, alert, and fault patterns
- Boot-safe buzzer initialization on D8 (GPIO15)
- Buzzer timing management

**Acceptance Criteria**:
- Buzzer operates on/off only (no tone generation)
- Boot-safe: D8 defaults to LOW/OFF
- Non-blocking timing for all durations
- Confirmation beep on feature activation
- Alert sounds on faults
- No buzzer overlap issues

### Phase 9: Feature Control and Sequencing
**Deliverables**:
- Circulation pump control with preconditions
- Water heater control with absolute circulation dependency
- Massage, jet, ozone, speaker, lights control
- Feature sequencing enforcement
- Feature activation timing

**Acceptance Criteria**:
- Circulation pump checks all preconditions before activation
- Heater NEVER activates without circulation (absolute rule)
- All features inhibited until circulation active
- Feature deactivation when circulation stops
- Feature activation delays implemented correctly

### Phase 10: Integration Testing and Validation
**Deliverables**:
- Complete system integration
- End-to-end testing procedures
- Hardware validation checklist
- Performance verification
- Documentation in `docs/phase-10-integration-testing.md`

**Acceptance Criteria**:
- All requirements validated on hardware
- Boot fault persistence verified
- Thermal runaway detection tested
- Multi-fault browsing tested with multiple simultaneous faults
- Safe shutdown timing measured
- All safety interlocks verified
- Display refresh rate acceptable
- Input response time acceptable
- Memory usage within limits

### Task Execution Protocol (Mandatory for Each Phase)

**Step 1: Pre-Git**
- Execute `git status`, `git branch -vv`, `git fetch origin`
- Verify clean working state
- Create feature branch: `feature/phase-<N>-<description>`

**Step 2: Deep Codebase Analysis**
- Read all files in `include/*`
- Read all files in `lib/*`
- Read `src/main.cpp`
- Read all files in `docs/*`

**Step 3: Deep Previous-Phase Analysis**
- Review `docs/phase-<N-1>-*.md` documentation
- Analyze previous phase implementation
- Identify integration points

**Step 4: Phase Execution**
- Implement only current phase scope
- Integrate with `src/main.cpp`
- NO hardcoded constants (all from `include/*`)
- Follow non-blocking architecture
- Add debug logging (conditional on `DENABLE_SERIAL_DEBUG`)

**Step 5: User Review Gate**
- Summarize changes made
- **AI agent MUST ask user to run PlatformIO commands** (build, upload, monitor)
- **AI agent CANNOT execute PlatformIO commands directly**
- User performs manual hardware testing on actual hardware
- Get explicit approval before proceeding
- NO automated tests (manual hardware review only)

**Step 6: Post-Git**
- Create documentation in `docs/phase-<N>-<description>.md`
- Execute `git add`, `git commit` with descriptive message
- Execute `git push`
- Merge feature branch
- Delete feature branch

**CRITICAL NOTES** (Requirements 16.8, 16.10):
- **AI agent MUST ask user to manually execute PlatformIO commands** (build, upload, monitor)
- AI agent cannot execute these commands directly
- Manual hardware testing required at Step 5 - NO automated test framework
- User approval required before proceeding to next phase


## Build Configuration

### PlatformIO Configuration

**platformio.ini** requirements:

```ini
[env:esp12e]
platform = espressif8266
board = esp12e
framework = arduino

lib_deps =
    adafruit/Adafruit SH110X@^2.1.14
    adafruit/Adafruit GFX Library@^1.11.0
    paulstoffregen/OneWire@^2.3.7
    milesburton/DallasTemperature@^3.11.0
    arduinogetstarted/ezButton@^1.0.6

build_flags =
    -DENABLE_SERIAL_DEBUG  ; Enable debug output (comment out for production)

build_src_filter =
    +<*>
    +<../lib/*/*.cpp>

monitor_speed = 115200
upload_speed = 921600

; CRITICAL: NO flto (Link Time Optimization)
; Do NOT add: -flto
```

**Key Points**:
- Exact library version for Adafruit SH110X: `@^2.1.14`
- `DENABLE_SERIAL_DEBUG` flag (exact name with D prefix) for conditional debug output
- `build_src_filter` includes lib directory (required)
- NO `flto` flag (absolute rule)
- Serial monitor at 115200 baud

### Conditional Debug Output

```cpp
#ifdef ENABLE_SERIAL_DEBUG
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(fmt, ...)
#endif
```

Usage:
```cpp
DEBUG_PRINTLN("Entering FAULT state");
DEBUG_PRINTF("Temperature: %.2f°C\n", temperature);
```

### Memory Optimization

**PROGMEM Usage**:
```cpp
// All bitmaps in PROGMEM
const unsigned char water_drop_bitmap[] PROGMEM = { /* data */ };

// String constants in PROGMEM
const char FAULT_MSG_LOW_WATER[] PROGMEM = "Low Water Level";

// Access with pgm_read_byte()
uint8_t byte = pgm_read_byte(&water_drop_bitmap[index]);
```

**Static Allocation**:
- All data structures statically allocated
- Avoid dynamic memory allocation (new/delete, malloc/free)
- Use fixed-size arrays and buffers
- Minimize heap fragmentation

**Bit Fields for State**:
```cpp
struct SystemFlags {
    uint8_t circulationActive : 1;
    uint8_t heaterActive : 1;
    uint8_t massageActive : 1;
    uint8_t jetActive : 1;
    uint8_t ozoneActive : 1;
    uint8_t speakerActive : 1;
    uint8_t lightsActive : 1;
    uint8_t thermalRunawayAck : 1;
};
```


## Key Design Decisions

### 1. Boot Fault Persistence
**Decision**: System remains in FAULT state from boot until ALL preconditions and safety conditions are satisfied.

**Rationale**: Safety-first approach ensures the controller never progresses to operational states with unsafe conditions. Prevents automatic recovery from boot-time faults that could indicate hardware issues.

**Implementation**: Self-check state verifies all conditions before allowing transition to READY state. Faults detected during self-check persist until explicitly resolved.

### 2. Thermal Runaway Detection
**Decision**: Thermal runaway defined as unexpected temperature CHANGE or DELTA (not just absolute threshold), requires manual acknowledgment.

**Rationale**: Distinguishes between normal high temperature (auto-recoverable) and abnormal temperature behavior (requires investigation). Manual acknowledgment ensures operator awareness before heater can operate again.

**Implementation**: Temperature history buffer tracks recent readings. Delta and rate-of-change calculations detect abnormal behavior. Separate flag tracks acknowledgment requirement.

### 3. Bitmap-Only UI with Scale-by-2 Rule
**Decision**: All UI elements rendered exclusively using bitmaps, scaled down by factor of 2, with exact naming convention.

**Rationale**: Consistent visual appearance, predictable memory usage, no font rendering overhead. Scale-by-2 rule allows higher-resolution bitmap design while fitting 128x64 display.

**Implementation**: All bitmaps stored in PROGMEM with `_bitmap` suffix. Custom scaling function samples every other pixel. Temperature displayed using bitmap digits.

### 4. Absolute Circulation Dependency for Heater
**Decision**: Water heater can NEVER activate without circulation pump actively running.

**Rationale**: Critical safety requirement prevents heater operation without water flow, which could cause element damage or safety hazards.

**Implementation**: Heater precondition check explicitly verifies circulation pump state. State machine enforces dependency through guard conditions.

### 5. Multi-Fault Browsing
**Decision**: When multiple faults active, user can scroll left/right through faults using encoder rotation.

**Rationale**: Provides clear visibility into all active fault conditions. Allows operator to understand complete system state when multiple issues present.

**Implementation**: Fault bit field stores multiple faults. FAULT_INSPECTION state enables browsing. UI displays current fault index and total count.

### 6. Non-Blocking Architecture Throughout
**Decision**: NO delay() calls anywhere in application logic. All timing uses millis()-based state machines.

**Rationale**: Maintains system responsiveness. Allows concurrent operations (sensor reading, display updates, input processing). Enables deterministic timing behavior.

**Implementation**: Every time-dependent operation uses timer objects. State machines track operation progress. Event loop processes all subsystems each iteration.

### 7. Centralized Configuration with NO Hardcoded Constants
**Decision**: All constants defined in `include/*` headers. Absolute prohibition on hardcoded values.

**Rationale**: Single source of truth for all configuration. Easy tuning and adjustment. Clear documentation of all system parameters. Prevents scattered magic numbers.

**Implementation**: Separate headers for hardware, safety, timing, states, faults. All implementation files import from centralized headers.

### 8. Active-Low Relay Logic
**Decision**: Relays controlled with active-low logic (HIGH=off, LOW=on) through PCF8574.

**Rationale**: Matches common relay module design. Ensures relays default to OFF state on power-up or I2C failure (pull-ups on PCF8574).

**Implementation**: Relay state bit field inverted before writing to PCF8574. All relay operations account for active-low logic.

### 9. I2C Bus Serialization with Priority
**Decision**: All I2C transactions serialized through bus manager. Safety-critical relay operations prioritized over display updates.

**Rationale**: Prevents bus conflicts between OLED and PCF8574. Ensures safety operations not starved by display refresh. Timeout handling prevents deadlock.

**Implementation**: Lock/release mechanism for bus access. Priority queue or explicit priority checks. Timeout-based lock release.

### 10. Manual Hardware Testing Only
**Decision**: No automated test framework. All validation through manual hardware testing at each phase.

**Rationale**: Embedded firmware with tight hardware coupling makes automated testing complex. Manual testing on actual hardware provides highest confidence. Matches project constraints and development workflow.

**Implementation**: Detailed hardware test procedures for each phase. Checklist-based validation. User review gate in Task Execution Protocol.


## Risk Analysis and Mitigation

### Hardware Risks

**Risk 1: Pin A0 (ADC0/GPIO17) Cannot Function as Encoder DT Input**
- **Impact**: High - Encoder would not work correctly
- **Likelihood**: Medium - ADC pins may have limitations for digital input
- **Mitigation**: Validate pin capability in Phase 7. If incompatible, select alternative digital GPIO pin (e.g., D0/GPIO16, D4/GPIO2) and update hardware mapping.

**Risk 2: Boot-Strap Pin D3 (GPIO0) Forces Programming Mode**
- **Impact**: Critical - System would not boot normally
- **Likelihood**: Medium - Depends on switch circuit design
- **Mitigation**: Ensure switch circuit includes proper pull-up resistor. Test boot behavior with button pressed. Document circuit requirements clearly.

**Risk 3: Boot-Strap Pin D8 (GPIO15) Prevents Boot if HIGH**
- **Impact**: Critical - System would not boot
- **Likelihood**: Low - Design explicitly sets LOW at boot
- **Mitigation**: Initialize D8 as OUTPUT LOW before any other operations. Verify boot behavior in Phase 1.

**Risk 4: I2C Bus Conflicts Between OLED and PCF8574**
- **Impact**: High - Display or relay control could fail
- **Likelihood**: Medium - Concurrent access possible
- **Mitigation**: Implement I2C bus manager with locking. Serialize all transactions. Add timeout handling.

### Safety Risks

**Risk 5: Heater Activates Without Circulation**
- **Impact**: Critical - Equipment damage, safety hazard
- **Likelihood**: Low - Multiple safeguards in place
- **Mitigation**: Absolute precondition check. State machine guard conditions. Safe shutdown on circulation loss. Extensive testing in Phase 9.

**Risk 6: Thermal Runaway Not Detected**
- **Impact**: Critical - Overheating, safety hazard
- **Likelihood**: Low - Detection algorithm in place
- **Mitigation**: Temperature history buffer. Delta and rate-of-change thresholds. Manual acknowledgment requirement. Thorough testing with simulated temperature changes.

**Risk 7: Fault Persistence Fails, System Operates Unsafely**
- **Impact**: Critical - Unsafe operation
- **Likelihood**: Low - Design enforces persistence
- **Mitigation**: Boot fault persistence logic. State machine prevents READY transition with active faults. Extensive fault injection testing.

**Risk 8: Water Level Sensor Fails, Pumps Run Dry**
- **Impact**: High - Equipment damage
- **Likelihood**: Medium - Sensor could fail
- **Mitigation**: Sensor fault detection. Consecutive error counting. Fault state entry on sensor failure. Precondition checks before pump activation.

### Software Risks

**Risk 9: Memory Exhaustion on ESP8266**
- **Impact**: High - System crash, undefined behavior
- **Likelihood**: Medium - Limited RAM (80KB)
- **Mitigation**: PROGMEM for all constants. Static allocation. Minimize heap usage. Memory monitoring during development.

**Risk 10: Non-Blocking Timing Overflow**
- **Impact**: Medium - Timing glitches every 49 days
- **Likelihood**: Low - millis() overflow rare in practice
- **Mitigation**: Use unsigned long arithmetic. Test overflow handling. Document known limitation.

**Risk 11: Display Refresh Starves Safety Operations**
- **Impact**: High - Delayed fault response
- **Likelihood**: Low - Priority handling in place
- **Mitigation**: I2C bus priority for relay operations. Rate-limit display updates. Measure worst-case event loop timing.

**Risk 12: Hardcoded Constants Introduced During Development**
- **Impact**: Medium - Maintenance issues, inconsistency
- **Likelihood**: Medium - Easy to accidentally hardcode
- **Mitigation**: Code review checklist. Grep for magic numbers. Enforce centralized configuration rule strictly.

### Development Risks

**Risk 13: Phase Integration Breaks Previous Functionality**
- **Impact**: High - Regression, rework required
- **Likelihood**: Medium - Complex integration
- **Mitigation**: Task Execution Protocol Step 2 (deep codebase analysis). Incremental integration. Manual testing at each phase. Git branching strategy.

**Risk 14: Hardware Not Available for Testing**
- **Impact**: High - Cannot validate implementation
- **Likelihood**: Low - Hardware specified upfront
- **Mitigation**: User review gate at Step 5. Clear hardware requirements. Simulation/mocking for initial development.

**Risk 15: Bitmap Scaling Degrades Visual Quality**
- **Impact**: Low - Aesthetic issue
- **Likelihood**: Medium - Scale-by-2 may lose detail
- **Mitigation**: Design bitmaps at 2x resolution. Test visual appearance early. Adjust bitmap designs if needed.


## Performance Considerations

### Timing Requirements

**Event Loop Iteration Target**: < 50ms
- Ensures responsive user input
- Allows 20Hz minimum update rate
- Provides margin for I2C operations

**Sensor Reading Periods**:
- Temperature: 2000ms (DS18B20 conversion time ~750ms)
- Water level: 500ms (fast response for safety)

**Display Update Rate**: 100ms (10Hz maximum)
- Balances visual smoothness with I2C bandwidth
- Prevents display operations from dominating bus

**Input Debouncing**: 50ms
- Standard debounce time for mechanical switches
- Prevents spurious encoder readings

**I2C Transaction Timeout**: 1000ms
- Allows recovery from bus hang
- Prevents indefinite blocking

### Memory Budget

**ESP8266 Resources**:
- Flash: 4MB (program storage)
- RAM: 80KB (runtime memory)
- Heap: ~40KB available after system overhead

**Memory Allocation Strategy**:
- Bitmaps in PROGMEM: ~10KB (13 bitmaps × ~800 bytes each)
- Static buffers: ~5KB (display buffer, sensor data, state)
- Stack: ~4KB (function call overhead)
- Heap: Minimize usage, prefer static allocation
- Reserve: ~20KB margin for safety

**Memory Monitoring**:
```cpp
#ifdef ENABLE_SERIAL_DEBUG
void printMemoryUsage() {
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t heapFragmentation = ESP.getHeapFragmentation();
    DEBUG_PRINTF("Free heap: %u bytes, Fragmentation: %u%%\n", 
                 freeHeap, heapFragmentation);
}
#endif
```

### I2C Bus Utilization

**Devices on Bus**:
- OLED Display (0x3C): High bandwidth (display buffer updates)
- PCF8574 (0x20): Low bandwidth (single byte writes)

**Transaction Priorities**:
1. Safety-critical relay writes (immediate)
2. Sensor reads (periodic, non-critical)
3. Display updates (rate-limited, lowest priority)

**Bus Speed**: 100kHz (standard mode)
- Reliable for ESP8266
- Adequate bandwidth for both devices
- Reduces EMI and signal integrity issues

### CPU Utilization

**Expected Load Distribution**:
- Event loop overhead: 10%
- Sensor processing: 15%
- State machine: 10%
- Display rendering: 30%
- I2C transactions: 20%
- Input processing: 5%
- Safety checks: 10%

**Optimization Strategies**:
- Minimize display updates (only on state changes)
- Cache relay state (avoid redundant I2C writes)
- Use bit operations for state flags
- Avoid floating-point where possible (use integer math)


## Traceability Matrix

### Requirements to Design Components

| Requirement | Design Component(s) | Implementation Phase |
|-------------|---------------------|---------------------|
| Req 1: System Initialization | Hardware initialization, I2C Bus Manager, Relay Controller | Phase 1, 3 |
| Req 2: Temperature Monitoring | Sensor Manager, Safety System | Phase 2, 5 |
| Req 3: Water Level Detection | Sensor Manager, Safety System | Phase 2, 5 |
| Req 4: Relay Control | Relay Controller, I2C Bus Manager | Phase 3 |
| Req 5: Circulation Pump Control | Safety System, State Machine, Relay Controller | Phase 4, 5, 9 |
| Req 6: Water Heater Control | Safety System, State Machine, Relay Controller | Phase 5, 9 |
| Req 7: Feature Sequencing | Safety System, State Machine, Relay Controller | Phase 5, 9 |
| Req 8: State Machine | State Machine, State Definitions | Phase 4 |
| Req 9: User Interface Display | UI Manager, Bitmap rendering | Phase 6 |
| Req 10: Rotary Encoder Input | Input Handler | Phase 7 |
| Req 11: Fault Detection | Safety System, Fault Codes, UI Manager | Phase 5, 6 |
| Req 12: Safe Shutdown | Safety System, Relay Controller | Phase 3, 5 |
| Req 13: Non-Blocking Architecture | All components, Event loop | All phases |
| Req 14: Memory Optimization | PROGMEM usage, Static allocation, Build config | All phases |
| Req 15: Timing and Determinism | Timing Config, Non-blocking timers | All phases |
| Req 16: Development Process | Task Execution Protocol, Phase structure | All phases |
| Req 17: I2C Bus Management | I2C Bus Manager | Phase 3 |
| Req 18: Configuration Management | Centralized headers in include/* | Phase 1, all phases |
| Req 19: Error Recovery | Safety System, State Machine | Phase 5 |
| Req 20: Buzzer Feedback | Buzzer Controller | Phase 8 |

### Design Decisions to Requirements

| Design Decision | Validates Requirement(s) |
|-----------------|--------------------------|
| Boot Fault Persistence | Req 1.9, 1.10, 8.5, 8.6, 8.7, 11.2, 11.3 |
| Thermal Runaway Detection | Req 2.8, 2.9, 2.11, 6.6, 11.17, 19.5, 19.7 |
| Bitmap-Only UI with Scale-by-2 | Req 9.1-9.6, 14.1 |
| Absolute Circulation Dependency | Req 6.1, 6.2, 6.3, 6.8, 7.1-7.7 |
| Multi-Fault Browsing | Req 9.8, 9.9, 9.10, 10.9, 11.9, 11.10, 11.11 |
| Non-Blocking Architecture | Req 13.1-13.12, 15.1-15.8 |
| Centralized Configuration | Req 1.13, 1.14, 14.2, 14.3, 18.1-18.5 |
| Active-Low Relay Logic | Req 1.3, 4.1-4.3, 4.7 |
| I2C Bus Serialization | Req 1.1, 1.8, 4.12, 17.1-17.10 |
| Manual Hardware Testing | Req 16.6, 16.10 |

### State Machine to Requirements

| State | Validates Requirement(s) |
|-------|--------------------------|
| Boot | Req 1.1-1.15, 8.2 |
| Self_Check | Req 1.9, 1.10, 8.3, 8.4, 8.5, 8.6, 8.7 |
| Ready | Req 8.4, 8.7 |
| Active_Circulation | Req 5.4, 5.5, 8.8 |
| Feature_Enabled_Bath | Req 7.9, 8.9 |
| Warning | Req 2.7, 8.12 |
| Fault | Req 1.11, 8.10, 11.1, 11.6, 11.7 |
| Fault_Inspection | Req 8.11, 11.9, 11.10, 11.12, 11.13 |
| Shutdown | Req 12.1-12.14 |


## Summary

This design document provides a comprehensive architecture for the ESP8266 Jacuzzi Controller firmware. The system implements a safety-first approach with boot fault persistence, thermal runaway detection, and strict feature sequencing. The non-blocking event-driven architecture ensures responsive operation while maintaining deterministic timing behavior.

Key architectural highlights:
- **9-state state machine** with formal transitions and guard conditions
- **Bitmap-only UI** with scale-by-2 rule and exact naming conventions
- **Centralized configuration** with NO hardcoded constants
- **Multi-fault browsing** for comprehensive fault visibility
- **Absolute safety interlocks** preventing unsafe operations
- **10-phase development plan** with mandatory Task Execution Protocol

The design addresses all 20 requirements from the requirements document and provides detailed implementation guidance for each phase. Manual hardware testing at each phase ensures reliable operation on actual hardware.

---

**Document Status**: Ready for user review and implementation

**Next Steps**:
1. User reviews design document
2. User provides feedback on open questions
3. Begin Phase 1: Hardware Initialization and Configuration
4. Follow Task Execution Protocol for each phase

