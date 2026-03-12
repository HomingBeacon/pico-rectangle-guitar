#ifndef PERSISTENCE_PAGES__WHAMMY_CALIBRATION_HPP
#define PERSISTENCE_PAGES__WHAMMY_CALIBRATION_HPP

#include "pico/stdlib.h"
#include "persistence/page_indexes.hpp"

namespace Persistence {
namespace Pages {

struct WhammyCalibration {
    static const int index = (int) PageIndexes::WHAMMY_CALIBRATION;

    uint8_t configured;   // 1 = calibrated, 0xFF = uncalibrated (flash default)
    uint8_t whammyHigh;   // ADC value at max lightshield (first A press)
    uint8_t whammyLow;    // ADC value at min lightshield (second A press)
};

}
}

#endif
