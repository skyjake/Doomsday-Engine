/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2008 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * driver_didinput8.cpp: Doomsday user input plugin. Uses DirectInput8.
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800

#include <windows.h>
#include <dinput.h>
#include <assert.h>
#include <strsafe.h>

#include "doomsday.h"
#include "sys_inputd.h"

// MACROS ------------------------------------------------------------------

#define NUMKKEYS            256
#define KEYBUFSIZE          32

#define I_SAFE_RELEASE(d) { if(d) { (d)->Release(); (d) = NULL; } }

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

extern "C" {
int             DI_Init(void);
void            DI_Shutdown(void);
void            DI_Event(int type);
boolean         DI_MousePresent(void);
boolean         DI_JoystickPresent(void);
size_t          DI_GetKeyEvents(keyevent_t *evbuf, size_t bufsize);
void            DI_GetMouseState(mousestate_t *state);
void            DI_GetJoystickState(joystate_t *state);
}

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     joydevice = 0;          // Joystick index to use.
byte    usejoystick = false;    // Joystick input enabled?

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initIOk = false;
static int verbose;

static HRESULT hr;
static LPDIRECTINPUT8 dInput = NULL;
static LPDIRECTINPUTDEVICE8 didKeyb = NULL, didMouse = NULL;
static LPDIRECTINPUTDEVICE8 didJoy = NULL;
static DIDEVICEINSTANCE firstJoystick;

static int counter;

// A customizable mapping of the scantokey array.
//static char keyMapPath[NUMKKEYS] = "}Data\\KeyMaps\\";
static byte *keymap;

// CODE --------------------------------------------------------------------

static void error(const char *where, const char *msg)
{
    Con_Message("%s(DInput8): %s [Result = 0x%x]\n", where, msg, hr);
}

const char *I_ErrorMsg(HRESULT hr)
{
    return hr == DI_OK ? "OK" : hr == DIERR_GENERIC ? "Generic error" : hr ==
        DI_PROPNOEFFECT ? "Property has no effect" : hr ==
        DIERR_INVALIDPARAM ? "Invalid parameter" : hr ==
        DIERR_NOTINITIALIZED ? "Not initialized" : hr ==
        DIERR_UNSUPPORTED ? "Unsupported" : hr ==
        DIERR_NOTFOUND ? "Not found" : "?";
}

