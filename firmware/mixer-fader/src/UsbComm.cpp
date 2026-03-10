#include "UsbComm.hpp"


UsbComm::UsbComm(Display& disp, LevelMeter& lvl):
    m_rxBufIdxIn(0),
    m_rxBufIdxOut(0),
    m_txBufIdxIn(0),
    m_txBufIdxOut(0),
    m_disp(disp),
    m_lvl(lvl)
{
    for(int i = 0; i < Constants::Usb::RX_BUF_FRAMECOUNT; ++i){
        m_rxBuf[i].resize(DF_ADDR_DATA + Constants::Display::BUFSIZE16 * sizeof(uint16_t));
    }
    for(int i = 0; i < Constants::Usb::TX_BUF_FRAMECOUNT; ++i){
        m_txBuf[i].resize(FD_ADDR_ARGUMENTS + ARGUMENT_LENGTH);
    }
}


void UsbComm::putRxByteBuf(uint8_t b){
    static uint32_t idx = DF_ADDR_FRAMELENGTH + sizeof(uint32_t);
    static uint32_t acceptingBytes = 0;
    static uint32_t ignoreingBytes = 0;
    static uint32_t incomingFrameSize = 0;
    static uint32_t nextFrameSize = 0;

    if(acceptingBytes > 0){
        m_rxBuf[m_rxBufIdxIn][idx] = b;
        idx++;
        acceptingBytes--;

        if(acceptingBytes == 0){
            idx = DF_ADDR_FRAMELENGTH + sizeof(uint32_t);
            m_rxBufIdxIn = (m_rxBufIdxIn + 1) % Constants::Usb::RX_BUF_FRAMECOUNT;
            ignoreingBytes = DF_ADDR_FRAMELENGTH;
        }
        return;
    }

    if(ignoreingBytes > 0){
        ignoreingBytes--; 
        return;
    }


    nextFrameSize |= b << (8 * incomingFrameSize);
    if(++incomingFrameSize != sizeof(uint32_t)) return;

    if(!isRxFull() && nextFrameSize <= m_rxBuf[0].size()){
        acceptingBytes = nextFrameSize - DF_ADDR_FRAMELENGTH - sizeof(uint32_t);
        memcpy(&m_rxBuf[m_rxBufIdxIn][DF_ADDR_FRAMELENGTH], &nextFrameSize, sizeof(uint32_t));
        idx = DF_ADDR_FRAMELENGTH + sizeof(uint32_t);
    }else{
        ignoreingBytes = nextFrameSize - DF_ADDR_FRAMELENGTH - sizeof(uint32_t);
    }

    sendStatus();

    nextFrameSize = 0;
    incomingFrameSize = 0;
}


void UsbComm::processReceivedFrame(){
    if(isRxEmpty()) return;
    auto f = DfFrame();
    bool deserialized = f.deserialize(m_rxBuf[m_rxBufIdxOut]);
    m_rxBufIdxOut = (m_rxBufIdxOut + 1) % Constants::Usb::RX_BUF_FRAMECOUNT;
    if(!deserialized) return;

    switch(f.dataType){
    case DfDataType::DISPLAY:
        if(f.frameData.size() >= Constants::Display::BUFSIZE16 * sizeof(uint16_t)){
            auto imgSpan = std::span<uint16_t, Constants::Display::BUFSIZE16>(
                reinterpret_cast<uint16_t*>(f.frameData.data()),
                Constants::Display::BUFSIZE16
            );
            m_disp.transferBuffer(f.slaveId, imgSpan);
        }
        break;
    
    case DfDataType::LEVELMETER:
        if(f.frameData.size() >= sizeof(uint16_t)){
            uint16_t pattern = 0;
            memcpy(&pattern, f.frameData.data(), sizeof(uint16_t));
            m_lvl.setLevelAsPattern(f.slaveId, pattern);
        }
        break;
    }
}


bool UsbComm::putTxFrame(FdFrame& f){
    if(isTxFull()) return false;
    f.ready = !isRxFull();
    auto serialized = f.serialize();
    if(serialized.size() > m_txBuf[m_txBufIdxIn].size()) return false;
    memcpy(m_txBuf[m_txBufIdxIn].data(), serialized.data(), serialized.size());
    m_txBufIdxIn = (m_txBufIdxIn + 1) % Constants::Usb::TX_BUF_FRAMECOUNT;
    return true;
}

void UsbComm::sendStatus(){
    bool isBusy = isRxFull();
    auto f = FdFrame();
    f.eventType = FdEventType::STATUS;
    f.ready = !isBusy;
    f.eventArguments.clear();
    putTxFrame(f);
}

void UsbComm::sendInitialized(){
    auto f = FdFrame();
    f.eventType = FdEventType::INITIALIZED;
    f.ready = !isRxFull();
    f.eventArguments.clear();
    putTxFrame(f);
}


size_t UsbComm::getTxByteBuf(const uint8_t*& p, size_t& maxsize){
    p = nullptr;
    if(isTxEmpty()) return 0;

    auto& frame = m_txBuf[m_txBufIdxOut];
    if(frame.size() < FD_ADDR_ARGUMENTS) return 0;

    uint32_t frameSize = 0;
    memcpy(&frameSize, &frame[FD_ADDR_FRAMELENGTH], sizeof(uint32_t));

    if(frameSize < FD_ADDR_ARGUMENTS) return 0;
    if(frameSize > frame.size()) return 0;
    if(frameSize > maxsize) return 0;

    p = frame.data();
    maxsize = frameSize;
    m_txBufIdxOut = (m_txBufIdxOut + 1) % Constants::Usb::TX_BUF_FRAMECOUNT;
    return frameSize;
}


bool UsbComm::isRxFull(){
    if(m_rxBufIdxOut == 0 && m_rxBufIdxIn == Constants::Usb::RX_BUF_FRAMECOUNT - 1) return true;
    if(m_rxBufIdxOut - m_rxBufIdxIn == 1) return true;
    return false;
}
bool UsbComm::isTxFull(){
    if(m_txBufIdxOut == 0 && m_txBufIdxIn == Constants::Usb::TX_BUF_FRAMECOUNT - 1) return true;
    if(m_txBufIdxOut - m_txBufIdxIn == 1) return true;
    return false;
}

bool UsbComm::isRxEmpty(){
    return m_rxBufIdxOut == m_rxBufIdxIn;
}
bool UsbComm::isTxEmpty(){
    return m_txBufIdxOut == m_txBufIdxIn;
}