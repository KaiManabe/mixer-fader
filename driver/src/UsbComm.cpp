#include "UsbComm.hpp"
#include <string.h>
#include <stdio.h>


/// @brief USB 通信オブジェクトを初期化する
/// @param dev 使用する WinUSB デバイス
UsbComm::UsbComm(WinUsbDevice& dev)
    : m_dev(dev),
      m_running(false),
      m_deviceReady(false)
{
}


// ---- 送信 ----

/// @brief DfFrame をシリアライズして送信する
/// @param frame 送信するフレーム
/// @return 送信に成功したら true
bool UsbComm::send(DfFrame& frame)
{
    // ---------------- シリアライズして送信 ----------------
    auto buf = frame.serialize();
    int written = m_dev.bulkWrite(buf);
    return written >= 0;
}


/// @brief レベルメータ点灯パターンを送信する
/// @param slaveId 対象 slave ID
/// @param pattern 16bit の点灯パターン
bool UsbComm::sendLevelMeter(uint8_t slaveId, uint16_t pattern)
{
    // ---------------- フレームを組み立て ----------------
    DfFrame frame;
    frame.dataType = DfDataType::LEVELMETER;
    frame.slaveId  = slaveId;
    frame.frameData.resize(sizeof(uint16_t));
    memcpy(frame.frameData.data(), &pattern, sizeof(uint16_t));

    // -------------------- フレームを送信 --------------------
    return send(frame);
}


/// @brief ディスプレイ描画データを送信する
/// @param slaveId 対象 slave ID
/// @param pixelData 描画データ
/// @param size 描画データサイズ
bool UsbComm::sendDisplay(uint8_t slaveId, const uint8_t* pixelData, size_t size)
{
    // ---------------- フレームを組み立て ----------------
    DfFrame frame;
    frame.dataType = DfDataType::DISPLAY;
    frame.slaveId  = slaveId;
    frame.frameData.assign(pixelData, pixelData + size);

    // -------------------- フレームを送信 --------------------
    return send(frame);
}


// ---- 受信 ----

/// @brief FdFrame を 1 フレーム受信する
/// @param timeoutMs 受信タイムアウト
/// @return 受信できたフレーム
std::optional<FdFrame> UsbComm::receive(uint32_t timeoutMs)
{
    // ------------------- 生データを受信 -------------------
    auto rxData = m_dev.bulkRead(64, timeoutMs);
    if (rxData.empty()) return std::nullopt;

    // ---------------- フレームへデシリアライズ ----------------
    FdFrame frame;
    if (!frame.deserialize(rxData)) return std::nullopt;

    // ------------------- ready 状態を更新 -------------------
    m_deviceReady = frame.ready;
    return frame;
}


/// @brief 受信ループを回してイベントを配送する
/// @param timeoutMs 受信タイムアウト
void UsbComm::poll(uint32_t timeoutMs)
{
    // ------------------- ループ開始を記録 -------------------
    m_running = true;
    while (m_running) {
        // デバイスが切断されている場合は再接続を試みる
        if (m_dev.isDisconnected()) {
            if (m_onDisconnect) m_onDisconnect();
            waitAndReconnect();
            if (!m_running) break;
            if (m_onConnect) m_onConnect();
            continue;
        }

        // ------------------- 受信イベントを配送 -------------------
        auto frame = receive(timeoutMs);
        if (frame.has_value()) {
            dispatch(frame.value());
        }
    }
}


/// @brief poll() ループを停止させる
void UsbComm::stop()
{
    m_running = false;
}


// ---- コールバック登録 ----

/// @brief 全イベント共通コールバックを登録する
/// @param cb 登録するコールバック
void UsbComm::onEvent(EventCallback cb) { m_onEvent = cb; }
/// @brief INITIALIZED コールバックを登録する
/// @param cb 登録するコールバック
void UsbComm::onInitialized(EventCallback cb) { m_onInitialized = cb; }
/// @brief STATUS コールバックを登録する
/// @param cb 登録するコールバック
void UsbComm::onStatus(EventCallback cb) { m_onStatus = cb; }
/// @brief ENC_ROTP コールバックを登録する
/// @param cb 登録するコールバック
void UsbComm::onEncRotP(EventCallback cb) { m_onEncRotP = cb; }
/// @brief ENC_ROTN コールバックを登録する
/// @param cb 登録するコールバック
void UsbComm::onEncRotN(EventCallback cb) { m_onEncRotN = cb; }
/// @brief ENC_PUSHD コールバックを登録する
/// @param cb 登録するコールバック
void UsbComm::onEncPushD(EventCallback cb) { m_onEncPushD = cb; }
/// @brief ENC_PUSHU コールバックを登録する
/// @param cb 登録するコールバック
void UsbComm::onEncPushU(EventCallback cb) { m_onEncPushU = cb; }
/// @brief 切断時コールバックを登録する
/// @param cb 登録するコールバック
void UsbComm::onDisconnect(std::function<void()> cb) { m_onDisconnect = cb; }
/// @brief 再接続時コールバックを登録する
/// @param cb 登録するコールバック
void UsbComm::onConnect(std::function<void()> cb) { m_onConnect = cb; }


// ---- 状態 ----

/// @brief firmware が ready かどうかを返す
/// @return ready なら true
bool UsbComm::isDeviceReady() const
{
    return m_deviceReady;
}


// ---- 内部 ----

/// @brief 受信フレームをイベント種別ごとに配送する
/// @param frame 受信したフレーム
void UsbComm::dispatch(const FdFrame& frame)
{
    // ---------------- 共通コールバックを通知 ----------------
    if (m_onEvent) m_onEvent(frame);

    // ---------------- 種別ごとにコールバックを通知 ----------------
    switch (frame.eventType) {
    case FdEventType::INITIALIZED: if (m_onInitialized) m_onInitialized(frame); break;
    case FdEventType::STATUS:      if (m_onStatus)      m_onStatus(frame);      break;
    case FdEventType::ENC_ROTP:    if (m_onEncRotP)     m_onEncRotP(frame);     break;
    case FdEventType::ENC_ROTN:    if (m_onEncRotN)     m_onEncRotN(frame);     break;
    case FdEventType::ENC_PUSHD:   if (m_onEncPushD)    m_onEncPushD(frame);    break;
    case FdEventType::ENC_PUSHU:   if (m_onEncPushU)    m_onEncPushU(frame);    break;
    }
}


/// @brief 切断後に再接続できるまで待機する
/// @param intervalMs 再試行間隔
void UsbComm::waitAndReconnect(uint32_t intervalMs)
{
    // ------------------ 再接続できるまで待機 ------------------
    while (m_running) {
        Sleep(intervalMs);
        if (!m_running) break;
        if (m_dev.open()) return;
    }
}
