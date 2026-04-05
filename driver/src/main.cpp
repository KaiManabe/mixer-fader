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
	uint8_t activeSlaveLastId = static_cast<uint8_t>(MAX_SLAVES - 1);

	auto syncAllLevels = [&comm, &mixer](uint8_t slaveLastId) {
		// ---------------- 送信対象 slave 数を制限 ----------------
		const uint8_t count = (std::min)(static_cast<uint8_t>(slaveLastId + 1), static_cast<uint8_t>(MAX_SLAVES));

		// ---------------- 全 slave のレベルを送信 ----------------
		for (uint8_t sid = 0; sid < count; ++sid) {
			comm.sendLevelMeter(sid, mixer.getLevelmeterData(sid));
		}
	};

	auto syncDirtyDisplays = [&](uint8_t slaveLastId) {
		// ---------------- 送信対象 slave 数を制限 ----------------
		const uint8_t count = (std::min)(static_cast<uint8_t>(slaveLastId + 1), static_cast<uint8_t>(MAX_SLAVES));

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

	auto syncAllDisplays = [&](uint8_t slaveLastId) {
		// ---------------- 送信対象 slave 数を制限 ----------------
		const uint8_t count = (std::min)(static_cast<uint8_t>(slaveLastId + 1), static_cast<uint8_t>(MAX_SLAVES));

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
		syncAllLevels(activeSlaveLastId);
		syncDirtyDisplays(activeSlaveLastId);
	};

	auto updateSlaveCountAndSync = [&](const FdFrame& frame, bool forceSync) {
		// ---------------- slave 数をフレームから反映 ----------------
		uint8_t nextSlaveLastId = activeSlaveLastId;
		if (!frame.eventArguments.empty()) {
			nextSlaveLastId = (std::min)(frame.eventArguments[0], static_cast<uint8_t>(MAX_SLAVES - 1));
		}

		// ---------------- slave 数変化時のみ反映 ----------------
		if (!forceSync && nextSlaveLastId == activeSlaveLastId) {
			return;
		}

		activeSlaveLastId = nextSlaveLastId;
		printf("[INFO] Device status (slaves=%u)\n", static_cast<unsigned>(activeSlaveLastId + 1));
		mixer.setSlaveCount(activeSlaveLastId);
		mixer.refleshProcess();
		syncAllLevels(activeSlaveLastId);
		syncAllDisplays(activeSlaveLastId);
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
				updateSlaveCountAndSync(f, true);
				continue;
			}

			// ------------------ ステータスイベントを反映 ------------------
			if (f.eventType == FdEventType::STATUS) {
				updateSlaveCountAndSync(f, false);
				continue;
			}

			// --------------- エンコーダ入力を反映 ---------------
			if ((f.eventType == FdEventType::ENC_ROTP || f.eventType == FdEventType::ENC_ROTN || f.eventType == FdEventType::ENC_PUSHD) && !f.eventArguments.empty()) {
				const uint8_t sid = f.eventArguments[0];

				// ------------- ページ送り入力を処理 -------------
				if (f.eventType == FdEventType::ENC_ROTP && sid == 255) {
					mixer.nextPage();
					syncDirtyDisplays(activeSlaveLastId);
					continue;
				}

				if (f.eventType == FdEventType::ENC_ROTN && sid == 255) {
					mixer.prevPage();
					syncDirtyDisplays(activeSlaveLastId);
					continue;
				}

				// -------------- 個別 slave の入力を処理 --------------
				if (sid <= activeSlaveLastId) {
					if (f.eventType == FdEventType::ENC_ROTP) {
						mixer.setVolume(sid, true);
					} else if (f.eventType == FdEventType::ENC_ROTN) {
						mixer.setVolume(sid, false);
					} else {
						mixer.toggleMuted(sid);
					}

					comm.sendLevelMeter(sid, mixer.getLevelmeterData(sid));
					syncDirtyDisplays(activeSlaveLastId);
				}
			}
		}

		// ------------------ 定期的に状態を再同期 ------------------
		const auto now = clock::now();
		if (now - lastRefresh >= std::chrono::milliseconds(50)) {
			refreshAndSync();
			lastRefresh = now;
		}
	}

	dev.close();
	return 0;

}
