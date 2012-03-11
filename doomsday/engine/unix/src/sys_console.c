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

#include <curses.h>

#define mainWindow  (*Sys_MainWindow())

#define LINELEN 256  // @todo Lazy: This is the max acceptable window width.

static WINDOW* cursesRootWin;
static boolean conInputInited = false;

static boolean isValidWindow(ddwindow_t* win)
{
    if(win->type == WT_CONSOLE)
    {
        return win->console.winText && win->console.winTitle && win->console.winCommand;
    }
    return true;
}

static void setAttrib(int flags)
{
    if(!isValidWindow(&mainWindow))
        return;

    if(flags & (CPF_YELLOW | CPF_LIGHT))
        wattrset(mainWindow.console.winText, A_BOLD);
    else
        wattrset(mainWindow.console.winText, A_NORMAL);
}

/**
 * Writes the text in winText at (cx,cy).
 */
static void writeText(const char *line, int len)
{
    if(!isValidWindow(&mainWindow)) return;

    wmove(mainWindow.console.winText, mainWindow.console.cy, mainWindow.console.cx);
    waddnstr(mainWindow.console.winText, line, len);
    wclrtoeol(mainWindow.console.winText);
}

static int getScreenSize(int axis)
{
    int                 x, y;

    if(!isValidWindow(&mainWindow)) return;

    getmaxyx(mainWindow.console.winText, y, x);
    return axis == VX ? x : y;
}

static void setConWindowCmdLine(uint idx, const char *text,
                                unsigned int cursorPos, int flags)
{
    ddwindow_t  *win;
    unsigned int i;
    char        line[LINELEN], *ch;
    int         maxX;
    int         length;

    if(idx != 1)
    {
        // We only support one console window; (this isn't for us).
        return;
    }
    win = &mainWindow;
    if(!isValidWindow(win)) return;

    maxX = getScreenSize(VX);

    if(!text)
    {
        int         y, x;

        // Just move the cursor into the command line window.
        getyx(win->console.winCommand, y, x);
        wmove(win->console.winCommand, y, x);
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
        wmove(win->console.winCommand, 0, 0);

        // Can't print longer than the window.
        length = strlen(text);
        waddnstr(win->console.winCommand, line, MIN_OF(maxX, length + 1));
        wclrtoeol(win->console.winCommand);
    }
    wrefresh(win->console.winCommand);
}

void Sys_ConPrint(uint idx, const char *text, int clflags)
{
    ddwindow_t         *win;
    char                line[LINELEN];
    int                 count = strlen(text), lineStart, bPos;
    const char         *ptr = text;
    char                ch;
    int                 maxPos[2];

    if(!text)
        return;

    if(!novideo && idx != 1)
    {
        // We only support one terminal window (this isn't for us).
        return;
    }

    win = &mainWindow;
    if(!isValidWindow(win)) return;

    // Determine the size of the text window.
    getmaxyx(win->console.winText, maxPos[VY], maxPos[VX]);

    if(win->console.needNewLine)
    {
        // Need to make some room.
        win->console.cx = 0;
        win->console.cy++;
        while(win->console.cy >= maxPos[VY])
        {
            win->console.cy--;
            scroll(win->console.winText);
        }
        win->console.needNewLine = false;
    }
    bPos = lineStart = win->console.cx;

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
            win->console.cx += bPos - lineStart;
            bPos = 0;
            lineStart = 0;
            if(count > 1)       // Not the last character?
            {
                win->console.needNewLine = false;
                win->console.cx = 0;
                win->console.cy++;
                while(win->console.cy >= maxPos[VY])
                {
                    scroll(win->console.winText);
                    win->console.cy--;
                }
            }
            else
                win->console.needNewLine = true;
        }
    }

    // Something in the buffer?
    if(bPos - lineStart)
    {
        writeText(line + lineStart, bPos - lineStart);
        win->console.cx += bPos - lineStart;
    }

    wrefresh(win->console.winText);

    // Move the cursor back onto the command line.
    setConWindowCmdLine(1, NULL, 0, 0);
}