static void initDIKeyToDDKeyTlat(void)
{
    if(keymap)
        return; // Already been here.

    keymap = new byte[NUMKKEYS];

    keymap[DIK_0] = '0';
    keymap[DIK_1] = '1';
    keymap[DIK_2] = '2';
    keymap[DIK_3] = '3';
    keymap[DIK_4] = '4';
    keymap[DIK_5] = '5';
    keymap[DIK_6] = '6';
    keymap[DIK_7] = '7';
    keymap[DIK_8] = '8';
    keymap[DIK_9] = '9';
    keymap[DIK_A] = 'a';
    //keymap[DIK_ABNT_C1] = ;
    //keymap[DIK_ABNT_C2] = ;
    keymap[DIK_ADD] = DDKEY_ADD;
    keymap[DIK_APOSTROPHE] = '\'';
    //keymap[DIK_APPS] = ;
    //keymap[DIK_AT] = ;
    //keymap[DIK_AX] = ;
    keymap[DIK_B] = 'b';
    keymap[DIK_BACK] = DDKEY_BACKSPACE;
    keymap[DIK_BACKSLASH] = DDKEY_BACKSLASH;
    keymap[DIK_C] = 'c';
    //keymap[DIK_CALCULATOR] = ;
    //keymap[DIK_CAPITAL] = ;
    //keymap[DIK_COLON] = ; // On Japanese keyboard
    keymap[DIK_COMMA] = ',';
    //keymap[DIK_CONVERT] = ;
    keymap[DIK_D] = 'd';
    keymap[DIK_DECIMAL] = DDKEY_DECIMAL;
    keymap[DIK_DELETE] = DDKEY_DEL;
    keymap[DIK_DIVIDE] = '/';
    keymap[DIK_DOWN] = DDKEY_DOWNARROW;
    keymap[DIK_E] = 'e';
    keymap[DIK_END] = DDKEY_END;
    keymap[DIK_EQUALS] = DDKEY_EQUALS;
    keymap[DIK_ESCAPE] = DDKEY_ESCAPE;
    keymap[DIK_F] = 'f';
    keymap[DIK_F1] = DDKEY_F1;
    keymap[DIK_F2] = DDKEY_F2;
    keymap[DIK_F3] = DDKEY_F3;
    keymap[DIK_F4] = DDKEY_F4;
    keymap[DIK_F5] = DDKEY_F5;
    keymap[DIK_F6] = DDKEY_F6;
    keymap[DIK_F7] = DDKEY_F7;
    keymap[DIK_F8] = DDKEY_F8;
    keymap[DIK_F9] = DDKEY_F9;
    keymap[DIK_F10] = DDKEY_F10;
    keymap[DIK_F11] = DDKEY_F11;
    keymap[DIK_F12] = DDKEY_F12;
    //keymap[DIK_F13] = ;
    //keymap[DIK_F14] = ;
    //keymap[DIK_F15] = ;
    keymap[DIK_G] = 'g';
    keymap[DIK_GRAVE] = '`';
    keymap[DIK_H] = 'h';
    keymap[DIK_HOME] = DDKEY_HOME;
    keymap[DIK_I] = 'i';
    keymap[DIK_INSERT] = DDKEY_INS;
    keymap[DIK_J] = 'j';
    keymap[DIK_K] = 'k';
    //keymap[DIK_KANA] = ;
    //keymap[DIK_KANJI] = ;
    keymap[DIK_L] = 'l';
    keymap[DIK_LBRACKET] = '[';
    keymap[DIK_LCONTROL] = ']';
    keymap[DIK_LEFT] = DDKEY_LEFTARROW;
    keymap[DIK_LMENU] = DDKEY_LALT; // Left Alt
    keymap[DIK_LSHIFT] = DDKEY_LSHIFT;
    //keymap[DIK_LWIN] = ; // Left Windows logo key
    keymap[DIK_M] = 'm';
    //keymap[DIK_MAIL] = ;
    //keymap[DIK_MEDIASELECT] = ; Media Select key, which displays a selection of supported media players on the system
    //keymap[DIK_MEDIASTOP] = ;
    keymap[DIK_MINUS] = '-'; // On main keyboard.
    keymap[DIK_MULTIPLY] = '*'; // Asterisk (*) on numeric keypad
    //keymap[DIK_MUTE] = ;
    //keymap[DIK_MYCOMPUTER] = ;
    keymap[DIK_N] = 'n';
    keymap[DIK_NEXT] = DDKEY_PGDN; // Page down
    //keymap[DIK_NEXTTRACK] = ;
    //keymap[DIK_NOCONVERT] = ; // On Japanese keyboard
    keymap[DIK_NUMLOCK] = DDKEY_NUMLOCK;
    keymap[DIK_NUMPAD0] = DDKEY_NUMPAD0;
    keymap[DIK_NUMPAD1] = DDKEY_NUMPAD1;
    keymap[DIK_NUMPAD2] = DDKEY_NUMPAD2;
    keymap[DIK_NUMPAD3] = DDKEY_NUMPAD3;
    keymap[DIK_NUMPAD4] = DDKEY_NUMPAD4;
    keymap[DIK_NUMPAD5] = DDKEY_NUMPAD5;
    keymap[DIK_NUMPAD6] = DDKEY_NUMPAD6;
    keymap[DIK_NUMPAD7] = DDKEY_NUMPAD7;
    keymap[DIK_NUMPAD8] = DDKEY_NUMPAD8;
    keymap[DIK_NUMPAD9] = DDKEY_NUMPAD9;
    //keymap[DIK_NUMPADCOMMA] = ; // On numeric keypad of NEC PC-98 Japanese keyboard
    //keymap[DIK_NUMPADENTER] = ;
    //keymap[DIK_NUMPADEQUALS] = ; // On numeric keypad of NEC PC-98 Japanese keyboard
    keymap[DIK_O] = 'o';
    //keymap[DIK_OEM_102] = ; // On British and German keyboards
    keymap[DIK_P] = 'p';
    keymap[DIK_PAUSE] = DDKEY_PAUSE;
    keymap[DIK_PERIOD] = '.';
    //keymap[DIK_PLAYPAUSE] = ;
    //keymap[DIK_POWER] = ;
    //keymap[DIK_PREVTRACK] = ; // Previous track; circumflex on Japanese keyboard
    keymap[DIK_PRIOR] = DDKEY_PGUP; // Page up
    keymap[DIK_Q] = 'q';
    keymap[DIK_R] = 'r';
    keymap[DIK_RBRACKET] = ']';
    keymap[DIK_RCONTROL] = DDKEY_RCTRL;
    keymap[DIK_RETURN] = DDKEY_RETURN; // Return on main keyboard
    keymap[DIK_RIGHT] = DDKEY_RIGHTARROW;
    keymap[DIK_RMENU] = DDKEY_RALT; // Right alt
    keymap[DIK_RSHIFT] = DDKEY_RSHIFT;
    //keymap[DIK_RWIN] = ; // Right Windows logo key
    keymap[DIK_S] = 's';
    keymap[DIK_SCROLL] = DDKEY_SCROLL;
    keymap[DIK_SEMICOLON] = ';';
    keymap[DIK_SLASH] = '/';
    //keymap[DIK_SLEEP] = ;
    keymap[DIK_SPACE] = ' ';
    //keymap[DIK_STOP] = ; // On NEC PC-98 Japanese keyboard
    keymap[DIK_SUBTRACT] = DDKEY_SUBTRACT; // On numeric keypad
    //keymap[DIK_SYSRQ] = ;
    keymap[DIK_T] = 't';
    keymap[DIK_TAB] = DDKEY_TAB;
    keymap[DIK_U] = 'u';
    //keymap[DIK_UNDERLINE] = ; // On NEC PC-98 Japanese keyboard
    //keymap[DIK_UNLABELED] = ; // On Japanese keyboard
    keymap[DIK_UP] = DDKEY_UPARROW;
    keymap[DIK_V] = 'v';
    //keymap[DIK_VOLUMEDOWN] = ;
    //keymap[DIK_VOLUMEUP] = ;
    keymap[DIK_W] = 'w';
    //keymap[DIK_WAKE] = ;
    //keymap[DIK_WEBBACK] = ;
    //keymap[DIK_WEBFAVORITES] = ;
    //keymap[DIK_WEBFORWARD] = ;
    //keymap[DIK_WEBHOME] = ;
    //keymap[DIK_WEBREFRESH] = ;
    //keymap[DIK_WEBSEARCH] = ;
    //keymap[DIK_WEBSTOP] = ;
    keymap[DIK_X] = 'x';
    keymap[DIK_Y] = 'y';
    // keymap[DIK_YEN] = ;
    keymap[DIK_Z] = 'z';
}

