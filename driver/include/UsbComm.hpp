#ifndef _DRIVER_USBCOMM_HPP_
#define _DRIVER_USBCOMM_HPP_

#include <stdint.h>
#include <functional>
#include <optional>

#include "WinUsbDevice.hpp"
#include "common/DfFrame.hpp"
#include "common/FdFrame.hpp"

/// @brief Driver 側 USB 通信インターフェイス
///
/// WinUsbDevice をラップし，DfFrame / FdFrame 単位で送受信する．
/// USB 切断時は自動で再接続を試みる．
class UsbComm {
public:
    /// イベントコールバック型
    using EventCallback = std::function<void(const FdFrame&)>;

    explicit UsbComm(WinUsbDevice& dev);

    // ---- 送信 ----

    bool send(DfFrame& frame);
    bool sendLevelMeter(uint8_t slaveId, uint16_t pattern);
    bool sendDisplay(uint8_t slaveId, const uint8_t* pixelData, size_t size);

    // ---- 受信 ----

    std::optional<FdFrame> receive(uint32_t timeoutMs = 2000);
    void poll(uint32_t timeoutMs = 2000);
    void stop();

    // ---- コールバック登録 ----

    void onEvent(EventCallback cb);
    void onInitialized(EventCallback cb);
    void onStatus(EventCallback cb);
    void onEncRotP(EventCallback cb);
    void onEncRotN(EventCallback cb);
    void onEncPushD(EventCallback cb);
    void onEncPushU(EventCallback cb);
    void onDisconnect(std::function<void()> cb);
    void onConnect(std::function<void()> cb);

    // ---- 状態 ----

    bool isDeviceReady() const;

private:
    WinUsbDevice& m_dev;
    bool m_running;
    bool m_deviceReady;

    EventCallback m_onEvent;
    EventCallback m_onInitialized;
    EventCallback m_onStatus;
    EventCallback m_onEncRotP;
    EventCallback m_onEncRotN;
    EventCallback m_onEncPushD;
    EventCallback m_onEncPushU;
    std::function<void()> m_onDisconnect;
    std::function<void()> m_onConnect;

    void dispatch(const FdFrame& frame);
    void waitAndReconnect(uint32_t intervalMs = 1000);
};

#endif