void Sys_SetConWindowCmdLine(uint idx, const char* text, uint cursorPos, int flags)
{
    ddwindow_t* win;

    win = Sys_Window(idx - 1);

    if(!win || win->type != WT_CONSOLE || !isValidWindow(win))
        return;

    assert(win == &mainWindow);

    setConWindowCmdLine(idx, text, cursorPos, flags);
}

void Sys_ConSetTitle(uint idx, const char* title)
{
    ddwindow_t* window = Sys_Window(idx);

    if(!isValidWindow(window)) return;

    // The background will also be in reverse.
    wbkgdset(window->console.winTitle, ' ' | A_REVERSE);

    // First clear the whole line.
    wmove(window->console.winTitle, 0, 0);
    wclrtoeol(window->console.winTitle);

    // Center the title.
    wmove(window->console.winTitle, 0, getmaxx(window->console.winTitle) / 2 - strlen(title) / 2);
    waddstr(window->console.winTitle, title);
    wrefresh(window->console.winTitle);
}

static void Sys_ConInputInit(void)
{
    if(conInputInited)
        return; // Already active.

    conInputInited = true;
}

ddwindow_t* Sys_ConInit(const char* title)
{
    if(ArgExists("-daemon"))
    {
        // Create a blank dummy window.
        memset(&mainWindow, 0, sizeof(mainWindow));
        mainWindow.type = WT_CONSOLE;
    }
    else
    {
        int maxPos[2];

        // Initialize curses.
        if(!(cursesRootWin = initscr()))
        {
            Sys_CriticalMessage("createDDWindow: Failed creating terminal.");
            return 0;
        }

        cbreak();
        noecho();
        nonl();

        mainWindow.type = WT_CONSOLE;

        // The current size of the screen.
        getmaxyx(stdscr, maxPos[VY], maxPos[VX]);

        // Create the three windows we will be using.
        mainWindow.console.winTitle = newwin(1, maxPos[VX], 0, 0);
        mainWindow.console.winText = newwin(maxPos[VY] - 2, maxPos[VX], 1, 0);
        mainWindow.console.winCommand = newwin(1, maxPos[VX], maxPos[VY] - 1, 0);

        // Set attributes.
        wattrset(mainWindow.console.winTitle, A_REVERSE);
        wattrset(mainWindow.console.winText, A_NORMAL);
        wattrset(mainWindow.console.winCommand, A_BOLD);

        scrollok(mainWindow.console.winText, TRUE);
        wclear(mainWindow.console.winText);
        wrefresh(mainWindow.console.winText);

        keypad(mainWindow.console.winCommand, TRUE);
        nodelay(mainWindow.console.winCommand, TRUE);
        setConWindowCmdLine(1, "", 1, 0);

        // The background will also be in reverse.
        wbkgdset(mainWindow.console.winTitle, ' ' | A_REVERSE);

        // First clear the whole line.
        wmove(mainWindow.console.winTitle, 0, 0);
        wclrtoeol(mainWindow.console.winTitle);

        // Center the title.
        wmove(mainWindow.console.winTitle, 0, getmaxx(mainWindow.console.winTitle) / 2 - strlen(title) / 2);
        waddstr(mainWindow.console.winTitle, title);
        wrefresh(mainWindow.console.winTitle);

        // We'll need the input event handler.
        Sys_ConInputInit();
    }
    return &mainWindow;
}

static void Sys_ConInputShutdown(void)
{
    if(!conInputInited)
        return;

    conInputInited = false;
}

void Sys_ConShutdown(uint idx)
{
    ddwindow_t* window = Sys_Window(idx);

    if(!isValidWindow(window)) return;

    // We should only have one window.
    assert(window == &mainWindow);

    // Delete windows and shut down curses.
    delwin(window->console.winTitle);
    delwin(window->console.winText);
    delwin(window->console.winCommand);

    window->console.winTitle = window->console.winText = window->console.winCommand = NULL;

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
