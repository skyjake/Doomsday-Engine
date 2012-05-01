#ifndef LIBDENG_DIRECTINPUT_H
#define LIBDENG_DIRECTINPUT_H

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <dinput.h>
#include <assert.h>

#define I_SAFE_RELEASE(d) { if(d) { IDirectInputDevice_Release(d); (d) = NULL; } }
#define I_SAFE_RELEASE2(d) { if(d) { IDirectInputDevice2_Release(d); (d) = NULL; } }

#ifdef __cplusplus
extern "C" {
#endif

int DirectInput_Init(void);

void DirectInput_Shutdown(void);

LPDIRECTINPUT8 DirectInput_Instance();

void DirectInput_KillDevice(LPDIRECTINPUTDEVICE8 *dev);

HRESULT DirectInput_SetProperty(LPDIRECTINPUTDEVICE8 dev, REFGUID property, DWORD how, DWORD obj, DWORD data);

HRESULT DirectInput_SetRangeProperty(LPDIRECTINPUTDEVICE8 dev, REFGUID property, DWORD how, DWORD obj, int min, int max);

const char* DirectInput_ErrorMsg(HRESULT hr);

#ifdef __cplusplus
}
#endif

#endif // LIBDENG_DIRECTINPUT_H
