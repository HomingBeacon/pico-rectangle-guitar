#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include <vector>

#include "global.hpp"

#include "dac_algorithms/melee_F1.hpp"
#include "dac_algorithms/project_plus_F1.hpp"
#include "dac_algorithms/ultimate_F1.hpp"
#include "dac_algorithms/set_of_8_keys.hpp"
#include "dac_algorithms/wired_fight_pad_pro_default.hpp"
#include "dac_algorithms/xbox_360.hpp"

#include "gpio_to_button_sets/F1.hpp"
#ifdef SG_GUITAR
#include "gpio_to_button_sets/SG.hpp"
#endif

#include "usb_configurations/gcc_to_usb_adapter.hpp"
#include "usb_configurations/hid_with_triggers.hpp"
#include "usb_configurations/keyboard_8kro.hpp"
#include "usb_configurations/wired_fight_pad_pro.hpp"
#include "usb_configurations/xbox_360.hpp"

#include "communication_protocols/joybus.hpp"

#include "other/runtime_remapping_mode.hpp"

int main() {

    set_sys_clock_khz(1000*us, true);
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1); // LED on at boot to confirm firmware is running

    #ifndef SG_GUITAR
    gpio_init(USB_POWER_PIN);
    gpio_set_dir(USB_POWER_PIN, GPIO_IN);
    #endif

    #if USE_UART0
    // Initialise UART 0
    uart_init(uart0, 115200);
 
    // Set the GPIO pin mux to the UART - 0 is TX, 1 is RX
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);
    #endif

    #ifndef SG_GUITAR
    const uint8_t keyboardPin =
    #if USE_UART0
    3
    #else
    0
    #endif
    ;
    #endif

    #ifdef SG_GUITAR
    std::vector<uint8_t> modePins = { 22, 21, 16, 8, 7, 6, 5, 4, 3, 2 };
    #else
    std::vector<uint8_t> modePins = { 22, 21, 20, 16, 17, 14, 13, 7, 6, 5, 4, 2, keyboardPin }; // DO NOT USE PIN GP15
    #endif

    for (uint8_t modePin : modePins) {
        gpio_init(modePin);
        gpio_set_dir(modePin, GPIO_IN);
        gpio_pull_up(modePin);
    }

    // 21 - GP16 - BOOTSEL
    if (!gpio_get(16)) reset_usb_boot(0, 0);

    #ifndef SG_GUITAR
    // 22 - GP17 - Up : runtime remapping
    if (!gpio_get(17)) Other::enterRuntimeRemappingMode();
    #endif
    
    #ifndef SG_GUITAR
    gpio_init(gcDataPin);
    gpio_set_dir(gcDataPin, GPIO_IN);
    gpio_pull_up(gcDataPin);

    uint32_t origin = time_us_32();
    while ( time_us_32() - origin < 100'000 );
    while ( time_us_32() - origin < 500'000 ) {
        if (!gpio_get(gcDataPin)) goto stateLabel__forceJoybusEntry;
    }
    #endif

    /* Mode selection logic */

#ifdef SG_GUITAR

    // SG: No GP28 joybus detection — go straight to USB modes.
    goto sg_usb_modes;

    stateLabel__forceJoybusEntry:

    // Green (GP2) or Up Strum (GP7): P+
    if ((!gpio_get(7)) || (!gpio_get(2))) {
        CommunicationProtocols::Joybus::enterMode(gcDataPin, [](){
            GCReport report = DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
            uint8_t whammy = GpioToButtonSets::SG::readWhammy();
            if (whammy > report.analogR) report.analogR = whammy;
            return report;
        });
    }

    // Orange (GP6): Ultimate
    if (!gpio_get(6)) {
        CommunicationProtocols::Joybus::enterMode(gcDataPin, [](){
            GCReport report = DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
            uint8_t whammy = GpioToButtonSets::SG::readWhammy();
            if (whammy > report.analogR) report.analogR = whammy;
            return report;
        });
    }

    // Else: SG / Melee
    CommunicationProtocols::Joybus::enterMode(gcDataPin, [](){
        GCReport report = DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        return report;
    });

    sg_usb_modes:

    // 3 quick blinks to confirm firmware reached USB mode selection
    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PIN, 0);
        busy_wait_ms(150);
        gpio_put(LED_PIN, 1);
        busy_wait_ms(150);
    }

    // Red (GP3): Melee / XInput
    if (!gpio_get(3)) USBConfigurations::Xbox360::enterMode([](){
        GCReport report = DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::Xbox360::actuateReportFromGCState(report);
    });

    // Down Strum (GP8): Xbox360 / XInput
    if (!gpio_get(8)) USBConfigurations::Xbox360::enterMode([](){
        DACAlgorithms::Xbox360::actuateXbox360Report(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > USBConfigurations::Xbox360::xInputReport.rightTrigger)
            USBConfigurations::Xbox360::xInputReport.rightTrigger = whammy;
    });

    // Select (GP21): Melee / HID
    if (!gpio_get(21)) USBConfigurations::HidWithTriggers::enterMode([](){
        GCReport report = DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::HidWithTriggers::actuateReportFromGCState(report);
    });

    // Start (GP22): Ult / HID
    if (!gpio_get(22)) USBConfigurations::HidWithTriggers::enterMode([](){
        GCReport report = DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::HidWithTriggers::actuateReportFromGCState(report);
    });

    // Green (GP2): P+ / WFPP (Switch)
    if (!gpio_get(2)) USBConfigurations::WiredFightPadPro::enterMode([](){
        GCReport report = DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::WiredFightPadPro::actuateReportFromGCState(report);
    });

    // Up Strum (GP7): P+ / GCC Adapter
    if (!gpio_get(7)) USBConfigurations::GccToUsbAdapter::enterMode([](){
        GCReport report = DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(report);
    });

    // Orange (GP6): Ultimate / GCC Adapter
    if (!gpio_get(6)) USBConfigurations::GccToUsbAdapter::enterMode([](){
        GCReport report = DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(report);
    });

    // Blue (GP5): Melee / WFPP (Switch)
    if (!gpio_get(5)) USBConfigurations::WiredFightPadPro::enterMode([](){
        GCReport report = DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::WiredFightPadPro::actuateReportFromGCState(report);
    });

    // Yellow (GP4): WFPP Default / WFPP (Switch)
    if (!gpio_get(4)) USBConfigurations::WiredFightPadPro::enterMode([](){
        DACAlgorithms::WiredFightPadProDefault::actuateWFPPReport(GpioToButtonSets::SG::defaultConversion());
    });

    // Default: Melee / GCC Adapter
    USBConfigurations::GccToUsbAdapter::enterMode(
        [](){
            GCReport report = DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
            uint8_t whammy = GpioToButtonSets::SG::readWhammy();
            if (whammy > report.analogR) report.analogR = whammy;
            USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(report);
        },
        [](){
            GCReport report = DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
            uint8_t whammy = GpioToButtonSets::SG::readWhammy();
            if (whammy > report.analogR) report.analogR = whammy;
            USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(report);
        }
    );

