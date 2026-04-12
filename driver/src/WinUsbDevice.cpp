#include "WinUsbDevice.hpp"

#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>
#include <stdio.h>
#include <string>


/// @brief WinUSB デバイス状態を初期化する
WinUsbDevice::WinUsbDevice()
    : m_deviceHandle(INVALID_HANDLE_VALUE),
      m_winusbHandle(NULL),
      m_disconnected(false)
{
}

/// @brief オブジェクト破棄時にデバイスを閉じる
WinUsbDevice::~WinUsbDevice()
{
    close();
}


/// @brief 対象 USB デバイスのパスを列挙して取得する
/// @return 見つかったデバイスパス
std::string WinUsbDevice::findDevicePath()
{
    // ------------------- デバイス一覧を取得 -------------------
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(
        &DEVICE_INTERFACE_GUID,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[ERROR] SetupDiGetClassDevs failed: %lu\n", GetLastError());
        return "";
    }

    SP_DEVICE_INTERFACE_DATA interfaceData = {};
    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    std::string result;

    // ---------------- インターフェイスを順に列挙 ----------------
    for (DWORD idx = 0; ; ++idx) {
        if (!SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL,
                &DEVICE_INTERFACE_GUID, idx, &interfaceData)) {
            break;
        }

        // Get required buffer size
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(
            deviceInfoSet, &interfaceData, NULL, 0, &requiredSize, NULL);

        if (requiredSize == 0) continue;

        std::vector<uint8_t> detailBuf(requiredSize);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(detailBuf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        // ----------------- VID/PID が一致するか判定 -----------------
        if (SetupDiGetDeviceInterfaceDetailA(
                deviceInfoSet, &interfaceData, detail, requiredSize, NULL, NULL)) {
            // Check if VID/PID match by looking at the device path string
            std::string path(detail->DevicePath);
            // Device paths typically contain vid_XXXX&pid_XXXX
            char vidStr[16], pidStr[16];
            snprintf(vidStr, sizeof(vidStr), "vid_%04x", USB_VID);
            snprintf(pidStr, sizeof(pidStr), "pid_%04x", USB_PID);

            // Case-insensitive search in path
            std::string pathLower = path;
            for (auto& c : pathLower) c = static_cast<char>(tolower(c));

            if (pathLower.find(vidStr) != std::string::npos &&
                pathLower.find(pidStr) != std::string::npos) {
                result = path;
                break;
            }
        }
    }

    // -------------------- 列挙ハンドルを解放 --------------------
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return result;
}


