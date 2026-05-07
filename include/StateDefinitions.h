#pragma once

// ============================================================================
// State Definitions
// ============================================================================
// This file contains state machine state enumerations and related constants.
// NO hardcoded state values allowed elsewhere.
// ============================================================================

// ----------------------------------------------------------------------------
// System State Enumeration
// ----------------------------------------------------------------------------

enum SystemState {
    STATE_BOOT,                    // Initial state during hardware initialization
    STATE_SELF_CHECK,              // Safety and sensor validation (boot fault persistence)
    STATE_READY,                   // System safe, accepting user commands
    STATE_ACTIVE_CIRCULATION,      // Circulation pump running (foundation for features)
    STATE_FEATURE_ENABLED_BATH,    // Circulation + additional features active
    STATE_WARNING,                 // Non-critical warning (e.g., high temperature warning)
    STATE_FAULT,                   // Safety or operational fault (all loads off)
    STATE_FAULT_INSPECTION,        // Browsing and reviewing active faults
    STATE_SHUTDOWN                 // Controlled shutdown sequence in progress
};

// ----------------------------------------------------------------------------
// State Descriptions
// ----------------------------------------------------------------------------
// STATE_BOOT:
//   - Entry: Power-on or reset
//   - Actions: Initialize I2C bus, configure pins, set relays to OFF
//   - Exit: When hardware initialization completes
//   - Transitions: Boot → Self_Check (success) or Boot → Fault (failure)
//
// STATE_SELF_CHECK:
//   - Entry: After hardware initialization
//   - Actions: Verify all preconditions and safety conditions
//     * Water level sufficient
//     * Temperature sensor operational and reading valid
//     * Temperature within safe range
//     * I2C devices responding
//     * All relays in OFF state
//   - CRITICAL: Boot fault persistence - system does NOT progress to Ready
//     until ALL conditions satisfied
//   - Transitions: Self_Check → Ready (all OK) or Self_Check → Fault (any fail)
//
// STATE_READY:
//   - Entry: All preconditions and safety conditions satisfied
//   - Actions: Display ready screen, await user input
//   - Transitions: Ready → Active_Circulation (user activates circulation)
//                  Ready → Fault (safety condition fails)
//
// STATE_ACTIVE_CIRCULATION:
//   - Entry: Circulation pump activated
//   - Actions: Monitor water level and temperature continuously
//   - Foundation state for all feature operations
//   - Transitions: Active_Circulation → Feature_Enabled_Bath (features activated)
//                  Active_Circulation → Ready (circulation deactivated)
//                  Active_Circulation → Fault (safety condition fails)
//
// STATE_FEATURE_ENABLED_BATH:
//   - Entry: Additional features activated beyond circulation
//   - Actions: Monitor all active features, enforce dependencies
//   - Multiple features can be active simultaneously
//   - Transitions: Feature_Enabled_Bath → Active_Circulation (all features off)
//                  Feature_Enabled_Bath → Warning (non-critical warning)
//                  Feature_Enabled_Bath → Fault (safety condition fails)
//
// STATE_WARNING:
//   - Entry: Non-critical warning condition detected
//   - Actions: Display warning overlay, continue operation
//   - Example: High temperature warning (below critical threshold)
//   - Transitions: Warning → Feature_Enabled_Bath (warning clears)
//                  Warning → Fault (warning escalates)
//
// STATE_FAULT:
//   - Entry: Safety or operational fault detected
//   - Actions: Execute safe shutdown, display fault indicators
//   - Fault persists until underlying conditions resolved
//   - Supports multiple simultaneous faults (bit field)
//   - Transitions: Fault → Fault_Inspection (user reviews faults)
//                  Fault → Self_Check (faults cleared, re-validate)
//
// STATE_FAULT_INSPECTION:
//   - Entry: User requests fault review (multiple faults active)
//   - Actions: Allow browsing through active faults with encoder
//   - Display each fault with specific error bitmap
//   - Transitions: Fault_Inspection → Fault (user exits inspection)
//                  Fault_Inspection → Self_Check (faults cleared during inspection)
//
// STATE_SHUTDOWN:
//   - Entry: Safe shutdown sequence initiated
//   - Actions: Deactivate loads in priority order (heater first, circulation last)
//   - Non-blocking timing for shutdown sequence
//   - Transitions: Shutdown → Fault (shutdown complete, fault persists)
//                  Shutdown → Ready (shutdown complete, no faults)

// ----------------------------------------------------------------------------
// State Machine Notes
// ----------------------------------------------------------------------------
// 1. All state transitions protected by guard conditions
// 2. Each state has entry and exit actions
// 3. Boot fault persistence is CRITICAL safety feature
// 4. Circulation is foundation for all feature operations
// 5. Multiple faults supported via bit field in FaultCodes.h
