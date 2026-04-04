#ifndef _SOUNDMIXER_HPP_
#define _SOUNDMIXER_HPP_

#include <Windows.h>
#include <string>
#include <memory>
#include <vector>
#include "Images.hpp"

struct IMMDevice;

/// @brief 1 つの表示対象プロセスの状態
struct Process {
    DWORD pid;
    std::unique_ptr<IconImage> img;
    uint8_t level;
    uint8_t volume;
    bool isMuted;
};

/// @brief オーディオセッションの列挙と表示用状態を管理する
class SoundMixer{
public:
    static constexpr DWORD MASTER_PID = 0xFFFFFFFF;

    SoundMixer(uint8_t slave_count);
    void setSlaveCount(uint8_t slave_count);
    void prevPage();
    void nextPage();
    void setVolume(uint8_t slave_id, bool positive);
    void toggleMuted(uint8_t slave_id);
    void refleshProcess();
    bool hasDisplayChange(uint8_t slave_id);
    std::vector<uint8_t>& getDisplayData(uint8_t slave_id);
    uint16_t getLevelmeterData(uint8_t slave_id);

private:
    void markPageDisplayChanged();
    void updateDisplayFlags(const std::vector<Process>& latestProcesses);
    void buildDisplayCache(const Process& process);
    size_t getDisplayProcessIndex(uint8_t slave_id) const;
    bool applyMasterVolumeStep(float step, size_t processIdx, uint8_t slave_id, IMMDevice* defaultDevice);
    bool applySessionVolumeStep(float step, size_t processIdx, uint8_t slave_id, IMMDevice* defaultDevice);
    bool applyMasterMuteToggle(size_t processIdx, uint8_t slave_id, IMMDevice* defaultDevice);
    bool applySessionMuteToggle(size_t processIdx, uint8_t slave_id, IMMDevice* defaultDevice);

    std::vector<Process> m_processes;
    std::vector<bool> m_hasUpdate;
    std::vector<uint8_t> m_displayCache;
    uint8_t m_slaveCount;
    uint8_t m_offset;
};

#endif
