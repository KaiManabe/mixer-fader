#include "SoundMixer.hpp"

#include <algorithm>
#include <cmath>
#include <stdio.h>

#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>

// MinGW endpointvolume.h forward-declares IAudioMeterInformation without defining it.
// Provide the interface definition so peak metering APIs can be used.
#ifndef __IAudioMeterInformation_INTERFACE_DEFINED__
#define __IAudioMeterInformation_INTERFACE_DEFINED__
MIDL_INTERFACE("C02216F6-8C67-4B5B-9D00-D008E73E0064")
IAudioMeterInformation : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(
        FLOAT *pfPeak) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(
        UINT *pnChannelCount) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(
        UINT u32ChannelCount,
        FLOAT *afPeakValues) = 0;

    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(
        DWORD *pdwHardwareSupportMask) = 0;
};
#endif

static const IID IID_IAudioMeterInformation_Local =
{0xC02216F6, 0x8C67, 0x4B5B, {0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64}};


/* ---------------------------------------------------------
 ヘルパー関数
--------------------------------------------------------- */
template <typename T>
void safeRelease(T*& ptr) {
    // -------------------- 有効ポインタのみ解放 --------------------
    if (ptr != nullptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

/// @brief 0.0-1.0 の値を 0-100 の整数へ変換する
/// @param value 正規化された値
/// @return パーセント値
uint8_t toPercent(float value) {
    // -------------------- 範囲を 0-1 に制限 --------------------
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(clamped * 100.0f));
}

/// @brief 0-100 の整数を 0.0-1.0 の値へ変換する
/// @param value パーセント値
/// @return 正規化された値
float toScalar(uint8_t value) {
    return std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 1.0f);
}

/// @brief 既定の再生デバイスを取得する
/// @param deviceEnumerator 作成した列挙子
/// @param defaultDevice 取得した既定デバイス
/// @return 取得に成功したら true
bool createDefaultAudioDevice(IMMDeviceEnumerator*& deviceEnumerator, IMMDevice*& defaultDevice) {
    // -------------------- 列挙子を作成 --------------------
    const HRESULT createResult = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&deviceEnumerator));
    if (FAILED(createResult)) {
        return false;
    }

    // ------------------ 既定再生デバイスを取得 ------------------
    if (FAILED(deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {
        safeRelease(deviceEnumerator);
        return false;
    }

    return true;
}

/// @brief PID から実行ファイルパスを取得する
/// @param pid 対象プロセス ID
/// @return 実行ファイルパス
std::wstring getProcessImagePath(DWORD pid) {
    // -------------------- システム PID を除外 --------------------
    if (pid == 0) {
        return L"";
    }

    // ---------------- プロセスハンドルを取得 ----------------
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return L"";
    }

    std::wstring path(32768, L'\0');
    DWORD len = static_cast<DWORD>(path.size());
    const BOOL ok = QueryFullProcessImageNameW(process, 0, path.data(), &len);
    CloseHandle(process);

    // -------------------- 取得結果を判定 --------------------
    if (!ok) {
        return L"";
    }

    path.resize(len);
    return path;
}

/// @brief PID に対応するセッション音量インターフェイスを探す
/// @param sessionManager セッション管理インターフェイス
/// @param pid 対象プロセス ID
/// @return 見つかった音量インターフェイス
ISimpleAudioVolume* findSessionVolumeByPid(IAudioSessionManager2* sessionManager, DWORD pid) {
    // -------------------- null 入力を判定 --------------------
    if (sessionManager == nullptr) {
        return nullptr;
    }

    IAudioSessionEnumerator* sessionEnumerator = nullptr;
    ISimpleAudioVolume* foundVolume = nullptr;

    // ---------------- セッション列挙子を取得 ----------------
    if (FAILED(sessionManager->GetSessionEnumerator(&sessionEnumerator)) || sessionEnumerator == nullptr) {
        return nullptr;
    }

    int sessionCount = 0;
    if (SUCCEEDED(sessionEnumerator->GetCount(&sessionCount))) {
        // -------------------- 全セッションを走査 --------------------
        for (int i = 0; i < sessionCount; ++i) {
            IAudioSessionControl* sessionControl = nullptr;
            IAudioSessionControl2* sessionControl2 = nullptr;
            ISimpleAudioVolume* simpleVolume = nullptr;

            if (FAILED(sessionEnumerator->GetSession(i, &sessionControl)) || sessionControl == nullptr) {
                continue;
            }

            // ------------------ PID 一致のセッションを探す ------------------
            if (SUCCEEDED(sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&sessionControl2))) && sessionControl2 != nullptr) {
                DWORD foundPid = 0;
                if (SUCCEEDED(sessionControl2->GetProcessId(&foundPid)) && foundPid == pid) {
                    if (SUCCEEDED(sessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&simpleVolume))) && simpleVolume != nullptr) {
                        foundVolume = simpleVolume;
                        safeRelease(sessionControl2);
                        safeRelease(sessionControl);
                        break;
                    }
                }
            }

            safeRelease(sessionControl2);
            safeRelease(sessionControl);
        }
    }

    safeRelease(sessionEnumerator);
    return foundVolume;
}

