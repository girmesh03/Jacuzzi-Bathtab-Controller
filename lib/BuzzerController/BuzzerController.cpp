#include "BuzzerController.h"

// ============================================================================
// Beep Pattern Definitions
// ============================================================================

const BeepPattern BuzzerController::PATTERN_BRIEF = {
    BUZZER_BRIEF_MS,
    0,
    1,
    BUZZER_PRIORITY_BRIEF
};

const BeepPattern BuzzerController::PATTERN_CONFIRMATION = {
    BUZZER_CONFIRMATION_MS,
    0,
    1,
    BUZZER_PRIORITY_CONFIRMATION
};

const BeepPattern BuzzerController::PATTERN_WARNING = {
    BUZZER_WARNING_MS,
    200,
    2,
    BUZZER_PRIORITY_WARNING
};

const BeepPattern BuzzerController::PATTERN_ALERT = {
    BUZZER_ALERT_MS,
    300,
    3,
    BUZZER_PRIORITY_ALERT
};

// ============================================================================
// Constructor
// ============================================================================

BuzzerController::BuzzerController()
    : state(BEEP_IDLE)
    , currentPattern{0, 0, 0, BUZZER_PRIORITY_NONE}
    , phaseStartTime(0)
    , remainingRepeats(0)
{
}

// ============================================================================
// Public Methods
// ============================================================================

void BuzzerController::begin() {
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);
    state = BEEP_IDLE;
}

void BuzzerController::update() {
    if (state == BEEP_IDLE) {
        return;
    }

    unsigned long now = millis();
    unsigned long elapsed = now - phaseStartTime;

    switch (state) {
        case BEEP_ACTIVE:
            if (elapsed >= currentPattern.onTimeMs) {
                digitalWrite(PIN_BUZZER, LOW);
                remainingRepeats--;

                if (remainingRepeats > 0 && currentPattern.offTimeMs > 0) {
                    state = BEEP_SILENCE;
                    phaseStartTime = now;
                } else {
                    state = BEEP_IDLE;
                }
            }
            break;

        case BEEP_SILENCE:
            if (elapsed >= currentPattern.offTimeMs) {
                state = BEEP_ACTIVE;
                phaseStartTime = now;
                digitalWrite(PIN_BUZZER, HIGH);
            }
            break;

        default:
            break;
    }
}

void BuzzerController::briefBeep() {
    beep(PATTERN_BRIEF);
}

void BuzzerController::confirmationBeep() {
    beep(PATTERN_CONFIRMATION);
}

void BuzzerController::warningBeep() {
    beep(PATTERN_WARNING);
}

void BuzzerController::alertBeep() {
    beep(PATTERN_ALERT);
}

bool BuzzerController::isActive() {
    return state != BEEP_IDLE;
}

// ============================================================================
// Private Methods
// ============================================================================

void BuzzerController::beep(const BeepPattern& pattern) {
    if (state == BEEP_IDLE) {
        startPattern(pattern);
        return;
    }

    if (pattern.priority > currentPattern.priority) {
        stop();
        startPattern(pattern);
    }
}

void BuzzerController::startPattern(const BeepPattern& pattern) {
    currentPattern = pattern;
    remainingRepeats = pattern.repeatCount;
    state = BEEP_ACTIVE;
    phaseStartTime = millis();
    digitalWrite(PIN_BUZZER, HIGH);
}

void BuzzerController::stop() {
    digitalWrite(PIN_BUZZER, LOW);
    state = BEEP_IDLE;
    remainingRepeats = 0;
}
