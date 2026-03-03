#include "gpio_to_button_sets/SG.hpp"

#include "pico/stdlib.h"
#include "hardware/adc.h"

#include "global.hpp"

namespace GpioToButtonSets {
namespace SG {

struct PinMapping {
    uint8_t pin;
    bool ButtonSet::* ptrToMember;
};

const PinMapping pinMappings[] = {
    { 2, &ButtonSet::a },       // Green fret
    { 3, &ButtonSet::left },    // Red fret
    { 4, &ButtonSet::down },    // Yellow fret
    { 5, &ButtonSet::right },   // Blue fret
    { 6, &ButtonSet::b },       // Orange fret
    { 7, &ButtonSet::ls },      // Up strum
    { 8, &ButtonSet::up },      // Down strum
    { 15, &ButtonSet::r },      // Button under whammy
    { 17, &ButtonSet::z },      // Tilt
    { 21, &ButtonSet::mx },     // Select
    { 22, &ButtonSet::start },  // Start
};

bool init = false;

void initDefaultConversion() {
    for (PinMapping pinMapping : pinMappings) {
        gpio_init(pinMapping.pin);
        gpio_set_dir(pinMapping.pin, GPIO_IN);
        gpio_pull_up(pinMapping.pin);
    }

    // Initialize ADC for whammy on GP26 (ADC0)
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);

    init = true;
}

ButtonSet defaultConversion() {

    if (!init) initDefaultConversion();

    ButtonSet sgButtonSet = {};

    uint32_t inputSnapshot = sio_hw->gpio_in;

    for (PinMapping pinMapping : pinMappings) {
        sgButtonSet.*(pinMapping.ptrToMember) = !(inputSnapshot & (1 << (pinMapping.pin)));
    }

    return sgButtonSet;
}

uint8_t readWhammy() {
    adc_select_input(0); // ADC0 = GP26
    uint16_t raw = adc_read(); // 12-bit: 0-4095
    uint8_t scaled = (uint8_t)(raw >> 4); // Scale to 0-255
    // Dead zone to suppress potentiometer noise at rest
    return scaled < 10 ? 0 : scaled;
}

}
}
