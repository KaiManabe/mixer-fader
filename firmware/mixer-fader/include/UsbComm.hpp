#ifndef _USBCOMM_HPP_
#define _USBCOMM_HPP_

#include "Endian.hpp"
#include <array>
#include <cstddef>
#include <stdint.h>
#include <vector>
#include "Constants.hpp"
#include "Display.hpp"
#include "LevelMeter.hpp"
#include "common/DfFrame.hpp"
#include "common/FdFrame.hpp"

class UsbComm{
public:
    static constexpr size_t DISPLAY_ICON_PIXELS =
        static_cast<size_t>(Constants::Display::WIDTH) *
        static_cast<size_t>(Constants::Display::HEIGHT / 2u);
    static constexpr size_t DISPLAY_ICON_BYTES =
        DISPLAY_ICON_PIXELS * sizeof(uint16_t);
    static constexpr uint8_t DISPLAY_QUEUE_DEPTH = 2;
    static constexpr uint8_t CONTROL_QUEUE_DEPTH = 16;

    UsbComm(Display& disp, LevelMeter& lvl);
    void putRxBytes(const uint8_t* data, size_t size);
    void putRxByteBuf(uint8_t b);
    size_t getTxByteBuf(const uint8_t*& p, size_t& maxsize);
    bool putTxFrame(FdFrame& f);
    void sendInitialized();
    void routine();

    bool isTxFull();
    bool isTxEmpty();
    
private:
    struct DisplayCommand {
        uint8_t slaveId;
        uint16_t sequence;
        std::array<uint8_t, DISPLAY_ICON_BYTES> payload;
    };

    struct ControlCommand {
        uint8_t slaveId;
        uint16_t sequence;
        uint16_t pattern;
    };

    Display& m_disp;
    LevelMeter& m_lvl;

    std::vector<uint8_t> m_txBuf[Constants::Usb::TX_BUF_FRAMECOUNT];
    std::vector<uint8_t> m_rxCurrentFrame;
    uint32_t m_rxExpectedSize;
    std::array<uint8_t, sizeof(uint32_t)> m_rxLengthBuf;
    size_t m_rxLengthBytes;
    size_t m_rxBytesReceived;

    size_t m_txBufIdxIn;
    size_t m_txBufIdxOut;
    std::array<DisplayCommand, DISPLAY_QUEUE_DEPTH> m_displayQueue;
    size_t m_displayQueueHead;
    size_t m_displayQueueTail;
    size_t m_displayQueueCount;
    std::array<ControlCommand, CONTROL_QUEUE_DEPTH> m_controlQueue;
    size_t m_controlQueueHead;
    size_t m_controlQueueTail;
    size_t m_controlQueueCount;
    std::array<uint16_t, Constants::Display::BUFSIZE16> m_displayScratch;
    uint16_t m_lastDisplayAck;
    uint16_t m_lastControlAck;
    bool m_statusDirty;

    uint8_t getSlaveCount() const;
    uint8_t getDisplayCredits() const;
    uint8_t getControlCredits() const;
    void setStatusDirty();
    void sendStatus(FdEventType eventType = FdEventType::STATUS, const std::vector<uint8_t>& args = {});
    void fillFrameStatus(FdFrame& f) const;
    void tryQueueStatusFrame();
    void handleReceivedFrame(const DfFrame& f);
    bool enqueueDisplayCommand(const DfFrame& f);
    bool enqueueControlCommand(const DfFrame& f);
    bool isDisplayQueueFull() const;
    bool isControlQueueFull() const;
    bool isDisplayQueueEmpty() const;
    bool isControlQueueEmpty() const;
    void processAllControlCommands();
    void processOneDisplayCommand();
};

#endif