#else

    // Not plugged through USB =>  Joybus
    if (!gpio_get(USB_POWER_PIN)) {
        stateLabel__forceJoybusEntry:

        if ((!gpio_get(7)) || (!gpio_get(2))) { // 10-GP7 OR 4-GP2 : F1 / P+
            CommunicationProtocols::Joybus::enterMode(gcDataPin, [](){
                return DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::F1::defaultConversion());
            });
        }

        if (!gpio_get(6)) { // 9-GP6 : F1 / ultimate
            CommunicationProtocols::Joybus::enterMode(gcDataPin, [](){
                return DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::F1::defaultConversion());
            });
        }

        // Else: F1 / Melee
        CommunicationProtocols::Joybus::enterMode(gcDataPin, [](){ return DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::F1::defaultConversion()); });
    }

    // Else:

    // 17 - GP13 - CLeft - Melee / XInput
    if (!gpio_get(13)) USBConfigurations::Xbox360::enterMode([](){
        USBConfigurations::Xbox360::actuateReportFromGCState(DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));
    });

    // 19 - GP14 - A - Xbox360/Xbox360 (aka XInput)
    if (!gpio_get(14)) USBConfigurations::Xbox360::enterMode([](){
        DACAlgorithms::Xbox360::actuateXbox360Report(GpioToButtonSets::F1::defaultConversion());
    });

    // 27 - GP21 - X - Melee / HID
    if (!gpio_get(21)) USBConfigurations::HidWithTriggers::enterMode([](){
        USBConfigurations::HidWithTriggers::actuateReportFromGCState(DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));
    });

    // 29 - GP22 - Y - Ult / HID
    if (!gpio_get(22)) USBConfigurations::HidWithTriggers::enterMode([](){
        USBConfigurations::HidWithTriggers::actuateReportFromGCState(DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));
    });

    // 26 - GP20 - LS - P+ / HID
    if (!gpio_get(20)) USBConfigurations::HidWithTriggers::enterMode([](){
        USBConfigurations::HidWithTriggers::actuateReportFromGCState(DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));
    });

    // 4 - GP2 - Right : F1 / P+ / WFPP
    if (!gpio_get(2)) USBConfigurations::WiredFightPadPro::enterMode([](){
        USBConfigurations::WiredFightPadPro::actuateReportFromGCState(DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));
    });

    // 10 - GP7 - MY : F1 / P+ / adapter
    if (!gpio_get(7)) USBConfigurations::GccToUsbAdapter::enterMode([](){
        USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));
    });

    // 9 - GP6 - MX : F1 / ultimate / adapter
    if (!gpio_get(6)) USBConfigurations::GccToUsbAdapter::enterMode([](){
        USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));
    });

    // 7 - GP5 - L: F1 / melee / wired_fight_pad_pro
    if (!gpio_get(5)) USBConfigurations::WiredFightPadPro::enterMode([](){
        USBConfigurations::WiredFightPadPro::actuateReportFromGCState(DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));
    });

    // 6 - GP4 - Left: F1 / wired_fight_pad_pro_default / wired_fight_pad_pro
    if (!gpio_get(4)) USBConfigurations::WiredFightPadPro::enterMode([](){
        DACAlgorithms::WiredFightPadProDefault::actuateWFPPReport(GpioToButtonSets::F1::defaultConversion());
    });

    // 0 - 0 - Start: F1 / 8 keys set / 8KRO keyboard
    if (!gpio_get(keyboardPin)) USBConfigurations::Keyboard8KRO::enterMode([](){
        DACAlgorithms::SetOf8Keys::actuate8KeysReport(GpioToButtonSets::F1::defaultConversion());
    });

    // Default: F1 / melee / adapter
    USBConfigurations::GccToUsbAdapter::enterMode(
        [](){USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(DACAlgorithms::MeleeF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));},
        [](){USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::F1::defaultConversion()));}
        );

#endif
}