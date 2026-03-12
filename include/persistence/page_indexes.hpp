#ifndef PERSISTENCE__PAGE_INDEXES_HPP
#define PERSISTENCE__PAGE_INDEXES_HPP

enum class PageIndexes : int {
    MASTER = 0,
    RUNTIME_REMAPPING = 1,
    WHAMMY_CALIBRATION = 2,
    SG_BINDS = 3
};

#endif