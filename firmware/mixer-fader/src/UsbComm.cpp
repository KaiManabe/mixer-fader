#include "UsbComm.hpp"

#include <algorithm>
#include <span>
#include <string.h>

namespace {

constexpr size_t TX_FRAME_STORAGE_BYTES = FD_ADDR_ARGUMENTS + 8u;

}

UsbComm::UsbComm(Display& disp, LevelMeter& lvl):
    m_disp(disp),
    m_lvl(lvl),
    m_rxExpectedSize(0),
    m_rxLengthBytes(0),
    m_rxBytesReceived(0),
    m_txBufIdxIn(0),
    m_txBufIdxOut(0),
    m_displayQueueHead(0),
    m_displayQueueTail(0),
    m_displayQueueCount(0),
    m_controlQueueHead(0),
    m_controlQueueTail(0),
    m_controlQueueCount(0),
    m_lastDisplayAck(0),
    m_lastControlAck(0),
    m_statusDirty(true)
{
    m_rxCurrentFrame.reserve(DF_ADDR_DATA + DISPLAY_ICON_BYTES);

    for (size_t i = 0; i < Constants::Usb::TX_BUF_FRAMECOUNT; ++i) {
        m_txBuf[i].resize(TX_FRAME_STORAGE_BYTES);
    }

    std::fill(m_displayScratch.begin(), m_displayScratch.end(), 0);
}


void UsbComm::putRxBytes(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        putRxByteBuf(data[i]);
    }
}


void UsbComm::putRxByteBuf(uint8_t b) {
    if (m_rxExpectedSize == 0) {
        m_rxLengthBuf[m_rxLengthBytes++] = b;
        if (m_rxLengthBytes < sizeof(uint32_t)) {
            return;
        }

        uint32_t nextFrameSize = 0;
        memcpy(&nextFrameSize, m_rxLengthBuf.data(), sizeof(nextFrameSize));
        m_rxLengthBytes = 0;

        if (nextFrameSize < DF_ADDR_DATA || nextFrameSize > (DF_ADDR_DATA + DISPLAY_ICON_BYTES)) {
            m_rxExpectedSize = 0;
            m_rxBytesReceived = 0;
            m_rxCurrentFrame.clear();
            return;
        }

        m_rxExpectedSize = nextFrameSize;
        m_rxBytesReceived = sizeof(uint32_t);
        m_rxCurrentFrame.assign(nextFrameSize, 0);
        memcpy(m_rxCurrentFrame.data(), m_rxLengthBuf.data(), sizeof(uint32_t));
        return;
    }

    if (m_rxBytesReceived >= m_rxExpectedSize) {
        m_rxExpectedSize = 0;
        m_rxBytesReceived = 0;
        m_rxCurrentFrame.clear();
        return;
    }

    m_rxCurrentFrame[m_rxBytesReceived++] = b;

    if (m_rxBytesReceived < m_rxExpectedSize) {
        return;
    }

    DfFrame frame;
    if (frame.deserialize(m_rxCurrentFrame)) {
        handleReceivedFrame(frame);
    }

    m_rxExpectedSize = 0;
    m_rxBytesReceived = 0;
    m_rxCurrentFrame.clear();
}


size_t UsbComm::getTxByteBuf(const uint8_t*& p, size_t& maxsize){
    p = nullptr;
    if (isTxEmpty()) {
        return 0;
    }

    auto& frame = m_txBuf[m_txBufIdxOut];
    if (frame.size() < FD_ADDR_ARGUMENTS) {
        return 0;
    }

    uint32_t frameSize = 0;
    memcpy(&frameSize, &frame[FD_ADDR_FRAMELENGTH], sizeof(frameSize));

    if (frameSize < FD_ADDR_ARGUMENTS) {
        return 0;
    }
    if (frameSize > frame.size()) {
        return 0;
    }
    if (frameSize > maxsize) {
        return 0;
    }

    p = frame.data();
    maxsize = frameSize;
    m_txBufIdxOut = (m_txBufIdxOut + 1) % Constants::Usb::TX_BUF_FRAMECOUNT;
    return frameSize;
}


bool UsbComm::putTxFrame(FdFrame& f){
    if (isTxFull()) {
        return false;
    }

    fillFrameStatus(f);

    auto serialized = f.serialize();
    if (serialized.size() > m_txBuf[m_txBufIdxIn].size()) {
        return false;
    }

    memcpy(m_txBuf[m_txBufIdxIn].data(), serialized.data(), serialized.size());
    m_txBufIdxIn = (m_txBufIdxIn + 1) % Constants::Usb::TX_BUF_FRAMECOUNT;
    return true;
}


void UsbComm::sendInitialized(){
    sendStatus(FdEventType::INITIALIZED);
}


void UsbComm::routine() {
    processAllControlCommands();
    processOneDisplayCommand();
    tryQueueStatusFrame();
}


bool UsbComm::isTxFull(){
    return ((m_txBufIdxIn + 1) % Constants::Usb::TX_BUF_FRAMECOUNT) == m_txBufIdxOut;
}


bool UsbComm::isTxEmpty(){
    return m_txBufIdxOut == m_txBufIdxIn;
}


