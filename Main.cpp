#include <Windows.h>
#include <Xinput.h>
#include <cstring>
#include <iostream>
#include <detours.h>
#include <dinput.h>
#include <vector>

// Treat the first controller that records a button press as primary
constexpr DWORD InvalidControllerIndex = 0xFFFFFFFFU;
static DWORD gTargetController = InvalidControllerIndex;
typedef DWORD (WINAPI* XInputGetStateFunction)(DWORD, XINPUT_STATE*);
static XInputGetStateFunction XInputGetStateTrue = nullptr;
DWORD WINAPI XInputGetStateCustom(DWORD userIndex, XINPUT_STATE* state)
{
    // We want the active controller to always be on zero
    if(userIndex != 0)
    {
        return(ERROR_DEVICE_NOT_CONNECTED);
    }

    if(gTargetController == InvalidControllerIndex)
    {
        // Try to find an active controller that pressed a button
        for(DWORD user = 0; user < XUSER_MAX_COUNT; ++user)
        {
            const DWORD result = XInputGetStateTrue(user, state);
            if((result == ERROR_SUCCESS) && state->Gamepad.wButtons)
            {
                gTargetController = user;
                return(result);
            }
        }
    }
    else
    {
        const DWORD result = XInputGetStateTrue(gTargetController, state);
        if(result != ERROR_SUCCESS)
        {
            gTargetController = InvalidControllerIndex;
        }
        else if(state->Gamepad.wButtons)
        {
        }
        return(result);
    }

    // Connected... just inactive.
    ZeroMemory(state, sizeof(XINPUT_STATE));
    return(ERROR_SUCCESS);
}

LPDIENUMDEVICESCALLBACKW OriginalCallback;
const wchar_t* TargetControllerName = L"360";
BOOL DIEnumDevicesCallback(LPCDIDEVICEINSTANCEW device, LPVOID reference)
{
    // Don't fire the original callback unless this device is reporting as X360 controller
    if(!wcsstr(device->tszProductName, TargetControllerName))
    {
        return(DIENUM_CONTINUE);
    }
    return(OriginalCallback(device, reference));
}

intptr_t EnumDevicesTrue;
intptr_t EnumDevicesCustom;
intptr_t* EnumDevicesLocation;
struct DirectInput8Custom : public IDirectInput8W
{
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) { return(0); }
    STDMETHOD_(ULONG,AddRef)(THIS) { return(0); }
    STDMETHOD_(ULONG,Release)(THIS) { return(0); }
    STDMETHOD(CreateDevice)(THIS_ REFGUID,LPDIRECTINPUTDEVICE8W *,LPUNKNOWN) { return(0); }
    static constexpr size_t EnumDevicesIndex = 4;
    STDMETHOD(EnumDevices)(THIS_ DWORD deviceType, LPDIENUMDEVICESCALLBACKW callback, LPVOID reference, DWORD flags)
    {
        OriginalCallback = callback;
        *EnumDevicesLocation = EnumDevicesTrue;
        const DWORD result = EnumDevices(deviceType, &DIEnumDevicesCallback, reference, flags);
        *EnumDevicesLocation = EnumDevicesCustom;
        return(result);
    }
    STDMETHOD(GetDeviceStatus)(THIS_ REFGUID) { return(0); }
    STDMETHOD(RunControlPanel)(THIS_ HWND,DWORD) { return(0); }
    STDMETHOD(Initialize)(THIS_ HINSTANCE,DWORD) { return(0); }
    STDMETHOD(FindDevice)(THIS_ REFGUID,LPCWSTR,LPGUID) { return(0); }
    STDMETHOD(EnumDevicesBySemantics)(THIS_ LPCWSTR,LPDIACTIONFORMATW,LPDIENUMDEVICESBYSEMANTICSCBW,LPVOID,DWORD) { return(0); }
    STDMETHOD(ConfigureDevices)(THIS_ LPDICONFIGUREDEVICESCALLBACK,LPDICONFIGUREDEVICESPARAMSW,DWORD,LPVOID) { return(0); }
};

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if(DetourIsHelperProcess())
    {
        return(TRUE);
    }

    switch(reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            // DirectInput hooking, make custom one writable so we can change to original
            DirectInput8Custom directInputCustom;
            intptr_t* customTable = *reinterpret_cast<intptr_t**>(&directInputCustom);
            EnumDevicesCustom = customTable[DirectInput8Custom::EnumDevicesIndex];

            // Now make the real one writable and hack in the custom version
            IDirectInput8W* directInput = nullptr;
            const HINSTANCE rootInstance = GetModuleHandle(nullptr);
            if(DirectInput8Create(rootInstance, DIRECTINPUT_VERSION, IID_IDirectInput8W, reinterpret_cast<LPVOID*>(&directInput), nullptr) != DI_OK)
            {
                return(FALSE);
            }
            intptr_t** directInputTablePointer = reinterpret_cast<intptr_t**>(directInput);
            intptr_t* directInputTable = *directInputTablePointer;
            MEMORY_BASIC_INFORMATION memoryInfo = {};
            VirtualQuery(reinterpret_cast<LPCVOID>(directInputTable), &memoryInfo, sizeof(memoryInfo));
            VirtualProtect(memoryInfo.BaseAddress, memoryInfo.RegionSize, PAGE_EXECUTE_READWRITE, &memoryInfo.Protect);
            EnumDevicesLocation= &directInputTable[DirectInput8Custom::EnumDevicesIndex];
            EnumDevicesTrue = *EnumDevicesLocation;
            *EnumDevicesLocation = EnumDevicesCustom;
            directInput->Release();
            directInput = nullptr;

            // Get the true version
            const HMODULE input = LoadLibrary("xinput1_3.dll");
            if(input)
            {
                XInputGetStateTrue = reinterpret_cast<XInputGetStateFunction>(GetProcAddress(input, "XInputGetState"));
                if(XInputGetStateTrue)
                {
                    DetourRestoreAfterWith();
                    DetourTransactionBegin();
                    DetourUpdateThread(GetCurrentThread());
                    DetourAttach(&reinterpret_cast<PVOID&>(XInputGetStateTrue), &XInputGetStateCustom);
                    DetourTransactionCommit();
                }
                else
                {
                    MessageBox(nullptr, "Failed to inject!", "Failed", MB_OK);
                    return(FALSE);
                }
            }
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
        {
            break;
        }
    }
    return(TRUE);
}
