#include <stdio.h>
#include "Constants.hpp"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include <memory>
#include <vector>
#include <string.h>
#include "Encoder.hpp"
#include "Display.hpp"
#include "LevelMeter.hpp"
#include "UsbComm.hpp"
#include "usb_adapter.hpp"
#include "tusb.h"

void init_gpio();

void sendTxFrame(UsbComm& usb, const EncoderEvent& e){
    auto f = FdFrame();
    switch(e.event){
    case EncoderEventType::ENC_PUSHDOWN:
        f.eventType = FdEventType::ENC_PUSHD;
        break;
    case EncoderEventType::ENC_PUSHUP:
        f.eventType = FdEventType::ENC_PUSHU;
        break;
    case EncoderEventType::ENC_ROTP:
        f.eventType = FdEventType::ENC_ROTP;
        break;
    case EncoderEventType::ENC_ROTN:
        f.eventType = FdEventType::ENC_ROTN;
        break;
    default:
        break;
    }
    f.eventArguments.resize(1);
    f.eventArguments[0] = e.slave_id;
    usb.putTxFrame(f);
}



int main(){
    stdio_init_all();

    init_gpio();

    /* ---------------------------------------------------------
     初期化
    --------------------------------------------------------- */
    printf("Initializing Display manager...\n");
    auto& disp = Display::getInstance();
    printf("Initialized Display manager...\n");

    printf("Initializing Encoder manager...\n");
    auto enc = Encoder(disp.getSlaveCountReference());
    printf("Initialized Encoder manager...\n");
    printf("Initializing LevelMeter manager...\n");
    auto lvl = LevelMeter(disp.getSlaveCountReference());
    printf("Initialized LevelMeter manager...\n");

    printf("Initializing TinyUSB...\n");
    tusb_init();
    printf("Initialized TinyUSB...\n");

    printf("Initializing UsbComm...\n");
    auto usb = std::make_unique<UsbComm>(disp, lvl);
    printf("Initialized UsbComm...\n");
    
    printf("Initializing UsbAdapter...\n");
    UsbAdapter::getInstance().init(usb.get());
    printf("Initialized UsbAdapter...\n");

    /* ---------------------------------------------------------
     コールバック割当
    --------------------------------------------------------- */
    const auto callback = [&usb](EncoderEvent e){
        sendTxFrame(*usb, e);
    };
    enc.attachEventListener(callback);

    printf("Initialized. Slave count: %d\n", disp.getSlaveCountReference());

    /* ---------------------------------------------------------
     初期化完了応答
    --------------------------------------------------------- */
    while(1){
        tud_task();
        enc.routine();
        lvl.routine();
        disp.routine();
        UsbAdapter::getInstance().process();
    }
}
