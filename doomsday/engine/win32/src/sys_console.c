/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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

// CODE --------------------------------------------------------------------

void Sys_ConInit(void)
{
    char        title[256];

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
        if(key->wVirtualKeyCode == VK_UP)
            ev.toggle.id = DDKEY_UPARROW;
        else if(key->wVirtualKeyCode == VK_DOWN)
            ev.toggle.id = DDKEY_DOWNARROW;
        else
            ev.toggle.id = DD_ScanToKey(key->wVirtualScanCode);

        DD_PostEvent(&ev);
    }
}

static void setCursor(int x, int y)
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
    CHAR_INFO   line[LINELEN], *ch;
    unsigned int i;
    COORD       linesize = {LINELEN, 1};
    COORD       from = {0, 0};
    SMALL_RECT  rect;

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
    setCursor((int) strlen(text) + 1, cbInfo.dwSize.Y - 1);
}
