#ifndef GPIO_TO_BUTTON_SETS__SG_HPP
#define GPIO_TO_BUTTON_SETS__SG_HPP

#include <cstdint>
#include "gpio_to_button_sets/F1.hpp"
#include <cstdint> 
namespace GpioToButtonSets {
namespace SG {

using ButtonSet = GpioToButtonSets::F1::ButtonSet;

void initDefaultConversion();
ButtonSet defaultConversion();
uint8_t readWhammy();

// Whammy disable flag (set by configurator, stored in flash)
extern bool whammyDisabled;
void setWhammyDisabled(bool disabled);

// Bind management (for configurator)
void loadBinds();
uint8_t getBindPin(int slot);
uint8_t getBindButton(int slot);
void setBindButton(int slot, uint8_t buttonIndex);
void setBindPin(int slot, uint8_t pin);
uint8_t getBindPullMode(int slot);
void setBindPullMode(int slot, uint8_t pullDown);
void saveBinds();
void resetBinds();

}
}

#endif