/// @brief PID から表示用アイコンを読み込む
/// @param pid 対象プロセス ID
/// @return 読み込んだアイコン画像
std::unique_ptr<IconImage> loadProcessIcon(DWORD pid) {
    // -------------------- 実行ファイルパスを取得 --------------------
    const std::wstring exePath = getProcessImagePath(pid);
    if (exePath.empty()) {
        return nullptr;
    }

    // -------------------- 実パスからアイコン化 --------------------
    return std::make_unique<IconImage>(exePath);
}

/// @brief マスター音量の状態を Process へ変換する
/// @param defaultDevice 既定再生デバイス
/// @return マスター用 Process
Process createMasterProcess(IMMDevice* defaultDevice) {
    IAudioEndpointVolume* endpointVolume = nullptr;
    IAudioMeterInformation* endpointMeter = nullptr;

    float masterVolume = 0.0f;
    float masterPeak = 0.0f;
    BOOL masterMuted = FALSE;

    // ------------------ マスター音量を取得 ------------------
    if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&endpointVolume))) && endpointVolume != nullptr) {
        endpointVolume->GetMasterVolumeLevelScalar(&masterVolume);
        endpointVolume->GetMute(&masterMuted);
    }

    // ------------------ マスターピークを取得 ------------------
    if (SUCCEEDED(defaultDevice->Activate(IID_IAudioMeterInformation_Local, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&endpointMeter))) && endpointMeter != nullptr) {
        endpointMeter->GetPeakValue(&masterPeak);
    }

    safeRelease(endpointMeter);
    safeRelease(endpointVolume);

    return Process{
        SoundMixer::MASTER_PID,
        nullptr,
        toPercent(masterPeak),
        toPercent(masterVolume),
        (masterMuted != FALSE),
    };
}

/// @brief すべてのセッションを Process 一覧へ追加する
/// @param defaultDevice 既定再生デバイス
/// @param latestProcesses 追加先の一覧
void appendSessionProcesses(IMMDevice* defaultDevice, std::vector<Process>& latestProcesses) {
    IAudioSessionManager2* sessionManager = nullptr;
    IAudioSessionEnumerator* sessionEnumerator = nullptr;

    // ---------------- セッション管理を取得 ----------------
    if (FAILED(defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&sessionManager))) ||
        sessionManager == nullptr) {
        return;
    }

    // ---------------- セッション列挙子を取得 ----------------
    if (FAILED(sessionManager->GetSessionEnumerator(&sessionEnumerator)) || sessionEnumerator == nullptr) {
        safeRelease(sessionManager);
        return;
    }

    int sessionCount = 0;
    if (SUCCEEDED(sessionEnumerator->GetCount(&sessionCount))) {
        // -------------------- 全セッションを変換 --------------------
        for (int i = 0; i < sessionCount; ++i) {
            IAudioSessionControl* sessionControl = nullptr;
            IAudioSessionControl2* sessionControl2 = nullptr;
            ISimpleAudioVolume* simpleVolume = nullptr;
            IAudioMeterInformation* sessionMeter = nullptr;

            if (FAILED(sessionEnumerator->GetSession(i, &sessionControl)) || sessionControl == nullptr) {
                continue;
            }

            DWORD pid = 0;
            float volume = 0.0f;
            float peak = 0.0f;
            BOOL muted = FALSE;

            // -------------------- PID を取得 --------------------
            if (SUCCEEDED(sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&sessionControl2))) && sessionControl2 != nullptr) {
                sessionControl2->GetProcessId(&pid);
            }

            // -------------------- 音量状態を取得 --------------------
            if (SUCCEEDED(sessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&simpleVolume))) && simpleVolume != nullptr) {
                simpleVolume->GetMasterVolume(&volume);
                simpleVolume->GetMute(&muted);
            }

            // -------------------- ピーク値を取得 --------------------
            if (SUCCEEDED(sessionControl->QueryInterface(IID_IAudioMeterInformation_Local, reinterpret_cast<void**>(&sessionMeter))) && sessionMeter != nullptr) {
                sessionMeter->GetPeakValue(&peak);
            }

            // ------------------- 無効セッションを除外 -------------------
            if (pid == 0) {
                safeRelease(sessionMeter);
                safeRelease(simpleVolume);
                safeRelease(sessionControl2);
                safeRelease(sessionControl);
                continue;
            }

            // -------------------- 一覧へ追加 --------------------
            latestProcesses.emplace_back(Process{
                pid,
                loadProcessIcon(pid),
                toPercent(peak),
                toPercent(volume),
                (muted != FALSE),
            });

            safeRelease(sessionMeter);
            safeRelease(simpleVolume);
            safeRelease(sessionControl2);
            safeRelease(sessionControl);
        }
    }

    safeRelease(sessionEnumerator);
    safeRelease(sessionManager);
}

