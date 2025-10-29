/**
 * @file Joystick.cpp
 * @brief Implementation of the Joystick controller library
 * 
 * This file contains the implementation of all Joystick class methods,
 * including initialization, calibration, position calculation, and
 * callback management.
 * 
 * @author Johannes
 * @version 1.0
 * @date 2024
 */

#include "Joystick.h"

/**
 * @brief Constructor - Initialize joystick object with pin assignments
 * 
 * Sets up default calibration values and initializes all member variables.
 * Does not configure hardware pins (call begin() for that).
 */

Joystick::Joystick(int pinX, int pinY, int pinSwitch) {
    _pinX = pinX;
    _pinY = pinY;
    _pinSwitch = pinSwitch;
    
    // Standardwerte
    _minVal = 1200;
    _maxVal = 4095;
    _centerVal = 2559;
    _deadzone = 300;
    
    _invertX = false;
    _invertY = false;
    
    _debugMode = false;
    
    _currentPosition = JOY_CENTER;
    _previousPosition = JOY_CENTER;
    _currentSwitch = false;
    _previousSwitch = false;
    
    _lastSwitchTime = 0;
    _lastPositionTime = 0;
    
    // Callbacks auf nullptr setzen
    _callbackLeft = nullptr;
    _callbackRight = nullptr;
    _callbackUp = nullptr;
    _callbackDown = nullptr;
    _callbackCenter = nullptr;
    _callbackSwitch = nullptr;
}

/**
 * @brief Initialize hardware pins for joystick
 * 
 * Configures analog input pins for X and Y axes, and digital input
 * with pull-up resistor for the button.
 */
void Joystick::begin() {
    pinMode(_pinX, INPUT);
    pinMode(_pinY, INPUT);
    pinMode(_pinSwitch, INPUT_PULLUP);
}

void Joystick::setThresholds(int minVal, int maxVal, int centerVal, int deadzone) {
    _minVal = minVal;
    _maxVal = maxVal;
    _centerVal = centerVal;
    _deadzone = deadzone;
}

void Joystick::setInvertX(bool invert) {
    _invertX = invert;
}

void Joystick::setInvertY(bool invert) {
    _invertY = invert;
}

/**
 * @brief Enable or disable debug output to serial console
 * 
 * When enabled, outputs raw joystick values, position changes, and
 * button events to Serial for calibration and troubleshooting.
 * 
 * @param enabled true to enable debug mode, false to disable
 */
void Joystick::setDebugMode(bool enabled) {
    _debugMode = enabled;
    if (_debugMode) {
        Serial.println("=== Joystick Debug Mode Activated ===");
    }
}

/**
 * @brief Update joystick state and trigger appropriate callbacks
 * 
 * This method should be called regularly in the main loop (recommended: every 10ms).
 * It reads the current joystick state, applies debouncing, calculates position,
 * and triggers registered callbacks when state changes occur.
 * 
 * Debug output (if enabled):
 * - Raw X/Y values every 500ms
 * - Position changes in real-time
 * - Button press events
 * - Delta values from center every 1000ms
 */

void Joystick::onLeft(void (*callback)()) {
    _callbackLeft = callback;
}

void Joystick::onRight(void (*callback)()) {
    _callbackRight = callback;
}

void Joystick::onUp(void (*callback)()) {
    _callbackUp = callback;
}

void Joystick::onDown(void (*callback)()) {
    _callbackDown = callback;
}

void Joystick::onCenter(void (*callback)()) {
    _callbackCenter = callback;
}

void Joystick::onSwitch(void (*callback)()) {
    _callbackSwitch = callback;
}

