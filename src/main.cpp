#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "hardware/structs/ioqspi.h"
#include "hardware/sync.h"
#include <vector>

#include "global.hpp"

#include "dac_algorithms/melee_F1.hpp"
#ifdef SG_GUITAR
#include "dac_algorithms/melee_SG.hpp"
#include "hardware/watchdog.h"
#endif
#include "dac_algorithms/project_plus_F1.hpp"
#include "dac_algorithms/ultimate_F1.hpp"
#include "dac_algorithms/set_of_8_keys.hpp"
#include "dac_algorithms/wired_fight_pad_pro_default.hpp"
#include "dac_algorithms/xbox_360.hpp"

#include "gpio_to_button_sets/F1.hpp"
#ifdef SG_GUITAR
#include "gpio_to_button_sets/SG.hpp"
#include "persistence/functions.hpp"
#include "persistence/pages/whammy_calibration.hpp"
#include "persistence/pages/sg_binds.hpp"
#include "usb_configurations/configurator.hpp"
#endif

#include "usb_configurations/gcc_to_usb_adapter.hpp"
#include "usb_configurations/hid_with_triggers.hpp"
#include "usb_configurations/keyboard_8kro.hpp"
#include "usb_configurations/wired_fight_pad_pro.hpp"
#include "usb_configurations/xbox_360.hpp"

#include "communication_protocols/joybus.hpp"

#include "other/runtime_remapping_mode.hpp"
#include "pico/stdlib.h"

#ifdef SG_GUITAR
// Read the BOOTSEL button without disrupting flash operation.
// Must not be in flash (XIP) since we temporarily disable flash CS.
static bool __no_inline_not_in_flash_func(get_bootsel_button)() {
    const uint CS_PIN_INDEX = 1;
    uint32_t flags = save_and_disable_interrupts();
    // Drive QSPI CS pin low so it doesn't float (flash stays deselected)
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    // Small delay for the pin to settle
    for (volatile int i = 0; i < 1000; ++i);
    // Read the BOOTSEL button state (active low)
    bool pressed = !(sio_hw->gpio_hi_in & (1u << CS_PIN_INDEX));
    // Restore normal CS operation
    hw_write_masked(&ioqspi_hw->io[CS_PIN_INDEX].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SS_CTRL_OEOVER_BITS);
    restore_interrupts(flags);
    return pressed;
}
#endif