/// @brief Process 一覧を表示順にソートする
/// @param processes ソート対象
void sortProcesses(std::vector<Process>& processes) {
    const auto processSorter = [](const Process& a, const Process& b) {
        if (a.pid == SoundMixer::MASTER_PID) return true;
        if (b.pid == SoundMixer::MASTER_PID) return false;
        return a.pid < b.pid;
    };

    std::sort(processes.begin(), processes.end(), processSorter);
}

/// @brief 指定 index が表示中スロットに含まれるかを判定する
/// @param processIdx Process 配列 index
/// @param offset ページオフセット
/// @param slaveCount 表示 slave 数
/// @return 表示中なら true
bool isVisibleProcessIndex(size_t processIdx, uint8_t offset, uint8_t slaveCount) {
    return (processIdx == 0) ||
        (processIdx >= static_cast<size_t>(offset + 1) &&
         processIdx <= static_cast<size_t>(offset + slaveCount));
}

/// @brief Process index から表示スロット index を求める
/// @param processIdx Process 配列 index
/// @param offset ページオフセット
/// @param slotCount スロット数
/// @return 対応する表示スロット index
size_t getViewSlotFromProcessIndex(size_t processIdx, uint8_t offset, size_t slotCount) {
    if (processIdx == 0) {
        return 0;
    }
    if (processIdx >= static_cast<size_t>(offset + 1)) {
        return processIdx - offset;
    }
    return slotCount;
}


/* ---------------------------------------------------------
 クラス実装
--------------------------------------------------------- */
/// @brief slave 数を指定して SoundMixer を初期化する
/// @param slave_count 接続される slave 数
SoundMixer::SoundMixer(uint8_t slave_count):
    m_slaveCount(0),
    m_offset(0)
{
    setSlaveCount(slave_count);
}

/// @brief slave 数を更新し，必要な表示更新フラグを立てる
/// @param slave_count 接続される slave 数
void SoundMixer::setSlaveCount(uint8_t slave_count){
    // -------------------- 表示更新領域を拡張 --------------------
    m_hasUpdate.resize(slave_count + 1);

    // ------------------- 追加領域を更新扱い -------------------
    for(uint8_t i = m_slaveCount; i < slave_count + 1; ++i){
        m_hasUpdate[i] = true;
    }

    // ---------------------- slave 数を反映 ----------------------
    m_slaveCount = slave_count;
}


/// @brief 1 ページ前へ移動する
void SoundMixer::prevPage(){
    // -------------------- 先頭ページを判定 --------------------
    if(m_offset == 0) return;

    // -------------------- オフセットを更新 --------------------
    m_offset--;
    markPageDisplayChanged();
}


/// @brief 1 ページ次へ移動する
void SoundMixer::nextPage(){
    // -------------------- 末尾ページを判定 --------------------
    if(static_cast<size_t>(m_offset) + static_cast<size_t>(m_slaveCount) + 1u >= m_processes.size()) return;

    // -------------------- オフセットを更新 --------------------
    m_offset++;
    markPageDisplayChanged();
}

