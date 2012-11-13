#ifndef LIBDENG_DIRECTINPUT_H
#define LIBDENG_DIRECTINPUT_H

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <dinput.h>
#include <assert.h>

#define I_SAFE_RELEASE(d) { if(d) { (d)->Release(); (d) = NULL; } }

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Attempt to initialize an application-global interface to DirectInput. First
 * the version 8 interface (if available on the host system) and if unsuccessful,
 * then the older version 3 interface.
 *
 * @note The caller must ensure that COM has been initialized else this will
 *       fail and false will be returned.
 *
 * @return  @c true if an interface was initialized successfully.
 */
int DirectInput_Init(void);

/**
 * Shutdown the open DirectInput interface if initialized.
 */
void DirectInput_Shutdown(void);

/**
 * Retrieve a handle to the version 8 interface.
 * @return  Interface instance handle or @c NULL if not initialized.
 */
LPDIRECTINPUT8 DirectInput_IVersion8();

/**
 * Retrieve a handle to the version 3 interface.
 * @return  Interface instance handle or @c NULL if not initialized.
 */
LPDIRECTINPUT DirectInput_IVersion3();

/**
 * Releases and then destroys a DirectInput device.
 * @param dev  Address of the device instance to be destroyed. Can be @c NULL.
 */
void DirectInput_KillDevice(LPDIRECTINPUTDEVICE8* dev);

/**
 * Retrieve a plain text explanation of a DirectInput error code suitable for
 * printing to the error log/displaying to the user.
 *
 * @param hr  DirectInput Error code to be translated.
 * @return  Plain text explanation. Always returns a valid cstring.
 */
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
