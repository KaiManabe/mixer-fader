#include "usb_adapter.hpp"
#include "UsbComm.hpp"
#include "tusb.h"

// Static instance
UsbAdapter* UsbAdapter::instance = nullptr;

UsbAdapter::UsbAdapter() : m_usb(nullptr) {}

UsbAdapter& UsbAdapter::getInstance() {
    if (instance == nullptr) {
        instance = new UsbAdapter();
    }
    return *instance;
}

void UsbAdapter::init(UsbComm* usb) {
    m_usb = usb;
}

void UsbAdapter::onBulkOutComplete(uint8_t const* buffer, uint16_t bufsize) {
    if (m_usb == nullptr) return;

    m_usb->putRxBytes(buffer, bufsize);
}

void UsbAdapter::handleBulkInTransmit() {
    if (m_usb == nullptr || m_usb->isTxEmpty()) return;
    
    const uint8_t* txPtr = nullptr;
    size_t maxSize = 64;  // Bulk packet size
    
    size_t frameLen = m_usb->getTxByteBuf(txPtr, maxSize);
    if (frameLen > 0 && txPtr != nullptr) {
        tud_vendor_n_write(0, txPtr, frameLen);
        tud_vendor_n_write_flush(0);
    }
}

void UsbAdapter::process() {
    if (m_usb == nullptr) return;

    // Poll for received vendor OUT data
    while (tud_vendor_n_available(0)) {
        uint8_t buf[64];
        uint32_t count = tud_vendor_n_read(0, buf, sizeof(buf));
        if (count == 0) break;
        onBulkOutComplete(buf, static_cast<uint16_t>(count));
    }
    
    // Apply queued commands and queue status updates.
    m_usb->routine();
    
    // Handle TX transmission
    handleBulkInTransmit();
}

void UsbAdapter::sendInitialized() {
    if (m_usb == nullptr) return;
    m_usb->sendInitialized();
}

// TinyUSB vendor class callbacks
extern "C" {
    // Called when device is mounted (USB host connected & configured)
    void tud_mount_cb(void) {
        UsbAdapter::getInstance().sendInitialized();
    }

    // Called when bulk IN transfer completes
    void tud_vendor_tx_cb(uint8_t itf, uint32_t sent_bytes) {
        (void)itf;
        (void)sent_bytes;
        UsbAdapter::getInstance().handleBulkInTransmit();
    }
}
