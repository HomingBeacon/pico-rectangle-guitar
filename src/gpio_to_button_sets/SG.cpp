#include "gpio_to_button_sets/SG.hpp"

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <cstdint>
#include "global.hpp"
#include "persistence/functions.hpp"
#include "persistence/pages/sg_binds.hpp"

namespace GpioToButtonSets {
namespace SG {

struct PinMapping {
    uint8_t pin;
    bool ButtonSet::* ptrToMember;
    bool pullDown;  // true = pull-down (active-high), false = pull-up (active-low)
};

// Button index to member pointer lookup table
static bool ButtonSet::* const buttonMembers[NUM_BUTTON_NAMES] = {
    &ButtonSet::a, &ButtonSet::b, &ButtonSet::x, &ButtonSet::y, &ButtonSet::z,
    &ButtonSet::l, &ButtonSet::r, &ButtonSet::ls, &ButtonSet::ms,
    &ButtonSet::mx, &ButtonSet::my, &ButtonSet::start,
    &ButtonSet::left, &ButtonSet::right, &ButtonSet::up, &ButtonSet::down,
    &ButtonSet::cLeft, &ButtonSet::cRight, &ButtonSet::cUp, &ButtonSet::cDown
};

// Default (compiled-in) pin mappings
static const PinMapping defaultPinMappings[NUM_SG_BIND_SLOTS] = {
    { 2, &ButtonSet::a },       // Green fret  -> A
    { 3, &ButtonSet::up },      // Red fret    -> Left stick up
    { 4, &ButtonSet::down },    // Yellow fret -> Left stick down
    { 5, &ButtonSet::b },       // Blue fret   -> B
    { 6, &ButtonSet::z },       // Orange fret -> Z
    { 7, &ButtonSet::left },    // Up strum    -> Left stick left
    { 8, &ButtonSet::right },   // Down strum  -> Left stick right
    { 15, &ButtonSet::r },      // Button under whammy -> R
    { 17, &ButtonSet::x },      // Tilt        -> X
    { 21, &ButtonSet::mx },     // Select      -> ModX
    { 22, &ButtonSet::start },  // Start
    { 6, &ButtonSet::z },        // Foot pedal -> Z
};

// Default button indices matching defaultPinMappings order
static const uint8_t defaultButtonIndices[NUM_SG_BIND_SLOTS] = {
    0, 14, 15, 1, 4, 12, 13, 6, 2, 9, 11, 4
};

// Active (mutable) pin mappings
static PinMapping activePinMappings[NUM_SG_BIND_SLOTS];
static uint8_t activeButtonIndices[NUM_SG_BIND_SLOTS];
static bool bindsLoaded = false;

void loadBinds() {
    const auto* page = Persistence::read<Persistence::Pages::SgBinds>();
    if (page->configured == 1) {
        for (int i = 0; i < NUM_SG_BIND_SLOTS; i++) {
            uint8_t bi = page->entries[i].buttonIndex;
            if (bi >= NUM_BUTTON_NAMES) bi = defaultButtonIndices[i];
            activePinMappings[i].pin = page->entries[i].pin;
            activePinMappings[i].ptrToMember = buttonMembers[bi];
            activePinMappings[i].pullDown = (page->pullModes >> i) & 1;
            activeButtonIndices[i] = bi;
        }
    } else {
        for (int i = 0; i < NUM_SG_BIND_SLOTS; i++) {
            activePinMappings[i] = defaultPinMappings[i];
            activeButtonIndices[i] = defaultButtonIndices[i];
        }
    }
    bindsLoaded = true;
}

uint8_t getBindPin(int slot) {
    if (slot < 0 || slot >= NUM_SG_BIND_SLOTS) return 0;
    if (!bindsLoaded) loadBinds();
    return activePinMappings[slot].pin;
}

uint8_t getBindButton(int slot) {
    if (slot < 0 || slot >= NUM_SG_BIND_SLOTS) return 0;
    if (!bindsLoaded) loadBinds();
    return activeButtonIndices[slot];
}

void setBindButton(int slot, uint8_t buttonIndex) {
    if (slot < 0 || slot >= NUM_SG_BIND_SLOTS) return;
    if (buttonIndex >= NUM_BUTTON_NAMES) return;
    if (!bindsLoaded) loadBinds();
    activeButtonIndices[slot] = buttonIndex;
    activePinMappings[slot].ptrToMember = buttonMembers[buttonIndex];
}

void setBindPin(int slot, uint8_t pin) {
    if (slot < 0 || slot >= NUM_SG_BIND_SLOTS) return;
    if (pin > 28) return;
    if (!bindsLoaded) loadBinds();
    activePinMappings[slot].pin = pin;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    if (activePinMappings[slot].pullDown) {
        gpio_pull_down(pin);
    } else {
        gpio_pull_up(pin);
    }
}

uint8_t getBindPullMode(int slot) {
    if (slot < 0 || slot >= NUM_SG_BIND_SLOTS) return 0;
    if (!bindsLoaded) loadBinds();
    return activePinMappings[slot].pullDown ? 1 : 0;
}

void setBindPullMode(int slot, uint8_t pullDown) {
    if (slot < 0 || slot >= NUM_SG_BIND_SLOTS) return;
    if (!bindsLoaded) loadBinds();
    activePinMappings[slot].pullDown = (pullDown != 0);
    // Re-init pin with new pull mode
    uint8_t pin = activePinMappings[slot].pin;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    if (activePinMappings[slot].pullDown) {
        gpio_pull_down(pin);
    } else {
        gpio_pull_up(pin);
    }
}

void saveBinds() {
    Persistence::Pages::SgBinds binds = {};
    binds.configured = 1;
    uint16_t pullModes = 0;
    for (int i = 0; i < NUM_SG_BIND_SLOTS; i++) {
        binds.entries[i].pin = activePinMappings[i].pin;
        binds.entries[i].buttonIndex = activeButtonIndices[i];
        if (activePinMappings[i].pullDown) pullModes |= (1 << i);
    }
    binds.pullModes = pullModes;
    Persistence::commit(binds);
}

void resetBinds() {
    for (int i = 0; i < NUM_SG_BIND_SLOTS; i++) {
        activePinMappings[i] = defaultPinMappings[i];
        activeButtonIndices[i] = defaultButtonIndices[i];
    }
    // Write unconfigured marker to flash
    Persistence::Pages::SgBinds binds = {};
    binds.configured = 0xFF;
    Persistence::commit(binds);
}

bool whammyDisabled = false;

void setWhammyDisabled(bool disabled) {
    whammyDisabled = disabled;
}

bool init = false;

void initDefaultConversion() {
    if (!bindsLoaded) loadBinds();

    for (int i = 0; i < NUM_SG_BIND_SLOTS; i++) {
        gpio_init(activePinMappings[i].pin);
        gpio_set_dir(activePinMappings[i].pin, GPIO_IN);
        if (activePinMappings[i].pullDown) {
            gpio_pull_down(activePinMappings[i].pin);
        } else {
            gpio_pull_up(activePinMappings[i].pin);
        }
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

    for (int i = 0; i < NUM_SG_BIND_SLOTS; i++) {
        bool pinHigh = inputSnapshot & (1 << (activePinMappings[i].pin));
        // Pull-up (default): pressed = low. Pull-down: pressed = high.
        sgButtonSet.*(activePinMappings[i].ptrToMember) = activePinMappings[i].pullDown ? pinHigh : !pinHigh;
    }

    return sgButtonSet;
}

uint8_t readWhammy() {
    if (whammyDisabled) return 0;
    adc_select_input(0); // ADC0 = GP26
    uint16_t raw = adc_read(); // 12-bit: 0-4095
    uint8_t scaled = (uint8_t)(raw >> 4); // Scale to 0-255
    // Invert: pot is wired so rest position reads high, pressed reads low.
    // Calibration deadzone in whammyToLightshield handles noise at rest.
    return 255 - scaled;
}

}
}
