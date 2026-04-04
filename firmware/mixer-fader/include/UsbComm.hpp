#ifndef _USBCOMM_HPP_
#define _USBCOMM_HPP_

#include "Endian.hpp"
#include <cstddef>
#include <stdint.h>
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
    UsbComm(Display& disp, LevelMeter& lvl);
    void putRxByteBuf(uint8_t b);
    size_t getTxByteBuf(const uint8_t*& p, size_t& maxsize);
    bool putTxFrame(FdFrame& f);
    void sendStatus();
    void sendInitialized();
    
    void processReceivedFrame();

    bool isRxFull();
    bool isTxFull();
    bool isTxEmpty();
    bool isRxEmpty();
    
private:
    Display& m_disp;
    LevelMeter& m_lvl;

    std::vector<uint8_t> m_rxBuf[Constants::Usb::RX_BUF_FRAMECOUNT];
    std::vector<uint8_t> m_txBuf[Constants::Usb::TX_BUF_FRAMECOUNT];

    size_t m_rxBufIdxIn;
    size_t m_rxBufIdxOut;
    size_t m_txBufIdxIn;
    size_t m_txBufIdxOut;

};

#endif
