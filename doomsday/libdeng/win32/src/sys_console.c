/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2009 Daniel Swanson <danij@dengine.net>
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

#if 0
/**
 * sys_console.c: Std input handling - Win32 specific
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "doomsday.h"
#include "de_platform.h"
#include "de_console.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAXRECS             128

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean conInputInited = false;

static HANDLE hcInput = NULL;
static byte *keymap = NULL;
static byte *vKeyDown = NULL; // Used for tracking the state of the vKeys.

static INPUT_RECORD *inputBuf = NULL;
static DWORD inputBufsize = 0;

// CODE --------------------------------------------------------------------

static void initVKeyToDDKeyTlat(void)
{
    if(keymap)
        return; // Already been here.

    keymap = M_Calloc(sizeof(byte) * 256);

    keymap[VK_BACK] = DDKEY_BACKSPACE; // Backspace
    keymap[VK_TAB ] = DDKEY_TAB;
    //keymap[VK_CLEAR] = ;
    keymap[VK_RETURN] = DDKEY_RETURN;
    keymap[VK_SHIFT] = DDKEY_RSHIFT;
    keymap[VK_CONTROL] = DDKEY_RCTRL;
    keymap[VK_MENU] = DDKEY_RALT;
    keymap[VK_PAUSE] = DDKEY_PAUSE;
    //keymap[VK_CAPITAL] = ;
    //keymap[VK_KANA] = ;
    //keymap[VK_HANGEUL] = ;
    //keymap[VK_HANGUL] = ;
    //keymap[VK_JUNJA] = ;
    //keymap[VK_FINAL] = ;
    //keymap[VK_HANJA] = ;
    //keymap[VK_KANJI] = ;
    keymap[VK_ESCAPE] = DDKEY_ESCAPE;
    //keymap[VK_CONVERT] = ;
    //keymap[VK_NONCONVERT] = ;
    //keymap[VK_ACCEPT] = ;
    //keymap[VK_MODECHANGE] = ;
    keymap[VK_SPACE] = ' ';
    keymap[VK_OEM_PLUS] = '+';
    keymap[VK_OEM_COMMA] = ',';
    keymap[VK_OEM_MINUS] = '-';
    keymap[VK_OEM_PERIOD] = '.';
    keymap[VK_OEM_1] = ';';
    keymap[VK_OEM_2] = '/';
    keymap[VK_OEM_3] = '\'';
    keymap[VK_OEM_4] = '[';
    keymap[VK_OEM_5] = DDKEY_BACKSLASH;
    keymap[VK_OEM_6] = ']';
    keymap[VK_OEM_7] = '#';
    keymap[VK_OEM_8] = '`';
    keymap[VK_PRIOR] = DDKEY_PGUP;
    keymap[VK_NEXT] = DDKEY_PGDN;
    keymap[VK_END] = DDKEY_END;
    keymap[VK_HOME] = DDKEY_HOME;
    keymap[VK_LEFT] = DDKEY_LEFTARROW;
    keymap[VK_UP] = DDKEY_UPARROW;
    keymap[VK_RIGHT] = DDKEY_RIGHTARROW;
    keymap[VK_DOWN] = DDKEY_DOWNARROW;
    //keymap[VK_SELECT] = ;
    //keymap[VK_PRINT] = ;
    //keymap[VK_EXECUTE] = ;
    //keymap[VK_SNAPSHOT] = ;
    keymap[VK_INSERT] = DDKEY_INS;
    keymap[VK_DELETE] = DDKEY_DEL;
    //keymap[VK_HELP] = ;
    //keymap[VK_LWIN] = ;
    //keymap[VK_RWIN] = ;
    //keymap[VK_APPS] = ;
    //keymap[VK_SLEEP] = ;
    keymap[VK_NUMPAD0] = DDKEY_NUMPAD0;
    keymap[VK_NUMPAD1] = DDKEY_NUMPAD1;
    keymap[VK_NUMPAD2] = DDKEY_NUMPAD2;
    keymap[VK_NUMPAD3] = DDKEY_NUMPAD3;
    keymap[VK_NUMPAD4] = DDKEY_NUMPAD4;
    keymap[VK_NUMPAD5] = DDKEY_NUMPAD5;
    keymap[VK_NUMPAD6] = DDKEY_NUMPAD6;
    keymap[VK_NUMPAD7] = DDKEY_NUMPAD7;
    keymap[VK_NUMPAD8] = DDKEY_NUMPAD8;
    keymap[VK_NUMPAD9] = DDKEY_NUMPAD9;
    keymap[VK_MULTIPLY] = '*';
    keymap[VK_ADD] = DDKEY_ADD;
    //keymap[VK_SEPARATOR] = ;
    keymap[VK_SUBTRACT] = DDKEY_SUBTRACT;
    keymap[VK_DECIMAL] = DDKEY_DECIMAL;
    keymap[VK_DIVIDE] = '/';
    keymap[VK_F1] = DDKEY_F1;
    keymap[VK_F2] = DDKEY_F2;
    keymap[VK_F3] = DDKEY_F3;
    keymap[VK_F4] = DDKEY_F4;
    keymap[VK_F5] = DDKEY_F5;
    keymap[VK_F6] = DDKEY_F6;
    keymap[VK_F7] = DDKEY_F7;
    keymap[VK_F8] = DDKEY_F8;
    keymap[VK_F9] = DDKEY_F9;
    keymap[VK_F10] = DDKEY_F10;
    keymap[VK_F11] = DDKEY_F11;
    keymap[VK_F12] = DDKEY_F12;

    keymap[0x30] = '0';
    keymap[0x31] = '1';
    keymap[0x32] = '2';
    keymap[0x33] = '3';
    keymap[0x34] = '4';
    keymap[0x35] = '5';
    keymap[0x36] = '6';
    keymap[0x37] = '7';
    keymap[0x38] = '8';
    keymap[0x39] = '9';
    keymap[0x41] = 'a';
    keymap[0x42] = 'b';
    keymap[0x43] = 'c';
    keymap[0x44] = 'd';
    keymap[0x45] = 'e';
    keymap[0x46] = 'f';
    keymap[0x47] = 'g';
    keymap[0x48] = 'h';
    keymap[0x49] = 'i';
    keymap[0x4A] = 'j';
    keymap[0x4B] = 'k';
    keymap[0x4C] = 'l';
    keymap[0x4D] = 'm';
    keymap[0x4E] = 'n';
    keymap[0x4F] = 'o';
    keymap[0x50] = 'p';
    keymap[0x51] = 'q';
    keymap[0x52] = 'r';
    keymap[0x53] = 's';
    keymap[0x54] = 't';
    keymap[0x55] = 'u';
    keymap[0x56] = 'v';
    keymap[0x57] = 'w';
    keymap[0x58] = 'x';
    keymap[0x59] = 'y';
    keymap[0x5A] = 'z';
}

/**
 * Convert a VKey (VK_*) to a DDkey (DDKEY_*) constant.
 */
