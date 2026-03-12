#include "usb_configurations/configurator.hpp"
#include "communication_protocols/usb.hpp"
#include "gpio_to_button_sets/SG.hpp"
#include "persistence/functions.hpp"
#include "persistence/pages/whammy_calibration.hpp"
#include "persistence/pages/sg_binds.hpp"
#include "dac_algorithms/melee_SG.hpp"

#include "pico/stdlib.h"
#include "global.hpp"
#include <string.h>

namespace USBConfigurations {
namespace Configurator {

// Config protocol command IDs (OUT reports from host)
#define CMD_SET_WHAMMY_CAL   0x01
#define CMD_SET_BIND         0x02
#define CMD_SAVE_BINDS       0x03
#define CMD_RESET_BINDS      0x04
#define CMD_RESET_WHAMMY_CAL 0x05
#define CMD_SET_BIND_PIN     0x06

// Status report (IN reports to host)
#define STATUS_REPORT        0x01
// Command acknowledgement
#define ACK_REPORT           0x02

#define REPORT_SIZE 32

static uint8_t statusReport[REPORT_SIZE];

// Vendor-defined HID report descriptor: 32 bytes in, 32 bytes out
static uint8_t hidReportDescriptor[] = {
    0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,        // Usage (Vendor Usage 1)
    0xA1, 0x01,        // Collection (Application)
    // 32-byte Input report (device -> host)
    0x09, 0x01,        //   Usage (Vendor Usage 1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, REPORT_SIZE, //   Report Count (32)
    0x81, 0x02,        //   Input (Data, Var, Abs)
    // 32-byte Output report (host -> device)
    0x09, 0x02,        //   Usage (Vendor Usage 2)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, REPORT_SIZE, //   Report Count (32)
    0x91, 0x02,        //   Output (Data, Var, Abs)
    0xC0               // End Collection
};
static const uint16_t hidReportDescriptorLen = sizeof(hidReportDescriptor);

static const char *descriptorStrings[] = {
    "pico-rectangle",
    "SG Guitar Configurator",
    "config-001"
};

static void buildStatusReport() {
    memset(statusReport, 0, REPORT_SIZE);

    statusReport[0] = STATUS_REPORT;

    // Live whammy value
    statusReport[1] = GpioToButtonSets::SG::readWhammy();

    // Whammy calibration
    const auto* whammyCal = Persistence::read<Persistence::Pages::WhammyCalibration>();
    statusReport[2] = (whammyCal->configured == 1) ? 1 : 0;
    statusReport[3] = DACAlgorithms::MeleeSG::whammyHigh;
    statusReport[4] = DACAlgorithms::MeleeSG::whammyLow;

    // Button bindings: 11 slots × 2 bytes (pin, buttonIndex)
    for (int i = 0; i < NUM_SG_BIND_SLOTS; i++) {
        statusReport[5 + i * 2] = GpioToButtonSets::SG::getBindPin(i);
        statusReport[5 + i * 2 + 1] = GpioToButtonSets::SG::getBindButton(i);
    }
    // Bytes 5-26 = 11 × 2 = 22 bytes of bind data

    // Live button state (which buttons are currently pressed)
    GpioToButtonSets::SG::ButtonSet bs = GpioToButtonSets::SG::defaultConversion();
    uint8_t state0 = 0, state1 = 0, state2 = 0;
    if (bs.a)      state0 |= (1 << 0);
    if (bs.b)      state0 |= (1 << 1);
    if (bs.x)      state0 |= (1 << 2);
    if (bs.y)      state0 |= (1 << 3);
    if (bs.z)      state0 |= (1 << 4);
    if (bs.l)      state0 |= (1 << 5);
    if (bs.r)      state0 |= (1 << 6);
    if (bs.ls)     state0 |= (1 << 7);
    if (bs.ms)     state1 |= (1 << 0);
    if (bs.mx)     state1 |= (1 << 1);
    if (bs.my)     state1 |= (1 << 2);
    if (bs.start)  state1 |= (1 << 3);
    if (bs.left)   state1 |= (1 << 4);
    if (bs.right)  state1 |= (1 << 5);
    if (bs.up)     state1 |= (1 << 6);
    if (bs.down)   state1 |= (1 << 7);
    if (bs.cLeft)  state2 |= (1 << 0);
    if (bs.cRight) state2 |= (1 << 1);
    if (bs.cUp)    state2 |= (1 << 2);
    if (bs.cDown)  state2 |= (1 << 3);
    statusReport[27] = state0;
    statusReport[28] = state1;
    statusReport[29] = state2;

    // Byte 31: binds configured flag (1 = configured, 0 = first boot / unconfigured)
    const auto* bindPage = Persistence::read<Persistence::Pages::SgBinds>();
    statusReport[31] = (bindPage->configured == 1) ? 1 : 0;

    // Raw GPIO scan: report which candidate pins are currently pressed (low).
    // Used by the pin labeling wizard to detect which GPIO a physical input is on.
    // Pack as a bitmask: byte 30 = pins 0-7, byte 31 = pins 8-22 (shifted).
    // We only scan safe candidate pins (not LED, ADC, data, BOOTSEL).
    static const uint8_t candidatePins[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 20, 21 };
    uint32_t gpioState = sio_hw->gpio_in;
    // Find first pressed (low) candidate pin, report its number (0xFF if none)
    statusReport[30] = 0xFF;
    for (unsigned i = 0; i < sizeof(candidatePins); i++) {
        uint8_t p = candidatePins[i];
        if (!(gpioState & (1u << p))) {
            statusReport[30] = p;
            break;
        }
    }
}

static void processCommand(volatile uint8_t *cmd, uint8_t len) {
    if (len < 1) return;

    switch (cmd[0]) {
        case CMD_SET_WHAMMY_CAL: {
            if (len < 3) break;
            Persistence::Pages::WhammyCalibration cal = {};
            cal.configured = 1;
            cal.whammyHigh = cmd[1];
            cal.whammyLow = cmd[2];
            Persistence::commit(cal);
            // Reload into runtime
            DACAlgorithms::MeleeSG::loadCalibration();
            break;
        }
        case CMD_SET_BIND: {
            if (len < 3) break;
            uint8_t slot = cmd[1];
            uint8_t buttonIndex = cmd[2];
            GpioToButtonSets::SG::setBindButton(slot, buttonIndex);
            break;
        }
        case CMD_SAVE_BINDS: {
            GpioToButtonSets::SG::saveBinds();
            break;
        }
        case CMD_RESET_BINDS: {
            GpioToButtonSets::SG::resetBinds();
            break;
        }
        case CMD_SET_BIND_PIN: {
            if (len < 3) break;
            uint8_t slot = cmd[1];
            uint8_t pin = cmd[2];
            GpioToButtonSets::SG::setBindPin(slot, pin);
            break;
        }
        case CMD_RESET_WHAMMY_CAL: {
            Persistence::Pages::WhammyCalibration cal = {};
            cal.configured = 0xFF;
            cal.whammyHigh = 255;
            cal.whammyLow = 0;
            Persistence::commit(cal);
            DACAlgorithms::MeleeSG::loadCalibration();
            break;
        }
    }
}

void enterMode() {
    // Init all candidate GPIO pins as inputs with pull-ups for raw scanning.
    // This ensures pin detection works even before binds are configured.
    static const uint8_t candidatePins[] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 20, 21 };
    for (unsigned i = 0; i < sizeof(candidatePins); i++) {
        gpio_init(candidatePins[i]);
        gpio_set_dir(candidatePins[i], GPIO_IN);
        gpio_pull_up(candidatePins[i]);
    }

