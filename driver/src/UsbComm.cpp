#include "UsbComm.hpp"
#include <string.h>
#include <stdio.h>


UsbComm::UsbComm(WinUsbDevice& dev)
    : m_dev(dev),
      m_running(false),
      m_deviceReady(false)
{
}


// ---- 送信 ----

bool UsbComm::send(DfFrame& frame)
{
    auto buf = frame.serialize();
    int written = m_dev.bulkWrite(buf);
    return written >= 0;
}


bool UsbComm::sendLevelMeter(uint8_t slaveId, uint16_t pattern)
{
    DfFrame frame;
    frame.dataType = DfDataType::LEVELMETER;
    frame.slaveId  = slaveId;
    frame.frameData.resize(sizeof(uint16_t));
    memcpy(frame.frameData.data(), &pattern, sizeof(uint16_t));
    return send(frame);
}


bool UsbComm::sendDisplay(uint8_t slaveId, const uint8_t* pixelData, size_t size)
{
    DfFrame frame;
    frame.dataType = DfDataType::DISPLAY;
    frame.slaveId  = slaveId;
    frame.frameData.assign(pixelData, pixelData + size);
    return send(frame);
}


// ---- 受信 ----

std::optional<FdFrame> UsbComm::receive(uint32_t timeoutMs)
{
    auto rxData = m_dev.bulkRead(64, timeoutMs);
    if (rxData.empty()) return std::nullopt;

    FdFrame frame;
    if (!frame.deserialize(rxData)) return std::nullopt;

    m_deviceReady = frame.ready;
    return frame;
}


void UsbComm::poll(uint32_t timeoutMs)
{
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

        auto frame = receive(timeoutMs);
        if (frame.has_value()) {
            dispatch(frame.value());
        }
    }
}


void UsbComm::stop()
{
    m_running = false;
}


// ---- コールバック登録 ----

void UsbComm::onEvent(EventCallback cb) { m_onEvent = cb; }
void UsbComm::onInitialized(EventCallback cb) { m_onInitialized = cb; }
void UsbComm::onStatus(EventCallback cb) { m_onStatus = cb; }
void UsbComm::onEncRotP(EventCallback cb) { m_onEncRotP = cb; }
void UsbComm::onEncRotN(EventCallback cb) { m_onEncRotN = cb; }
void UsbComm::onEncPushD(EventCallback cb) { m_onEncPushD = cb; }
void UsbComm::onEncPushU(EventCallback cb) { m_onEncPushU = cb; }
void UsbComm::onDisconnect(std::function<void()> cb) { m_onDisconnect = cb; }
void UsbComm::onConnect(std::function<void()> cb) { m_onConnect = cb; }


// ---- 状態 ----

bool UsbComm::isDeviceReady() const
{
    return m_deviceReady;
}


// ---- 内部 ----

void UsbComm::dispatch(const FdFrame& frame)
{
    if (m_onEvent) m_onEvent(frame);

    switch (frame.eventType) {
    case FdEventType::INITIALIZED: if (m_onInitialized) m_onInitialized(frame); break;
    case FdEventType::STATUS:      if (m_onStatus)      m_onStatus(frame);      break;
    case FdEventType::ENC_ROTP:    if (m_onEncRotP)     m_onEncRotP(frame);     break;
    case FdEventType::ENC_ROTN:    if (m_onEncRotN)     m_onEncRotN(frame);     break;
    case FdEventType::ENC_PUSHD:   if (m_onEncPushD)    m_onEncPushD(frame);    break;
    case FdEventType::ENC_PUSHU:   if (m_onEncPushU)    m_onEncPushU(frame);    break;
    }
}


void UsbComm::waitAndReconnect(uint32_t intervalMs)
{
    while (m_running) {
        Sleep(intervalMs);
        if (!m_running) break;
        if (m_dev.open()) return;
    }
}