/**
 * Convert a DInput Key (DIK_*) to a DDkey (DDKEY_*) constant.
 */
static byte dIKeyToDDKey(DWORD dIKey)
{
    return keymap[dIKey];
}

#if 0 // Currently unused.
static DFILE *openKeymapFile(const char *fileName)
{
    char        path[512];

    // Try with and without .DKM.
    strcpy(path, fileName);
    if(!F_Access(path))
    {
        // Try the path.
        M_TranslatePath(keyMapPath, path);
        strcat(path, fileName);
        if(!F_Access(path))
        {
            strcpy(path, fileName);
            strcat(path, ".dkm");
            if(!F_Access(path))
            {
                M_TranslatePath(keyMapPath, path);
                strcat(path, fileName);
                strcat(path, ".dkm");
            }
        }
    }

    return F_Open(path, "rt");
}

static boolean closeKeymapFile(DFILE *file)
{
    if(file)
        F_Close(file);

    return true;
}

static int parseKeymapFile(DFILE *file, const char *fileName)
{
    int         warnCount = 0;
    char        buf[512], *ptr;
    boolean     shiftMode = false, altMode = false;
    int         key, mapTo, lineNumber = 0;

    VERBOSE(Con_Message("parseKeymapFile: Parsing \"%s\"...\n", fileName));

    do
    {
        lineNumber++;
        M_ReadLine(buf, sizeof(buf), file);
        ptr = M_SkipWhite(buf);
        if(!*ptr || M_IsComment(ptr))
            continue;

        // Modifiers?
        if(!strnicmp(ptr + 1, "shift", 5))
        {
            shiftMode = (*ptr == '+');
            continue;
        }
        else if(!strnicmp(ptr + 1, "alt", 3))
        {
            altMode = (*ptr == '+');
            continue;
        }

        key = DD_KeyOrCode(ptr);
        if(key < 0 || key > 255)
        {
            Con_Message("  #%i: Invalid key %i.\n", lineNumber, key);
            warnCount++;
        }

        ptr = M_SkipWhite(M_FindWhite(ptr));
        mapTo = DD_KeyOrCode(ptr);
        // Check the mapping.
        if(mapTo < 0 || mapTo > 255)
        {
            Con_Message("  #%i: Invalid mapping %i.\n", lineNumber, mapTo);
            warnCount++;
        }

        if(shiftMode)
            shiftKeyMappings[key] = mapTo;
        else if(altMode)
            altKeyMappings[key] = mapTo;
        else
            keyMappings[key] = mapTo;
    } while(!deof(file));

    return warnCount;
}

