#include "WinUsbDevice.hpp"

#include <setupapi.h>
#include <initguid.h>
#include <devpkey.h>
#include <stdio.h>
#include <string>


WinUsbDevice::WinUsbDevice()
    : m_deviceHandle(INVALID_HANDLE_VALUE),
      m_winusbHandle(NULL)
{
}

WinUsbDevice::~WinUsbDevice()
{
    close();
}


std::string WinUsbDevice::findDevicePath()
{
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

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return result;
}


bool WinUsbDevice::open()
{
    if (isOpen()) {
        fprintf(stderr, "[WARN] Device already open\n");
        return true;
    }

    std::string devicePath = findDevicePath();
    if (devicePath.empty()) {
        fprintf(stderr, "[ERROR] Device not found (VID=%04X, PID=%04X)\n",
                USB_VID, USB_PID);
        return false;
    }

    printf("[INFO] Device path: %s\n", devicePath.c_str());

    // Open device file handle (FILE_FLAG_OVERLAPPED required by WinUsb_Initialize)
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

    // Initialize WinUSB
    if (!WinUsb_Initialize(m_deviceHandle, &m_winusbHandle)) {
        fprintf(stderr, "[ERROR] WinUsb_Initialize failed: %lu\n", GetLastError());
        CloseHandle(m_deviceHandle);
        m_deviceHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    // Reset pipes to clear any stale state
    WinUsb_ResetPipe(m_winusbHandle, USB_EP_OUT);
    WinUsb_ResetPipe(m_winusbHandle, USB_EP_IN);

    printf("[INFO] WinUSB device opened successfully\n");
    return true;
}


void WinUsbDevice::close()
{
    if (m_winusbHandle != NULL) {
        WinUsb_Free(m_winusbHandle);
        m_winusbHandle = NULL;
    }
    if (m_deviceHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_deviceHandle);
        m_deviceHandle = INVALID_HANDLE_VALUE;
    }
}


bool WinUsbDevice::isOpen() const
{
    return m_deviceHandle != INVALID_HANDLE_VALUE && m_winusbHandle != NULL;
}


int WinUsbDevice::bulkWrite(const std::vector<uint8_t>& data)
{
    if (!isOpen()) return -1;

    OVERLAPPED ov = {};
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ov.hEvent == NULL) return -1;

    ULONG bytesWritten = 0;
    BOOL ok = WinUsb_WritePipe(
        m_winusbHandle,
        USB_EP_OUT,
        const_cast<uint8_t*>(data.data()),
        static_cast<ULONG>(data.size()),
        &bytesWritten,
        &ov
    );

    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        DWORD waitResult = WaitForSingleObject(ov.hEvent, 5000);
        if (waitResult == WAIT_OBJECT_0) {
            WinUsb_GetOverlappedResult(m_winusbHandle, &ov, &bytesWritten, FALSE);
            ok = TRUE;
        } else {
            WinUsb_AbortPipe(m_winusbHandle, USB_EP_OUT);
            WinUsb_ResetPipe(m_winusbHandle, USB_EP_OUT);
            fprintf(stderr, "[ERROR] WinUsb_WritePipe timed out\n");
            CloseHandle(ov.hEvent);
            return -1;
        }
    }

    CloseHandle(ov.hEvent);

    if (!ok) {
        fprintf(stderr, "[ERROR] WinUsb_WritePipe failed: %lu\n", GetLastError());
        return -1;
    }

    return static_cast<int>(bytesWritten);
}


std::vector<uint8_t> WinUsbDevice::bulkRead(size_t maxBytes, uint32_t timeoutMs)
{
    if (!isOpen()) return {};

    OVERLAPPED ov = {};
    ov.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ov.hEvent == NULL) return {};

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

    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        DWORD waitResult = WaitForSingleObject(ov.hEvent, timeoutMs);
        if (waitResult == WAIT_OBJECT_0) {
            WinUsb_GetOverlappedResult(m_winusbHandle, &ov, &bytesRead, FALSE);
            ok = TRUE;
        } else {
            WinUsb_AbortPipe(m_winusbHandle, USB_EP_IN);
            WinUsb_ResetPipe(m_winusbHandle, USB_EP_IN);
            CloseHandle(ov.hEvent);
            return {};  // timeout
        }
    }

    CloseHandle(ov.hEvent);

    if (!ok) {
        fprintf(stderr, "[ERROR] WinUsb_ReadPipe failed: %lu\n", GetLastError());
        return {};
    }

    buf.resize(bytesRead);
    return buf;
}
