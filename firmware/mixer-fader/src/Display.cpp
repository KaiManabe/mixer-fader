#include "Display.hpp"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

#define OFFSET_X 26
#define OFFSET_Y 1
#define MINIMUM_SLEEP() __asm volatile ("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n")

Display *Display::instance = nullptr;

Display& Display::getInstance(){
    if(instance == nullptr){
        instance = new Display();
    }
    return *instance;
}


Display::Display():
    m_slaveCount(0),
    m_spi(spi0)
{   
    printf("Initializing display manager...\n");
    sem_init(&m_dffSemaphore, 1, 1);
    sem_init(&m_bufSemaphore, 1, 1);

    sem_acquire_blocking(&m_bufSemaphore);
    memset(m_buf, 0, sizeof(m_buf));
    sem_release(&m_bufSemaphore);

    sem_acquire_blocking(&m_dffSemaphore);
    m_slaveCount = 255;
    recountSlaves();
    sem_release(&m_dffSemaphore);

    sem_acquire_blocking(&m_bufSemaphore);
    sem_acquire_blocking(&m_dffSemaphore);
    init();
    sem_release(&m_dffSemaphore);
    sem_release(&m_bufSemaphore);


    // ------------------------- 黒塗り -------------------------
    sem_acquire_blocking(&m_bufSemaphore);
    sem_acquire_blocking(&m_dffSemaphore);
    dim();
    sem_release(&m_dffSemaphore);
    sem_release(&m_bufSemaphore);

    gpio_put(Constants::Gpio::DISP_DFF_CLR, 1);
}


void Display::dim(){
    for(uint8_t i = 0; i < m_slaveCount + 1; ++i){
        selectSlave(i);
        transfer();
    }
}

uint8_t& Display::getSlaveCountReference(){
    return m_slaveCount;
}


void Display::transferBuffer(
    uint8_t slave_id,
    std::span<uint16_t, Constants::Display::BUFSIZE16> img
){
    sem_acquire_blocking(&m_bufSemaphore);
    sem_acquire_blocking(&m_dffSemaphore);
    memcpy(m_buf, img.data(), sizeof(m_buf));
    selectSlave(slave_id);
    transfer();
    sem_release(&m_dffSemaphore);
    sem_release(&m_bufSemaphore);
}


void Display::routine(){
    sem_acquire_blocking(&m_dffSemaphore);
    recountSlaves();
    sem_release(&m_dffSemaphore);
}


void Display::recountSlaves(){
    using namespace Constants::Gpio;
    
    // -------------------------- CLR --------------------------
    gpio_put(DISP_DFF_CLR, 0);
    gpio_put(DISP_DFF_CLK, 0);
    MINIMUM_SLEEP();
    gpio_put(DISP_DFF_CLR, 1);
    MINIMUM_SLEEP();

    // ----------------------- クロック送信 -----------------------
    gpio_put(DISP_DFF_D, 1);
    
    uint8_t ct = 0;
    for(uint8_t i = 0; i < Constants::MAX_SLAVES; ++i){
        MINIMUM_SLEEP();
        gpio_put(DISP_DFF_CLK, 1);
        MINIMUM_SLEEP();
        gpio_put(DISP_DFF_CLK, 0);

        if(gpio_get(DISP_LOOPBACK)){
            ct = i;
            break;
        }
    }
    
    if(m_slaveCount != ct){
        m_slaveCount = ct;
        initializeAllSlaves();
        printf("Detected %d slaves.\n", ct);
    }
}

void Display::selectSlave(uint8_t slave_id){
    if(slave_id > m_slaveCount) return;

    using namespace Constants::Gpio;
    // -------------------------- CLR --------------------------
    gpio_put(DISP_DFF_CLR, 0);
    gpio_put(DISP_DFF_D, 0);
    gpio_put(DISP_DFF_CLK, 0);
    MINIMUM_SLEEP();
    gpio_put(DISP_DFF_CLR, 1);
    MINIMUM_SLEEP();


    // ----------------------- クロック送信 -----------------------
    for(int i = Constants::MAX_SLAVES + 2; i >= 0; --i){
        bool toSelect = i == slave_id;
        gpio_put(DISP_DFF_D, !toSelect);
        MINIMUM_SLEEP();
        gpio_put(DISP_DFF_CLK, 1);
        MINIMUM_SLEEP();
        gpio_put(DISP_DFF_CLK, 0);
    }
}

void Display::init(){
    m_dma = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(m_dma);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, spi_get_dreq(m_spi, true));
    channel_config_set_bswap(&c, false);

    dma_channel_configure(
        m_dma,
        &c,
        &spi_get_hw(m_spi)->dr,
        m_buf,
        Constants::Display::BUFSIZE16,
        false
    );
}


void Display::initializeAllSlaves(){
    using namespace Constants::Gpio;
    
    for(uint8_t i = 0; i < m_slaveCount + 1; ++i){
        selectSlave(i);

        spiWriteCmd(0x01);
        sleep_ms(120);
        spiWriteCmd(0x11);
        sleep_ms(120);

        spiWriteCmd(0x3A);
        spiWriteData(0x05);

        // BGR order
        spiWriteCmd(0x36);
        spiWriteData(0x08);

        // Inversion
        spiWriteCmd(0x21);

        // DISPON
        spiWriteCmd(0x29);
    }
}


void Display::transfer(){
    // --------------------- windowサイズ送信 ---------------------
    const uint16_t x1 = OFFSET_X;
    const uint16_t x2 = Constants::Display::WIDTH - 1 + OFFSET_X;
    const uint16_t y1 = OFFSET_Y;
    const uint16_t y2 = Constants::Display::HEIGHT - 1 + OFFSET_Y;

    spiWriteCmd(0x2A);
    spiWriteData(x1 >> 8);
    spiWriteData(x1 & 0xFF);
    spiWriteData(x2 >> 8);
    spiWriteData(x2 & 0xFF);

    spiWriteCmd(0x2B);
    spiWriteData(y1 >> 8);
    spiWriteData(y1 & 0xFF);
    spiWriteData(y2 >> 8);
    spiWriteData(y2 & 0xFF);



    // ------------------------ データ転送 ------------------------
    spiWriteCmd(0x2C);
    spiDataMode();
    while (spi_is_busy(m_spi)) tight_loop_contents();
    spi_set_format(m_spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    dma_channel_transfer_from_buffer_now(m_dma, m_buf, Constants::Display::BUFSIZE16);
    dma_channel_wait_for_finish_blocking(m_dma);
    while (spi_is_busy(m_spi)) tight_loop_contents();
    spi_set_format(m_spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
}


void Display::spiWriteCmd(uint8_t cmd){
    spiCmdMode();
    spi_write_blocking(m_spi, &cmd, 1);

}

void Display::spiWriteData(uint8_t data){
    spiDataMode();
    spi_write_blocking(m_spi, &data, 1);

}

void Display::spiCmdMode(){
    using namespace Constants::Gpio;
    gpio_put(DISP_DC, 0);
}

void Display::spiDataMode(){
    using namespace Constants::Gpio;
    gpio_put(DISP_DC, 1);
}
