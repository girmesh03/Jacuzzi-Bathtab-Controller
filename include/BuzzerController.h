#pragma once

#include <Arduino.h>
#include "HardwareConfig.h"
#include "TimingConfig.h"

// ============================================================================
// BuzzerController - Priority-Based Buzzer Management
// ============================================================================
// Non-blocking buzzer control with priority-based beep patterns.
// Higher priority beeps preempt lower priority active beeps.
//
// Priority order (highest to lowest):
//   1. ALERT  (fault)   - 3 beeps, 1000ms each
//   2. WARNING (warning) - 2 beeps, 500ms each
//   3. CONFIRMATION      - 1 beep, 200ms
//   4. BRIEF   (button)  - 1 beep, 50ms
//
// Pin: D8 (GPIO15) - active-high, MUST be LOW at boot
// ============================================================================

enum BuzzerPriority : int8_t {
    BUZZER_PRIORITY_NONE = -1,
    BUZZER_PRIORITY_BRIEF = 0,
    BUZZER_PRIORITY_CONFIRMATION = 1,
    BUZZER_PRIORITY_WARNING = 2,
    BUZZER_PRIORITY_ALERT = 3
};

struct BeepPattern {
    uint16_t onTimeMs;
    uint16_t offTimeMs;
    uint8_t repeatCount;
    BuzzerPriority priority;
};

class BuzzerController {
public:
    BuzzerController();

    void begin();
    void update();

    void briefBeep();
    void confirmationBeep();
    void warningBeep();
    void alertBeep();

    bool isActive();

private:
    enum BeepState : uint8_t {
        BEEP_IDLE,
        BEEP_ACTIVE,
        BEEP_SILENCE
    };

    BeepState state;
    BeepPattern currentPattern;
    unsigned long phaseStartTime;
    uint8_t remainingRepeats;

    static const BeepPattern PATTERN_BRIEF;
    static const BeepPattern PATTERN_CONFIRMATION;
    static const BeepPattern PATTERN_WARNING;
    static const BeepPattern PATTERN_ALERT;

    void beep(const BeepPattern& pattern);
    void startPattern(const BeepPattern& pattern);
    void stop();
};
