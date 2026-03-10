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

class WinUsbDevice {
public:
    WinUsbDevice();
    ~WinUsbDevice();

    // Noncopyable
    WinUsbDevice(const WinUsbDevice&) = delete;
    WinUsbDevice& operator=(const WinUsbDevice&) = delete;

    /// @brief デバイスを検索しオープンする
    /// @return 成功時true
    bool open();

    /// @brief デバイスをクローズする
    void close();

    /// @brief デバイスがオープン済みかどうか
    bool isOpen() const;

    /// @brief Bulk OUT転送でデータを送信する
    /// @param data 送信するバイト列
    /// @return 送信バイト数 (失敗時 -1)
    int bulkWrite(const std::vector<uint8_t>& data);

    /// @brief Bulk IN転送でデータを受信する
    /// @param maxBytes 最大受信バイト数
    /// @param timeoutMs タイムアウト(ms), 0=デフォルト
    /// @return 受信データ (失敗時は空)
    std::vector<uint8_t> bulkRead(size_t maxBytes, uint32_t timeoutMs = 1000);

private:
    HANDLE m_deviceHandle;
    WINUSB_INTERFACE_HANDLE m_winusbHandle;

    /// @brief SetupAPI でデバイスパスを取得する
    /// @return デバイスパス (見つからない場合は空文字列)
    std::string findDevicePath();
};

#endif
