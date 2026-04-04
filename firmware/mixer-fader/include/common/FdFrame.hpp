#ifndef _FDFRAME_HPP_
#define _FDFRAME_HPP_

#include "Endian.hpp"
#include <span>
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

#define FD_ADDR_FRAMELENGTH 0x0
#define FD_ADDR_EVENTTYPE 0x4
#define FD_ADDR_READY 0x5
#define FD_ADDR_ARGUMENTS 0x6

#define ARGUMENT_LENGTH 1

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

    FdEventType eventType;
    bool ready;
    std::vector<uint8_t> eventArguments;

    FdFrame(){
        eventType = FdEventType::INITIALIZED;
        ready = false;
        eventArguments = std::vector<uint8_t>(0);
    }

    bool deserialize(const std::vector<uint8_t>& data){
        if(data.size() < FD_ADDR_FRAMELENGTH + sizeof(FrameLength)) return false;
        FrameLength size = 0;
        memcpy(&size, &data[FD_ADDR_FRAMELENGTH], sizeof(size));

        if(size < FD_ADDR_ARGUMENTS) return false;
        if(data.size() < static_cast<size_t>(size)) return false;

        eventType = static_cast<FdEventType>(data[FD_ADDR_EVENTTYPE]);
        ready = data[FD_ADDR_READY] != 0;

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
        data[FD_ADDR_EVENTTYPE] = static_cast<uint8_t>(eventType);
        data[FD_ADDR_READY] = static_cast<uint8_t>(ready);
        memcpy(&data[FD_ADDR_ARGUMENTS], eventArguments.data(), eventArguments.size());
        return data;
    }
};

#endif
