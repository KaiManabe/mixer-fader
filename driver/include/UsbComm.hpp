#ifndef _DRIVER_USBCOMM_HPP_
#define _DRIVER_USBCOMM_HPP_

#include <array>
#include <cstddef>
#include <optional>
#include <stdint.h>

#include "WinUsbDevice.hpp"
#include "common/DfFrame.hpp"
#include "common/FdFrame.hpp"

class UsbComm {
public:
    static constexpr size_t MAX_TARGETS = 9;

    explicit UsbComm(WinUsbDevice& dev);

    bool send(DfFrame& frame);
    bool sendLevelMeter(uint8_t slaveId, uint16_t pattern);
    bool sendDisplay(uint8_t slaveId, const uint8_t* pixelData, size_t size);
    void flush();
    void clearPending();
    void resetFlowControl();

    std::optional<FdFrame> receive(uint32_t timeoutMs = 2000);
    uint8_t deviceSlaveCount() const;
    uint8_t displayCredits() const;
    uint8_t controlCredits() const;
    uint16_t displayAck() const;
    uint16_t controlAck() const;
    bool hasFlowState() const;

private:
    struct PendingFrame {
        bool occupied;
        DfFrame frame;
    };

    WinUsbDevice& m_dev;
    std::array<PendingFrame, MAX_TARGETS> m_pendingDisplays;
    std::array<PendingFrame, MAX_TARGETS> m_pendingControls;
    uint8_t m_displayCredits;
    uint8_t m_controlCredits;
    uint8_t m_deviceSlaveCount;
    uint16_t m_displayAck;
    uint16_t m_controlAck;
    uint16_t m_nextDisplaySeq;
    uint16_t m_nextControlSeq;
    bool m_haveFlowState;

    void updateFlowState(const FdFrame& frame);
    bool flushOneControl();
    bool flushOneDisplay();
};

#endif
