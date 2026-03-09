#ifndef _DISPLAY_HPP_
#define _DISPLAY_HPP_

#include <stdint.h>
#include <span>
#include <memory>
#include "Constants.hpp"
#include "pico/sem.h"
#include "hardware/spi.h"

class Display{
    public:
        static Display& getInstance();

        uint8_t& getSlaveCountReference();
        void transferBuffer(uint8_t slave_id, std::span<uint16_t, Constants::Display::BUFSIZE16> img);
        void routine();

    private:
        static Display *instance;
        uint16_t m_buf[Constants::Display::BUFSIZE16];
        uint8_t m_slaveCount;
        semaphore_t m_dffSemaphore;
        semaphore_t m_bufSemaphore;
        spi_inst_t *m_spi;
        int m_dma;

        Display();
        void init();
        void dim();

        void initializeAllSlaves();
        void recountSlaves();
        void selectSlave(uint8_t slave_id);
        void transfer();

        void spiWriteCmd(uint8_t cmd);
        void spiWriteData(uint8_t data);
        void spiCmdMode();
        void spiDataMode();
};

#endif