/// @brief 指定 slave の表示内容に更新があるかを返す
/// @param slave_id 対象 slave ID
/// @return 更新が必要なら true
bool SoundMixer::hasDisplayChange(uint8_t slave_id){
    // ------------------- 範囲外アクセスを回避 -------------------
    if(slave_id >= m_hasUpdate.size()) return false;
    return m_hasUpdate[slave_id];
}

/// @brief 現在ページの表示スロットを更新扱いにする
void SoundMixer::markPageDisplayChanged(){
    // ------------------ 表示中ページを更新扱い ------------------
    for(uint8_t i = 1; i < m_slaveCount + 1; ++i){
        m_hasUpdate[i] = true;
    }
}

/// @brief 新旧 Process 一覧を比較して表示更新フラグを立てる
/// @param latestProcesses 最新の Process 一覧
void SoundMixer::updateDisplayFlags(const std::vector<Process>& latestProcesses){
    const size_t oldSize = m_processes.size();
    const size_t newSize = latestProcesses.size();

    // ----------------------- 表示更新を判定 -----------------------
    for (size_t i = 0; i < newSize; ++i) {
        const bool inView = isVisibleProcessIndex(i, m_offset, m_slaveCount);
        const size_t viewSlot = getViewSlotFromProcessIndex(i, m_offset, m_hasUpdate.size());

        // --------------- 明らかに新規追加なら更新フラグ ---------------
        if(i >= oldSize){
            if (inView && viewSlot < m_hasUpdate.size()) {
                m_hasUpdate[viewSlot] = true;
            }
            continue;
        }

        // ------------------- 表示対象なら中身を検査 -------------------
        if(inView){
            if(m_processes[i].pid != latestProcesses[i].pid){
                if (viewSlot < m_hasUpdate.size()) {
                    m_hasUpdate[viewSlot] = true;
                }
                continue;
            }
        }
    }
}


/// @brief オーディオセッションを再列挙して内部状態を更新する
void SoundMixer::refleshProcess() {
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return;
    }

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* defaultDevice = nullptr;

    // ------------------ 既定オーディオデバイスを取得 ------------------
    if (!createDefaultAudioDevice(deviceEnumerator, defaultDevice)) {
        if (shouldUninitialize) CoUninitialize();
        return;
    }

    std::vector<Process> latestProcesses;

    // ------------------- マスターのレベルを格納 -------------------
    latestProcesses.emplace_back(createMasterProcess(defaultDevice));

    // ------------------ 全プロセスのレベルを格納 ------------------
    appendSessionProcesses(defaultDevice, latestProcesses);

    // ----------------------- pidでソート -----------------------
    sortProcesses(latestProcesses);
    sortProcesses(m_processes);

    // -------------------- 表示更新を反映 --------------------
    updateDisplayFlags(latestProcesses);

    // ---------------------- 状態を更新 ----------------------
    m_processes = std::move(latestProcesses);

    safeRelease(defaultDevice);
    safeRelease(deviceEnumerator);

    if (shouldUninitialize) {
        CoUninitialize();
    }
}



/// @brief 指定 slave の表示用バイナリを返す
/// @param slave_id 対象 slave ID
/// @return 表示データ
std::vector<uint8_t>& SoundMixer::getDisplayData(uint8_t slave_id){
    static std::vector<uint8_t> empty;

    // -------------------- 対象プロセスを取得 --------------------
    const size_t processIdx = getDisplayProcessIndex(slave_id);
    if(processIdx >= m_processes.size()){
        return empty;
    }

    // ------------------- 表示バッファを構築 -------------------
    buildDisplayCache(m_processes[processIdx]);

    if (slave_id < m_hasUpdate.size()) {
        m_hasUpdate[slave_id] = false;
    }

    return m_displayCache;
}


/// @brief 指定 slave のレベルメータ点灯パターンを返す
/// @param slave_id 対象 slave ID
/// @return レベルメータの 16bit パターン
uint16_t SoundMixer::getLevelmeterData(uint8_t slave_id){
    // -------------------- 対象プロセスを取得 --------------------
    const size_t processIdx = getDisplayProcessIndex(slave_id);
    if(processIdx >= m_processes.size()){
        return 0;
    }

    // --------------------- 点灯数へ変換 ---------------------
    uint8_t lvl = m_processes[processIdx].level * 15 / 100;

    return static_cast<uint16_t>((1u << lvl) - 1);
}

