#include "SoundMixer.hpp"

#include <algorithm>
#include <cmath>

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
    if (ptr != nullptr) {
        ptr->Release();
        ptr = nullptr;
    }
}

uint8_t toPercent(float value) {
    const float clamped = std::clamp(value, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(clamped * 100.0f));
}

float toScalar(uint8_t value) {
    return std::clamp(static_cast<float>(value) / 100.0f, 0.0f, 1.0f);
}

std::wstring getProcessImagePath(DWORD pid) {
    if (pid == 0) {
        return L"";
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (process == nullptr) {
        return L"";
    }

    std::wstring path(32768, L'\0');
    DWORD len = static_cast<DWORD>(path.size());
    const BOOL ok = QueryFullProcessImageNameW(process, 0, path.data(), &len);
    CloseHandle(process);

    if (!ok) {
        return L"";
    }

    path.resize(len);
    return path;
}

ISimpleAudioVolume* findSessionVolumeByPid(IAudioSessionManager2* sessionManager, DWORD pid) {
    if (sessionManager == nullptr) {
        return nullptr;
    }

    IAudioSessionEnumerator* sessionEnumerator = nullptr;
    ISimpleAudioVolume* foundVolume = nullptr;

    if (FAILED(sessionManager->GetSessionEnumerator(&sessionEnumerator)) || sessionEnumerator == nullptr) {
        return nullptr;
    }

    int sessionCount = 0;
    if (SUCCEEDED(sessionEnumerator->GetCount(&sessionCount))) {
        for (int i = 0; i < sessionCount; ++i) {
            IAudioSessionControl* sessionControl = nullptr;
            IAudioSessionControl2* sessionControl2 = nullptr;
            ISimpleAudioVolume* simpleVolume = nullptr;

            if (FAILED(sessionEnumerator->GetSession(i, &sessionControl)) || sessionControl == nullptr) {
                continue;
            }

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


/* ---------------------------------------------------------
 クラス実装
--------------------------------------------------------- */
SoundMixer::SoundMixer(uint8_t slave_count):
    m_slaveCount(0),
    m_offset(0)
{
    setSlaveCount(slave_count);
}

void SoundMixer::setSlaveCount(uint8_t slave_count){
    m_hasUpdate.resize(slave_count + 1);
    for(uint8_t i = m_slaveCount; i < slave_count + 1; ++i){
        m_hasUpdate[i] = true;
    }
    m_slaveCount = slave_count;
}


void SoundMixer::prevPage(){
    if(m_offset == 0) return;
    m_offset--;
    for(uint8_t i = 1; i < m_slaveCount + 1; ++i){
        m_hasUpdate[i] = true;
    }
}


void SoundMixer::nextPage(){
    if(m_offset + m_slaveCount >= m_processes.size()) return;
    m_offset++;
    for(uint8_t i = 1; i < m_slaveCount + 1; ++i){
        m_hasUpdate[i] = true;
    }
}

bool SoundMixer::hasDisplayChange(uint8_t slave_id){
    if(slave_id >= m_hasUpdate.size()) return false;
    return m_hasUpdate[slave_id];
}

void SoundMixer::refleshProcess() {
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return;
    }

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* defaultDevice = nullptr;
    IAudioEndpointVolume* endpointVolume = nullptr;
    IAudioMeterInformation* endpointMeter = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;
    IAudioSessionEnumerator* sessionEnumerator = nullptr;

    const HRESULT createResult = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&deviceEnumerator));
    if (FAILED(createResult)) {
        if (shouldUninitialize) CoUninitialize();
        return;
    }

    if (FAILED(deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {
        safeRelease(deviceEnumerator);
        if (shouldUninitialize) CoUninitialize();
        return;
    }

    std::vector<Process> latestProcesses;

    float masterVolume = 0.0f;
    float masterPeak = 0.0f;
    BOOL masterMuted = FALSE;

    if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&endpointVolume)))) {
        endpointVolume->GetMasterVolumeLevelScalar(&masterVolume);
        endpointVolume->GetMute(&masterMuted);
    }

    if (SUCCEEDED(defaultDevice->Activate(IID_IAudioMeterInformation_Local, CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&endpointMeter))) && endpointMeter != nullptr) {
        endpointMeter->GetPeakValue(&masterPeak);
    }

    // ------------------- マスターのレベルを格納 -------------------
    latestProcesses.emplace_back(Process{
        MASTER_PID,
        nullptr,
        toPercent(masterPeak),
        toPercent(masterVolume),
        (masterMuted != FALSE),
    });
    


    // ------------------ 全プロセスのレベルを格納 ------------------
    if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&sessionManager))) &&
        SUCCEEDED(sessionManager->GetSessionEnumerator(&sessionEnumerator))) {
        int sessionCount = 0;
        if (SUCCEEDED(sessionEnumerator->GetCount(&sessionCount))) {
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

                if (SUCCEEDED(sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), reinterpret_cast<void**>(&sessionControl2))) && sessionControl2 != nullptr) {
                    sessionControl2->GetProcessId(&pid);
                }

                if (SUCCEEDED(sessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), reinterpret_cast<void**>(&simpleVolume))) && simpleVolume != nullptr) {
                    simpleVolume->GetMasterVolume(&volume);
                    simpleVolume->GetMute(&muted);
                }

                if (SUCCEEDED(sessionControl->QueryInterface(IID_IAudioMeterInformation_Local, reinterpret_cast<void**>(&sessionMeter))) && sessionMeter != nullptr) {
                    sessionMeter->GetPeakValue(&peak);
                }

                std::unique_ptr<IconImage> icon = nullptr;
                LPWSTR iconPath = nullptr;
                if (SUCCEEDED(sessionControl->GetIconPath(&iconPath)) && iconPath != nullptr) {
                    icon = std::make_unique<IconImage>(iconPath);
                    CoTaskMemFree(iconPath);
                }

                if (icon == nullptr) {
                    std::wstring exePath = getProcessImagePath(pid);
                    if (!exePath.empty()) {
                        icon = std::make_unique<IconImage>(exePath.data());
                    }
                }

                latestProcesses.emplace_back(Process{
                    pid,
                    std::move(icon),
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
    }

    // ----------------------- pidでソート -----------------------
    const auto processSorter = [](const Process& a, const Process& b) {
        if (a.pid == SoundMixer::MASTER_PID) return true;
        if (b.pid == SoundMixer::MASTER_PID) return false;
        return a.pid < b.pid;
    };

    std::sort(latestProcesses.begin(), latestProcesses.end(), processSorter);
    std::sort(m_processes.begin(), m_processes.end(), processSorter);


    const size_t oldSize = m_processes.size();
    const size_t newSize = latestProcesses.size();

    for (size_t i = 0; i < newSize; ++i) {
        const bool inView = (i == 0) ||
            (i >= static_cast<size_t>(m_offset + 1) && i <= static_cast<size_t>(m_offset + m_slaveCount));

        size_t viewSlot = m_hasUpdate.size();
        if (i == 0) {
            viewSlot = 0;
        } else if (i >= static_cast<size_t>(m_offset + 1)) {
            viewSlot = i - m_offset;
        }

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
            if(m_processes[i].volume != latestProcesses[i].volume){
                if (viewSlot < m_hasUpdate.size()) {
                    m_hasUpdate[viewSlot] = true;
                }
                continue;
            }
            if(m_processes[i].isMuted != latestProcesses[i].isMuted){
                if (viewSlot < m_hasUpdate.size()) {
                    m_hasUpdate[viewSlot] = true;
                }
                continue;
            }
        }
    }

    m_processes = std::move(latestProcesses);

    safeRelease(sessionEnumerator);
    safeRelease(sessionManager);
    safeRelease(endpointMeter);
    safeRelease(endpointVolume);
    safeRelease(defaultDevice);
    safeRelease(deviceEnumerator);

    if (shouldUninitialize) {
        CoUninitialize();
    }
}



