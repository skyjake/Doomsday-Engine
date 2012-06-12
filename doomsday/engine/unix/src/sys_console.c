/** @file sys_console.c
 * Text-mode console for Unix. @ingroup base
 *
 * Only used in novideo/dedicated mode. Implemented using the curses library.
 *
 * @authors Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

// Without the following, curses.h defines false 0 and true 1
#define NCURSES_ENABLE_STDBOOL_H 0

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"
#include "de_misc.h"
#include "consolewindow.h"

#include <curses.h>

#define mainConsole (*Window_Console(Window_Main()))

#define LINELEN 256  // @todo Lazy: This is the max acceptable window width.

static WINDOW* cursesRootWin;
static boolean conInputInited = false;

static boolean isValidConsoleWindow(const Window* win)
{
    const consolewindow_t* console = Window_ConsoleConst(win);
    return console->winText && console->winTitle && console->winCommand;
}

static void setAttrib(int flags)
{
    if(!isValidConsoleWindow(Window_Main()))
        return;

    if(flags & (CPF_YELLOW | CPF_LIGHT))
        wattrset(mainConsole.winText, A_BOLD);
    else
        wattrset(mainConsole.winText, A_NORMAL);
}

/**
 * Writes the text in winText at (cx,cy).
 */
static void writeText(const char *line, int len)
{
    if(!isValidConsoleWindow(Window_Main())) return;

    wmove(mainConsole.winText, mainConsole.cy, mainConsole.cx);
    waddnstr(mainConsole.winText, line, len);
    wclrtoeol(mainConsole.winText);
}

static int getScreenSize(int axis)
{
    int                 x, y;

    if(!isValidConsoleWindow(Window_Main())) return 0;

    getmaxyx(mainConsole.winText, y, x);
    return axis == VX ? x : y;
}

static void setConWindowCmdLine(uint idx, const char *text,
                                unsigned int cursorPos, int flags)
{
    Window  *win;
    unsigned int i;
    char        line[LINELEN], *ch;
    int         maxX;
    int         length;
    consolewindow_t* console;

    if(idx != 1)
    {
        // We only support one console window; (this isn't for us).
        return;
    }
    win = Window_Main();
    if(!isValidConsoleWindow(win)) return;
    console = Window_Console(win);

    maxX = getScreenSize(VX);

    if(!text)
    {
        int         y, x;

        // Just move the cursor into the command line window.
        getyx(console->winCommand, y, x);
        wmove(console->winCommand, y, x);
    }
    else
    {
        memset(line, 0, sizeof(line));
        line[0] = '>';
        for(i = 0, ch = line + 1; i < LINELEN - 1; ++i, ch++)
        {
            if(i < strlen(text))
                *ch = text[i];
            else
                *ch = 0;
        }
        wmove(console->winCommand, 0, 0);

        // Can't print longer than the window.
        length = strlen(text);
        waddnstr(console->winCommand, line, MIN_OF(maxX, length + 1));
        wclrtoeol(console->winCommand);
    }
    wrefresh(console->winCommand);
}

void Sys_ConPrint(uint idx, const char *text, int clflags)
{
    Window *win;
    char                line[LINELEN];
    int                 count = strlen(text), lineStart, bPos;
    const char         *ptr = text;
    char                ch;
    int                 maxPos[2];
    consolewindow_t    *console;

    if(!text)
        return;

    if(!novideo && idx != 1)
    {
        // We only support one terminal window (this isn't for us).
        return;
    }

    win = Window_Main();
    if(!isValidConsoleWindow(win)) return;
    console = Window_Console(win);

    // Determine the size of the text window.
    getmaxyx(console->winText, maxPos[VY], maxPos[VX]);

    if(console->needNewLine)
    {
        // Need to make some room.
        console->cx = 0;
        console->cy++;
        while(console->cy >= maxPos[VY])
        {
            console->cy--;
            scroll(console->winText);
        }
        console->needNewLine = false;
    }
    bPos = lineStart = console->cx;

    setAttrib(clflags);

    for(; count > 0; count--, ptr++)
    {
        ch = *ptr;
        // Ignore carriage returns.
        if(ch == '\r')
            continue;
        if(ch != '\n' && bPos < maxPos[VX])
        {
            line[bPos] = ch;
            bPos++;
        }
        // Time for newline?
        if(ch == '\n' || bPos >= maxPos[VX])
        {
            writeText(line + lineStart, bPos - lineStart);
            console->cx += bPos - lineStart;
            bPos = 0;
            lineStart = 0;
            if(count > 1)       // Not the last character?
            {
                console->needNewLine = false;
                console->cx = 0;
                console->cy++;
                while(console->cy >= maxPos[VY])
                {
                    scroll(console->winText);
                    console->cy--;
                }
            }
            else
                console->needNewLine = true;
        }
    }

    // Something in the buffer?
    if(bPos - lineStart)
    {
        writeText(line + lineStart, bPos - lineStart);
        console->cx += bPos - lineStart;
    }

    wrefresh(console->winText);

    // Move the cursor back onto the command line.
    setConWindowCmdLine(1, NULL, 0, 0);
}