/// @brief slave ID から Process 配列 index を求める
/// @param slave_id 対象 slave ID
/// @return Process 配列 index
size_t SoundMixer::getDisplayProcessIndex(uint8_t slave_id) const{
    if(slave_id == 0) return 0;
    return static_cast<size_t>(slave_id) + static_cast<size_t>(m_offset);
}

/// @brief 1 つの Process から表示バッファを生成する
/// @param process 表示対象 Process
void SoundMixer::buildDisplayCache(const Process& process){
    // -------------------- 数値画像を生成 --------------------
    auto num = NumberImage(process.volume, process.isMuted);

    // -------------------- 合成画像を生成 --------------------
    if (process.img) {
        auto img = DisplayingImage(*process.img, num);
        m_displayCache = img.getBinary();
    } else {
        IconImage fallback(L"");
        auto img = DisplayingImage(fallback, num);
        m_displayCache = img.getBinary();
    }
}

/// @brief マスター音量を増減する
/// @param step 増減量
/// @param processIdx 更新対象の Process 配列 index
/// @param slave_id 表示更新対象の slave ID
/// @param defaultDevice 既定再生デバイス
/// @return 更新できたら true
bool SoundMixer::applyMasterVolumeStep(float step, size_t processIdx, uint8_t slave_id, IMMDevice* defaultDevice){
    IAudioEndpointVolume* endpointVolume = nullptr;

    // ----------------- マスター音量インターフェイス取得 -----------------
    if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&endpointVolume))) && endpointVolume != nullptr) {
        float current = 0.0f;

        // -------------------- 音量を増減して反映 --------------------
        if (SUCCEEDED(endpointVolume->GetMasterVolumeLevelScalar(&current))) {
            current = std::clamp(current + step, 0.0f, 1.0f);
            endpointVolume->SetMasterVolumeLevelScalar(current, nullptr);
            m_processes[processIdx].volume = toPercent(current);
            m_hasUpdate[slave_id] = true;
            safeRelease(endpointVolume);
            return true;
        }
    }

    safeRelease(endpointVolume);
    return false;
}

/// @brief セッション音量を増減する
/// @param step 増減量
/// @param processIdx 更新対象の Process 配列 index
/// @param slave_id 表示更新対象の slave ID
/// @param defaultDevice 既定再生デバイス
/// @return 更新できたら true
bool SoundMixer::applySessionVolumeStep(float step, size_t processIdx, uint8_t slave_id, IMMDevice* defaultDevice){
    IAudioSessionManager2* sessionManager = nullptr;

    // ---------------- セッション管理インターフェイス取得 ----------------
    if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&sessionManager))) && sessionManager != nullptr) {
        ISimpleAudioVolume* simpleVolume = findSessionVolumeByPid(sessionManager, m_processes[processIdx].pid);
        if (simpleVolume != nullptr) {
            float current = 0.0f;

            // -------------------- 音量を増減して反映 --------------------
            if (SUCCEEDED(simpleVolume->GetMasterVolume(&current))) {
                current = std::clamp(current + step, 0.0f, 1.0f);
                simpleVolume->SetMasterVolume(current, nullptr);
                m_processes[processIdx].volume = toPercent(current);
                m_hasUpdate[slave_id] = true;
                safeRelease(simpleVolume);
                safeRelease(sessionManager);
                return true;
            }
            safeRelease(simpleVolume);
        }
    }

    safeRelease(sessionManager);
    return false;
}


/// @brief 指定 slave に対応する音量を増減する
/// @param slave_id 対象 slave ID
/// @param positive true で増加，false で減少
void SoundMixer::setVolume(uint8_t slave_id, bool positive){
    // -------------------- 対象プロセスを取得 --------------------
    const size_t processIdx = getDisplayProcessIndex(slave_id);
    if(processIdx >= m_processes.size()) return;
    const DWORD pid = m_processes[processIdx].pid;

    if(m_processes[processIdx].volume >= 100 && positive) return;
    if(m_processes[processIdx].volume <= 0 && !positive) return;

    HRESULT initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return;
    }

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* defaultDevice = nullptr;

    // ------------------ 既定オーディオデバイスを取得 ------------------
    if (!createDefaultAudioDevice(deviceEnumerator, defaultDevice)) {
        if (shouldUninitialize) CoUninitialize();
        return;
    }

    const float step = positive ? 0.05f : -0.05f;

    // ------------------- 対象に応じて音量変更 -------------------
    if (pid == MASTER_PID) {
        applyMasterVolumeStep(step, processIdx, slave_id, defaultDevice);
    } else {
        applySessionVolumeStep(step, processIdx, slave_id, defaultDevice);
    }

    safeRelease(defaultDevice);
    safeRelease(deviceEnumerator);

    if (shouldUninitialize) {
        CoUninitialize();
    }
}