std::vector<uint8_t>& SoundMixer::getDisplayData(uint8_t slave_id){
    static std::vector<uint8_t> empty;
    const size_t processIdx = static_cast<size_t>(slave_id) + static_cast<size_t>(m_offset);
    if(processIdx >= m_processes.size()){
        return empty;
    }

    auto num = NumberImage(m_processes[processIdx].volume, m_processes[processIdx].isMuted);
    if (m_processes[processIdx].img) {
        auto img = DisplayingImage(*m_processes[processIdx].img, num);
        m_displayCache = img.getBinary();
    } else {
        IconImage fallback(nullptr);
        auto img = DisplayingImage(fallback, num);
        m_displayCache = img.getBinary();
    }

    if (slave_id < m_hasUpdate.size()) {
        m_hasUpdate[slave_id] = false;
    }

    return m_displayCache;
}


uint16_t SoundMixer::getLevelmeterData(uint8_t slave_id){
    const size_t processIdx = 1 + static_cast<size_t>(slave_id) + static_cast<size_t>(m_offset);
    if(processIdx >= m_processes.size()){
        return 0;
    }

    uint8_t lvl = m_processes[processIdx].level * 15 / 100;

    return static_cast<uint16_t>((1u << lvl) - 1);
}


void SoundMixer::setVolume(uint8_t slave_id, bool positive){
    DWORD processIdx = 1 + slave_id + m_offset;
    if(processIdx >= m_processes.size()) return;
    const DWORD pid = m_processes[processIdx].pid;

    HRESULT initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return;
    }

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* defaultDevice = nullptr;
    IAudioEndpointVolume* endpointVolume = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;

    const HRESULT createResult = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&deviceEnumerator));
    if (FAILED(createResult)) {
        if (shouldUninitialize) CoUninitialize();
        return;
    }

    if (FAILED(deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {
        safeRelease(deviceEnumerator);
        if (shouldUninitialize) CoUninitialize();
        return;
    }

    const float step = positive ? 0.05f : -0.05f;

    if (pid == MASTER_PID) {
        if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&endpointVolume))) && endpointVolume != nullptr) {
            float current = 0.0f;
            if (SUCCEEDED(endpointVolume->GetMasterVolumeLevelScalar(&current))) {
                current = std::clamp(current + step, 0.0f, 1.0f);
                endpointVolume->SetMasterVolumeLevelScalar(current, nullptr);
                m_processes[processIdx].volume = static_cast<uint8_t>(std::lround(current * 100.0f));
                m_hasUpdate[slave_id] = true;
            }
        }
    } else if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&sessionManager))) && sessionManager != nullptr) {
        ISimpleAudioVolume* simpleVolume = findSessionVolumeByPid(sessionManager, pid);
        if (simpleVolume != nullptr) {
            float current = 0.0f;
            if (SUCCEEDED(simpleVolume->GetMasterVolume(&current))) {
                current = std::clamp(current + step, 0.0f, 1.0f);
                simpleVolume->SetMasterVolume(current, nullptr);
                m_processes[processIdx].volume = static_cast<uint8_t>(std::lround(current * 100.0f));
                m_hasUpdate[slave_id] = true;
            }
            safeRelease(simpleVolume);
        }
    }

    safeRelease(sessionManager);
    safeRelease(endpointVolume);
    safeRelease(defaultDevice);
    safeRelease(deviceEnumerator);

    if (shouldUninitialize) {
        CoUninitialize();
    }
}