boolean DD_LoadKeymap(const char *fileName)
{
    DFILE      *file;
    int         warnCount;

    if(!(file = openKeymapFile(fileName)))
    {
        Con_Message("DD_LoadKeymap: A keymap file by the name \"%s\" could "
                    "not be found.\n", fileName);
        return false;
    }

    // Any missing entries are set to the default.
    DD_DefaultKeyMapping();

    Con_Message("DD_LoadKeymap: Loading \"%s\"...\n", fileName);

    warnCount = parseKeymapFile(file, fileName);
    if(warnCount > 0)
        Con_Message("  %i: Warnings.\n", warnCount);
    closeKeymapFile(file);

    Con_Message("  Loaded keymap \"%s\" successfully.\n", fileName);

    return true;
}

/**
 * Dumps the key mapping table to filename.
 */
void DD_DumpKeymap(const char *fileName)
{
    int             i;
    FILE           *file;

    file = fopen(fileName, "wt");
    for(i = 0; i < 256; ++i)
    {
        fprintf(file, "%03i\t", i);
        fprintf(file, !isspace(keyMappings[i]) &&
                isprint(keyMappings[i]) ? "%c\n" : "%03i\n", keyMappings[i]);
    }

    fprintf(file, "\n+Shift\n");
    for(i = 0; i < 256; ++i)
    {
        if(shiftKeyMappings[i] == i)
            continue;
        fprintf(file, !isspace(i) && isprint(i) ? "%c\t" : "%03i\t", i);
        fprintf(file, !isspace(shiftKeyMappings[i]) &&
                isprint(shiftKeyMappings[i]) ? "%c\n" : "%03i\n",
                shiftKeyMappings[i]);
    }

    fprintf(file, "-Shift\n\n+Alt\n");
    for(i = 0; i < 256; ++i)
    {
        if(altKeyMappings[i] == i)
            continue;
        fprintf(file, !isspace(i) && isprint(i) ? "%c\t" : "%03i\t", i);
        fprintf(file, !isspace(altKeyMappings[i]) &&
                isprint(altKeyMappings[i]) ? "%c\n" : "%03i\n",
                altKeyMappings[i]);
    }
    fclose(file);

    Con_Message("DD_DumpKeymap: Current keymap was dumped to \"%s\".\n",
                fileName);
}
#endif

HRESULT I_SetProperty(void *dev, REFGUID property, DWORD how, DWORD obj,
                      DWORD data)
{
    DIPROPDWORD dipdw;

    dipdw.diph.dwSize = sizeof(dipdw);
    dipdw.diph.dwHeaderSize = sizeof(dipdw.diph);
    dipdw.diph.dwObj = obj;
    dipdw.diph.dwHow = how;
    dipdw.dwData = data;
    return IDirectInputDevice8_SetProperty((LPDIRECTINPUTDEVICE8) dev,
                                           property, &dipdw.diph);
}