    // Init SG GPIO/ADC
    GpioToButtonSets::SG::initDefaultConversion();

    // Load whammy calibration
    DACAlgorithms::MeleeSG::loadCalibration();

    CommunicationProtocols::USB::ConfigurationNoFunc config = {
        .inEpMaxPacketSize = REPORT_SIZE,
        .inEpActualPacketSize = REPORT_SIZE,
        .outEpMaxPacketSize = REPORT_SIZE,
        .epOutId = 2,
        .descriptorStrings = descriptorStrings,
        .descriptorStringsLen = 3,
        .hid = true,
        .bcdHID = 0x0111,
        .hidReportDescriptor = hidReportDescriptor,
        .hidReportDescriptorLen = hidReportDescriptorLen,
        .useWinUSB = false,
        .VID = 0x2E8A,  // Raspberry Pi VID
        .PID = 0x1080,  // Custom PID for configurator
        .bcdDevice = 0x0100,
        .xinput = false,
        .hidReportPtr = statusReport
    };

    CommunicationProtocols::USB::initMode(config);

    // Main config loop
    while (1) {
        // Wait for previous IN transfer
        while (!ep1_in_handler_happened) tight_loop_contents();
        ep1_in_handler_happened = false;

        // Build and send status
        buildStatusReport();
        usb_start_transfer(
            usb_get_endpoint_configuration(ep_in_addr()),
            statusReport,
            REPORT_SIZE
        );

        // Check for incoming commands
        if (epOutRecvReady) {
            processCommand(epOutRecvBuf, epOutRecvLen);
            epOutRecvReady = false;
        }
    }
}

}
}
