#ifndef _LEVELMETER_HPP_
#define _LEVELMETER_HPP_

#include <stdint.h>
#include "hardware/pio.h"
#include "Constants.hpp"


class LevelMeter{
    public:
        LevelMeter(uint8_t& slave_count);
        void setLevelAsValue(uint8_t slave_id, uint8_t v);
        void setLevelAsPattern(uint8_t slave_id, uint16_t p);
        void setDutyRatio(uint8_t x);
        void routine();

    private:
        uint8_t& m_slaveCount;
        uint16_t m_pattern[Constants::MAX_SLAVES + 1];
        uint16_t m_buf[Constants::MAX_SLAVES + 1];
        uint8_t m_dmaCh;
        uint8_t m_pioSm;
        PIO m_pio;
        uint8_t m_duty;

        void init_pio();
};

#endif