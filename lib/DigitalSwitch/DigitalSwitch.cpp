#include "DigitalSwitch.h"

DigitalSwitch::DigitalSwitch(uint8_t pin, unsigned int debounceDelay)
  : _pin(pin),
    _lastStableState(LOW),
    _currentReading(LOW),
    _lastReading(LOW),
    _lastDebounceTime(0),
    _debounceDelay(debounceDelay),
    _firstRead(true),
    _onTurnOn(nullptr),
    _onTurnOnContext(nullptr),
    _onTurnOff(nullptr),
    _onTurnOffContext(nullptr)
{
}

void DigitalSwitch::begin(uint8_t mode) {
  pinMode(_pin, mode);

  // Read initial state
  _currentReading = digitalRead(_pin);
  _lastReading = _currentReading;
  _lastStableState = _currentReading;
  _lastDebounceTime = millis();
  _firstRead = false;
}

void DigitalSwitch::update() {
  _currentReading = digitalRead(_pin);

  // If reading changed (noise or actual change), reset debounce timer
  if (_currentReading != _lastReading) {
    _lastDebounceTime = millis();
  }

  // If reading has been stable for debounce period
  if ((millis() - _lastDebounceTime) > _debounceDelay) {
    // If state has actually changed
    if (_currentReading != _lastStableState) {
      _lastStableState = _currentReading;

      // Trigger callbacks on transitions
      if (_currentReading == HIGH) {
        if (_onTurnOn) {
          _onTurnOn(_onTurnOnContext);
        }
      } else {
        if (_onTurnOff) {
          _onTurnOff(_onTurnOffContext);
        }
      }
    }
  }

  _lastReading = _currentReading;
}

void DigitalSwitch::setOnTurnOn(void (*callback)(void*), void* context) {
  _onTurnOn = callback;
  _onTurnOnContext = context;
}

void DigitalSwitch::setOnTurnOff(void (*callback)(void*), void* context) {
  _onTurnOff = callback;
  _onTurnOffContext = context;
}

void DigitalSwitch::sync() {
  uint8_t reading = digitalRead(_pin);
  _currentReading = reading;
  _lastReading = reading;
  _lastStableState = reading;
  _lastDebounceTime = millis();
}

bool DigitalSwitch::getState() const {
  return _lastStableState == HIGH;
}

bool DigitalSwitch::getRawState() const {
  return digitalRead(_pin) == HIGH;
}
