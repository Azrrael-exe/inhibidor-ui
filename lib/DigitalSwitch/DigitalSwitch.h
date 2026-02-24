#ifndef DIGITAL_SWITCH_H
#define DIGITAL_SWITCH_H

#include <Arduino.h>

/**
 * @brief Generic digital switch class with debouncing and callbacks
 *
 * Manages a single digital input pin with software debouncing and
 * callback functions for state transitions.
 *
 * @note Uses millis() for non-blocking timing
 * @note Memory footprint: ~15 bytes per instance
 */
class DigitalSwitch {
public:
  /**
   * @brief Constructor
   * @param pin Pin number to monitor
   * @param debounceDelay Debounce delay in milliseconds (default: 50ms)
   */
  DigitalSwitch(uint8_t pin, unsigned int debounceDelay = 50);

  /**
   * @brief Initialize the switch pin
   * @param mode Pin mode (INPUT or INPUT_PULLUP, default: INPUT_PULLUP)
   * @note Call this in setup() before using the switch
   */
  void begin(uint8_t mode = INPUT_PULLUP);

  /**
   * @brief Update switch state and trigger callbacks
   * @note Call this repeatedly in loop()
   * @note Non-blocking, safe to call frequently
   */
  void update();

  /**
   * @brief Register callback for LOW to HIGH transition
   * @param callback Function pointer to call when switch turns on
   * @param context Optional context pointer passed to callback (default: nullptr)
   */
  void setOnTurnOn(void (*callback)(void*), void* context = nullptr);

  /**
   * @brief Register callback for HIGH to LOW transition
   * @param callback Function pointer to call when switch turns off
   * @param context Optional context pointer passed to callback (default: nullptr)
   */
  void setOnTurnOff(void (*callback)(void*), void* context = nullptr);

  /**
   * @brief Get current debounced state
   * @return true if HIGH, false if LOW
   */
  bool getState() const;

  /**
   * @brief Get raw state without debouncing
   * @return true if HIGH, false if LOW
   */
  bool getRawState() const;

private:
  uint8_t _pin;                    // Pin number
  uint8_t _lastStableState;        // Last stable debounced state
  uint8_t _currentReading;         // Current raw reading
  uint8_t _lastReading;            // Previous raw reading
  unsigned long _lastDebounceTime; // Last time reading changed
  unsigned int _debounceDelay;     // Debounce delay in milliseconds
  bool _firstRead;                 // Flag for first read

  void (*_onTurnOn)(void*);        // Callback for LOW->HIGH transition
  void* _onTurnOnContext;          // Context for onTurnOn callback
  void (*_onTurnOff)(void*);       // Callback for HIGH->LOW transition
  void* _onTurnOffContext;         // Context for onTurnOff callback
};

#endif
