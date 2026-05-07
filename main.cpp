#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <cstring>

DWORD GetProcessIdByName(const wchar_t* processName)
{
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;

    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, processName) == 0)
            {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return 0;
}

bool IsModuleLoaded(DWORD processId, const wchar_t* moduleName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(
        TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,
        processId
    );

    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    MODULEENTRY32W moduleEntry{};
    moduleEntry.dwSize = sizeof(moduleEntry);

    if (Module32FirstW(snapshot, &moduleEntry))
    {
        do
        {
            if (_wcsicmp(moduleEntry.szModule, moduleName) == 0)
            {
                CloseHandle(snapshot);
                return true;
            }

        } while (Module32NextW(snapshot, &moduleEntry));
    }

    CloseHandle(snapshot);

    return false;
}

constexpr const wchar_t* processName = L"isaac-ng.exe";
constexpr const wchar_t* DLL_NAME = L"IsaacEntityHook.dll";
constexpr const char* DLL_PATH ="Y:\\VSProjects\\IsaacEntityHook\\Debug\\IsaacEntityHook.dll";

int main()
{
    std::cout << "DLL Injector start\n";

    DWORD pid = GetProcessIdByName(processName);

    if (!pid)
    {
        std::cout << "Nie znaleziono procesu\n";
        MessageBoxA(nullptr, "Process not found", "Injector", MB_OK);
        return 0;
    }

    if (IsModuleLoaded(pid, DLL_NAME))
    {
        std::cout << "DLL already loaded\n";
        MessageBoxA(nullptr, "DLL already loaded", "Injector", MB_OK);
        return 0;
    }

    std::cout << "PID: " << pid << "\n";

    // 🔥 otwieramy proces
    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD |
        PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION |
        PROCESS_VM_WRITE |
        PROCESS_VM_READ,
        FALSE,
        pid
    );

    if (!hProcess)
    {
        MessageBoxA(
            nullptr,
            "OpenProcess failed",
            "Injector",
            MB_OK | MB_ICONERROR
        );

        return 0;
    }

    std::cout << "Process opened\n";

    // 🔥 ścieżka do DLL
    const char* dllPath = DLL_PATH;

    size_t len = strlen(dllPath) + 1;

    // 🔥 alokacja pamięci w procesie
    LPVOID remoteMem = VirtualAllocEx(
        hProcess,
        nullptr,
        len,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (!remoteMem)
    {
        MessageBoxA(
            nullptr,
            "VirtualAllocEx failed",
            "Injector",
            MB_OK | MB_ICONERROR
        );

        CloseHandle(hProcess);

        return 0;
    }

    std::cout << "Allocated at: " << remoteMem << "\n";

    // 🔥 zapis ścieżki DLL
    WriteProcessMemory(
        hProcess,
        remoteMem,
        dllPath,
        len,
        nullptr
    );

    std::cout << "DLL path written\n";

    // 🔥 LoadLibraryA
    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    FARPROC loadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");

    if (!hKernel32 || !loadLibrary)
    {
        MessageBoxA(
            nullptr,
            "Failed to resolve LoadLibraryA",
            "Injector",
            MB_OK | MB_ICONERROR
        );

        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
        CloseHandle(hProcess);

        return 0;
    }

    std::cout << "LoadLibraryA: " << loadLibrary << "\n";

    // 🔥 create remote thread
    HANDLE hThread = CreateRemoteThread(
        hProcess,
        nullptr,
        0,
        (LPTHREAD_START_ROUTINE)loadLibrary,
        remoteMem,
        0,
        nullptr
    );

    if (!hThread)
    {
        MessageBoxA(
            nullptr,
            "CreateRemoteThread failed",
            "Injector",
            MB_OK | MB_ICONERROR
        );

        VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);

        CloseHandle(hProcess);

        return 0;
    }

    std::cout << "DLL injected successfully\n";
    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);

    CloseHandle(hThread);
    CloseHandle(hProcess);

    return 0;
}