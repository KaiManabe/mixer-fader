#ifndef _USB_ADAPTER_HPP_
#define _USB_ADAPTER_HPP_

#include <stdint.h>
#include "common/UsbDesc.h"

// Forward declaration
class UsbComm;

class UsbAdapter {
public:
    static UsbAdapter& getInstance();
    
    void init(UsbComm* usb);
    void process();
    
    // Called by TinyUSB bulk OUT endpoint callback
    void onBulkOutComplete(uint8_t const* buffer, uint16_t bufsize);
    
    // Called to handle bulk IN transmission
    void handleBulkInTransmit();
    
private:
    UsbAdapter();
    
    UsbComm* m_usb;
    static UsbAdapter* instance;
    
    static constexpr uint8_t BULK_OUT_EP = USB_EP_OUT;
    static constexpr uint8_t BULK_IN_EP  = USB_EP_IN;
};

#endif