HRESULT I_SetRangeProperty(void *dev, REFGUID property, DWORD how, DWORD obj,
                           int min, int max)
{
    DIPROPRANGE dipr;

    dipr.diph.dwSize = sizeof(dipr);
    dipr.diph.dwHeaderSize = sizeof(dipr.diph);
    dipr.diph.dwObj = obj;
    dipr.diph.dwHow = how;
    dipr.lMin = min;
    dipr.lMax = max;
    return IDirectInputDevice8_SetProperty((LPDIRECTINPUTDEVICE8) dev,
                                           property, &dipr.diph);
}

static boolean initMouse(HWND hWnd)
{
    HRESULT         hr;

    if(ArgCheck("-nomouse"))
        return false;

    hr = dInput->CreateDevice(GUID_SysMouse, &didMouse, NULL);
    if(FAILED(hr))
    {
        Con_Message("I_InitMouse: Failed to create device (0x%x).\n", hr);
        return false;
    }

    // Set data format.
    hr = didMouse->SetDataFormat(&c_dfDIMouse2);
    if(FAILED(hr))
    {
        Con_Message("I_InitMouse: Failed to set data format (0x%x).\n", hr);
        goto kill_mouse;
    }

    // Set behaviour.
    hr = didMouse->SetCooperativeLevel(hWnd, DISCL_FOREGROUND |
                                             DISCL_EXCLUSIVE);
    if(FAILED(hr))
    {
        Con_Message("I_InitMouse: Failed to set co-op level (0x%x).\n", hr);
        goto kill_mouse;
    }

    // Acquire the device.
    didMouse->Acquire();

    // Init was successful.
    return true;

  kill_mouse:
    I_SAFE_RELEASE(didMouse);
    return false;
}

BOOL CALLBACK I_JoyEnum(LPCDIDEVICEINSTANCE lpddi, void *ref)
{
    // The first joystick is used by default.
    if(!firstJoystick.dwSize)
        memcpy(&firstJoystick, lpddi, sizeof(*lpddi));

    if(counter == joydevice)
    {
        // We'll use this one.
        memcpy(ref, lpddi, sizeof(*lpddi));
        return DIENUM_STOP;
    }
    counter++;
    return DIENUM_CONTINUE;
}

