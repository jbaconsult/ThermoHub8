#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <Arduino.h>

enum JoystickPosition {
    JOY_CENTER,
    JOY_UP,
    JOY_DOWN,
    JOY_LEFT,
    JOY_RIGHT
};

class Joystick {
public:
    // Konstruktor
    Joystick(int pinX, int pinY, int pinSwitch);
    
    // Initialisierung
    void begin();
    
    // Konfiguration
    void setThresholds(int minVal, int maxVal, int centerVal, int deadzone);
    void setInvertX(bool invert);
    void setInvertY(bool invert);
    void setDebugMode(bool enabled);  // Debug-Modus aktivieren/deaktivieren
    
    // Callback-Funktionen setzen
    void onLeft(void (*callback)());
    void onRight(void (*callback)());
    void onUp(void (*callback)());
    void onDown(void (*callback)());
    void onCenter(void (*callback)());
    void onSwitch(void (*callback)());
    
    // Update-Funktion (muss regelmäßig aufgerufen werden)
    void update();
    
    // Aktuelle Position abfragen
    JoystickPosition getPosition();
    int getRawX();
    int getRawY();
    bool isSwitchPressed();

private:
    // Pin-Definitionen
    int _pinX;
    int _pinY;
    int _pinSwitch;
    
    // Schwellwerte
    int _minVal;
    int _maxVal;
    int _centerVal;
    int _deadzone;
    
    // Invertierung
    bool _invertX;
    bool _invertY;
    
    // Debug-Modus
    bool _debugMode;
    
    // Aktuelle Werte
    int _currentX;
    int _currentY;
    bool _currentSwitch;
    JoystickPosition _currentPosition;
    
    // Vorherige Werte (für Änderungserkennung)
    JoystickPosition _previousPosition;
    bool _previousSwitch;
    
    // Callback-Funktionen
    void (*_callbackLeft)();
    void (*_callbackRight)();
    void (*_callbackUp)();
    void (*_callbackDown)();
    void (*_callbackCenter)();
    void (*_callbackSwitch)();
    
    // Debouncing
    unsigned long _lastSwitchTime;
    unsigned long _lastPositionTime;
    const unsigned long _debounceDelay = 50;
    
    // Hilfsfunktionen
    JoystickPosition calculatePosition();
    const char* getPositionString(JoystickPosition pos);
};

#endif // JOYSTICK_H