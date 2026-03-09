#ifndef _USBCOMM_HPP_
#define _USBCOMM_HPP_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <array>
#include "Constants.hpp"
#include "Display.hpp"
#include "LevelMeter.hpp"
#include "common/DfFrame.hpp"
#include "common/FdFrame.hpp"

class UsbComm{
public:
    UsbComm(Display& display, LevelMeter& levelMeter, uint8_t& slaveCount);

    void appendRxData(std::span<const uint8_t> data);
    void appendRxData(const uint8_t* data, size_t size);
    size_t routine();
    bool tryPopTxFrame(std::vector<uint8_t>& frame);

private:
    static constexpr size_t MAX_FRAME_LENGTH = DF_ADDR_DATA + (Constants::Display::BUFSIZE16 * sizeof(uint16_t));
    static constexpr size_t LEVELMETER_PAYLOAD_SIZE = sizeof(uint16_t);
    static constexpr size_t RX_BUFFER_CAPACITY = MAX_FRAME_LENGTH * Constants::Usb::RX_BUF_FRAMECOUNT;
    static constexpr size_t TX_QUEUE_CAPACITY = Constants::Usb::RX_BUF_FRAMECOUNT;

    Display& m_display;
    LevelMeter& m_levelMeter;
    uint8_t& m_slaveCount;
    std::vector<uint8_t> m_rxBuffer;
    std::vector<std::vector<uint8_t>> m_txFrameQueue;

    bool m_dropFrameActive;
    size_t m_dropLengthBytes;
    size_t m_dropRemainingBytes;
    std::array<uint8_t, sizeof(DfFrame::FrameLength)> m_dropLengthBuffer;

    bool tryPopFrame(std::vector<uint8_t>& rawFrame);
    bool applyFrame(const DfFrame& frame);
    void handleDropFrame(const uint8_t* data, size_t size, size_t& index);
    void enqueueReceiveResult(bool ok);
};

#endif
