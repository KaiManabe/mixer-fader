#include <Constants.hpp>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"

void init_gpio(){
    gpio_init(Constants::Gpio::DISP_LOOPBACK);
    gpio_set_dir(Constants::Gpio::DISP_LOOPBACK, GPIO_IN);
    gpio_pull_down(Constants::Gpio::DISP_LOOPBACK);
    
    gpio_init(Constants::Gpio::DISP_DFF_D);
    gpio_set_dir(Constants::Gpio::DISP_DFF_D, GPIO_OUT);

    gpio_init(Constants::Gpio::DISP_DFF_CLK);
    gpio_set_dir(Constants::Gpio::DISP_DFF_CLK, GPIO_OUT);
    
    gpio_init(Constants::Gpio::DISP_DFF_CLR);
    gpio_set_dir(Constants::Gpio::DISP_DFF_CLR, GPIO_OUT);
    gpio_put(Constants::Gpio::DISP_DFF_CLR, 0);
    sleep_us(10);
    gpio_put(Constants::Gpio::DISP_DFF_CLR, 1);
    sleep_us(10);
    
    gpio_init(Constants::Gpio::DISP_DC);
    gpio_set_dir(Constants::Gpio::DISP_DC, GPIO_OUT);
    
    gpio_init(Constants::Gpio::LED_DATA);
    gpio_set_dir(Constants::Gpio::LED_DATA, GPIO_OUT);

    gpio_init(Constants::Gpio::LED_CLK);
    gpio_set_dir(Constants::Gpio::LED_CLK, GPIO_OUT);

    gpio_init(Constants::Gpio::LED_LATCH);
    gpio_set_dir(Constants::Gpio::LED_LATCH, GPIO_OUT);
    
    gpio_init(Constants::Gpio::ENC_DATA);
    gpio_set_dir(Constants::Gpio::ENC_DATA, GPIO_IN);
    gpio_pull_up(Constants::Gpio::ENC_DATA);

    gpio_init(Constants::Gpio::ENC_CLK);
    gpio_set_dir(Constants::Gpio::ENC_CLK, GPIO_OUT);

    gpio_init(Constants::Gpio::ENC_LATCH);
    gpio_set_dir(Constants::Gpio::ENC_LATCH, GPIO_OUT);

    
    spi_init(spi0, Constants::Display::SPI_FREQ);
    gpio_set_function(Constants::Gpio::DISP_SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(Constants::Gpio::DISP_SPI_MOSI, GPIO_FUNC_SPI);
}