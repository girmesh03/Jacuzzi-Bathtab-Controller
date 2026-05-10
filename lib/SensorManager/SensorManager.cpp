#include "SensorManager.h"
#include "FaultCodes.h"

// ============================================================================
// Debug Macros
// ============================================================================
#ifdef ENABLE_SERIAL_DEBUG
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// ============================================================================
// Initialization
// ============================================================================

void SensorManager::begin()
{
    // ------------------------------------------------------------------------
    // Temperature Sensor Initialization (DS18B20)
    // ------------------------------------------------------------------------

    DEBUG_PRINTLN(F(""));
    DEBUG_PRINTLN(F("Initializing Sensor Manager..."));

    // Initialize OneWire on temperature sensor pin
    oneWire = OneWire(PIN_TEMP_SENSOR);
    sensors = DallasTemperature(&oneWire);

    // Start DallasTemperature library
    sensors.begin();

    // Verify sensor count
    uint8_t sensorCount = sensors.getDeviceCount();

#ifdef ENABLE_SERIAL_DEBUG
    Serial.print(F("DS18B20 temperature sensor on D5 (GPIO"));
    Serial.print(PIN_TEMP_SENSOR);
    Serial.println(F(")"));
    Serial.print(F("  Sensors found: "));
    Serial.println(sensorCount);
#endif

    // Set resolution to 12-bit (highest accuracy, ~750ms conversion)
    if (sensorCount > 0)
    {
        sensors.setResolution(12);
        DEBUG_PRINTLN(F("  Resolution: 12-bit"));
    }

    // Initialize temperature sensor state
    tempState = TEMP_IDLE;
    currentTemperature = TEMP_INVALID_VALUE;
    lastTempReadTime = 0;
    tempRequestTime = 0;
    tempErrorCount = 0;
    tempValidCount = 0;
    tempSensorOperational = (sensorCount > 0); // Operational if sensor found

    // Initialize temperature history buffer
    historyIndex = 0;
    historyCount = 0;
    for (uint8_t i = 0; i < TEMP_HISTORY_SIZE; i++)
    {
        temperatureHistory[i] = 0.0f;
    }

#ifdef ENABLE_SERIAL_DEBUG
    Serial.print(F("  Initial status: "));
    Serial.println(tempSensorOperational ? F("Operational") : F("NOT FOUND"));
#endif

    // ------------------------------------------------------------------------
    // Water Level Sensor Initialization (XKC-Y25-V)
    // ------------------------------------------------------------------------

    // Configure water level pin with pull-up (active-low sensor)
    pinMode(PIN_WATER_LEVEL, INPUT_PULLUP);

    // Read initial state (active-low: LOW = sufficient, HIGH = insufficient)
    bool pinState = digitalRead(PIN_WATER_LEVEL);
    waterLevelSufficient = (pinState == LOW);

#ifdef ENABLE_SERIAL_DEBUG
    Serial.print(F("XKC-Y25-V water level sensor on D7 (GPIO"));
    Serial.print(PIN_WATER_LEVEL);
    Serial.println(F(")"));
    Serial.print(F("  Active-low logic: LOW=sufficient, HIGH=insufficient"));
    Serial.println(F(""));
    Serial.print(F("  Initial status: "));
    Serial.println(waterLevelSufficient ? F("Sufficient") : F("Insufficient"));
#endif

    // Initialize water level timing
    lastWaterLevelReadTime = 0;
    waterLevelFaultStartTime = 0;
    waterLevelRecoveryStartTime = 0;
    waterLevelFaultActive = false;

    DEBUG_PRINTLN(F("Sensor Manager initialization complete"));
    DEBUG_PRINTLN(F(""));
}

// ============================================================================
// Update (Non-Blocking)
// ============================================================================

void SensorManager::update()
{
    updateTemperatureSensor();
    updateWaterLevelSensor();
}

// ============================================================================
// Temperature Sensor State Machine (Non-Blocking)
// ============================================================================

