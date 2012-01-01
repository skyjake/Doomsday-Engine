/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * sys_console.c: Text-Mode Console
 *
 * Standalone console window handling.  Used in dedicated mode.
 * \fixme Some lazy programming in here...
 */

// HEADER FILES ------------------------------------------------------------

// Without the following, curses.h defines false 0 and true 1
#define NCURSES_ENABLE_STDBOOL_H 0

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"

#include <curses.h>

// MACROS ------------------------------------------------------------------

#define LINELEN 256  // \fixme Lazy: This is the max acceptable window width.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean conInputInited = false;

// CODE --------------------------------------------------------------------

void Sys_ConInputInit(void)
{
    if(conInputInited)
        return; // Already active.

    conInputInited = true;
}

void Sys_ConInputShutdown(void)
{
    if(!conInputInited)
        return;

    conInputInited = false;
}

static int translateKey(int key)
{
    if(key >= 32 && key <= 127)
        return key;

    //fprintf(outFile, "key=%i\n", key);

    switch(key)
    {
    case '\r':
    case '\n':
    case KEY_ENTER:
        return DDKEY_RETURN;

    case KEY_BACKSPACE:
    case KEY_DC:
        return DDKEY_BACKSPACE;

    case '\t':
        return DDKEY_TAB;

    case KEY_UP:
        return DDKEY_UPARROW;

    case KEY_DOWN:
        return DDKEY_DOWNARROW;

    default:
        // Unknown key.
        return 0;
    }
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
    int                 key;
    byte                ddkey;
    size_t              n;

    if(!conInputInited)
        return 0;

    for(n = 0, key = wgetch(theWindow->console.winCommand); key != ERR && n < bufsize;
        key = wgetch(theWindow->console.winCommand))
    {
        // Use the table to translate the vKey to a DDKEY.
        ddkey = translateKey(key);

        evbuf[n].ddkey = ddkey;
        evbuf[n].event = IKE_KEY_DOWN;
        n++;

        // Release immediately.
        evbuf[n].ddkey = ddkey;
        evbuf[n].event = IKE_KEY_UP;
        n++;
    }

    return n;
}