void Joystick::update() {
    unsigned long currentTime = millis();
    
    // Analoge Werte auslesen
    _currentX = analogRead(_pinX);
    _currentY = analogRead(_pinY);
    
    if (_debugMode) {
        static unsigned long lastDebugOutput = 0;
        if (currentTime - lastDebugOutput > 500) {  // Alle 500ms ausgeben
            Serial.print("Joystick Raw: X=");
            Serial.print(_currentX);
            Serial.print(" Y=");
            Serial.print(_currentY);
            Serial.print(" | Position: ");
            Serial.println(getPositionString(_currentPosition));
            lastDebugOutput = currentTime;
        }
    }
    
    // Switch-Status auslesen (invertiert wegen INPUT_PULLUP)
    bool switchPressed = (digitalRead(_pinSwitch) == LOW);
    
    // Switch mit Debouncing prüfen
    if (switchPressed != _previousSwitch) {
        if (currentTime - _lastSwitchTime > _debounceDelay) {
            if (switchPressed && !_previousSwitch) {
                // Switch wurde gedrückt
                if (_debugMode) {
                    Serial.println(">>> Joystick Switch gedrückt");
                }
                if (_callbackSwitch != nullptr) {
                    _callbackSwitch();
                }
            }
            _previousSwitch = switchPressed;
            _lastSwitchTime = currentTime;
        }
    }
    _currentSwitch = switchPressed;
    
    // Position berechnen
    JoystickPosition newPosition = calculatePosition();
    
    // Position-Änderung mit Debouncing prüfen
    if (newPosition != _previousPosition) {
        if (currentTime - _lastPositionTime > _debounceDelay) {
            // Callback für vorherige Position (wenn sie CENTER war und wir uns wegbewegen)
            // oder Callback für neue Position aufrufen
            
            if (_debugMode) {
                Serial.print(">>> Position geändert: ");
                Serial.print(getPositionString(_previousPosition));
                Serial.print(" -> ");
                Serial.println(getPositionString(newPosition));
            }
            
            switch (newPosition) {
                case JOY_LEFT:
                    if (_callbackLeft != nullptr) _callbackLeft();
                    break;
                case JOY_RIGHT:
                    if (_callbackRight != nullptr) _callbackRight();
                    break;
                case JOY_UP:
                    if (_callbackUp != nullptr) _callbackUp();
                    break;
                case JOY_DOWN:
                    if (_callbackDown != nullptr) _callbackDown();
                    break;
                case JOY_CENTER:
                    if (_callbackCenter != nullptr) _callbackCenter();
                    break;
            }
            
            _previousPosition = newPosition;
            _currentPosition = newPosition;
            _lastPositionTime = currentTime;
        }
    }
}

/**
 * @brief Calculate joystick position from current analog readings
 * 
 * Applies axis inversion if configured, calculates delta from center position,
 * checks deadzone, and determines which direction is dominant.
 * 
 * Algorithm:
 * 1. Apply axis inversion if configured
 * 2. Calculate distance from center (deltaX, deltaY)
 * 3. If both deltas within deadzone → CENTER
 * 4. Otherwise, determine dominant axis (larger absolute delta)
 * 5. Return direction based on sign of dominant delta
 * 
 * @return JoystickPosition Current calculated position
 */
JoystickPosition Joystick::calculatePosition() {
    int x = _currentX;
    int y = _currentY;
    
    // Invertierung anwenden
    if (_invertX) {
        x = _maxVal - (x - _minVal) + _minVal;
    }
    if (_invertY) {
        y = _maxVal - (y - _minVal) + _minVal;
    }
    
    // Berechne Abstand vom Zentrum
    int deltaX = x - _centerVal;
    int deltaY = y - _centerVal;
    
    if (_debugMode) {
        static unsigned long lastCalcDebug = 0;
        unsigned long currentTime = millis();
        if (currentTime - lastCalcDebug > 1000) {  // Alle 1 Sekunde
            Serial.print("Calculate: deltaX=");
            Serial.print(deltaX);
            Serial.print(" deltaY=");
            Serial.print(deltaY);
            Serial.print(" | abs(deltaX)=");
            Serial.print(abs(deltaX));
            Serial.print(" abs(deltaY)=");
            Serial.println(abs(deltaY));
            lastCalcDebug = currentTime;
        }
    }
    
    // Prüfe ob im Totbereich
    if (abs(deltaX) < _deadzone && abs(deltaY) < _deadzone) {
        return JOY_CENTER;
    }
    
    // Bestimme dominante Achse
    if (abs(deltaX) > abs(deltaY)) {
        // X-Achse ist dominant
        if (deltaX > 0) {
            return JOY_RIGHT;
        } else {
            return JOY_LEFT;
        }
    } else {
        // Y-Achse ist dominant
        if (deltaY > 0) {
            return JOY_UP;
        } else {
            return JOY_DOWN;
        }
    }
}

/**
 * @brief Get current calculated position
 * @return Current joystick position as enum value
 */
JoystickPosition Joystick::getPosition() {
    return _currentPosition;
}

/**
 * @brief Get raw X-axis analog value
 * @return Raw ADC reading (0-4095) from X-axis
 */
int Joystick::getRawX() {
    return _currentX;
}

/**
 * @brief Get raw Y-axis analog value
 * @return Raw ADC reading (0-4095) from Y-axis
 */
int Joystick::getRawY() {
    return _currentY;
}

/**
 * @brief Check if button is currently pressed
 * @return true if button is pressed, false if released
 */
bool Joystick::isSwitchPressed() {
    return _currentSwitch;
}

/**
 * @brief Convert position enum to human-readable string
 * 
 * Helper function for debug output and logging.
 * 
 * @param pos Position enum to convert
 * @return const char* String representation ("CENTER", "UP", etc.)
 */

const char* Joystick::getPositionString(JoystickPosition pos) {
    switch (pos) {
        case JOY_CENTER: return "CENTER";
        case JOY_UP: return "UP";
        case JOY_DOWN: return "DOWN";
        case JOY_LEFT: return "LEFT";
        case JOY_RIGHT: return "RIGHT";
        default: return "UNKNOWN";
    }
}