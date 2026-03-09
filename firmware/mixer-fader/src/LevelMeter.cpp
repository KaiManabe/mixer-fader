#include "LevelMeter.hpp"
#include <string.h>
#include <stdio.h>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "LedSipo.pio.h"


LevelMeter::LevelMeter(uint8_t& slave_count):
    m_slaveCount(slave_count),
    m_duty(Constants::LevelMeter::DUTY_RATIO)
{
    printf("Initializing LevelMeter manager...\n");
    memset(m_pattern, 0, sizeof(m_pattern));
    memset(m_buf, 0, sizeof(m_buf));
    init_pio();
}

void LevelMeter::setLevelAsValue(uint8_t slave_id, uint8_t v){
    uint16_t p = 0x0;
    for(uint8_t i = 0; i < 16; ++i){
        if(i < v) p |= 0x1 << i;
    }
    setLevelAsPattern(slave_id, p);
}


void LevelMeter::setLevelAsPattern(uint8_t slave_id, uint16_t p){
    if(slave_id >= m_slaveCount + 1) return;
    if(slave_id >= Constants::MAX_SLAVES + 1) return;
    m_pattern[slave_id] = p;
}


void LevelMeter::setDutyRatio(uint8_t x){
    if(x > 100) return;
    m_duty = x;
}


void LevelMeter::routine(){
    static uint8_t state = 0;
    const uint8_t MAXLED = Constants::LevelMeter::LIGHTING_LED_MAX;
    const uint8_t MODULE_COUNT = Constants::MAX_SLAVES + 1;

    for(uint8_t i = 0; i < MODULE_COUNT; ++i){
        const uint16_t invertedIdx = MODULE_COUNT - i - 1;
        m_buf[invertedIdx] = 0;
        for(uint8_t ii = 0; ii < 16; ++ii){
            bool pat = (m_pattern[i] >> ii) & 0x1;
            if (pat && ((uint8_t)(ii + 16 - state) % 16 < MAXLED)) {
                m_buf[invertedIdx] |= 0x1 << ii;
            }
        }
    }

    dma_channel_transfer_from_buffer_now(m_dmaCh, m_buf, MODULE_COUNT);

    if(++state >= 16){
        state = 0;
    }
}


void LevelMeter::init_pio(){
    using namespace Constants::Gpio;
    const uint8_t MODULE_COUNT = Constants::MAX_SLAVES + 1;
    const size_t sendingBits = MODULE_COUNT * 16;
    m_pio = pio0;
    m_pioSm = pio_claim_unused_sm(m_pio, true);

    // ------------------------ PIO初期化 ------------------------
    uint offset = pio_add_program(m_pio, &led_sipo_program);

    pio_sm_config c = led_sipo_program_get_default_config(offset);

    sm_config_set_out_pins(&c, LED_DATA, 1);
    sm_config_set_sideset_pins(&c, LED_CLK);
    sm_config_set_set_pins(&c, LED_LATCH, 1);

    sm_config_set_out_shift(&c, true, true, 16);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    sm_config_set_clkdiv(&c, 5.0f);

    pio_gpio_init(m_pio, LED_DATA);
    pio_gpio_init(m_pio, LED_CLK);
    pio_gpio_init(m_pio, LED_LATCH);
    pio_sm_set_consecutive_pindirs(m_pio, m_pioSm, LED_DATA, 1, true);
    pio_sm_set_consecutive_pindirs(m_pio, m_pioSm, LED_CLK, 1, true);
    pio_sm_set_consecutive_pindirs(m_pio, m_pioSm, LED_LATCH, 1, true);

    pio_sm_init(m_pio, m_pioSm, offset, &c);
    pio_sm_set_enabled(m_pio, m_pioSm, true);



    // ------------------------ DMA初期化 ------------------------
    m_dmaCh = dma_claim_unused_channel(true);
    dma_channel_config dmacfg = dma_channel_get_default_config(m_dmaCh);

    channel_config_set_transfer_data_size(&dmacfg, DMA_SIZE_16);
    channel_config_set_read_increment(&dmacfg, true);
    channel_config_set_write_increment(&dmacfg, false);
    channel_config_set_dreq(&dmacfg, pio_get_dreq(m_pio, m_pioSm, true));



    pio_sm_put_blocking(m_pio, m_pioSm, sendingBits - 1);
    dma_channel_configure(
        m_dmaCh, &dmacfg,
        &m_pio->txf[m_pioSm],
        m_buf,
        MODULE_COUNT,
        false 
    );
    pio_sm_clear_fifos(m_pio, m_pioSm);
    dma_channel_start(m_dmaCh);
}