void SensorManager::updateTemperatureSensor()
{
    unsigned long currentTime = millis();

    switch (tempState)
    {
    // --------------------------------------------------------------------
    // IDLE State: Wait for read interval
    // --------------------------------------------------------------------
    case TEMP_IDLE:
        if (currentTime - lastTempReadTime >= TEMP_SENSOR_READ_INTERVAL_MS)
        {
            // Time to request new temperature reading
            sensors.requestTemperatures();
            tempRequestTime = currentTime;
            tempState = TEMP_WAITING;

            // DEBUG_PRINTLN(F("[TEMP] Conversion requested"));
            // Temporarily disabled for clearer serial output
            // Will be re-enabled after Phase 6/7 validation
        }
        break;

    // --------------------------------------------------------------------
    // WAITING State: Wait for conversion to complete
    // --------------------------------------------------------------------
    case TEMP_WAITING:
        if (currentTime - tempRequestTime >= TEMP_SENSOR_CONVERSION_TIME_MS)
        {
            // Conversion complete, proceed to process
            tempState = TEMP_PROCESS;
        }
        break;

    // --------------------------------------------------------------------
    // PROCESS State: Read and validate temperature
    // --------------------------------------------------------------------
    case TEMP_PROCESS:
    {
        // Read temperature from sensor
        float temp = sensors.getTempCByIndex(0);

        // Validate reading
        if (validateTemperature(temp))
        {
            // Valid reading
            currentTemperature = temp;
            addToHistory(temp);

            // Reset error counter
            tempErrorCount = 0;

            // Increment valid counter
            tempValidCount++;

            // Check for sensor recovery
            if (tempValidCount >= SENSOR_RECOVERY_THRESHOLD)
            {
                if (!tempSensorOperational)
                {
                    DEBUG_PRINTLN(F("[TEMP] Sensor recovered"));
                }
                tempSensorOperational = true;
                tempValidCount = 0; // Reset counter
            }

#ifdef ENABLE_SERIAL_DEBUG
            // Periodic temperature output (every read)
            // Temporarily disabled for clearer serial output
            // Will be re-enabled after Phase 6/7 validation
            /*
            Serial.print(F("[TEMP] "));
            Serial.print(temp, 1);
            Serial.println(F(" °C"));
            */
#endif
        }
        else
        {
            // Invalid reading
            tempErrorCount++;
            tempValidCount = 0; // Reset valid counter

#ifdef ENABLE_SERIAL_DEBUG
            Serial.print(F("[TEMP] Invalid reading: "));
            Serial.print(temp, 1);
            Serial.print(F(" °C (error count: "));
            Serial.print(tempErrorCount);
            Serial.println(F(")"));
#endif

            // Check for sensor fault
            if (tempErrorCount >= SENSOR_ERROR_THRESHOLD)
            {
                if (tempSensorOperational)
                {
                    DEBUG_PRINTLN(F("[TEMP] SENSOR FAULT"));
                }
                tempSensorOperational = false;
            }
        }

        // Update last read time
        lastTempReadTime = currentTime;

        // Return to IDLE state
        tempState = TEMP_IDLE;
    }
    break;
    }
}

// ============================================================================
// Water Level Sensor Polling (Non-Blocking)
// ============================================================================

