#ifndef _FDFRAME_HPP_
#define _FDFRAME_HPP_

#include "Endian.hpp"
#include <vector>
#include <stdint.h>
#include <string.h>
#include <limits>

/* ---------------------------------------------------------
 Firmware→Driver Frame

 パケット定義
 1. イベントタイプ
 2. イベント引数
 3. READY

 イベント一覧
 A. 初期化
    引数: 検知スレーブ台数
 B. エンコーダ正転
    引数: スレーブID
 C. エンコーダ逆転
    引数: スレーブID
 D. エンコーダプッシュスイッチDOWN
    引数: スレーブID
 E. エンコーダプッシュスイッチUP
    引数: スレーブID
 F. ステータス送信のみ
    引数: 検知スレーブ台数
--------------------------------------------------------- */

#define FD_PROTOCOL_VERSION 0x02

#define FD_ADDR_FRAMELENGTH 0x0
#define FD_ADDR_VERSION 0x4
#define FD_ADDR_EVENTTYPE 0x5
#define FD_ADDR_FLAGS 0x6
#define FD_ADDR_SLAVECOUNT 0x7
#define FD_ADDR_DISPLAYCREDITS 0x8
#define FD_ADDR_CONTROLCREDITS 0x9
#define FD_ADDR_DISPLAYACK 0xA
#define FD_ADDR_CONTROLACK 0xC
#define FD_ADDR_ARGUMENTS 0xE

enum class FdEventType : uint8_t{
    INITIALIZED,
    STATUS,
    ENC_ROTP,
    ENC_ROTN,
    ENC_PUSHD,
    ENC_PUSHU,
};

class FdFrame{
public:
    using FrameLength = uint32_t;

    uint8_t protocolVersion;
    FdEventType eventType;
    uint8_t flags;
    uint8_t slaveCount;
    uint8_t displayCredits;
    uint8_t controlCredits;
    uint16_t displayAck;
    uint16_t controlAck;
    std::vector<uint8_t> eventArguments;

    FdFrame(){
        protocolVersion = FD_PROTOCOL_VERSION;
        eventType = FdEventType::INITIALIZED;
        flags = 0;
        slaveCount = 0;
        displayCredits = 0;
        controlCredits = 0;
        displayAck = 0;
        controlAck = 0;
        eventArguments = std::vector<uint8_t>(0);
    }

    bool deserialize(const std::vector<uint8_t>& data){
        if(data.size() < FD_ADDR_FRAMELENGTH + sizeof(FrameLength)) return false;
        FrameLength size = 0;
        memcpy(&size, &data[FD_ADDR_FRAMELENGTH], sizeof(size));

        if(size < FD_ADDR_ARGUMENTS) return false;
        if(data.size() < static_cast<size_t>(size)) return false;

        protocolVersion = data[FD_ADDR_VERSION];
        if(protocolVersion != FD_PROTOCOL_VERSION) return false;

        eventType = static_cast<FdEventType>(data[FD_ADDR_EVENTTYPE]);
        flags = data[FD_ADDR_FLAGS];
        slaveCount = data[FD_ADDR_SLAVECOUNT];
        displayCredits = data[FD_ADDR_DISPLAYCREDITS];
        controlCredits = data[FD_ADDR_CONTROLCREDITS];
        memcpy(&displayAck, &data[FD_ADDR_DISPLAYACK], sizeof(displayAck));
        memcpy(&controlAck, &data[FD_ADDR_CONTROLACK], sizeof(controlAck));

        eventArguments = std::vector<uint8_t>(size - FD_ADDR_ARGUMENTS);
        memcpy(eventArguments.data(), &data[FD_ADDR_ARGUMENTS], eventArguments.size());
        return true;
    }

    std::vector<uint8_t> serialize(){
        if(eventArguments.size() > (std::numeric_limits<FrameLength>::max() - FD_ADDR_ARGUMENTS)){
            return std::vector<uint8_t>(0);
        }

        FrameLength size = static_cast<FrameLength>(FD_ADDR_ARGUMENTS + eventArguments.size());
        auto data = std::vector<uint8_t>(size);
        memcpy(&data[FD_ADDR_FRAMELENGTH], &size, sizeof(size));
        data[FD_ADDR_VERSION] = protocolVersion;
        data[FD_ADDR_EVENTTYPE] = static_cast<uint8_t>(eventType);
        data[FD_ADDR_FLAGS] = flags;
        data[FD_ADDR_SLAVECOUNT] = slaveCount;
        data[FD_ADDR_DISPLAYCREDITS] = displayCredits;
        data[FD_ADDR_CONTROLCREDITS] = controlCredits;
        memcpy(&data[FD_ADDR_DISPLAYACK], &displayAck, sizeof(displayAck));
        memcpy(&data[FD_ADDR_CONTROLACK], &controlAck, sizeof(controlAck));
        memcpy(&data[FD_ADDR_ARGUMENTS], eventArguments.data(), eventArguments.size());
        return data;
    }
};

#endif