uint8_t UsbComm::getSlaveCount() const {
    return static_cast<uint8_t>(std::min(
        static_cast<size_t>(m_disp.getSlaveCountReference()),
        static_cast<size_t>(Constants::MAX_SLAVES)));
}


uint8_t UsbComm::getDisplayCredits() const {
    return static_cast<uint8_t>(DISPLAY_QUEUE_DEPTH - m_displayQueueCount);
}


uint8_t UsbComm::getControlCredits() const {
    return static_cast<uint8_t>(CONTROL_QUEUE_DEPTH - m_controlQueueCount);
}


void UsbComm::setStatusDirty() {
    m_statusDirty = true;
}


void UsbComm::fillFrameStatus(FdFrame& f) const {
    f.protocolVersion = FD_PROTOCOL_VERSION;
    f.flags = 0;
    f.slaveCount = getSlaveCount();
    f.displayCredits = getDisplayCredits();
    f.controlCredits = getControlCredits();
    f.displayAck = m_lastDisplayAck;
    f.controlAck = m_lastControlAck;
}


void UsbComm::sendStatus(FdEventType eventType, const std::vector<uint8_t>& args) {
    FdFrame f;
    f.eventType = eventType;
    f.eventArguments = args;
    if (putTxFrame(f)) {
        m_statusDirty = false;
    }
}


void UsbComm::tryQueueStatusFrame() {
    if (!m_statusDirty || isTxFull()) {
        return;
    }

    sendStatus();
}


void UsbComm::handleReceivedFrame(const DfFrame& f) {
    bool accepted = false;

    switch (f.dataType) {
    case DfDataType::DISPLAY_ICON:
        accepted = enqueueDisplayCommand(f);
        break;

    case DfDataType::LEVELMETER:
        accepted = enqueueControlCommand(f);
        break;
    }

    if (!accepted) {
        setStatusDirty();
        tryQueueStatusFrame();
        return;
    }

    setStatusDirty();
}


bool UsbComm::enqueueDisplayCommand(const DfFrame& f) {
    if (f.frameData.size() != DISPLAY_ICON_BYTES) {
        return false;
    }
    if (isDisplayQueueFull()) {
        return false;
    }

    auto& slot = m_displayQueue[m_displayQueueTail];
    slot.slaveId = f.slaveId;
    slot.sequence = f.sequence;
    memcpy(slot.payload.data(), f.frameData.data(), DISPLAY_ICON_BYTES);

    m_displayQueueTail = (m_displayQueueTail + 1) % DISPLAY_QUEUE_DEPTH;
    ++m_displayQueueCount;
    return true;
}


bool UsbComm::enqueueControlCommand(const DfFrame& f) {
    if (f.frameData.size() != sizeof(uint16_t)) {
        return false;
    }
    if (isControlQueueFull()) {
        return false;
    }

    auto& slot = m_controlQueue[m_controlQueueTail];
    slot.slaveId = f.slaveId;
    slot.sequence = f.sequence;
    memcpy(&slot.pattern, f.frameData.data(), sizeof(slot.pattern));

    m_controlQueueTail = (m_controlQueueTail + 1) % CONTROL_QUEUE_DEPTH;
    ++m_controlQueueCount;
    return true;
}


bool UsbComm::isDisplayQueueFull() const {
    return m_displayQueueCount >= DISPLAY_QUEUE_DEPTH;
}


bool UsbComm::isControlQueueFull() const {
    return m_controlQueueCount >= CONTROL_QUEUE_DEPTH;
}


bool UsbComm::isDisplayQueueEmpty() const {
    return m_displayQueueCount == 0;
}


bool UsbComm::isControlQueueEmpty() const {
    return m_controlQueueCount == 0;
}


void UsbComm::processAllControlCommands() {
    while (!isControlQueueEmpty()) {
        auto& slot = m_controlQueue[m_controlQueueHead];
        m_lvl.setLevelAsPattern(slot.slaveId, slot.pattern);
        m_lastControlAck = slot.sequence;
        m_controlQueueHead = (m_controlQueueHead + 1) % CONTROL_QUEUE_DEPTH;
        --m_controlQueueCount;
        setStatusDirty();
    }
}


void UsbComm::processOneDisplayCommand() {
    if (isDisplayQueueEmpty()) {
        return;
    }

    auto& slot = m_displayQueue[m_displayQueueHead];
    const size_t iconWords = DISPLAY_ICON_PIXELS;
    const size_t iconBytes = iconWords * sizeof(uint16_t);

    memcpy(m_displayScratch.data(), slot.payload.data(), iconBytes);
    std::fill_n(m_displayScratch.begin() + iconWords, m_displayScratch.size() - iconWords, 0);

    auto fullFrame = std::span<uint16_t, Constants::Display::BUFSIZE16>(
        m_displayScratch.data(),
        m_displayScratch.size());
    m_disp.transferBuffer(slot.slaveId, fullFrame);

    m_lastDisplayAck = slot.sequence;
    m_displayQueueHead = (m_displayQueueHead + 1) % DISPLAY_QUEUE_DEPTH;
    --m_displayQueueCount;
    setStatusDirty();
}
