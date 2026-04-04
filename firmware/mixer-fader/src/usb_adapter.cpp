#include "usb_adapter.hpp"
#include "UsbComm.hpp"
#include "tusb.h"

// Static instance
UsbAdapter* UsbAdapter::instance = nullptr;

UsbAdapter::UsbAdapter() :
    m_usb(nullptr),
    m_txInFlight(false)
{}

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
    
    // Feed received bytes to UsbComm reception handler
    for (uint16_t i = 0; i < bufsize; i++) {
        m_usb->putRxByteBuf(buffer[i]);
    }
}

void UsbAdapter::handleBulkInTransmit() {
    if (m_usb == nullptr || m_usb->isTxEmpty()) return;
    if (m_txInFlight) return;
    if (!tud_mounted()) return;
    
    const uint8_t* txPtr = nullptr;
    size_t maxSize = 64;  // Bulk packet size
    
    size_t frameLen = m_usb->getTxByteBuf(txPtr, maxSize);
    if (frameLen > 0 && txPtr != nullptr) {
        m_txInFlight = true;
        tud_vendor_n_write(0, txPtr, frameLen);
        tud_vendor_n_write_flush(0);
    }
}

void UsbAdapter::onBulkInComplete() {
    m_txInFlight = false;
    handleBulkInTransmit();
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
    
    // Process any received complete frames
    m_usb->processReceivedFrame();
    
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
        UsbAdapter::getInstance().onBulkInComplete();
    }
}
