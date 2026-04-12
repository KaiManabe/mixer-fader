#include <stdio.h>
#include <stdint.h>
#include <algorithm>
#include <array>
#include <chrono>

#include "SoundMixer.hpp"
#include "WinUsbDevice.hpp"
#include "UsbComm.hpp"

static constexpr int MAX_SLAVES = 8;

int main(void){
    printf("=== Mixer Fader Driver ===\n");
    printf("VID: 0x%04X  PID: 0x%04X\n", USB_VID, USB_PID);
    printf("Press Ctrl+C to exit.\n\n");

    WinUsbDevice dev;
    printf("[INFO] Waiting for device...\n");
    while (!dev.open()) {
        Sleep(1000);
    }

    UsbComm comm(dev);
    SoundMixer mixer(MAX_SLAVES);
    uint8_t activeSlaveLastId = 0;
    std::array<uint16_t, MAX_SLAVES> lastLevelPattern{};
    std::array<bool, MAX_SLAVES> hasLastLevelPattern{};

    const auto invalidateLevelCache = [&]() {
        hasLastLevelPattern.fill(false);
    };

    const auto syncAllLevels = [&comm, &mixer, &lastLevelPattern, &hasLastLevelPattern](uint8_t slaveLastId, bool force) {
        const uint8_t count = (std::min)(static_cast<uint8_t>(slaveLastId + 1), static_cast<uint8_t>(MAX_SLAVES));

        for (uint8_t sid = 0; sid < count; ++sid) {
            const uint16_t pattern = mixer.getLevelmeterData(sid);
            if (!force && hasLastLevelPattern[sid] && lastLevelPattern[sid] == pattern) {
                continue;
            }

            comm.sendLevelMeter(sid, pattern);
            lastLevelPattern[sid] = pattern;
            hasLastLevelPattern[sid] = true;
        }
    };

    const auto syncDirtyDisplays = [&comm, &mixer](uint8_t slaveLastId) {
        const uint8_t count = (std::min)(static_cast<uint8_t>(slaveLastId + 1), static_cast<uint8_t>(MAX_SLAVES));

        for (uint8_t sid = 0; sid < count; ++sid) {
            if (!mixer.hasDisplayChange(sid)) {
                continue;
            }

            auto& displayData = mixer.getDisplayData(sid);
            if (!displayData.empty()) {
                comm.sendDisplay(sid, displayData.data(), displayData.size());
            }
        }
    };

    const auto syncAllDisplays = [&comm, &mixer](uint8_t slaveLastId) {
        const uint8_t count = (std::min)(static_cast<uint8_t>(slaveLastId + 1), static_cast<uint8_t>(MAX_SLAVES));

        for (uint8_t sid = 0; sid < count; ++sid) {
            auto& displayData = mixer.getDisplayData(sid);
            if (!displayData.empty()) {
                comm.sendDisplay(sid, displayData.data(), displayData.size());
            }
        }
    };

    const auto refreshAndSync = [&]() {
        mixer.refleshProcess();
        syncAllLevels(activeSlaveLastId, false);
        syncDirtyDisplays(activeSlaveLastId);
        comm.flush();
    };

    const auto updateSlaveCountAndSync = [&](const FdFrame& frame, bool forceSync) {
        const uint8_t nextSlaveLastId = (std::min)(frame.slaveCount, static_cast<uint8_t>(MAX_SLAVES - 1));

        if (!forceSync && nextSlaveLastId == activeSlaveLastId) {
            return;
        }

        activeSlaveLastId = nextSlaveLastId;
        printf("[INFO] Device status (slaves=%u, displayCredits=%u, controlCredits=%u)\n",
            static_cast<unsigned>(activeSlaveLastId + 1),
            static_cast<unsigned>(frame.displayCredits),
            static_cast<unsigned>(frame.controlCredits));

        mixer.setSlaveCount(activeSlaveLastId);
        invalidateLevelCache();
        mixer.refleshProcess();
        syncAllLevels(activeSlaveLastId, true);
        syncAllDisplays(activeSlaveLastId);
        comm.flush();
    };

    printf("[INFO] Entering poll loop...\n\n");
    using clock = std::chrono::steady_clock;
    auto lastRefresh = clock::now();

    while (true) {
        if (dev.isDisconnected()) {
            printf("\n[WARN] Device disconnected. Waiting for reconnect...\n");
            comm.clearPending();
            comm.resetFlowControl();
            invalidateLevelCache();

            while (!dev.open()) {
                Sleep(1000);
            }

            printf("[INFO] Device reconnected!\n\n");
            lastRefresh = clock::now();
            continue;
        }

        auto frame = comm.receive(5);
        if (frame.has_value()) {
            const FdFrame& f = frame.value();

            if (f.eventType == FdEventType::INITIALIZED) {
                printf("[INFO] Device initialized\n");
                updateSlaveCountAndSync(f, true);
                comm.flush();
                continue;
            }

            if (f.eventType == FdEventType::STATUS) {
                updateSlaveCountAndSync(f, false);
                comm.flush();
                continue;
            }

            if ((f.eventType == FdEventType::ENC_ROTP ||
                 f.eventType == FdEventType::ENC_ROTN ||
                 f.eventType == FdEventType::ENC_PUSHD) &&
                !f.eventArguments.empty()) {
                const uint8_t sid = f.eventArguments[0];

                if (f.eventType == FdEventType::ENC_ROTP && sid == 255) {
                    mixer.nextPage();
                    syncDirtyDisplays(activeSlaveLastId);
                    comm.flush();
                    continue;
                }

                if (f.eventType == FdEventType::ENC_ROTN && sid == 255) {
                    mixer.prevPage();
                    syncDirtyDisplays(activeSlaveLastId);
                    comm.flush();
                    continue;
                }

                if (sid <= activeSlaveLastId) {
                    if (f.eventType == FdEventType::ENC_ROTP) {
                        mixer.setVolume(sid, true);
                    } else if (f.eventType == FdEventType::ENC_ROTN) {
                        mixer.setVolume(sid, false);
                    } else {
                        mixer.toggleMuted(sid);
                    }

                    lastLevelPattern[sid] = mixer.getLevelmeterData(sid);
                    hasLastLevelPattern[sid] = true;
                    comm.sendLevelMeter(sid, lastLevelPattern[sid]);
                    syncDirtyDisplays(activeSlaveLastId);
                    comm.flush();
                }
            }
        }

        const auto now = clock::now();
        if (now - lastRefresh >= std::chrono::milliseconds(50)) {
            refreshAndSync();
            lastRefresh = now;
        }

        comm.flush();
    }

    dev.close();
    return 0;
}
