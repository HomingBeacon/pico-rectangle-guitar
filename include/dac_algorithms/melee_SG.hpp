#ifndef DAC_ALGORITHMS__MELEE_SG_HPP
#define DAC_ALGORITHMS__MELEE_SG_HPP

#include "communication_protocols/joybus/gcReport.hpp"

#include "gpio_to_button_sets/F1.hpp"

namespace DACAlgorithms {
namespace MeleeSG {

extern bool banParasolDashing;
extern bool banSlightSideB;

// Whammy calibration values (set by calibration mode, stored in flash)
extern uint8_t whammyHigh;  // whammy value at max lightshield
extern uint8_t whammyLow;   // whammy value at min lightshield
extern uint8_t whammyDeadzone; // 5% of range below which whammy is ignored

void loadCalibration();

GCReport getGCReport(GpioToButtonSets::F1::ButtonSet buttonSet, uint8_t whammyRaw);

}
}

#endif