/// @brief デバイスを検索してオープンする
/// @return オープンに成功したら true
bool WinUsbDevice::open()
{
    // ------------------- 二重オープンを回避 -------------------
    if (isOpen()) {
        fprintf(stderr, "[WARN] Device already open\n");
        return true;
    }

    // ------------------- デバイスパスを取得 -------------------
    std::string devicePath = findDevicePath();
    if (devicePath.empty()) {
        fprintf(stderr, "[ERROR] Device not found (VID=%04X, PID=%04X)\n",
                USB_VID, USB_PID);
        return false;
    }

    printf("[INFO] Device path: %s\n", devicePath.c_str());

    // ---------------- デバイスハンドルをオープン ----------------
    m_deviceHandle = CreateFileA(
        devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (m_deviceHandle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[ERROR] CreateFile failed: %lu\n", GetLastError());
        return false;
    }

    // -------------------- WinUSB を初期化 --------------------
    if (!WinUsb_Initialize(m_deviceHandle, &m_winusbHandle)) {
        fprintf(stderr, "[ERROR] WinUsb_Initialize failed: %lu\n", GetLastError());
        CloseHandle(m_deviceHandle);
        m_deviceHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    // ---------------- パイプ状態を初期化 ----------------
    WinUsb_ResetPipe(m_winusbHandle, USB_EP_OUT);
    WinUsb_ResetPipe(m_winusbHandle, USB_EP_IN);

    printf("[INFO] WinUSB device opened successfully\n");
    m_disconnected = false;
    return true;
}


/// @brief オープン中のデバイスを閉じる
void WinUsbDevice::close()
{
    // ------------------- WinUSB ハンドルを解放 -------------------
    if (m_winusbHandle != NULL) {
        WinUsb_Free(m_winusbHandle);
        m_winusbHandle = NULL;
    }

    // ------------------- デバイスハンドルを解放 -------------------
    if (m_deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_deviceHandle);
        m_deviceHandle = INVALID_HANDLE_VALUE;
    }
}


/// @brief デバイスがオープン済みかどうかを返す
/// @return オープン済みなら true
bool WinUsbDevice::isOpen() const
{
    return m_deviceHandle != INVALID_HANDLE_VALUE && m_winusbHandle != NULL;
}


/// @brief 切断状態かどうかを返す
/// @return 切断状態なら true
bool WinUsbDevice::isDisconnected() const
{
    return m_disconnected;
}


/// @brief I/O エラー時に切断状態へ遷移させる
void WinUsbDevice::handleDeviceError()
{
    // ------------------- 切断状態を記録 -------------------
    m_disconnected = true;

    // ----------------- 保持しているハンドルを閉じる -----------------
    close();
}


/// @brief Bulk OUT 転送でデータを送信する
/// @param data 送信データ
/// @return 送信できたバイト数
int WinUsbDevice::bulkWrite(const std::vector<uint8_t>& data)
{
    // ------------------ 未オープン状態を判定 ------------------
    if (!isOpen()) return -1;

    // ----------------- 非同期送信用イベントを作成 -----------------
    OVERLAPPED ov = {};
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ov.hEvent == NULL) return -1;

    // -------------------- 非同期送信を開始 --------------------
    ULONG bytesWritten = 0;
    BOOL ok = WinUsb_WritePipe(
        m_winusbHandle,
        USB_EP_OUT,
        const_cast<uint8_t*>(data.data()),
        static_cast<ULONG>(data.size()),
        &bytesWritten,
        &ov
    );

    // ----------------- 完了待ちまたはタイムアウト処理 -----------------
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        DWORD waitResult = WaitForSingleObject(ov.hEvent, 5000);
        if (waitResult == WAIT_OBJECT_0) {
            if (!WinUsb_GetOverlappedResult(m_winusbHandle, &ov, &bytesWritten, FALSE)) {
                CloseHandle(ov.hEvent);
                handleDeviceError();
                return -1;
            }
            ok = TRUE;
        } else {
            CancelIoEx(m_deviceHandle, &ov);
            WaitForSingleObject(ov.hEvent, 100);
            fprintf(stderr, "[ERROR] WinUsb_WritePipe timed out\n");
            CloseHandle(ov.hEvent);
            return -1;
        }
    }

    // -------------------- イベントを解放 --------------------
    CloseHandle(ov.hEvent);

    // ------------------- 失敗時は切断扱い -------------------
    if (!ok) {
        fprintf(stderr, "[ERROR] WinUsb_WritePipe failed: %lu\n", GetLastError());
        handleDeviceError();
        return -1;
    }

    return static_cast<int>(bytesWritten);
}


/// @brief Bulk IN 転送でデータを受信する
/// @param maxBytes 最大受信サイズ
/// @param timeoutMs 受信タイムアウト
/// @return 受信データ
std::vector<uint8_t> WinUsbDevice::bulkRead(size_t maxBytes, uint32_t timeoutMs)
{
    // ------------------ 未オープン状態を判定 ------------------
    if (!isOpen()) return {};

    // ----------------- 非同期受信用イベントを作成 -----------------
    OVERLAPPED ov = {};
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ov.hEvent == NULL) return {};

    // -------------------- 受信バッファを確保 --------------------
    std::vector<uint8_t> buf(maxBytes);
    ULONG bytesRead = 0;
    BOOL ok = WinUsb_ReadPipe(
        m_winusbHandle,
        USB_EP_IN,
        buf.data(),
        static_cast<ULONG>(buf.size()),
        &bytesRead,
        &ov
    );

    // ----------------- 完了待ちまたはタイムアウト処理 -----------------
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        DWORD waitResult = WaitForSingleObject(ov.hEvent, timeoutMs);
        if (waitResult == WAIT_OBJECT_0) {
            if (!WinUsb_GetOverlappedResult(m_winusbHandle, &ov, &bytesRead, FALSE)) {
                CloseHandle(ov.hEvent);
                handleDeviceError();
                return {};
            }
            ok = TRUE;
        } else {
            CancelIoEx(m_deviceHandle, &ov);
            WaitForSingleObject(ov.hEvent, 100);
            CloseHandle(ov.hEvent);
            return {};  // timeout
        }
    }

    // -------------------- イベントを解放 --------------------
    CloseHandle(ov.hEvent);

    // ------------------- 失敗時は切断扱い -------------------
    if (!ok) {
        fprintf(stderr, "[ERROR] WinUsb_ReadPipe failed: %lu\n", GetLastError());
        handleDeviceError();
        return {};
    }

    // -------------------- 実受信サイズへ調整 --------------------
    buf.resize(bytesRead);
    return buf;
}