static byte vKeyToDDKey(byte vkey)
{
    return keymap[vkey];
}

void Sys_ConInputInit(void)
{
    if(conInputInited)
        return; // Already active.

    /**
     * For now, always load the U.S. English layout.
     *
     * \todo Is this even necessary with virtualkeys?
     */
    LoadKeyboardLayout("00000409", KLF_SUBSTITUTE_OK);

    // We'll be needing the VKey to DDKey translation table.
    initVKeyToDDKeyTlat();

    // And the down key array;
    vKeyDown = M_Calloc(sizeof(byte) * 256);

    hcInput = GetStdHandle(STD_INPUT_HANDLE);
    if(hcInput == INVALID_HANDLE_VALUE)
        Con_Error("Sys_ConInit: Bad input handle\n");

    conInputInited = true;
}

void Sys_ConInputShutdown(void)
{
    if(!conInputInited)
        return;

    if(inputBuf)
    {
        M_Free(inputBuf);
        inputBuf = NULL;
        inputBufsize = 0;
    }

    M_Free(keymap);
    keymap = NULL;
    M_Free(vKeyDown);
    vKeyDown = NULL;

    conInputInited = false;
}

/**
 * Copy n key events from the console and encode them into given buffer.
 *
 * @param evbuf         Ptr to the buffer to encode events to.
 * @param bufsize       Size of the buffer.
 *
 * @return              Number of key events written to the buffer.
 */
size_t I_GetConsoleKeyEvents(keyevent_t *evbuf, size_t bufsize)
{
    DWORD           num;
    size_t          n = 0;

    if(!conInputInited)
        return 0;

    // Check for awaiting unprocessed events.
    if(!GetNumberOfConsoleInputEvents(hcInput, &num))
        Con_Error("Sys_ConPostEvents: error %i\n", (int) GetLastError());

    if(num > 0)
    {
        DWORD           read, min;
        INPUT_RECORD    *ptr;
        KEY_EVENT_RECORD *key;

        // Do we need to enlarge the input record buffer?
        min = MIN_OF((DWORD)bufsize, num);
        if(min > inputBufsize)
        {
            inputBuf = M_Malloc(sizeof(INPUT_RECORD) * min);
            inputBufsize = min;
        }

        // Get the input records.
        ReadConsoleInput(hcInput, inputBuf, min, &read);

        // Convert to key events and pack into the supplied buffer.
        for(ptr = inputBuf; read > 0; read--, ptr++)
        {
            if(ptr->EventType != KEY_EVENT)
                continue;

            key = &ptr->Event.KeyEvent;

            // Has the state changed?
            if((!vKeyDown[key->wVirtualKeyCode] && key->bKeyDown) ||
               (vKeyDown[key->wVirtualKeyCode] && !key->bKeyDown))
            {
                // Use the table to translate the vKey to a DDKEY.
                evbuf[n].ddkey = vKeyToDDKey(key->wVirtualKeyCode);
                evbuf[n].event =
                    (key->bKeyDown? IKE_KEY_DOWN : IKE_KEY_UP);

                // Record the new state of this vKey.
                vKeyDown[key->wVirtualKeyCode] =
                    (key->bKeyDown? true : false);
                n++;
            }
        }
    }

    return n;
}
#endif
