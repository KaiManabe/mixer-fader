#ifndef _DFFRAME_HPP_
#define _DFFRAME_HPP_


#include "Endian.hpp"
#include <span>
#include <vector>
#include <stdint.h>
#include <string.h>
#include <limits>

/* ---------------------------------------------------------
Driver→Firmware Frame

 パケット定義
 1. データ種類
 2. 対象スレーブID
 3. データ本体

 データ種類一覧
 A. ディスプレイ描画データ (160 * 80 * 2 Bytes)
 B. レベルメータ点灯パターン (2 Bytes)
--------------------------------------------------------- */

#define DF_ADDR_FRAMELENGTH 0x0
#define DF_ADDR_DATATYPE 0x4
#define DF_ADDR_SLAVE_ID 0x5
#define DF_ADDR_DATA 0x6

enum class DfDataType : uint8_t{
    DISPLAY,
    LEVELMETER,
};

class DfFrame{
public:
    using FrameLength = uint32_t;

    DfDataType dataType;
    uint8_t slaveId;
    std::vector<uint8_t> frameData;

    DfFrame(){
        dataType = DfDataType::DISPLAY;
        slaveId = 0;
        frameData = std::vector<uint8_t>(0);
    }

    bool deserialize(const std::vector<uint8_t>& data){
        if(data.size() < DF_ADDR_FRAMELENGTH + sizeof(FrameLength)) return false;
        FrameLength size = 0;
        memcpy(&size, &data[DF_ADDR_FRAMELENGTH], sizeof(size));

        if(size < DF_ADDR_DATA) return false;
        if(data.size() < static_cast<size_t>(size)) return false;

        dataType = static_cast<DfDataType>(data[DF_ADDR_DATATYPE]);
        slaveId = data[DF_ADDR_SLAVE_ID];

        frameData = std::vector<uint8_t>(size - DF_ADDR_DATA);
        memcpy(frameData.data(), &data[DF_ADDR_DATA], frameData.size());

        return true;
    }

    std::vector<uint8_t> serialize(){
        FrameLength size = static_cast<FrameLength>(DF_ADDR_DATA + frameData.size());
        auto data = std::vector<uint8_t>(size);

        memcpy(&data[DF_ADDR_FRAMELENGTH], &size, sizeof(size));
        data[DF_ADDR_DATATYPE] = static_cast<uint8_t>(dataType);
        data[DF_ADDR_SLAVE_ID] = slaveId;
        memcpy(&data[DF_ADDR_DATA], frameData.data(), frameData.size());

        return data;
    }
};

#endif
