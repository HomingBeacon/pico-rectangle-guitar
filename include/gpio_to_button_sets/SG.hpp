#ifndef GPIO_TO_BUTTON_SETS__SG_HPP
#define GPIO_TO_BUTTON_SETS__SG_HPP

#include "gpio_to_button_sets/F1.hpp"
#include <cstdint> 
namespace GpioToButtonSets {
namespace SG {

using ButtonSet = GpioToButtonSets::F1::ButtonSet;

ButtonSet defaultConversion();
uint8_t readWhammy();

}
}

#endif
