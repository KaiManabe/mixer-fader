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

    /// @brief DfFrame をシリアライズして送信する
    /// @return 成功時 true
    bool send(DfFrame& frame);

    /// @brief レベルメータ点灯パターンを送信する
    bool sendLevelMeter(uint8_t slaveId, uint16_t pattern);

    /// @brief ディスプレイ描画データを送信する
    bool sendDisplay(uint8_t slaveId, const uint8_t* pixelData, size_t size);

    // ---- 受信 ----

    /// @brief FdFrame を 1 フレーム受信する (ブロッキング)
    /// @param timeoutMs タイムアウト(ms)
    /// @return 受信したフレーム．タイムアウト時は std::nullopt
    std::optional<FdFrame> receive(uint32_t timeoutMs = 2000);

    /// @brief 受信ループを回し，イベントコールバックを呼ぶ
    ///
    /// デバイス切断時は自動で再接続を試みる．
    /// 外部から stop() が呼ばれるまでブロックする．
    void poll(uint32_t timeoutMs = 2000);

    /// @brief poll() ループを停止させる
    void stop();

    // ---- コールバック登録 ----

    /// @brief 全イベント共通コールバック
    void onEvent(EventCallback cb);

    /// @brief 特定イベントタイプのコールバック
    void onInitialized(EventCallback cb);
    void onStatus(EventCallback cb);
    void onEncRotP(EventCallback cb);
    void onEncRotN(EventCallback cb);
    void onEncPushD(EventCallback cb);
    void onEncPushU(EventCallback cb);

    /// @brief デバイス切断時コールバック
    void onDisconnect(std::function<void()> cb);

    /// @brief デバイス再接続時コールバック
    void onConnect(std::function<void()> cb);

    // ---- 状態 ----

    /// @brief firmware が ready (RXバッファ空きあり) かどうか
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
