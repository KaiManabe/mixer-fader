#ifndef _ENCODER_HPP_
#define _ENCODER_HPP_

#include <stdint.h>
#include <functional>
#include <vector>
#include <memory>
#include <Constants.hpp>
#include "hardware/dma.h"
#include "hardware/pio.h"

enum class EncoderEventType{
    ENC_ROTN,
    ENC_ROTP,
    ENC_PUSHDOWN,
    ENC_PUSHUP,
};

typedef struct{
    uint8_t slave_id;
    EncoderEventType event;
} EncoderEvent;


class Encoder{
    public:
        Encoder(uint8_t& slave_count);
        void attachEventListener(std::function<void(EncoderEvent)> f);
        void routine();
    private:
        std::vector<std::function<void(EncoderEvent)>> m_listeners;
        uint8_t m_buf[Constants::Encoder::BYTES_PER_UNIT * (2 + Constants::MAX_SLAVES)];
        uint8_t& m_slaveCount;
        uint8_t m_dmaCh;
        uint8_t m_pioSm;
        PIO m_pio;

        void initPio();
        void runPioOnce();
        void parseSerialData();
        void callAllEventListeners(EncoderEvent e);
};

#endif