#include "dac_algorithms/melee_SG.hpp"
#include "communication_protocols/joybus.hpp"
#include "persistence/functions.hpp"
#include "persistence/pages/whammy_calibration.hpp"
#include "gpio_to_button_sets/SG.hpp"

namespace DACAlgorithms {
namespace MeleeSG {

#define coord(x) ((uint8_t)(128. + 80.*x + 0.5))
#define oppositeCoord(x) -((uint8_t)x)

bool banParasolDashing = false;
bool banSlightSideB = false;

// Whammy calibration defaults (uncalibrated: full 0-255 range)
uint8_t whammyHigh = 255;
uint8_t whammyLow = 0;
uint8_t whammyDeadzone = 13; // ~5% of 255

void loadCalibration() {
    const auto* page = Persistence::read<Persistence::Pages::WhammyCalibration>();
    if (page->configured == 1) {
        whammyHigh = page->whammyHigh;
        whammyLow = page->whammyLow;
        // 5% of calibrated range
        uint8_t range = (whammyHigh > whammyLow)
            ? (whammyHigh - whammyLow)
            : (whammyLow - whammyHigh);
        whammyDeadzone = (range * 5) / 100;
        if (whammyDeadzone < 1) whammyDeadzone = 1;
    }
    // Load whammy disabled flag (0xFF = flash default = enabled)
    GpioToButtonSets::SG::setWhammyDisabled(page->disabled == 1);
}

// Maps raw whammy value to full 0-255 analog range using calibration
static uint8_t whammyToAnalog(uint8_t raw) {
    // Determine direction: high might be > or < low depending on pot wiring
    bool inverted = whammyLow > whammyHigh;
    uint8_t lo = inverted ? whammyHigh : whammyLow;
    uint8_t hi = inverted ? whammyLow : whammyHigh;

    if (inverted) {
        // Inverted: "rest" is near hi, active toward lo
        if (raw >= hi - whammyDeadzone) return 0;
        if (raw <= lo) return 255;
        uint8_t active = (hi - whammyDeadzone) - lo;
        if (active == 0) return 0;
        return (uint8_t)(((uint16_t)((hi - whammyDeadzone) - raw) * 255) / active);
    } else {
        // Normal: "rest" is near lo, active toward hi
        uint8_t dzThreshold = lo + whammyDeadzone;
        if (raw <= dzThreshold) return 0;
        if (raw >= hi) return 255;
        uint8_t active = hi - dzThreshold;
        if (active == 0) return 0;
        return (uint8_t)(((uint16_t)(raw - dzThreshold) * 255) / active);
    }
}

// 2 IP declarations
bool left_wasPressed = false;
bool right_wasPressed = false;
bool up_wasPressed = false;
bool down_wasPressed = false;

bool left_outlawUntilRelease = false;
bool right_outlawUntilRelease = false;
bool up_outlawUntilRelease = false;
bool down_outlawUntilRelease = false;

struct Coords {
    uint8_t x;
    uint8_t y;
};

Coords coords(float xFloat, float yFloat) {
    Coords r;
    r.x = coord(xFloat);
    r.y = coord(yFloat);
    return r;
}


GCReport getGCReport(GpioToButtonSets::F1::ButtonSet buttonSet, uint8_t whammyRaw) {

    GpioToButtonSets::F1::ButtonSet bs = buttonSet; // Alterable copy

    GCReport gcReport = defaultGcReport;

    /* 2IP No reactivation */

    if (left_wasPressed && bs.left && bs.right && !right_wasPressed) left_outlawUntilRelease=true;
    if (right_wasPressed && bs.left && bs.right && !left_wasPressed) right_outlawUntilRelease=true;
    if (up_wasPressed && bs.up && bs.down && !down_wasPressed) up_outlawUntilRelease=true;
    if (down_wasPressed && bs.up && bs.down && !up_wasPressed) down_outlawUntilRelease=true;

    if (!bs.left) left_outlawUntilRelease=false;
    if (!bs.right) right_outlawUntilRelease=false;
    if (!bs.up) up_outlawUntilRelease=false;
    if (!bs.down) down_outlawUntilRelease=false;

    left_wasPressed = bs.left;
    right_wasPressed = bs.right;
    up_wasPressed = bs.up;
    down_wasPressed = bs.down;

    if (left_outlawUntilRelease) bs.left=false;
    if (right_outlawUntilRelease) bs.right=false;
    if (up_outlawUntilRelease) bs.up=false;
    if (down_outlawUntilRelease) bs.down=false;

    /* Stick */

    bool vertical = bs.up || bs.down;
    bool readUp = bs.up;

    bool horizontal = bs.left || bs.right;
    bool readRight = bs.right;

    Coords xy;

    if (vertical && horizontal) {
        if (bs.l || bs.r) {
            if (bs.mx == bs.my) xy = coords(0.7, readUp ? 0.7 : 0.6875);
            else if (bs.mx) xy = coords(0.6375, 0.375);
            else xy = (banParasolDashing && readUp) ? coords(0.475, 0.875) : coords(0.5, 0.85);
        }
        else if (bs.b && (bs.mx != bs.my)) {
            if (bs.mx) {
                if (bs.cDown) xy = coords(0.9125, 0.45);
                else if (bs.cLeft) xy = coords(0.85, 0.525);
                else if (bs.cUp) xy = coords(0.7375, 0.5375);
                else if (bs.cRight) xy = coords(0.6375, 0.5375);
                else xy = coords(0.9125, 0.3875);
            }
            else {
                if (bs.cDown) xy = coords(0.45, 0.875);
                else if (bs.cLeft) xy = coords(0.525, 0.85);
                else if (bs.cUp) xy = coords(0.5875, 0.8);
                else if (bs.cRight) xy = coords(0.5875, 0.7125);
                else xy = coords(0.3875, 0.9125);
            }
        }
        else if (bs.mx != bs.my) {
            if (bs.mx) {
                if (bs.cDown) xy = coords(0.7, 0.3625);
                else if (bs.cLeft) xy = coords(0.7875, 0.4875);
                else if (bs.cUp) xy = coords(0.7, 0.5125);
                else if (bs.cRight) xy = coords(0.6125, 0.525);
                else xy = coords(0.7375, 0.3125);
            }
            else {
                if (bs.cDown) xy = coords(0.3625, 0.7);
                else if (bs.cLeft) xy = coords(0.4875, 0.7875);
                else if (bs.cUp) xy = coords(0.5125, 0.7);
                else if (bs.cRight) xy = coords(0.6375, 0.7625);
                else xy = coords(0.3125, 0.7375);
            }
        }
        else xy = coords(0.7,0.7);
    }
    else if (horizontal) {
        if (bs.mx == bs.my) xy = coords(1.0, 0.0);
        else if (bs.mx) xy =  (buttonSet.left && buttonSet.right) ? coords(1.0, 0.0) : coords(0.6625, 0.0);
        else xy = ((banSlightSideB && bs.b) || buttonSet.left && buttonSet.right) ? coords(1.0, 0.0) : coords(0.3375, 0.0);
        // Read the original rectangleInput to bypass SOCD
    }
    else if (vertical) {
        if (bs.mx == bs.my) xy = coords(0.0, 1.0);
        else if (bs.mx) xy=coords(0.0, 0.5375);
        else xy = coords(0.0, 0.7375);
    }
    else {
        xy = coords(0.0, 0.0);
    }

    if (horizontal && !readRight) xy.x = oppositeCoord(xy.x);
    if (vertical && !readUp) xy.y = oppositeCoord(xy.y);

    gcReport.xStick = xy.x;
    gcReport.yStick = xy.y;

    /* C-Stick */

    bool cVertical = bs.cUp != bs.cDown;
    bool cHorizontal = bs.cLeft != bs.cRight;

    Coords cxy;

    if (bs.mx && bs.my) cxy = coords(0.0, 0.0);
    else if (cVertical && cHorizontal) cxy = coords(0.525, 0.85);
    else if (cHorizontal) cxy = bs.mx ? coords(0.8375, readUp ? 0.3125 : -0.3125) : coords(1.0, 0.0);
    else if (cVertical) cxy = coords(0.0, 1.0);
    else cxy = coords(0.0, 0.0);

    if (cHorizontal && bs.cLeft) cxy.x = oppositeCoord(cxy.x);
    if (cVertical && bs.cDown) cxy.y = oppositeCoord(cxy.y);

    gcReport.cxStick = cxy.x;
    gcReport.cyStick = cxy.y;

    /* Dpad */
    if (bs.mx && bs.my) {
        gcReport.dDown = bs.cDown;
        gcReport.dLeft = bs.cLeft;
        gcReport.dUp = bs.cUp;
        gcReport.dRight = bs.cRight;
    }

    /* Triggers */
    // L button (orange fret) = hard press
    gcReport.analogL = bs.l ? 140 : 0;
    // R button (under whammy) = hard press; whammy bar = full analog R
    uint8_t whammyAnalog = whammyToAnalog(whammyRaw);
    gcReport.analogR = bs.r ? 140 : whammyAnalog;

    /* Buttons */
    gcReport.a = bs.a;
    gcReport.b = bs.b;
    gcReport.x = bs.x;
    gcReport.y = bs.y;
    gcReport.z = bs.z;
    gcReport.l = bs.l;
    gcReport.r = bs.r;
    gcReport.start = bs.start;

    return gcReport;
}

}
}
