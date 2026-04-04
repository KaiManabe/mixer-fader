#ifndef _WINUSBDEVICE_HPP_
#define _WINUSBDEVICE_HPP_

#include <windows.h>
#include <winusb.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "common/UsbDesc.h"

// DeviceInterfaceGUID constructed from common/UsbDesc.h components
static const GUID DEVICE_INTERFACE_GUID = {
    USB_DEVICE_INTERFACE_GUID_DATA1,
    USB_DEVICE_INTERFACE_GUID_DATA2,
    USB_DEVICE_INTERFACE_GUID_DATA3,
    USB_DEVICE_INTERFACE_GUID_DATA4
};

/// @brief WinUSB デバイスの接続と I/O を管理する
class WinUsbDevice {
public:
    WinUsbDevice();
    ~WinUsbDevice();

    // Noncopyable
    WinUsbDevice(const WinUsbDevice&) = delete;
    WinUsbDevice& operator=(const WinUsbDevice&) = delete;

    bool open();
    void close();
    bool isOpen() const;
    bool isDisconnected() const;
    int bulkWrite(const std::vector<uint8_t>& data);
    std::vector<uint8_t> bulkRead(size_t maxBytes, uint32_t timeoutMs = 1000);

private:
    HANDLE m_deviceHandle;
    WINUSB_INTERFACE_HANDLE m_winusbHandle;
    bool m_disconnected;

    void handleDeviceError();
    std::string findDevicePath();
};

#endif