static boolean initJoystick(HWND hWnd)
{
    DIDEVICEINSTANCE ddi;
    int             i, joyProp[] = {
        DIJOFS_X, DIJOFS_Y, DIJOFS_Z,
        DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ,
        DIJOFS_SLIDER(0), DIJOFS_SLIDER(1)
    };
    const char *axisName[] = {
        "X", "Y", "Z", "RX", "RY", "RZ", "Slider 1", "Slider 2"
    };
    HRESULT         hr;

    if(ArgCheck("-nojoy"))
        return false;

    // ddi will contain info for the joystick device.
    memset(&firstJoystick, 0, sizeof(firstJoystick));
    memset(&ddi, 0, sizeof(ddi));
    counter = 0;

    // Find the joystick we want by doing an enumeration.
    dInput->EnumDevices(DI8DEVCLASS_GAMECTRL, I_JoyEnum, &ddi,
                             DIEDFL_ALLDEVICES);

    // Was the joystick we want found?
    if(!ddi.dwSize)
    {
        // Use the default joystick.
        if(!firstJoystick.dwSize)
            return false; // Not found.
        Con_Message("I_InitJoystick: joydevice = %i, out of range.\n",
                    joydevice);
        // Use the first joystick that was found.
        memcpy(&ddi, &firstJoystick, sizeof(ddi));
    }

    // Show some info.
    Con_Message("I_InitJoystick: %s\n", ddi.tszProductName);

    // Create the joystick device.
    hr = dInput->CreateDevice(ddi.guidInstance, &didJoy, 0);
    if(FAILED(hr))
    {
        Con_Message("I_InitJoystick: Failed to create device (0x%x).\n", hr);
        return false;
    }

    // Set data format.
    if(FAILED(hr = didJoy->SetDataFormat(&c_dfDIJoystick)))
    {
        Con_Message("I_InitJoystick: Failed to set data format (0x%x).\n", hr);
        goto kill_joy;
    }

    // Set behaviour.
    if(FAILED
       (hr = didJoy->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE |
                                               DISCL_FOREGROUND)))
    {
        Con_Message("I_InitJoystick: Failed to set co-op level (0x%x: %s).\n",
                    hr, I_ErrorMsg(hr));
        goto kill_joy;
    }

    // Set properties.
    for(i = 0; i < sizeof(joyProp) / sizeof(joyProp[0]); i++)
    {
        if(FAILED
           (hr =
            I_SetRangeProperty(didJoy, DIPROP_RANGE, DIPH_BYOFFSET, joyProp[i],
                               IJOY_AXISMIN, IJOY_AXISMAX)))
        {
            if(verbose)
                Con_Message("I_InitJoystick: Failed to set %s "
                            "range (0x%x: %s).\n", axisName[i], hr,
                            I_ErrorMsg(hr));
        }
    }

    // Set no dead zone.
    if(FAILED(hr = I_SetProperty(didJoy, DIPROP_DEADZONE, DIPH_DEVICE, 0, 0)))
    {
        Con_Message("I_InitJoystick: Failed to set dead zone (0x%x: %s).\n",
                    hr, I_ErrorMsg(hr));
    }

    // Set absolute mode.
    if(FAILED
       (hr =
        I_SetProperty(didJoy, DIPROP_AXISMODE, DIPH_DEVICE, 0,
                      DIPROPAXISMODE_ABS)))
    {
        Con_Message
            ("I_InitJoystick: Failed to set absolute axis mode (0x%x: %s).\n",
             hr, I_ErrorMsg(hr));
    }

    // Acquire it.
    didJoy->Acquire();

    // Initialization was successful.
    return true;

  kill_joy:
    I_SAFE_RELEASE(didJoy);
    return false;
}

static void killDevice(LPDIRECTINPUTDEVICE8 *dev)
{
    if(*dev)
        (*dev)->Unacquire();

    I_SAFE_RELEASE(*dev);
}

static boolean initKeyboard(HWND hWnd)
{
    HRESULT         hr;

    // Create the keyboard device.
    hr = dInput->CreateDevice(GUID_SysKeyboard, &didKeyb, 0);
    if(FAILED(hr))
    {
        Con_Message("I_Init: Failed to create keyboard device (0x%x).\n", hr);
        return false;
    }

    // Setup the keyboard input device.
    hr = didKeyb->SetDataFormat(&c_dfDIKeyboard);
    if(FAILED(hr))
    {
        Con_Message("I_Init: Failed to set keyboard data format (0x%x).\n",
                    hr);
        return false;
    }

    // Set behaviour.
    hr = didKeyb->SetCooperativeLevel(hWnd, DISCL_FOREGROUND |
                                            DISCL_NONEXCLUSIVE);
    if(FAILED(hr))
    {
        Con_Message("I_Init: Failed to set keyboard co-op level (0x%x).\n",
                    hr);
        return false;
    }

    // The input buffer size.
    hr = I_SetProperty(didKeyb, DIPROP_BUFFERSIZE, DIPH_DEVICE, 0, KEYBUFSIZE);
    if(FAILED(hr))
    {
        Con_Message("I_Init: Failed to set keyboard buffer size (0x%x).\n",
                    hr);
        return false;
    }

    // We'll be needing the DIKey to DDKey translation table.
    initDIKeyToDDKeyTlat();

    return true;
}

boolean DI_MousePresent(void)
{
    return (didMouse != 0);
}

boolean DI_JoystickPresent(void)
{
    return (didJoy != 0);
}

/**
 * Copy n key events from the device and encode them into given buffer.
 *
 * @param evbuf         Ptr to the buffer to encode events to.
 * @param bufsize       Size of the buffer.
 *
 * @return              Number of key events written to the buffer.
 */
