#include "UsbComm.hpp"

#include <algorithm>
#include <string.h>

namespace {

constexpr size_t DISPLAY_ICON_BYTES = 80u * 80u * 2u;

}

UsbComm::UsbComm(WinUsbDevice& dev)
    : m_dev(dev),
      m_displayCredits(0),
      m_controlCredits(0),
      m_deviceSlaveCount(0),
      m_displayAck(0),
      m_controlAck(0),
      m_nextDisplaySeq(1),
      m_nextControlSeq(1),
      m_haveFlowState(false)
{
    clearPending();
}


bool UsbComm::send(DfFrame& frame)
{
    auto buf = frame.serialize();
    int written = m_dev.bulkWrite(buf);
    return written == static_cast<int>(buf.size());
}


bool UsbComm::sendLevelMeter(uint8_t slaveId, uint16_t pattern)
{
    if (slaveId >= MAX_TARGETS) {
        return false;
    }

    DfFrame frame;
    frame.dataType = DfDataType::LEVELMETER;
    frame.slaveId = slaveId;
    frame.sequence = m_nextControlSeq++;
    frame.frameData.resize(sizeof(uint16_t));
    memcpy(frame.frameData.data(), &pattern, sizeof(pattern));

    m_pendingControls[slaveId].occupied = true;
    m_pendingControls[slaveId].frame = std::move(frame);
    return true;
}


bool UsbComm::sendDisplay(uint8_t slaveId, const uint8_t* pixelData, size_t size)
{
    if (slaveId >= MAX_TARGETS) {
        return false;
    }
    if (pixelData == nullptr || size == 0) {
        return false;
    }

    DfFrame frame;
    frame.dataType = DfDataType::DISPLAY_ICON;
    frame.slaveId = slaveId;
    frame.sequence = m_nextDisplaySeq++;
    const size_t transportSize = (std::min)(size, DISPLAY_ICON_BYTES);
    frame.frameData.assign(pixelData, pixelData + transportSize);

    m_pendingDisplays[slaveId].occupied = true;
    m_pendingDisplays[slaveId].frame = std::move(frame);
    return true;
}


void UsbComm::flush()
{
    if (!m_haveFlowState) {
        return;
    }

    while (true) {
        bool sent = false;

        if (m_controlCredits > 0) {
            sent = flushOneControl();
        }

        if (!sent && m_displayCredits > 0) {
            sent = flushOneDisplay();
        }

        if (!sent) {
            break;
        }
    }
}


void UsbComm::clearPending()
{
    for (auto& pending : m_pendingDisplays) {
        pending.occupied = false;
        pending.frame.frameData.clear();
    }

    for (auto& pending : m_pendingControls) {
        pending.occupied = false;
        pending.frame.frameData.clear();
    }
}


void UsbComm::resetFlowControl()
{
    m_displayCredits = 0;
    m_controlCredits = 0;
    m_deviceSlaveCount = 0;
    m_displayAck = 0;
    m_controlAck = 0;
    m_haveFlowState = false;
}


std::optional<FdFrame> UsbComm::receive(uint32_t timeoutMs)
{
    auto rxData = m_dev.bulkRead(64, timeoutMs);
    if (rxData.empty()) {
        return std::nullopt;
    }

    FdFrame frame;
    if (!frame.deserialize(rxData)) {
        return std::nullopt;
    }

    updateFlowState(frame);
    return frame;
}


uint8_t UsbComm::deviceSlaveCount() const
{
    return m_deviceSlaveCount;
}


uint8_t UsbComm::displayCredits() const
{
    return m_displayCredits;
}


uint8_t UsbComm::controlCredits() const
{
    return m_controlCredits;
}


uint16_t UsbComm::displayAck() const
{
    return m_displayAck;
}


uint16_t UsbComm::controlAck() const
{
    return m_controlAck;
}


bool UsbComm::hasFlowState() const
{
    return m_haveFlowState;
}


void UsbComm::updateFlowState(const FdFrame& frame)
{
    m_deviceSlaveCount = frame.slaveCount;
    m_displayCredits = frame.displayCredits;
    m_controlCredits = frame.controlCredits;
    m_displayAck = frame.displayAck;
    m_controlAck = frame.controlAck;
    m_haveFlowState = true;
}


bool UsbComm::flushOneControl()
{
    if (m_controlCredits == 0) {
        return false;
    }

    for (auto& pending : m_pendingControls) {
        if (!pending.occupied) {
            continue;
        }

        if (!send(pending.frame)) {
            return false;
        }

        pending.occupied = false;
        pending.frame.frameData.clear();
        --m_controlCredits;
        return true;
    }

    return false;
}


bool UsbComm::flushOneDisplay()
{
    if (m_displayCredits == 0) {
        return false;
    }

    for (auto& pending : m_pendingDisplays) {
        if (!pending.occupied) {
            continue;
        }

        if (!send(pending.frame)) {
            return false;
        }

        pending.occupied = false;
        pending.frame.frameData.clear();
        --m_displayCredits;
        return true;
    }

    return false;
}
