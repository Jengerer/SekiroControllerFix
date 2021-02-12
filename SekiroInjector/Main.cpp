#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <cstring>

using namespace std;
const char* TargetLibrary = "SekiroControllerFix.dll";
const size_t TargetLibraryLength = strlen(TargetLibrary) + 1;
const char* TargetProcess = "sekiro.exe";

class InjectorHelper
{
public:
    InjectorHelper() :
        mSnapshot(INVALID_HANDLE_VALUE),
        mProcess(INVALID_HANDLE_VALUE),
        mTargetLibraryRemote(nullptr),
        mRemoteThread(INVALID_HANDLE_VALUE),
        mStep(0)
    {
    }

    ~InjectorHelper()
    {
        if(mRemoteThread != INVALID_HANDLE_VALUE)
        {
            CloseHandle(mRemoteThread);
        }
        if(mTargetLibraryRemote)
        {
            VirtualFreeEx(mProcess, mTargetLibraryRemote, TargetLibraryLength, MEM_RELEASE);
        }
        if(mProcess != INVALID_HANDLE_VALUE)
        {
            CloseHandle(mProcess);
        }
        if(mSnapshot != INVALID_HANDLE_VALUE)
        {
            CloseHandle(mSnapshot);
        }
    }

    bool ConnectToSekiro()
    {
        bool found = false;
        PROCESSENTRY32 entry = {};
        entry.dwSize = sizeof(entry);
        cout << ++mStep << ". Waiting for Sekiro process to start... ";
        while(!found)
        {
            if(mSnapshot)
            {
                CloseHandle(mSnapshot);
            }
            mSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if(mSnapshot == INVALID_HANDLE_VALUE)
            {
                cout << "FAILED!\nCould not create process snapshot, error code: " << GetLastError() << "\n";
                return(false);
            }
            if(!Process32First(mSnapshot, &entry))
            {
                cout << "FAILED!\nCould not get first process entry, error code: " << GetLastError() << "\n";
                return(false);
            }
            do 
            {
                if(!strcmp(entry.szExeFile, TargetProcess))
                {
                    found = true;
                    break;
                }
            } while(Process32Next(mSnapshot, &entry));
        }
        cout << "OK!\n";
        cout << ++mStep << ". Attempting to open connection to process... ";
        mProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
        if(!mProcess)
        {
            cout << "FAILED!\nCould not connect to Sekiro, error code: " << GetLastError() << "\n";
            return(false);
        }
        cout << "OK!\n";
        return(true);
    }

    bool Inject()
    {
        char targetLibraryPath[MAX_PATH] = {};
        const DWORD currentDirectoryLength = GetCurrentDirectory(MAX_PATH, targetLibraryPath);
        const DWORD remainingLength = MAX_PATH - currentDirectoryLength;
        sprintf_s(targetLibraryPath + currentDirectoryLength, remainingLength, "\\%s", TargetLibrary);
        const size_t TotalLength = currentDirectoryLength + TargetLibraryLength;
        cout << ++mStep << ". Checking that " << targetLibraryPath << " exists... ";
        const HANDLE targetLibraryHandle = CreateFile(targetLibraryPath, FILE_READ_ONLY, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if(targetLibraryHandle == INVALID_HANDLE_VALUE)
        {
            cout << "FAILED!\nCould not open target library path, error code: " << GetLastError() << "\n";
            return(false);
        }
        CloseHandle(targetLibraryHandle);
        cout << "OK!\n";

        // Write path name
        cout << ++mStep << ". Attempting to inject DLL into process... ";
        mTargetLibraryRemote = VirtualAllocEx(mProcess, nullptr, TotalLength, MEM_COMMIT, PAGE_READWRITE);
        if(!mTargetLibraryRemote)
        {
            cout << "FAILED!\nCould not allocate remote memory for target library, error code: " << GetLastError() << "\n";
            return(false);
        }
        const BOOL writeSuccess = WriteProcessMemory(mProcess, mTargetLibraryRemote, targetLibraryPath, TotalLength, nullptr);
        if(!writeSuccess)
        {
            cout << "FAILED!\nCould not write remote memory for target library, error code: " << GetLastError() << "\n";
            return(false);
        }
        const HMODULE kernel32 = GetModuleHandle("kernel32.dll");
        if(!kernel32)
        {
            cout << "FAILED!\nCould not retrieve Kernel32 module handle, error code: " << GetLastError() << "\n";
            return(false);
        }
        const LPVOID loadLibraryRemote = static_cast<LPVOID>(GetProcAddress(kernel32, "LoadLibraryA"));
        if(!loadLibraryRemote)
        {
            cout << "FAILED!\nCould not get handle to LoadLibrary in remote process, error code: " << GetLastError() << "\n";
            return(false);
        }
        mRemoteThread = CreateRemoteThread(mProcess, nullptr, 0, static_cast<LPTHREAD_START_ROUTINE>(loadLibraryRemote), mTargetLibraryRemote, 0, nullptr);
        if(mRemoteThread == INVALID_HANDLE_VALUE)
        {
            cout << "FAILED!\nCould not create remote thread, error code: " << GetLastError() << "\n";
            return(false);
        }
        cout << "OK!\n";
        return(true);
    }

private:
    HANDLE mSnapshot;
    HANDLE mProcess;
    LPVOID mTargetLibraryRemote;
    HANDLE mRemoteThread;
    size_t mStep;
};

int main(int argc, char** argv)
{
    InjectorHelper injector;
    if(injector.ConnectToSekiro() && injector.Inject())
    {
        cout << "5. Injection successful!\n";
        return(0);
    }
    system("pause");
    return(-1);
}