void SensorManager::updateWaterLevelSensor()
{
    unsigned long currentTime = millis();

    // Check if read interval elapsed
    if (currentTime - lastWaterLevelReadTime >= WATER_LEVEL_READ_INTERVAL_MS)
    {
        // Read water level pin (active-low: LOW = sufficient, HIGH = insufficient)
        bool pinState = digitalRead(PIN_WATER_LEVEL);
        bool sufficient = (pinState == LOW);

        // Update current status
        waterLevelSufficient = sufficient;

        if (!sufficient)
        {
            // Insufficient water detected

            // Start fault timer if first detection
            if (waterLevelFaultStartTime == 0)
            {
                waterLevelFaultStartTime = currentTime;
            }

            // Check fault duration
            unsigned long faultDuration = currentTime - waterLevelFaultStartTime;

            if (faultDuration >= WATER_LEVEL_FAULT_DURATION_MS)
            {
                if (!waterLevelFaultActive)
                {
                    DEBUG_PRINTLN(F("[WATER] FAULT: Insufficient water"));
                }
                waterLevelFaultActive = true;
            }

            // Reset recovery timer
            waterLevelRecoveryStartTime = 0;
        }
        else
        {
            // Sufficient water detected

            // Reset fault start timer
            waterLevelFaultStartTime = 0;

            // Handle fault recovery
            if (waterLevelFaultActive)
            {
                // Start recovery timer if first sufficient reading
                if (waterLevelRecoveryStartTime == 0)
                {
                    waterLevelRecoveryStartTime = currentTime;
                }

                // Check stabilization period
                unsigned long recoveryDuration = currentTime - waterLevelRecoveryStartTime;

                if (recoveryDuration >= WATER_LEVEL_STABILIZATION_MS)
                {
                    DEBUG_PRINTLN(F("[WATER] Fault cleared (stabilized)"));
                    waterLevelFaultActive = false;
                    waterLevelRecoveryStartTime = 0;
                }
            }
        }

        // Update last read time
        lastWaterLevelReadTime = currentTime;
    }
}

// ============================================================================
// Temperature Validation
// ============================================================================

bool SensorManager::validateTemperature(float temp)
{
    // Check for disconnected sensor (DallasTemperature returns -127.0)
    if (temp == DEVICE_DISCONNECTED_C)
    {
        return false;
    }

    // Check valid range
    if (temp < TEMP_VALID_MIN || temp > TEMP_VALID_MAX)
    {
        return false;
    }

    return true;
}

// ============================================================================
// Temperature History Management
// ============================================================================

void SensorManager::addToHistory(float temp)
{
    // Store in circular buffer
    temperatureHistory[historyIndex] = temp;

    // Increment index (wrap around)
    historyIndex = (historyIndex + 1) % TEMP_HISTORY_SIZE;

    // Increment count (up to buffer size)
    if (historyCount < TEMP_HISTORY_SIZE)
    {
        historyCount++;
    }
}

// ============================================================================
// Getter Methods
// ============================================================================

float SensorManager::getTemperature()
{
    return currentTemperature;
}

bool SensorManager::isTemperatureSensorOperational()
{
    return tempSensorOperational;
}

uint8_t SensorManager::getTemperatureSensorErrorCount()
{
    return tempErrorCount;
}

bool SensorManager::hasTemperatureSensorFault()
{
    return (tempErrorCount >= SENSOR_ERROR_THRESHOLD);
}

uint8_t SensorManager::getTemperatureHistoryCount() const
{
    return historyCount;
}

float SensorManager::getTemperatureHistoryValue(uint8_t index) const
{
    if (index >= historyCount)
    {
        return TEMP_INVALID_VALUE;
    }
    return temperatureHistory[index];
}

float SensorManager::getOldestTemperature() const
{
    if (historyCount == 0)
    {
        return TEMP_INVALID_VALUE;
    }
    
    // If buffer not full, oldest is at index 0
    if (historyCount < TEMP_HISTORY_SIZE)
    {
        return temperatureHistory[0];
    }
    
    // If buffer full, oldest is at current write index (circular buffer)
    return temperatureHistory[historyIndex];
}

float SensorManager::getNewestTemperature() const
{
    if (historyCount == 0)
    {
        return TEMP_INVALID_VALUE;
    }
    
    // Newest is always at (historyIndex - 1) wrapped around
    uint8_t newestIndex = (historyIndex == 0) ? (TEMP_HISTORY_SIZE - 1) : (historyIndex - 1);
    return temperatureHistory[newestIndex];
}

bool SensorManager::isWaterLevelSufficient()
{
    return waterLevelSufficient;
}

bool SensorManager::hasWaterLevelFault()
{
    return waterLevelFaultActive;
}

unsigned long SensorManager::getWaterLevelFaultDuration()
{
    if (waterLevelFaultActive && waterLevelFaultStartTime > 0)
    {
        return millis() - waterLevelFaultStartTime;
    }
    return 0;
}
