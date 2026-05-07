# Product Overview

## ESP8266 Jacuzzi Controller

An embedded firmware system for ESP8266 that provides automated control and monitoring for jacuzzi/bathtub operations. The controller manages multiple loads (pumps, heater, ozone generator, speaker, lighting) through relay control while enforcing strict safety rules based on water level and temperature monitoring.

### Key Features

- **8-Channel Relay Control**: Manages circulation pump, massage pump, jet pump, 3kW water heater, ozone generator, speaker relay, and lighting system
- **Safety-First Architecture**: Boot fault persistence, thermal runaway detection, safe shutdown sequences, and continuous monitoring
- **Temperature Monitoring**: DS18B20 sensor with thermal runaway detection and configurable thresholds
- **Water Level Monitoring**: XKC-Y25-V sensor with active-low logic and safety interlocks
- **OLED Display**: SH110X 128x64 display with bitmap-only UI (no text rendering)
- **Rotary Encoder Input**: User interface navigation and control
- **State Machine Driven**: Formal state management with 9 states (Boot, Self_Check, Ready, Active_Circulation, Feature_Enabled_Bath, Warning, Fault, Fault_Inspection, Shutdown)

### Design Philosophy

This implementation is based on **publicly observable behavior only** - replicating externally visible UI and system responses without copying any proprietary firmware or protected assets. The implementation is original, based solely on observable controller behavior.

### Safety Principles

1. **Safety First**: All operations prioritize safety through precondition checking, fault persistence, and safe shutdown sequences
2. **Boot Fault Persistence**: System remains in fault state from boot until ALL preconditions and safety conditions are satisfied
3. **Circulation Dependency**: Water heater and all features REQUIRE circulation pump to be actively running (absolute rule)
4. **Thermal Runaway Detection**: Monitors temperature delta and rate-of-change to detect unexpected heating behavior
5. **Multi-Fault Management**: Supports multiple simultaneous faults with browsing capability
