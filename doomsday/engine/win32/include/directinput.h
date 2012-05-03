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

void DirectInput_KillDevice(LPDIRECTINPUTDEVICE8* dev);

const char* DirectInput_ErrorMsg(HRESULT hr);

#ifdef __cplusplus
} // extern "C"
#endif

#ifdef __cplusplus
/**
 * A handy adaptor for manipulating a DIPROPDWORD structure.
 */
struct DIPropDWord : DIPROPDWORD
{
    DIPropDWord(DWORD how=0, DWORD object=0, DWORD data=0)
    {
        initHeader();
        setHow(how);
        setObject(object);
        setData(data);
    }

    operator DIPROPHEADER*() { return &diph; }
    operator const DIPROPHEADER*() const { return &diph; }

    inline DIPropDWord& setHow(DWORD how)    { diph.dwHow = how;  return *this; }
    inline DIPropDWord& setObject(DWORD obj) { diph.dwObj = obj;  return *this; }
    inline DIPropDWord& setData(DWORD data)  {     dwData = data; return *this; }

private:
    void initHeader()
    {
        diph.dwSize = sizeof(DIPROPDWORD);
        diph.dwHeaderSize = sizeof(diph);
    }
};

/**
 * A handy adaptor for manipulating a DIPROPRANGE structure.
 */
struct DIPropRange : DIPROPRANGE
{
    DIPropRange(DWORD how=0, DWORD object=0, int min=0, int max=0)
    {
        initHeader();
        setHow(how);
        setObject(object);
        setMin(min);
        setMax(max);
    }

    operator DIPROPHEADER*() { return &diph; }
    operator const DIPROPHEADER*() const { return &diph; }

    inline DIPropRange& setHow(DWORD how)    { diph.dwHow = how; return *this; }
    inline DIPropRange& setObject(DWORD obj) { diph.dwObj = obj; return *this; }
    inline DIPropRange& setMin(int min)      {       lMin = min; return *this; }
    inline DIPropRange& setMax(int max)      {       lMax = max; return *this; }

private:
    void initHeader()
    {
        diph.dwSize = sizeof(DIPROPRANGE);
        diph.dwHeaderSize = sizeof(diph);
    }
};
#endif // __cplusplus

#endif // LIBDENG_DIRECTINPUT_H
