#ifndef PERSISTENCE_PAGES__SG_BINDS_HPP
#define PERSISTENCE_PAGES__SG_BINDS_HPP

#include "pico/stdlib.h"
#include "persistence/page_indexes.hpp"

namespace Persistence {
namespace Pages {

#define NUM_SG_BIND_SLOTS 11

struct SgBinds {
    static const int index = (int) PageIndexes::SG_BINDS;

    uint8_t configured;   // 1 = configured, 0xFF = unconfigured (flash default)
    struct Entry {
        uint8_t pin;          // GPIO pin number
        uint8_t buttonIndex;  // Index into button member array (0-19)
    } entries[NUM_SG_BIND_SLOTS];
    uint16_t pullModes;   // Bitmask: bit i = 1 means slot i uses pull-down (active-high)
                          // 0 (default) = pull-up (active-low, normal buttons)
};

// Button index definitions (matches ButtonSet field order)
// 0=a, 1=b, 2=x, 3=y, 4=z, 5=l, 6=r, 7=ls, 8=ms,
// 9=mx, 10=my, 11=start, 12=left, 13=right, 14=up, 15=down,
// 16=cLeft, 17=cRight, 18=cUp, 19=cDown
#define NUM_BUTTON_NAMES 20

// Slot index definitions (physical input identity)
#define SLOT_GREEN      0
#define SLOT_RED        1
#define SLOT_YELLOW     2
#define SLOT_BLUE       3
#define SLOT_ORANGE     4
#define SLOT_UP_STRUM   5
#define SLOT_DOWN_STRUM 6
#define SLOT_R_BUTTON   7
#define SLOT_TILT       8
#define SLOT_SELECT     9
#define SLOT_START      10

}
}

#endif
