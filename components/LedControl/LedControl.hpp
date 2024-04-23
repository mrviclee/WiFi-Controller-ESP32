#ifndef LEDCONTROL_HPP
#define LEDCONTROL_HPP
#include <driver/gpio.h>
#include <esp_log.h>

#ifndef LED_ON
#define LED_ON 1
#endif

#ifndef LED_OFF
#define LED_OFF 0
#endif

class LedControl {
    public:
    /// @brief Default constructor sets the led pin to 2, which is the led on the esp32 board
    LedControl();

    /// @brief Initialize the class with a specific GPIO pin
    /// @param led_pin_number GPIO pin that is connected to the LED that you would like to control
    LedControl(gpio_num_t led_pin_number);

    //TODO: Desctructor
    // ~LedControl();

    void TurnOn();
    void TurnOff();

    private:
    gpio_num_t _led_pin_number;
};

#endif