/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 * sys_console.c: Win32 Console
 *
 * Win32 console window handling. Used in dedicated mode.
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <tchar.h>
#include <windows.h>

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"

// MACROS ------------------------------------------------------------------

#define MAXRECS         128
#define LINELEN         80
#define TEXT_ATTRIB     (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CMDLINE_ATTRIB  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HANDLE hcInput;
static HANDLE hcScreen;
static CONSOLE_SCREEN_BUFFER_INFO cbInfo;
static int cx, cy;
static WORD attrib;
static int needNewLine = false;

static byte keymap[256];

// CODE --------------------------------------------------------------------

static void initVKeyToDDKeyTlat(void)
{
    unsigned int    i;

    for(i = 0; i < 256; ++i)
        keymap[i] = 0;

    keymap[VK_BACK] = DDKEY_BACKSPACE; // Backspace
    keymap[VK_TAB ] = DDKEY_TAB;
    //keymap[VK_CLEAR] = ;
    keymap[VK_RETURN] = DDKEY_ENTER;
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

void Sys_ConInit(void)
{
    char        title[256];

    // We'll be needing the VKey to DDKey translation table.
    LoadKeyboardLayout(_TEXT("00000409"), KLF_SUBSTITUTE_OK);
    initVKeyToDDKeyTlat();

    FreeConsole();
    if(!AllocConsole())
    {
        Con_Error("couldn't allocate a console! error %i\n", GetLastError());
    }
    hcInput = GetStdHandle(STD_INPUT_HANDLE);
    if(hcInput == INVALID_HANDLE_VALUE)
        Con_Error("bad input handle\n");

    // Compose the title.
    sprintf(title, "Doomsday " DOOMSDAY_VERSION_TEXT " (Dedicated) : %s",
            gx.GetVariable(DD_GAME_ID));

    if(!SetConsoleTitle(title))
        Con_Error("setting console title: error %i\n", GetLastError());

    hcScreen = GetStdHandle(STD_OUTPUT_HANDLE);
    if(hcScreen == INVALID_HANDLE_VALUE)
        Con_Error("bad output handle\n");

    GetConsoleScreenBufferInfo(hcScreen, &cbInfo);

    // This is the location of the print cursor.
    cx = 0;
    cy = cbInfo.dwSize.Y - 2;
    Sys_ConUpdateCmdLine("");
}

void Sys_ConShutdown(void)
{
}

void Sys_ConPostEvents(void)
{
    ddevent_t   ev;
    DWORD       num, read;
    INPUT_RECORD rec[MAXRECS], *ptr;
    KEY_EVENT_RECORD *key;

    if(!GetNumberOfConsoleInputEvents(hcInput, &num))
        Con_Error("Sys_ConPostEvents: error %i\n", GetLastError());
    if(!num)
        return;

    ReadConsoleInput(hcInput, rec, MAXRECS, &read);
    for(ptr = rec; read > 0; read--, ptr++)
    {
        if(ptr->EventType != KEY_EVENT)
            continue;

        key = &ptr->Event.KeyEvent;
        ev.device = IDEV_KEYBOARD;
        ev.type =   E_TOGGLE;
        ev.toggle.state = (key->bKeyDown ? ETOG_DOWN : ETOG_UP);
        ev.toggle.id = vKeyToDDKey(key->wVirtualKeyCode);

        // Track modifiers like alt, shift etc.
        I_TrackInput(&ev, 0);

        DD_PostEvent(&ev);
    }
}

static void setCmdLineCursor(int x, int y)
{
    COORD       pos;

    pos.X = x;
    pos.Y = y;
    SetConsoleCursorPosition(hcScreen, pos);
}

static void scrollLine(void)
{
    SMALL_RECT  src;
    COORD       dest;
    CHAR_INFO   fill;

    src.Left = 0;
    src.Right = cbInfo.dwSize.X - 1;
    src.Top = 1;
    src.Bottom = cbInfo.dwSize.Y - 2;
    dest.X = 0;
    dest.Y = 0;
    fill.Attributes = TEXT_ATTRIB;
    fill.Char.AsciiChar = ' ';
    ScrollConsoleScreenBuffer(hcScreen, &src, NULL, dest, &fill);
}

static void setAttrib(int flags)
{
    attrib = 0;
    if(flags & CBLF_WHITE)
        attrib = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    if(flags & CBLF_BLUE)
        attrib = FOREGROUND_BLUE;
    if(flags & CBLF_GREEN)
        attrib = FOREGROUND_GREEN;
    if(flags & CBLF_CYAN)
        attrib = FOREGROUND_BLUE | FOREGROUND_GREEN;
    if(flags & CBLF_RED)
        attrib = FOREGROUND_RED;
    if(flags & CBLF_MAGENTA)
        attrib = FOREGROUND_RED | FOREGROUND_BLUE;
    if(flags & CBLF_YELLOW)
        attrib = FOREGROUND_RED | FOREGROUND_GREEN;
    if(flags & CBLF_LIGHT)
        attrib |= FOREGROUND_INTENSITY;
    if((flags & CBLF_WHITE) != CBLF_WHITE)
        attrib |= FOREGROUND_INTENSITY;
    SetConsoleTextAttribute(hcScreen, attrib);
}

/**
 * Writes the text at the (cx,cy).
 */
static void writeText(CHAR_INFO *line, int len)
{
    COORD       linesize;
    COORD       from = {0, 0};
    SMALL_RECT  rect;

    linesize.X = len;
    linesize.Y = 1;
    rect.Left = cx;
    rect.Right = cx + len;
    rect.Top = cy;
    rect.Bottom = cy;
    WriteConsoleOutput(hcScreen, line, linesize, from, &rect);
}

void Sys_ConPrint(int clflags, char *text)
{
    unsigned int    i;
    int             linestart, bpos;
    char           *ptr = text, ch;
    size_t          len = strlen(text);
    CHAR_INFO       line[LINELEN];

    if(needNewLine)
    {   // Need to make some room.
        cx = 0;
        cy++;
        if(cy == cbInfo.dwSize.Y - 1)
        {
            cy--;
            scrollLine();
        }
        needNewLine = false;
    }

    bpos = linestart = cx;
    setAttrib(clflags);
    for(i = 0; i < len; i++, ptr++)
    {
        ch = *ptr;
        if(ch != '\n' && bpos < LINELEN)
        {
            line[bpos].Attributes = attrib;
            line[bpos].Char.AsciiChar = ch;
            bpos++;
        }

        // Time for newline?
        if(ch == '\n' || bpos == LINELEN)
        {
            writeText(line + linestart, bpos - linestart);
            cx += bpos - linestart;
            bpos = 0;
            linestart = 0;
            if(i < len - 1)
            {   // Not the last character.
                needNewLine = false;
                cx = 0;
                cy++;
                if(cy == cbInfo.dwSize.Y - 1)
                {
                    scrollLine();
                    cy--;
                }
            }
            else
            {
                needNewLine = true;
            }
        }
    }

    // Something left in the buffer?
    if(bpos - linestart)
    {
        writeText(line + linestart, bpos - linestart);
        cx += bpos - linestart;
    }
}

void Sys_ConUpdateCmdLine(char *text)
{
    static int  lastInputMode = -1;

    CHAR_INFO   line[LINELEN], *ch;
    unsigned int i;
    COORD       linesize = {LINELEN, 1};
    COORD       from = {0, 0};
    SMALL_RECT  rect;
    int         currentInputMode;

    // Do we need to change the look of the cursor?
    currentInputMode = Con_InputMode();
    if(currentInputMode != lastInputMode)
    {
        CONSOLE_CURSOR_INFO curInfo;

        curInfo.bVisible = TRUE;
        curInfo.dwSize = (currentInputMode? 100 : 10);

        SetConsoleCursorInfo(hcScreen, &curInfo);
        lastInputMode = currentInputMode;
    }

    line[0].Char.AsciiChar = '>';
    line[0].Attributes = CMDLINE_ATTRIB;
    for(i = 0, ch = line + 1; i < LINELEN - 1; ++i, ch++)
    {
        if(i < strlen(text))
            ch->Char.AsciiChar = text[i];
        else
            ch->Char.AsciiChar = ' ';

        // Gray color.
        ch->Attributes = CMDLINE_ATTRIB;
    }

    rect.Left = 0;
    rect.Right = LINELEN - 1;
    rect.Top = cbInfo.dwSize.Y - 1;
    rect.Bottom = cbInfo.dwSize.Y - 1;
    WriteConsoleOutput(hcScreen, line, linesize, from, &rect);
    setCmdLineCursor(Con_CursorPosition() + 1, cbInfo.dwSize.Y - 1);
}
