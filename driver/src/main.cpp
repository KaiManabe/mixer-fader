#include <stdio.h>
#include <stdint.h>
#include <algorithm>
#include <chrono>

#include "SoundMixer.hpp"
#include "WinUsbDevice.hpp"
#include "UsbComm.hpp"

static constexpr int MAX_SLAVES = 8;


/// @brief driver のメインループを実行する
/// @return 終了コード
int main(void){
	// ---- 起動ログ ----
	printf("=== Mixer Fader Driver ===\n");
	printf("VID: 0x%04X  PID: 0x%04X\n", USB_VID, USB_PID);
	printf("Press Ctrl+C to exit.\n\n");

	// ---- デバイス接続待ち ----
	WinUsbDevice dev;
	printf("[INFO] Waiting for device...\n");
	while (!dev.open()) {
		Sleep(1000);
	}

	// ---- 実行状態を初期化 ----
	UsbComm comm(dev);
	SoundMixer mixer(MAX_SLAVES);
	uint8_t activeSlaveCount = static_cast<uint8_t>(MAX_SLAVES);

	auto syncAllLevels = [&comm, &mixer](uint8_t slaveCount) {
		// ---------------- 送信対象 slave 数を制限 ----------------
		const uint8_t count = (std::min)(slaveCount, static_cast<uint8_t>(MAX_SLAVES));

		// ---------------- 全 slave のレベルを送信 ----------------
		for (uint8_t sid = 0; sid < count; ++sid) {
			comm.sendLevelMeter(sid, mixer.getLevelmeterData(sid));
		}
	};

	auto syncDirtyDisplays = [&](uint8_t slaveCount) {
		// ---------------- 送信対象 slave 数を制限 ----------------
		const uint8_t count = (std::min)(slaveCount, static_cast<uint8_t>(MAX_SLAVES));

		// ---------------- 差分のある表示だけ送信 ----------------
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

	auto syncAllDisplays = [&](uint8_t slaveCount) {
		// ---------------- 送信対象 slave 数を制限 ----------------
		const uint8_t count = (std::min)(slaveCount, static_cast<uint8_t>(MAX_SLAVES));

		// ------------------ 全表示を送信 ------------------
		for (uint8_t sid = 0; sid < count; ++sid) {
			auto& displayData = mixer.getDisplayData(sid);
			if (!displayData.empty()) {
				comm.sendDisplay(sid, displayData.data(), displayData.size());
			}
		}
	};

	auto refreshAndSync = [&]() {
		// -------------------- 状態を再取得 --------------------
		mixer.refleshProcess();

		// -------------------- 現在状態を送信 --------------------
		syncAllLevels(activeSlaveCount);
		syncDirtyDisplays(activeSlaveCount);
	};

	auto updateSlaveCountAndSync = [&](const FdFrame& frame) {
		// ---------------- slave 数をフレームから反映 ----------------
		if (!frame.eventArguments.empty()) {
			activeSlaveCount = (std::min)(frame.eventArguments[0], static_cast<uint8_t>(MAX_SLAVES));
		}

		printf("[INFO] Device status (slaves=%u)\n", activeSlaveCount);
		mixer.setSlaveCount(activeSlaveCount);
		mixer.refleshProcess();
		syncAllLevels(activeSlaveCount);
		syncAllDisplays(activeSlaveCount);
	};

	// ---- メインループ ----
	printf("[INFO] Entering poll loop...\n\n");
	using clock = std::chrono::steady_clock;
	auto lastRefresh = clock::now();

	while (true) {
		// ------------------ 切断時は再接続を待機 ------------------
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

		// ---------------------- 入力を処理 ----------------------
		auto frame = comm.receive(30);
		if (frame.has_value()) {
			const FdFrame& f = frame.value();

			// ----------------- 初期化イベントを反映 -----------------
			if (f.eventType == FdEventType::INITIALIZED) {
				printf("[INFO] Device initialized\n");
				updateSlaveCountAndSync(f);
				continue;
			}

			// ------------------ ステータスイベントを反映 ------------------
			if (f.eventType == FdEventType::STATUS) {
				updateSlaveCountAndSync(f);
				continue;
			}

			// --------------- エンコーダ入力を反映 ---------------
			if ((f.eventType == FdEventType::ENC_ROTP || f.eventType == FdEventType::ENC_ROTN || f.eventType == FdEventType::ENC_PUSHD) && !f.eventArguments.empty()) {
				const uint8_t sid = f.eventArguments[0];

				// ------------- ページ送り入力を処理 -------------
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

				// -------------- 個別 slave の入力を処理 --------------
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

		// ------------------ 定期的に状態を再同期 ------------------
		const auto now = clock::now();
		if (now - lastRefresh >= std::chrono::milliseconds(100)) {
			refreshAndSync();
			lastRefresh = now;
		}
	}

	dev.close();
	return 0;

}
