#include <stdio.h>
#include "Constants.hpp"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include <vector>
#include <string.h>
#include "Encoder.hpp"
#include "Display.hpp"
#include "LevelMeter.hpp"

void init_gpio();


int main(){
    stdio_init_all();

    init_gpio();


    auto d = Display::getInstance();
    auto e = Encoder(d.getSlaveCountReference());
    auto l = LevelMeter(d.getSlaveCountReference());

    int8_t vol[Constants::MAX_SLAVES + 1] = {0};
    int16_t brightness[Constants::MAX_SLAVES + 1] = {0};

    auto test1 = [&d, &e, &l, &vol, &brightness](EncoderEvent event){
        if(event.slave_id == 255) return;

        if(event.event == EncoderEventType::ENC_ROTP) vol[event.slave_id]++;
        if(event.event == EncoderEventType::ENC_ROTN) vol[event.slave_id]--;

        if(event.event == EncoderEventType::ENC_ROTP) brightness[event.slave_id]+= 16;
        if(event.event == EncoderEventType::ENC_ROTN) brightness[event.slave_id]-= 16;

        if(vol[event.slave_id] > 16) vol[event.slave_id] = 16;
        if(vol[event.slave_id] < 0) vol[event.slave_id] = 0;

        l.setLevelAsValue(event.slave_id, vol[event.slave_id]);


        if(brightness[event.slave_id] > 255) brightness[event.slave_id] = 255;
        if(brightness[event.slave_id] < 0) brightness[event.slave_id] = 0;
        
        uint8_t r = (brightness[event.slave_id] >> 3);
        uint8_t g = (brightness[event.slave_id] >> 2);
        uint8_t b = (brightness[event.slave_id] >> 3);
        uint16_t color = 0x0;
        color |= (r << 11) & 0b1111100000000000;
        color |= (g << 5)  & 0b0000011111100000;
        color |= b         & 0b0000000000011111;
        
        // バッファ全体を同じ色で塗りつぶし
        uint16_t img[Constants::Display::BUFSIZE16];
        for(size_t i = 0; i < Constants::Display::BUFSIZE16; ++i){
            img[i] = color;
        }
        
        std::span<uint16_t, Constants::Display::BUFSIZE16> span_img(img);
        d.transferBuffer(event.slave_id, span_img);
    };


    e.attachEventListener(test1);
    printf("Initialized. Slave count: %d\n", d.getSlaveCountReference());

    while(1){
        e.routine();
        l.routine();
        d.routine();
    }
}