void Sys_SetConWindowCmdLine(uint idx, const char* text, uint cursorPos, int flags)
{
    Window* win = Window_ByIndex(idx);

    if(!win || Window_Type(win) != WT_CONSOLE || !isValidConsoleWindow(win))
        return;

    assert(win == Window_Main());

    setConWindowCmdLine(idx, text, cursorPos, flags);
}

void ConsoleWindow_SetTitle(const Window* window, const char* title)
{
    const consolewindow_t* console;

    if(!isValidConsoleWindow(window)) return;
    console = Window_ConsoleConst(window);

    // The background will also be in reverse.
    wbkgdset(console->winTitle, ' ' | A_REVERSE);

    // First clear the whole line.
    wmove(console->winTitle, 0, 0);
    wclrtoeol(console->winTitle);

    // Center the title.
    wmove(console->winTitle, 0, getmaxx(console->winTitle) / 2 - strlen(title) / 2);
    waddstr(console->winTitle, title);
    wrefresh(console->winTitle);
}

static void Sys_ConInputInit(void)
{
    if(conInputInited)
        return; // Already active.

    conInputInited = true;
}

Window* Sys_ConInit(const char* title)
{
    if(!CommandLine_Exists("-daemon"))
    {
        int maxPos[2];

		// Do not output to standard out, curses would get confused.
		LogBuffer_EnableStandardOutput(false);

        // Initialize curses.
        if(!(cursesRootWin = initscr()))
        {
            Sys_CriticalMessage("createDDWindow: Failed creating terminal.");
            return 0;
        }

        cbreak();
        noecho();
        nonl();

        // The current size of the screen.
        getmaxyx(stdscr, maxPos[VY], maxPos[VX]);

        // Create the three windows we will be using.
        mainConsole.winTitle = newwin(1, maxPos[VX], 0, 0);
        mainConsole.winText = newwin(maxPos[VY] - 2, maxPos[VX], 1, 0);
        mainConsole.winCommand = newwin(1, maxPos[VX], maxPos[VY] - 1, 0);

        // Set attributes.
        wattrset(mainConsole.winTitle, A_REVERSE);
        wattrset(mainConsole.winText, A_NORMAL);
        wattrset(mainConsole.winCommand, A_BOLD);

        scrollok(mainConsole.winText, TRUE);
        wclear(mainConsole.winText);
        wrefresh(mainConsole.winText);

        keypad(mainConsole.winCommand, TRUE);
        nodelay(mainConsole.winCommand, TRUE);
        setConWindowCmdLine(1, "", 1, 0);

        // The background will also be in reverse.
        wbkgdset(mainConsole.winTitle, ' ' | A_REVERSE);

        // First clear the whole line.
        wmove(mainConsole.winTitle, 0, 0);
        wclrtoeol(mainConsole.winTitle);

        // Center the title.
        wmove(mainConsole.winTitle, 0, getmaxx(mainConsole.winTitle) / 2 - strlen(title) / 2);
        waddstr(mainConsole.winTitle, title);
        wrefresh(mainConsole.winTitle);

        // We'll need the input event handler.
        Sys_ConInputInit();
    }

    return Window_Main();
}

static void Sys_ConInputShutdown(void)
{
    if(!conInputInited)
        return;

    conInputInited = false;
}

void Sys_ConShutdown(Window* window)
{
    consolewindow_t* console;

    if(!isValidConsoleWindow(window)) return;
    console = Window_Console(window);

    // We should only have one window.
    assert(window == Window_Main());

    // Delete windows and shut down curses.
    delwin(console->winTitle);
    delwin(console->winText);
    delwin(console->winCommand);

    console->winTitle = console->winText = console->winCommand = NULL;

    delwin(cursesRootWin);
    cursesRootWin = 0;

    endwin();
    refresh();

    Sys_ConInputShutdown();
}

static int translateKey(int key)
{
    if(key >= 32 && key <= 127)
        return key;

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

static void submitEvent(keyevent_t* ev, int type, int ddkey)
{
    memset(ev, 0, sizeof(*ev));
    ev->ddkey = ddkey;
    ev->type = type;
    if(ddkey >= ' ' && ddkey < 128)
    {
        // Simple ASCII.
        ev->text[0] = ddkey;
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

    for(n = 0, key = wgetch(Window_ConsoleConst(theWindow)->winCommand); key != ERR && n < bufsize;
        key = wgetch(Window_ConsoleConst(theWindow)->winCommand))
    {
        // Use the table to translate the vKey to a DDKEY.
        ddkey = translateKey(key);
        submitEvent(&evbuf[n++], IKE_DOWN, ddkey);

        // Release immediately.
        submitEvent(&evbuf[n++], IKE_UP, ddkey);
    }

    return n;
}
