#ifndef COMMUNICATION_PROTOCOLS__USB_HPP
#define COMMUNICATION_PROTOCOLS__USB_HPP

#include "usb/common.hpp"

namespace CommunicationProtocols
{
namespace USB
{

struct ConfigurationNoFunc {
    uint16_t inEpMaxPacketSize;
    uint16_t inEpActualPacketSize;
    uint16_t outEpMaxPacketSize;
    uint8_t epOutId; // 1 or 2
    const char **descriptorStrings;
    uint16_t descriptorStringsLen;
    bool hid;
    uint16_t bcdHID;
    uint8_t* hidReportDescriptor;
    uint16_t hidReportDescriptorLen;
    bool useWinUSB;
    uint16_t VID;
    uint16_t PID;
    uint16_t bcdDevice;
    bool xinput;

    uint8_t* hidReportPtr;
};

struct Configuration {
    ConfigurationNoFunc configNoFunc;
    void (*reportActuationFunc)(void);
};

struct FuncsDOP {
    void (*reportActuationFuncPC)(void);
    void (*reportActuationFuncSwitch)(void);
};

void enterMode(Configuration, int headroomUs = 120);
void enterMode(ConfigurationNoFunc, FuncsDOP, int headroomUs = 120);

// Initialize USB device without entering a polling loop.
// Used by config mode to run its own custom loop.
void initMode(ConfigurationNoFunc config);

}
}

// Low-level USB functions exposed for config mode
extern volatile bool ep1_in_handler_happened;
uint8_t ep_in_addr();
struct usb_endpoint_configuration;
usb_endpoint_configuration *usb_get_endpoint_configuration(uint8_t addr);
void usb_start_transfer(usb_endpoint_configuration *ep, const uint8_t *buf, uint16_t len);

// OUT endpoint receive buffer for config mode
extern volatile uint8_t epOutRecvBuf[64];
extern volatile uint8_t epOutRecvLen;
extern volatile bool epOutRecvReady;

#endif