void SoundMixer::toggleMuted(uint8_t slave_id){
    DWORD processIdx = slave_id + m_offset;
    if(processIdx >= m_processes.size()) return;
    const DWORD pid = m_processes[processIdx].pid;

    HRESULT initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninitialize = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return;
    }

    IMMDeviceEnumerator* deviceEnumerator = nullptr;
    IMMDevice* defaultDevice = nullptr;
    IAudioEndpointVolume* endpointVolume = nullptr;
    IAudioSessionManager2* sessionManager = nullptr;

    const HRESULT createResult = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&deviceEnumerator));
    if (FAILED(createResult)) {
        if (shouldUninitialize) CoUninitialize();
        return;
    }

    if (FAILED(deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultDevice))) {
        safeRelease(deviceEnumerator);
        if (shouldUninitialize) CoUninitialize();
        return;
    }

    if (pid == MASTER_PID) {
        if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&endpointVolume))) && endpointVolume != nullptr) {
            BOOL muted = FALSE;
            if (SUCCEEDED(endpointVolume->GetMute(&muted))) {
                const BOOL nextMuted = (muted == FALSE) ? TRUE : FALSE;
                endpointVolume->SetMute(nextMuted, nullptr);
                m_processes[processIdx].isMuted = (nextMuted != FALSE);
                m_hasUpdate[slave_id] = true;
            }
        }
    } else if (SUCCEEDED(defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&sessionManager))) && sessionManager != nullptr) {
        ISimpleAudioVolume* simpleVolume = findSessionVolumeByPid(sessionManager, pid);
        if (simpleVolume != nullptr) {
            BOOL muted = FALSE;
            if (SUCCEEDED(simpleVolume->GetMute(&muted))) {
                const BOOL nextMuted = (muted == FALSE) ? TRUE : FALSE;
                simpleVolume->SetMute(nextMuted, nullptr);
                m_processes[processIdx].isMuted = (nextMuted != FALSE);
                m_hasUpdate[slave_id] = true;
            }
            safeRelease(simpleVolume);
        }
    }

    safeRelease(sessionManager);
    safeRelease(endpointVolume);
    safeRelease(defaultDevice);
    safeRelease(deviceEnumerator);

    if (shouldUninitialize) {
        CoUninitialize();
    }
}