size_t DI_GetKeyEvents(keyevent_t *evbuf, size_t bufsize)
{
    DIDEVICEOBJECTDATA keyData[KEYBUFSIZE];
    DWORD               i, num = 0;
    BYTE                tries;
    BOOL                acquired;
    HRESULT             hr;

    if(!initIOk)
        return 0;

    // Try two times to get the data.
    tries = 1;
    acquired = FALSE;
    while(!acquired && tries > 0)
    {
        num = KEYBUFSIZE;
        hr = didKeyb->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), keyData,
                                    &num, 0);
        if(SUCCEEDED(hr))
        {
            acquired = TRUE;
        }
        else if(tries > 0)
        {
            // Try to reacquire.
            didKeyb->Acquire();
            tries--;
        }
    }

    if(!acquired)
        return 0; // The operation is a failure.

    // Get the events.
    for(i = 0; i < num && i < bufsize; ++i)
    {
        if(keyData[i].dwOfs >= 0 && keyData[i].dwOfs < 256)
        {
            evbuf[i].event =
                (keyData[i].dwData & 0x80? IKE_KEY_DOWN : IKE_KEY_UP);
            // Use the table to translate the scancode to a ddkey.
            evbuf[i].ddkey = dIKeyToDDKey(keyData[i].dwOfs);
        }
    }

    return (size_t) i;
}

void DI_GetMouseState(mousestate_t *state)
{
    static BOOL     oldButtons[8];
    static int      oldZ;

    DIMOUSESTATE2   mstate;
    DWORD           i;
    BYTE            tries;
    BOOL            acquired;
    HRESULT         hr;

    memset(state, 0, sizeof(*state));

    // Has the mouse been initialized?
    if(!didMouse || !initIOk)
        return;

    // Try to get the mouse state.
    tries = 1;
    acquired = false;
    while(!acquired && tries > 0)
    {
        hr = didMouse->GetDeviceState(sizeof(mstate), &mstate);
        if(SUCCEEDED(hr))
        {
            acquired = true;
        }
        else if(tries > 0)
        {
            // Try to reacquire.
            didMouse->Acquire();
            tries--;
        }
    }

    if(!acquired)
        return; // The operation is a failure.

    // Fill in the state structure.
    state->x = (int) mstate.lX;
    state->y = (int) mstate.lY;

    // Handle mouse wheel (convert to buttons).
    state->buttonDowns[3] = 0;
    state->buttonUps[4] = 0;
    if(mstate.lZ > 0 && !(oldZ > 0))
    {
        state->buttonDowns[3] = 1;
        state->buttonUps[4] = 1;
    }
    else if(mstate.lZ < 0 && !(oldZ < 0))
    {
        state->buttonDowns[4] = 1;
        state->buttonUps[3] = 1;
    }
    else if(mstate.lZ == 0)
    {
        if(oldZ > 0)
            state->buttonUps[3] = 1;
        else if(oldZ < 0)
            state->buttonUps[4] = 1;
    }
    oldZ = (int) mstate.lZ;

    for(i = 0; i < 8; ++i)
    {
        BOOL            isDown = (mstate.rgbButtons[i] & 0x80? TRUE : FALSE);
        int             id;

        id = i;
        switch(i)
        {
        case 0: id = 0; break;
        case 1: id = 2; break;
        case 2: id = 1; break;
        default:
            id += 2; // Wheel Up/down.
            break;
        }

        state->buttonDowns[id] =
            state->buttonUps[id] = 0;
        if(isDown && !oldButtons[i])
            state->buttonDowns[id] = 1;
        else if(!isDown && oldButtons[i])
            state->buttonUps[id] = 1;

        oldButtons[i] = isDown;
    }
}

