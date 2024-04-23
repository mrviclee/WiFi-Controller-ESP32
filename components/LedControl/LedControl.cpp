#include "LedControl.hpp"

LedControl::LedControl()
{
    _led_pin_number = GPIO_NUM_2;
    gpio_set_direction(_led_pin_number, GPIO_MODE_OUTPUT);
}

LedControl::LedControl(gpio_num_t led_pin_number)
{
    // TODO: validate the led_pin_number
    _led_pin_number = led_pin_number;
    gpio_set_direction(_led_pin_number, GPIO_MODE_OUTPUT);
}

void LedControl::TurnOn()
{
    ESP_LOGI("LedControl", "Turn on Led Pin: %d", _led_pin_number);
    gpio_set_level(_led_pin_number, LED_ON);
}

void LedControl::TurnOff()
{
    ESP_LOGI("LedControl", "Turn off Led Pin: %d", _led_pin_number);
    gpio_set_level(_led_pin_number, LED_OFF);
}