int main() {

 stdio_init_all();  // If using USB serial
    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    while (true) {  // Or put early in main
        gpio_put(25, 1);
        sleep_ms(200);
        gpio_put(25, 0);
        sleep_ms(200);
    }

    
    set_sys_clock_khz(125000, true);
   // set_sys_clock_khz(1000*us, true);
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

    #ifdef SG_GUITAR
    // Read configured pin assignments from flash for mode selection.
    // On first boot (unconfigured), uses compiled-in defaults.
    const auto* sgBinds = Persistence::read<Persistence::Pages::SgBinds>();
    uint8_t pinGreen, pinRed, pinYellow, pinBlue, pinOrange;
    uint8_t pinUpStrum, pinDownStrum, pinRButton, pinTilt, pinSelect, pinStart;
    if (sgBinds->configured == 1) {
        pinGreen    = sgBinds->entries[SLOT_GREEN].pin;
        pinRed      = sgBinds->entries[SLOT_RED].pin;
        pinYellow   = sgBinds->entries[SLOT_YELLOW].pin;
        pinBlue     = sgBinds->entries[SLOT_BLUE].pin;
        pinOrange   = sgBinds->entries[SLOT_ORANGE].pin;
        pinUpStrum  = sgBinds->entries[SLOT_UP_STRUM].pin;
        pinDownStrum= sgBinds->entries[SLOT_DOWN_STRUM].pin;
        pinRButton  = sgBinds->entries[SLOT_R_BUTTON].pin;
        pinTilt     = sgBinds->entries[SLOT_TILT].pin;
        pinSelect   = sgBinds->entries[SLOT_SELECT].pin;
        pinStart    = sgBinds->entries[SLOT_START].pin;
    } else {
        pinGreen = 2; pinRed = 3; pinYellow = 4; pinBlue = 5; pinOrange = 6;
        pinUpStrum = 7; pinDownStrum = 8; pinRButton = 15; pinTilt = 17;
        pinSelect = 21; pinStart = 22;
    }
    // Init configured pins (may differ from default modePins list)
    {
        uint8_t cfgPins[] = { pinGreen, pinRed, pinYellow, pinBlue, pinOrange,
                              pinUpStrum, pinDownStrum, pinRButton, pinTilt,
                              pinSelect, pinStart };
        for (unsigned i = 0; i < sizeof(cfgPins); i++) {
            gpio_init(cfgPins[i]);
            gpio_set_dir(cfgPins[i], GPIO_IN);
            gpio_pull_up(cfgPins[i]);
        }
    }
    busy_wait_ms(10); // Let internal pull-ups settle before reading boot combos

    // SG: Press BOOTSEL within 1s of power-on to force-enter configurator.
    // LED blinks rapidly during the window. Works with no buttons wired.
    // (Can't check BOOTSEL *at* power-on — ROM bootloader catches that first.)
    {
        const uint32_t windowMs = 1000;
        const uint32_t blinkMs = 100;
        uint32_t elapsed = 0;
        while (elapsed < windowMs) {
            if (get_bootsel_button()) {
                USBConfigurations::Configurator::enterMode();
            }
            gpio_put(LED_PIN, (elapsed / blinkMs) & 1);
            busy_wait_ms(5);
            elapsed += 5;
        }
        gpio_put(LED_PIN, 0);
    }

    // SG: Hold only Tilt/Z at boot to enter whammy calibration mode.
    // Blink LED slowly while waiting. First A press = high LS value,
    // second A press = low LS value. Saves to flash and reboots.
    if (!gpio_get(pinTilt) && gpio_get(pinDownStrum) && gpio_get(pinUpStrum) && gpio_get(pinGreen)) {
        // Init SG GPIO/ADC for whammy reads
        GpioToButtonSets::SG::defaultConversion();

        // Wait for Tilt release first
        while (!gpio_get(pinTilt)) {
            gpio_put(LED_PIN, 0); busy_wait_ms(100);
            gpio_put(LED_PIN, 1); busy_wait_ms(100);
        }

        Persistence::Pages::WhammyCalibration cal = {};

        // Blink slowly: waiting for first Green press (high LS whammy position)
        while (gpio_get(pinGreen)) {
            gpio_put(LED_PIN, 0); busy_wait_ms(500);
            gpio_put(LED_PIN, 1); busy_wait_ms(500);
        }
        cal.whammyHigh = GpioToButtonSets::SG::readWhammy();
        // Wait for Green release
        while (!gpio_get(pinGreen)) tight_loop_contents();

        // Blink fast: waiting for second Green press (low LS whammy position)
        while (gpio_get(pinGreen)) {
            gpio_put(LED_PIN, 0); busy_wait_ms(150);
            gpio_put(LED_PIN, 1); busy_wait_ms(150);
        }
        cal.whammyLow = GpioToButtonSets::SG::readWhammy();
        cal.configured = 1;

        // Save to flash
        Persistence::commit(cal);

        // Solid LED for 1 second to confirm, then reboot
        gpio_put(LED_PIN, 1);
        busy_wait_ms(1000);
        watchdog_reboot(0, 0, 0); // Clean reboot into normal firmware
        while (1) tight_loop_contents();
    }

    // SG: Hold Select+Start at boot to enter USB configurator mode.
    // Enumerates as a vendor HID device; use configurator.html to connect.
    if (!gpio_get(pinSelect) && !gpio_get(pinStart)) {
        USBConfigurations::Configurator::enterMode();
    }

    // SG: First boot — if binds have never been configured, auto-enter configurator
    // so the user can set up their layout before playing.
    if (sgBinds->configured != 1) {
        USBConfigurations::Configurator::enterMode();
    }

    // SG: Hold Down Strum at boot to enter console/joybus mode.
    // USB is the default when no combo is held.
    if (!gpio_get(pinDownStrum)) goto stateLabel__forceJoybusEntry;
    #endif

    /* Mode selection logic */

#ifdef SG_GUITAR

    goto sg_usb_modes;

    stateLabel__forceJoybusEntry:

    // LED OFF = joybus mode entered, waiting for console.
    // LED turns back ON when first probe is received (confirms GP28 data line works).
    gpio_put(LED_PIN, 0);

    // Pre-init SG GPIO/ADC so the first joybus poll isn't delayed by lazy init.
    GpioToButtonSets::SG::defaultConversion();

    // Load whammy calibration from flash
    DACAlgorithms::MeleeSG::loadCalibration();

    // Green or Up Strum: P+
    if ((!gpio_get(pinUpStrum)) || (!gpio_get(pinGreen))) {
        CommunicationProtocols::Joybus::enterMode(gcDataPin, [](){
            GCReport report = DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
            uint8_t whammy = GpioToButtonSets::SG::readWhammy();
            if (whammy > report.analogR) report.analogR = whammy;
            return report;
        });
    }

    // Orange: Ultimate
    if (!gpio_get(pinOrange)) {
        CommunicationProtocols::Joybus::enterMode(gcDataPin, [](){
            GCReport report = DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
            uint8_t whammy = GpioToButtonSets::SG::readWhammy();
            if (whammy > report.analogR) report.analogR = whammy;
            return report;
        });
    }

    // Else: SG / Melee
    CommunicationProtocols::Joybus::enterMode(gcDataPin, [](){
        return DACAlgorithms::MeleeSG::getGCReport(
            GpioToButtonSets::SG::defaultConversion(),
            GpioToButtonSets::SG::readWhammy()
        );
    });

    sg_usb_modes:

    // 3 quick blinks to confirm firmware reached USB mode selection
    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PIN, 0);
        busy_wait_ms(150);
        gpio_put(LED_PIN, 1);
        busy_wait_ms(150);
    }

    // Load whammy calibration for USB modes too
    DACAlgorithms::MeleeSG::loadCalibration();

    // Red: Melee / XInput
    if (!gpio_get(pinRed)) USBConfigurations::Xbox360::enterMode([](){
        GCReport report = DACAlgorithms::MeleeSG::getGCReport(
            GpioToButtonSets::SG::defaultConversion(),
            GpioToButtonSets::SG::readWhammy()
        );
        USBConfigurations::Xbox360::actuateReportFromGCState(report);
    });

    // Down Strum: Xbox360 / XInput
    if (!gpio_get(pinDownStrum)) USBConfigurations::Xbox360::enterMode([](){
        DACAlgorithms::Xbox360::actuateXbox360Report(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > USBConfigurations::Xbox360::xInputReport.rightTrigger)
            USBConfigurations::Xbox360::xInputReport.rightTrigger = whammy;
    });

    // Select: Melee / HID
    if (!gpio_get(pinSelect)) USBConfigurations::HidWithTriggers::enterMode([](){
        GCReport report = DACAlgorithms::MeleeSG::getGCReport(
            GpioToButtonSets::SG::defaultConversion(),
            GpioToButtonSets::SG::readWhammy()
        );
        USBConfigurations::HidWithTriggers::actuateReportFromGCState(report);
    });

    // Start: Ult / HID
    if (!gpio_get(pinStart)) USBConfigurations::HidWithTriggers::enterMode([](){
        GCReport report = DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::HidWithTriggers::actuateReportFromGCState(report);
    });

    // Green: P+ / WFPP (Switch)
    if (!gpio_get(pinGreen)) USBConfigurations::WiredFightPadPro::enterMode([](){
        GCReport report = DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::WiredFightPadPro::actuateReportFromGCState(report);
    });

    // Up Strum: P+ / GCC Adapter
    if (!gpio_get(pinUpStrum)) USBConfigurations::GccToUsbAdapter::enterMode([](){
        GCReport report = DACAlgorithms::ProjectPlusF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(report);
    });

    // Orange: Ultimate / GCC Adapter
    if (!gpio_get(pinOrange)) USBConfigurations::GccToUsbAdapter::enterMode([](){
        GCReport report = DACAlgorithms::UltimateF1::getGCReport(GpioToButtonSets::SG::defaultConversion());
        uint8_t whammy = GpioToButtonSets::SG::readWhammy();
        if (whammy > report.analogR) report.analogR = whammy;
        USBConfigurations::GccToUsbAdapter::actuateReportFromGCState(report);
    });

    // Blue: Melee / WFPP (Switch)
    if (!gpio_get(pinBlue)) USBConfigurations::WiredFightPadPro::enterMode([](){
        GCReport report = DACAlgorithms::MeleeSG::getGCReport(
            GpioToButtonSets::SG::defaultConversion(),
            GpioToButtonSets::SG::readWhammy()
        );
        USBConfigurations::WiredFightPadPro::actuateReportFromGCState(report);
    });

    // Yellow: WFPP Default / WFPP (Switch)
    if (!gpio_get(pinYellow)) USBConfigurations::WiredFightPadPro::enterMode([](){
        DACAlgorithms::WiredFightPadProDefault::actuateWFPPReport(GpioToButtonSets::SG::defaultConversion());
    });

    // Default: Melee / GCC Adapter
    USBConfigurations::GccToUsbAdapter::enterMode(
        [](){
            GCReport report = DACAlgorithms::MeleeSG::getGCReport(
                GpioToButtonSets::SG::defaultConversion(),
                GpioToButtonSets::SG::readWhammy()
            );
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
