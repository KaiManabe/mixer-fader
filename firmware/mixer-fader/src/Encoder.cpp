#include <stdio.h>
#include "Encoder.hpp"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "EncoderPiso.pio.h"

Encoder::Encoder(uint8_t& slave_count):
    m_slaveCount(slave_count),
    m_listeners(),
    m_dmaCh(0)
{
    printf("Initializing Encoder manager...\n");
    initPio();
}

void Encoder::attachEventListener(std::function<void(EncoderEvent)> f){
    m_listeners.push_back(f);
}

void Encoder::routine(){
    runPioOnce();
    parseSerialData();
}

void Encoder::parseSerialData(){
    const uint8_t ENC_COUNT = (2 + Constants::MAX_SLAVES);
    const uint8_t DEBOUNCE_CT = Constants::Encoder::DEBOUNCECOUNT;
    static uint8_t called = 0;
    static uint8_t prevSlaveCount = m_slaveCount;
    static bool prevBt[ENC_COUNT] = {0};
    static bool prevPhaseA[ENC_COUNT] = {0};
    static bool prevPhaseB[ENC_COUNT] = {0};

    /* ---------------------------------------------------------
     状態取得
    --------------------------------------------------------- */
    static bool currentBt[ENC_COUNT][DEBOUNCE_CT] = {0};
    static bool currentPhaseA[ENC_COUNT][DEBOUNCE_CT] = {0};
    static bool currentPhaseB[ENC_COUNT][DEBOUNCE_CT] = {0};

    for(uint8_t i = 0; i < (m_slaveCount + 2); ++i){
        for(uint8_t ii = 1; ii < DEBOUNCE_CT; ++ii){
            currentPhaseA[i][ii - 1] = currentPhaseA[i][ii];
        }
        currentPhaseA[i][DEBOUNCE_CT - 1] = m_buf[i] & 0x1;
        
        for(uint8_t ii = 1; ii < DEBOUNCE_CT; ++ii){
            currentPhaseB[i][ii - 1] = currentPhaseB[i][ii];
        }
        currentPhaseB[i][DEBOUNCE_CT - 1] = (m_buf[i] >> 1) & 0x1;
        
        for(uint8_t ii = 1; ii < DEBOUNCE_CT; ++ii){
            currentBt[i][ii - 1] = currentBt[i][ii];
        }
        currentBt[i][DEBOUNCE_CT - 1] = (m_buf[i] >> 2) & 0x1;
    }

    /* ---------------------------------------------------------
     遷移判定
    --------------------------------------------------------- */
    uint8_t changeBt[ENC_COUNT] = {0,};
    uint8_t changePhaseA[ENC_COUNT] = {0,};
    uint8_t changePhaseB[ENC_COUNT] = {0,};

    auto getTransition = [](bool prev, bool current[DEBOUNCE_CT]){
        bool noBounce = true;
        for(uint8_t ii = 1; ii < DEBOUNCE_CT; ++ii){
            if(current[ii - 1] != current[ii]) {
                noBounce = false;
                break;
            }
        }

        bool stabilized = current[DEBOUNCE_CT - 1];
        
        // 0 -> ?
        if(!noBounce && !prev) return 0;

        // 1 -> ?
        if(!noBounce && prev) return 3;
        
        // 0 -> 1
        if(!prev && stabilized) return 1;

        // 1 -> 0
        if(prev && !stabilized) return 2;

        // 1 -> 1
        if(prev && stabilized) return 3;

        // 0 -> 0
        return 0; 
    };

    for(uint8_t i = 0; i < (m_slaveCount + 2); ++i){
        changeBt[i] = getTransition(prevBt[i], currentBt[i]);
        changePhaseA[i] = getTransition(prevPhaseA[i], currentPhaseA[i]);
        changePhaseB[i] = getTransition(prevPhaseB[i], currentPhaseB[i]);

        prevBt[i] = (bool)(changeBt[i] & 0x1);
        prevPhaseA[i] = (bool)(changePhaseA[i] & 0x1);
        prevPhaseB[i] = (bool)(changePhaseB[i] & 0x1);
    }


    /* ---------------------------------------------------------
     全スレーブの充分なデータが集まるまで早期return
    --------------------------------------------------------- */
    if(prevSlaveCount != m_slaveCount){
        prevSlaveCount = m_slaveCount;
        called = 0;
    }

    if(called < DEBOUNCE_CT){
        called++;
        return;
    }


    /* ---------------------------------------------------------
     イベント発火
    --------------------------------------------------------- */
    for(uint8_t i = 0; i < (m_slaveCount + 2); ++i){
        uint8_t slave_id = i;
        if(i == 1) slave_id = 255;
        if(i > 1) slave_id = i - 1;

        // ------------------------- ボタン -------------------------
        if(changeBt[i] == 1){
            EncoderEvent e;
            e.slave_id = slave_id;
            e.event = EncoderEventType::ENC_PUSHUP;
            callAllEventListeners(e);
        }else if(changeBt[i] == 2){
            EncoderEvent e;
            e.slave_id = slave_id;
            e.event = EncoderEventType::ENC_PUSHDOWN;
            callAllEventListeners(e);
        }

        // ------------------------ エンコーダ ------------------------
        const uint8_t A = changePhaseA[i];
        const uint8_t B = changePhaseB[i];
        int8_t direction = 0;
        if(A == 1 && !(B & 0x1)) direction = 1;
        // else if(B == 1 && (A & 0x1)) direction = 1;
        // else if(A == 2 && (B & 0x1)) direction = 1;
        // else if(B == 2 && !(A & 0x1)) direction = 1;

        // else if(B == 2 && (A & 0x1)) direction = -1;
        // else if(A == 2 && !(B & 0x1)) direction = -1;
        else if(B == 1 && !(A & 0x1)) direction = -1;
        // else if(A == 1 && (B & 0x1)) direction = -1;

        if(direction < 0){
            EncoderEvent e;
            e.slave_id = slave_id;
            e.event = EncoderEventType::ENC_ROTP;
            callAllEventListeners(e);
        }else if(direction > 0){
            EncoderEvent e;
            e.slave_id = slave_id;
            e.event = EncoderEventType::ENC_ROTN;
            callAllEventListeners(e);
        }
    }
}


