#include <stdio.h>
#include <stdint.h>
#include <algorithm>
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
	uint8_t activeSlaveCount = static_cast<uint8_t>(MAX_SLAVES);

	auto syncAllLevels = [&comm, &mixer](uint8_t slaveCount) {
		const uint8_t count = (std::min)(slaveCount, static_cast<uint8_t>(MAX_SLAVES));
		for (uint8_t sid = 0; sid < count; ++sid) {
			comm.sendLevelMeter(sid, mixer.getLevelmeterData(sid));
		}
	};

	auto syncDirtyDisplays = [&](uint8_t slaveCount) {
		const uint8_t count = (std::min)(slaveCount, static_cast<uint8_t>(MAX_SLAVES));
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

	auto refreshAndSync = [&]() {
		mixer.refleshProcess();
		syncAllLevels(activeSlaveCount);
		syncDirtyDisplays(activeSlaveCount);
	};

	printf("[INFO] Entering poll loop...\n\n");
	using clock = std::chrono::steady_clock;
	auto lastRefresh = clock::now();

	while (true) {
		if (dev.isDisconnected()) {
			printf("\n[WARN] Device disconnected. Waiting for reconnect...\n");
			while (!dev.open()) {
				Sleep(1000);
			}
			printf("[INFO] Device reconnected!\n\n");
			refreshAndSync();
			lastRefresh = clock::now();
			continue;
		}

		auto frame = comm.receive(30);
		if (frame.has_value()) {
			const FdFrame& f = frame.value();

			if (f.eventType == FdEventType::INITIALIZED) {
				if (!f.eventArguments.empty()) {
					activeSlaveCount = (std::min)(f.eventArguments[0], static_cast<uint8_t>(MAX_SLAVES));
				}
				printf("[INFO] Device initialized (slaves=%u)\n", activeSlaveCount);
				mixer.setSlaveCount(activeSlaveCount);
				refreshAndSync();
				continue;
			}

			if ((f.eventType == FdEventType::ENC_ROTP || f.eventType == FdEventType::ENC_ROTN || f.eventType == FdEventType::ENC_PUSHD) && !f.eventArguments.empty()) {
				const uint8_t sid = f.eventArguments[0];

				if (f.eventType == FdEventType::ENC_ROTP && sid == 255) {
					mixer.nextPage();
					syncDirtyDisplays(activeSlaveCount);
					continue;
				}

				if (f.eventType == FdEventType::ENC_ROTN && sid == 255) {
					mixer.prevPage();
					syncDirtyDisplays(activeSlaveCount);
					continue;
				}

				if (sid < activeSlaveCount) {
					if (f.eventType == FdEventType::ENC_ROTP) {
						mixer.setVolume(sid, true);
					} else if (f.eventType == FdEventType::ENC_ROTN) {
						mixer.setVolume(sid, false);
					} else {
						mixer.toggleMuted(sid);
					}

					comm.sendLevelMeter(sid, mixer.getLevelmeterData(sid));
					syncDirtyDisplays(activeSlaveCount);
				}
			}
		}

		const auto now = clock::now();
		if (now - lastRefresh >= std::chrono::milliseconds(100)) {
			refreshAndSync();
			lastRefresh = now;
		}
	}

	dev.close();
	return 0;

}