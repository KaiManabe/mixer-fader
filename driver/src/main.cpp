#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <string.h>

#include "WinUsbDevice.hpp"
#include "common/DfFrame.hpp"
#include "common/FdFrame.hpp"


/* ---------------------------------------------------------
 テスト用レベルメータパターン定義
 16bit: bit0=LED0(緑,下) ... bit15=LED15(赤,上)

 パターン例:
   LOW   : 下位4つ点灯        0x000F
   MID   : 下位10個点灯(緑全) 0x03FF
   HIGH  : 下位15個点灯       0x7FFF
   FULL  : 全点灯             0xFFFF
   MUTE  : 全消灯             0x0000
   ALT   : 交互点灯           0x5555
--------------------------------------------------------- */
namespace TestPattern {
    static constexpr uint16_t LOW  = 0x000F;
    static constexpr uint16_t MID  = 0x03FF;
    static constexpr uint16_t HIGH = 0x7FFF;
    static constexpr uint16_t FULL = 0xFFFF;
    static constexpr uint16_t MUTE = 0x0000;
    static constexpr uint16_t ALT  = 0x5555;
}


/// @brief レベルメータ用のDfFrameを作成する
DfFrame buildLevelMeterFrame(uint8_t slaveId, uint16_t pattern)
{
    DfFrame frame;
    frame.dataType = DfDataType::LEVELMETER;
    frame.slaveId = slaveId;
    frame.frameData.resize(sizeof(uint16_t));
    memcpy(frame.frameData.data(), &pattern, sizeof(uint16_t));
    return frame;
}


/// @brief FdFrameの内容を表示する
void printFdFrame(const FdFrame& f)
{
    const char* eventName = "UNKNOWN";
    switch (f.eventType) {
    case FdEventType::INITIALIZED: eventName = "INITIALIZED"; break;
    case FdEventType::STATUS:      eventName = "STATUS";      break;
    case FdEventType::ENC_ROTP:    eventName = "ENC_ROTP";    break;
    case FdEventType::ENC_ROTN:    eventName = "ENC_ROTN";    break;
    case FdEventType::ENC_PUSHD:   eventName = "ENC_PUSHD";   break;
    case FdEventType::ENC_PUSHU:   eventName = "ENC_PUSHU";   break;
    }
    printf("  eventType : %s (%d)\n", eventName, static_cast<int>(f.eventType));
    printf("  ready     : %s\n", f.ready ? "true" : "false");
    printf("  arguments : [");
    for (size_t i = 0; i < f.eventArguments.size(); ++i) {
        if (i > 0) printf(", ");
        printf("0x%02X", f.eventArguments[i]);
    }
    printf("]\n");
}


/// @brief Bulk IN からFdFrameを受信する
bool receiveFdFrame(WinUsbDevice& dev, FdFrame& outFrame, uint32_t timeoutMs = 2000)
{
    auto rxData = dev.bulkRead(64, timeoutMs);
    if (rxData.empty()) {
        fprintf(stderr, "[WARN] No data received (timeout or error)\n");
        return false;
    }

    printf("[RX] Received %zu bytes:", rxData.size());
    for (size_t i = 0; i < rxData.size(); ++i) printf(" %02X", rxData[i]);
    printf("\n");

    if (!outFrame.deserialize(rxData)) {
        fprintf(stderr, "[ERROR] Failed to deserialize FdFrame\n");
        return false;
    }
    return true;
}


/// @brief DfFrameをBulk OUTで送信する
bool sendDfFrame(WinUsbDevice& dev, DfFrame& frame)
{
    auto serialized = frame.serialize();

    printf("[TX] Sending %zu bytes:", serialized.size());
    for (size_t i = 0; i < serialized.size(); ++i) printf(" %02X", serialized[i]);
    printf("\n");

    int written = dev.bulkWrite(serialized);
    if (written < 0) {
        fprintf(stderr, "[ERROR] Failed to send DfFrame\n");
        return false;
    }
    printf("[TX] Sent %d bytes\n", written);
    return true;
}


int main()
{
    printf("=== Mixer Fader Driver Test ===\n");
    printf("VID: 0x%04X  PID: 0x%04X\n\n", USB_VID, USB_PID);

    // ------ デバイスオープン ------
    WinUsbDevice dev;
    if (!dev.open()) {
        fprintf(stderr, "[FATAL] Could not open device. "
                "Ensure the device is connected and WinUSB driver is installed (Zadig).\n");
        return 1;
    }

    // ------ 初期 FdFrame 受信 (INITIALIZED) ------
    printf("\n--- Waiting for initial FdFrame ---\n");
    FdFrame initFrame;
    if (receiveFdFrame(dev, initFrame)) {
        printf("[OK] Received FdFrame:\n");
        printFdFrame(initFrame);
    } else {
        printf("[WARN] Could not receive initial frame, continuing anyway...\n");
    }

    // ------ テスト: LevelMeter DfFrame 送信 ------
    struct TestCase {
        const char* name;
        uint8_t slaveId;
        uint16_t pattern;
    };

    TestCase tests[] = {
        { "MID pattern to master (slave 0)",  0, TestPattern::MID  },
        { "HIGH pattern to slave 1",          1, TestPattern::HIGH },
        { "ALT pattern to master (slave 0)",  0, TestPattern::ALT  },
        { "MUTE pattern to master (slave 0)", 0, TestPattern::MUTE },
    };

    for (const auto& tc : tests) {
        printf("\n--- Test: %s ---\n", tc.name);
        printf("  slaveId=%d, pattern=0x%04X\n", tc.slaveId, tc.pattern);

        auto frame = buildLevelMeterFrame(tc.slaveId, tc.pattern);
        if (!sendDfFrame(dev, frame)) {
            fprintf(stderr, "[FAIL] Send failed\n");
            continue;
        }

        // レスポンス (STATUS) を受信
        FdFrame response;
        if (receiveFdFrame(dev, response, 2000)) {
            printf("[OK] Response:\n");
            printFdFrame(response);
        } else {
            printf("[WARN] No response received\n");
        }

        Sleep(200);  // デバイス処理待ち
    }

    // ------ クローズ ------
    printf("\n--- Closing device ---\n");
    dev.close();
    printf("[DONE] Test finished.\n");

    return 0;
}