void Encoder::callAllEventListeners(EncoderEvent e){
    const uint32_t ct = m_listeners.size();
    for(uint32_t i = 0; i < ct; ++i){
        m_listeners.at(i)(e);
    }
}


void Encoder::initPio(){
    using namespace Constants::Gpio;

    m_pio = pio0;
    m_pioSm = pio_claim_unused_sm(m_pio, true);

    // ------------------------ PIO初期化 ------------------------
    uint offset = pio_add_program(m_pio, &encoder_piso_program);

    pio_sm_config c = encoder_piso_program_get_default_config(offset);

    sm_config_set_in_pins(&c, ENC_DATA);
    sm_config_set_sideset_pins(&c, ENC_CLK);
    sm_config_set_set_pins(&c, ENC_LATCH, 1);


    sm_config_set_in_shift(&c, false, true, 8);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_NONE);

    sm_config_set_clkdiv(&c, 5.0f);

    pio_gpio_init(m_pio, ENC_DATA);
    pio_gpio_init(m_pio, ENC_CLK);
    pio_gpio_init(m_pio, ENC_LATCH);
    pio_sm_set_consecutive_pindirs(m_pio, m_pioSm, ENC_DATA, 1, false);
    pio_sm_set_consecutive_pindirs(m_pio, m_pioSm, ENC_CLK, 1, true);
    pio_sm_set_consecutive_pindirs(m_pio, m_pioSm, ENC_LATCH, 1, true);

    pio_sm_init(m_pio, m_pioSm, offset, &c);
    pio_sm_set_enabled(m_pio, m_pioSm, true);


    // ------------------------ DMA初期化 ------------------------
    const size_t reading_bytes = Constants::Encoder::BYTES_PER_UNIT * (2 + Constants::MAX_SLAVES);
    m_dmaCh = dma_claim_unused_channel(true);
    dma_channel_config dmacfg = dma_channel_get_default_config(m_dmaCh);

    channel_config_set_transfer_data_size(&dmacfg, DMA_SIZE_8);
    channel_config_set_read_increment(&dmacfg, false);
    channel_config_set_write_increment(&dmacfg, true);
    channel_config_set_dreq(&dmacfg, pio_get_dreq(m_pio, m_pioSm, false));

    dma_channel_configure(
        m_dmaCh, &dmacfg,
        m_buf,
        &m_pio->rxf[m_pioSm],
        reading_bytes,
        false 
    );
}


void Encoder::runPioOnce(){
    const size_t reading_bytes = Constants::Encoder::BYTES_PER_UNIT * (2 + Constants::MAX_SLAVES);
    const size_t reading_bits = reading_bytes * 8;
    
    
    dma_channel_transfer_to_buffer_now(m_dmaCh, m_buf, reading_bytes);
    pio_sm_put_blocking(m_pio, m_pioSm, reading_bits - 1);
    dma_channel_wait_for_finish_blocking(m_dmaCh);
}