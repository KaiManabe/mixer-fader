#ifndef _CONSTANTS_HPP_
#define _CONSTANTS_HPP_

#include <stdint.h>

namespace Constants{
    const uint8_t MAX_SLAVES = 8;

    namespace Gpio{
        const uint8_t DISP_LOOPBACK = 15;
        const uint8_t DISP_DFF_D    = 17;
        const uint8_t DISP_DFF_CLK  = 22;
        const uint8_t DISP_DFF_CLR  = 21;
        const uint8_t DISP_SPI_MOSI = 19;
        const uint8_t DISP_SPI_CLK  = 18;
        const uint8_t DISP_DC       = 20;
        
        const uint8_t LED_DATA      = 2;
        const uint8_t LED_CLK       = 3;
        const uint8_t LED_LATCH     = 4;
        
        const uint8_t ENC_DATA      = 5;
        const uint8_t ENC_CLK       = 6;
        const uint8_t ENC_LATCH     = 7;
    }


    namespace Encoder{
        const uint8_t BYTES_PER_UNIT = 1;
        const uint8_t DEBOUNCECOUNT = 3;
    }


    namespace Display{
        const uint32_t SPI_FREQ = 20 * 1000 * 1000;
        const uint16_t WIDTH = 80;
        const uint16_t HEIGHT = 160;
        const uint32_t BUFSIZE16 = WIDTH * HEIGHT;
    }

    namespace LevelMeter{
        const uint8_t LIGHTING_LED_MAX = 1;
        const uint8_t DUTY_RATIO = 100;
    }

    namespace Usb{
        const uint16_t RX_BUFSIZE = 64;
        const uint32_t RX_BUF_FRAMECOUNT = 5;
        const uint32_t TX_BUF_FRAMECOUNT = 64;
    }
}


#endif