/// @brief マスターミュートを切り替える
/// @param processIdx 更新対象の Process 配列 index
/// @param slave_id 表示更新対象の slave ID
/// @param defaultDevice 既定再生デバイス
/// @return 更新できたら true
bool SoundMixer::applyMasterMuteToggle(size_t processIdx, uint8_t slave_id, IMMDevice* defaultDevice){
    IAudioEndpointVolume* endpointVolume = nullptr;

    // ---------------- マスター音量インターフェイス取得 ----------------
    if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&endpointVolume))) && endpointVolume != nullptr) {
        BOOL muted = FALSE;

        // ------------------- ミュート状態を反転 -------------------
        if (SUCCEEDED(endpointVolume->GetMute(&muted))) {
            const BOOL nextMuted = (muted == FALSE) ? TRUE : FALSE;
            endpointVolume->SetMute(nextMuted, nullptr);
            m_processes[processIdx].isMuted = (nextMuted != FALSE);
            m_hasUpdate[slave_id] = true;
            safeRelease(endpointVolume);
            return true;
        }
    }

    safeRelease(endpointVolume);
    return false;
}

/// @brief セッションミュートを切り替える
/// @param processIdx 更新対象の Process 配列 index
/// @param slave_id 表示更新対象の slave ID
/// @param defaultDevice 既定再生デバイス
/// @return 更新できたら true
bool SoundMixer::applySessionMuteToggle(size_t processIdx, uint8_t slave_id, IMMDevice* defaultDevice){
    IAudioSessionManager2* sessionManager = nullptr;

    // ---------------- セッション管理インターフェイス取得 ----------------
    if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&sessionManager))) && sessionManager != nullptr) {
        ISimpleAudioVolume* simpleVolume = findSessionVolumeByPid(sessionManager, m_processes[processIdx].pid);
        if (simpleVolume != nullptr) {
            BOOL muted = FALSE;

            // ------------------- ミュート状態を反転 -------------------
            if (SUCCEEDED(simpleVolume->GetMute(&muted))) {
                const BOOL nextMuted = (muted == FALSE) ? TRUE : FALSE;
                simpleVolume->SetMute(nextMuted, nullptr);
                m_processes[processIdx].isMuted = (nextMuted != FALSE);
                m_hasUpdate[slave_id] = true;
                safeRelease(simpleVolume);
                safeRelease(sessionManager);
                return true;
            }
            safeRelease(simpleVolume);
        }
    }

    safeRelease(sessionManager);
    return false;
}


/// @brief 指定 slave に対応するミュート状態を切り替える
/// @param slave_id 対象 slave ID
void SoundMixer::toggleMuted(uint8_t slave_id){
    // -------------------- 対象プロセスを取得 --------------------
    const size_t processIdx = getDisplayProcessIndex(slave_id);
    if(processIdx >= m_processes.size()) return;
    const DWORD pid = m_processes[processIdx].pid;

    HRESULT initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return;
    }

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* defaultDevice = nullptr;

    // ------------------ 既定オーディオデバイスを取得 ------------------
    if (!createDefaultAudioDevice(deviceEnumerator, defaultDevice)) {
        if (shouldUninitialize) CoUninitialize();
        return;
    }

    // ------------------ 対象に応じてミュート切替 ------------------
    if (pid == MASTER_PID) {
        applyMasterMuteToggle(processIdx, slave_id, defaultDevice);
    } else {
        applySessionMuteToggle(processIdx, slave_id, defaultDevice);
    }

    safeRelease(defaultDevice);
    safeRelease(deviceEnumerator);

    if (shouldUninitialize) {
        CoUninitialize();
    }
}