void DI_GetJoystickState(joystate_t *state)
{
    static BOOL     oldButtons[IJOY_MAXBUTTONS]; // Thats a lot of buttons.

    DWORD           tries, i;
    DIJOYSTATE      dijoy;
    BOOL            acquired;
    HRESULT         hr;

    memset(state, 0, sizeof(*state));

    // Initialization has not been done.
    if(!didJoy || !usejoystick || !initIOk)
        return;

    // Some joysticks need to be polled.
    didJoy->Poll();

    tries = 1;
    acquired = FALSE;
    while(!acquired && tries > 0)
    {
        hr = didJoy->GetDeviceState(sizeof(dijoy), &dijoy);

        if(SUCCEEDED(hr))
        {
            acquired = TRUE;
        }
        else if(tries > 0)
        {
            // Try to reacquire.
            didJoy->Acquire();
            tries--;
        }
    }

    if(!acquired)
        return; // The operation is a failure.

    state->numAxes = 8;
    state->axis[0] = (int) dijoy.lX;
    state->axis[1] = (int) dijoy.lY;
    state->axis[2] = (int) dijoy.lZ;
    state->axis[3] = (int) dijoy.lRx;
    state->axis[4] = (int) dijoy.lRy;
    state->axis[5] = (int) dijoy.lRz;
    state->axis[6] = (int) dijoy.rglSlider[0];
    state->axis[7] = (int) dijoy.rglSlider[1];

    state->numButtons = 32;
    for(i = 0; i < IJOY_MAXBUTTONS; ++i)
    {
        BOOL            isDown = (dijoy.rgbButtons[i] & 0x80? TRUE : FALSE);

        state->buttonDowns[i] =
            state->buttonUps[i] = 0;
        if(isDown && !oldButtons[i])
            state->buttonDowns[i] = 1;
        else if(!isDown && oldButtons[i])
            state->buttonUps[i] = 1;

        oldButtons[i] = isDown;
    }

    state->numHats = 4;
    for(i = 0; i < IJOY_MAXHATS; ++i)
    {
        DWORD           pov = dijoy.rgdwPOV[i];

        if((pov & 0xffff) == 0xffff)
            state->hatAngle[i] = IJOY_POV_CENTER;
        else
            state->hatAngle[i] = pov / 100.0f;
    }
}

int DI_Init(void)
{
    HRESULT             hr;
    HWND                hWnd;
    HINSTANCE           hInstance;

    if(initIOk)
        return true; // Already initialized.

    // Are we in verbose mode?
    if((verbose = ArgExists("-verbose")))
        Con_Message("DI_Init(DInput8): Initializing input driver...\n");

    // Get Doomsday's window handle.
    hWnd = (HWND) DD_GetVariable(DD_WINDOW_HANDLE);
    hInstance = (HINSTANCE) GetWindowLong(hWnd, GWL_HINSTANCE);

    // We'll create the DirectInput object. The only required input device
    // is the keyboard. The others are optional.
    hr = DirectInput8Create(hInstance, DIRECTINPUT_VERSION,
                            IID_IDirectInput8, (void**)&dInput, NULL);
    if FAILED(hr)
    {
        Con_Message("I_Init: DirectInput 8 init failed (0x%x).\n", hr);
        return false;
    }

    if(!dInput)
    {
        Con_Message("I_Init: DirectInput init failed.\n");
        return false;
    }

    if(!initKeyboard(hWnd))
        return false; // We must have a keyboard!

    // Acquire the keyboard.
    IDirectInputDevice_Acquire(didKeyb);

    // Create the mouse and joystick devices. It doesn't matter if the init
    // fails for them.
    initMouse(hWnd);
    initJoystick(hWnd);

    // Success!
    initIOk = true;
    return true;
}

void DI_Shutdown(void)
{
    if(!initIOk)
        return;

    // Release all the input devices.
    killDevice(&didKeyb);
    killDevice(&didMouse);
    killDevice(&didJoy);

    // Release DirectInput.
    dInput->Release();
    dInput = 0;

    delete keymap;
    keymap = NULL;

    initIOk = false;
}

void DI_Event(int type)
{
    // Not supported.
}

/**
 * Console command to write the current keymap to a file.
 */
#if 0 // Currently unused.
D_CMD(DumpKeyMap)
{
    DD_DumpKeymap(argv[1]);
    return true;
}
#endif

/**
 * Console command to load a keymap file.
 */
#if 0 // Currently unsued.
D_CMD(KeyMap)
{
    DD_LoadKeymap(argv[1]);
    return true;
}
#endif
