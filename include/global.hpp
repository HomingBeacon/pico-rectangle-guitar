#ifndef __GLOBAL_HPP
#define __GLOBAL_HPP

#include <stdio.h>
#include "hardware/gpio.h"

const int us = 125;

#define LED_PIN 25
#define EXT_LED_PIN 22
#define USB_POWER_PIN 24

// Write to both onboard and external LED
inline void led_put(bool on) {
    gpio_put(LED_PIN, on);
    gpio_put(EXT_LED_PIN, on);
}

// Initialize both LED pins as outputs
inline void led_init() {
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_init(EXT_LED_PIN);
    gpio_set_dir(EXT_LED_PIN, GPIO_OUT);
}

const uint8_t gcDataPin = 28;
const uint8_t rumblePin = 11;

// Set to false at runtime when rumblePin is used as an input (e.g. SG foot pedal)
extern bool rumbleEnabled;

#define USE_UART0 0

#endif