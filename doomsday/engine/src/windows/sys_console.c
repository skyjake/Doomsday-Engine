/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
 * @todo  This code is on its way out... It will be replaced with a Qt based
 * console GUI window. On Windows there will be no true text-mode console.
 */

/**
 * sys_console.c: Std input handling - Win32 specific
 */

#include "de_platform.h"
#include "de_console.h"
#include "de_misc.h"
#include "ui/consolewindow.h"

#define WINDOWEDSTYLE (WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | \
                       WS_CLIPCHILDREN | WS_CLIPSIBLINGS)
#define FULLSCREENSTYLE (WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS)

#define LINELEN         80
#define TEXT_ATTRIB     (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CMDLINE_ATTRIB  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)

#define MAXRECS             128

static boolean conInputInited = false;

static HANDLE hcInput = NULL;
static byte *keymap = NULL;
static byte *vKeyDown = NULL; // Used for tracking the state of the vKeys.

static INPUT_RECORD *inputBuf = NULL;
static DWORD inputBufsize = 0;

static void Sys_ConInputInit(void);

static void setCmdLineCursor(consolewindow_t *win, int x, int y)
{
    COORD pos;

    pos.X = x;
    pos.Y = y;
    SetConsoleCursorPosition(win->hcScreen, pos);
}

static void scrollLine(consolewindow_t *win)
{
    SMALL_RECT  src;
    COORD       dest;
    CHAR_INFO   fill;

    src.Left = 0;
    src.Right = win->cbInfo.dwSize.X - 1;
    src.Top = 1;
    src.Bottom = win->cbInfo.dwSize.Y - 2;
    dest.X = 0;
    dest.Y = 0;
    fill.Attributes = TEXT_ATTRIB;
#ifdef UNICODE
    fill.Char.UnicodeChar = L' ';
#else
    fill.Char.AsciiChar = ' ';
#endif

    ScrollConsoleScreenBuffer(win->hcScreen, &src, NULL, dest, &fill);
}

/**
 * @param flags  @ref consolePrintFlags
 */
static void setAttrib(consolewindow_t* win, int flags)
{
    win->attrib = 0;
    if(flags & CPF_WHITE)
        win->attrib = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    if(flags & CPF_BLUE)
        win->attrib = FOREGROUND_BLUE;
    if(flags & CPF_GREEN)
        win->attrib = FOREGROUND_GREEN;
    if(flags & CPF_CYAN)
        win->attrib = FOREGROUND_BLUE | FOREGROUND_GREEN;
    if(flags & CPF_RED)
        win->attrib = FOREGROUND_RED;
    if(flags & CPF_MAGENTA)
        win->attrib = FOREGROUND_RED | FOREGROUND_BLUE;
    if(flags & CPF_YELLOW)
        win->attrib = FOREGROUND_RED | FOREGROUND_GREEN;
    if(flags & CPF_LIGHT)
        win->attrib |= FOREGROUND_INTENSITY;
    if((flags & CPF_WHITE) != CPF_WHITE)
        win->attrib |= FOREGROUND_INTENSITY;

    SetConsoleTextAttribute(win->hcScreen, win->attrib);
}

/**
 * Writes the text at the (cx,cy).
 */
static void writeText(consolewindow_t *win, CHAR_INFO *line, int len)
{
    COORD       linesize;
    COORD       from = {0, 0};
    SMALL_RECT  rect;

    linesize.X = len;
    linesize.Y = 1;
    rect.Left = win->cx;
    rect.Right = win->cx + len;
    rect.Top = win->cy;
    rect.Bottom = win->cy;

    WriteConsoleOutput(win->hcScreen, line, linesize, from, &rect);
}

void Sys_ConPrint(uint idx, const char* text, int flags)
{
    consolewindow_t* win;
    unsigned int i;
    int linestart, bpos;
    const char* ptr = text;
    CHAR_INFO line[LINELEN];
    size_t len;
    char ch;
#ifdef UNICODE
    wchar_t wch;
#endif

    win = Window_Console(Window_ByIndex(idx));
    if(!win || !text) return;

    if(win->needNewLine)
    {   // Need to make some room.
        win->cx = 0;
        win->cy++;
        if(win->cy == win->cbInfo.dwSize.Y - 1)
        {
            win->cy--;
            scrollLine(win);
        }
        win->needNewLine = false;
    }

    bpos = linestart = win->cx;
    setAttrib(win, flags);
    len = strlen(text);
    for(i = 0; i < len; i++, ptr++)
    {
        ch = *ptr;
        if(ch != '\n' && bpos < LINELEN)
        {
            line[bpos].Attributes = win->attrib;
#ifdef UNICODE
            mbtowc(&wch, &ch, MB_CUR_MAX);
            line[bpos].Char.UnicodeChar = wch;
#else
            line[bpos].Char.AsciiChar = ch;
#endif
            bpos++;
        }

        // Time for newline?
        if(ch == '\n' || bpos == LINELEN)
        {
            writeText(win, line + linestart, bpos - linestart);
            win->cx += bpos - linestart;
            bpos = 0;
            linestart = 0;
            if(i < len - 1)
            {   // Not the last character.
                win->needNewLine = false;
                win->cx = 0;
                win->cy++;
                if(win->cy == win->cbInfo.dwSize.Y - 1)
                {
                    scrollLine(win);
                    win->cy--;
                }
            }
            else
            {
                win->needNewLine = true;
            }
        }
    }

    // Something left in the buffer?
    if(bpos - linestart)
    {
        writeText(win, line + linestart, bpos - linestart);
        win->cx += bpos - linestart;
    }
}

static void setConWindowCmdLine(consolewindow_t* win, const char* text, uint cursorPos, int flags)
{
    CHAR_INFO line[LINELEN], *ch;
    COORD linesize = {LINELEN, 1};
    COORD from = {0, 0};
    SMALL_RECT rect;
    uint i;
#ifdef UNICODE
    wchar_t wch;
#endif

    // Do we need to change the look of the cursor?
    if((flags & CLF_CURSOR_LARGE) !=
        (win->cmdline.flags & CLF_CURSOR_LARGE))
    {
        CONSOLE_CURSOR_INFO curInfo;

        curInfo.bVisible = TRUE;
        curInfo.dwSize = ((flags & CLF_CURSOR_LARGE)? 100 : 10);

        SetConsoleCursorInfo(win->hcScreen, &curInfo);
        win->cmdline.flags ^= CLF_CURSOR_LARGE;
    }

#ifdef UNICODE
    line[0].Char.UnicodeChar = L'>';
#else
    line[0].Char.AsciiChar = '>';
#endif
    line[0].Attributes = CMDLINE_ATTRIB;
    for(i = 0, ch = line + 1; i < LINELEN - 1; ++i, ch++)
    {
#ifdef UNICODE
        if(i < strlen(text))
            mbtowc(&wch, &text[i], MB_CUR_MAX);
        else
            wch = L' ';
        ch->Char.UnicodeChar = wch;
#else
        ch->Char.AsciiChar = (i < strlen(text)? text[i] : ' ');
#endif
        // Gray color.
        ch->Attributes = CMDLINE_ATTRIB;
    }

    rect.Left = 0;
    rect.Right = LINELEN - 1;
    rect.Top = win->cbInfo.dwSize.Y - 1;
    rect.Bottom = win->cbInfo.dwSize.Y - 1;
    WriteConsoleOutput(win->hcScreen, line, linesize, from, &rect);
    setCmdLineCursor(win, cursorPos, win->cbInfo.dwSize.Y - 1);
}

void Sys_SetConWindowCmdLine(uint idx, const char* text, uint cursorPos, int flags)
{
    consolewindow_t* win = Window_Console(Window_ByIndex(idx));

    if(!win) return;
    setConWindowCmdLine(win, text, cursorPos, flags);
}

void ConsoleWindow_SetTitle(const Window *window, const char* title)
{
    const consolewindow_t* win = Window_ConsoleConst(window);
    if(win)
    {
        SetWindowText(win->hWnd, WIN_STRING(title));
    }
}

static void Sys_ConInputShutdown(void)
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

static void consoleShutdown()
{
    // We no longer need the input handler.
    Sys_ConInputShutdown();

    // Detach the console for this process.
    FreeConsole();
}

void Sys_ConShutdown(Window *window)
{
    if(window == Window_Main())
    {
        consoleShutdown();
    }
}


Window* Sys_ConInit(const char* title)
{
    consolewindow_t* win;
    //uint i;
    boolean ok = true;
    HANDLE hcScreen;

#if 0
    // We only support one dedicated console.
    for(i = 0; i < numWindows; ++i)
    {
        ddwindow_t* other = windows[i];

        if(other && other->type == WT_CONSOLE)
        {
            Con_Message("createWindow: maxConsoles limit reached.\n");
            return NULL;
        }
    }

    if(parentIdx)
    {
        pWin = getWindow(parentIdx - 1);
        if(pWin) phWnd = pWin->hWnd;
    }

    win = (ddwindow_t*) calloc(1, sizeof *win);
    if(!win) return NULL;
#endif

    if(!AllocConsole())
    {
        Con_Error("createWindow: Couldn't allocate a console! error %i\n", GetLastError());
    }

    win = Window_Console(Window_Main());
    win->hWnd = GetConsoleWindow();

    if(win->hWnd)
    {
        ConsoleWindow_SetTitle(Window_Main(), title);

        hcScreen = GetStdHandle(STD_OUTPUT_HANDLE);
        if(hcScreen == INVALID_HANDLE_VALUE)
            Con_Error("createWindow: Bad output handle\n");

        win->hcScreen = hcScreen;
        GetConsoleScreenBufferInfo(hcScreen, &win->cbInfo);

        // This is the location of the print cursor.
        win->cx = 0;
        win->cy = win->cbInfo.dwSize.Y - 2;

        setConWindowCmdLine(win, "", 1, 0);

        // We'll be needing the console input handler.
        Sys_ConInputInit();
    }
    else
    {
        win->hWnd = NULL;
        ok = false;
        consoleShutdown(win);
        return 0;
    }

    return Window_Main();

    /*
    setDDWindow(win, origin->x, origin->y, size->width, size->height, bpp, flags,
                DDSW_NOVISIBLE | DDSW_NOCENTER | DDSW_NOFULLSCREEN);

    // Ensure new windows are hidden on creation.
    ShowWindow(win->hWnd, SW_HIDE);
    if(!ok)
    {   // Damn, something went wrong... clean up.
        destroyWindow(win);
        return NULL;
    }

    return win;
*/
}

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
    keymap[VK_CAPITAL] = DDKEY_CAPSLOCK;
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
    keymap[VK_SNAPSHOT] = DDKEY_PRINT;

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

static void Sys_ConInputInit(void)
{
    if(conInputInited)
        return; // Already active.

    /**
     * For now, always load the U.S. English layout.
     *
     * \todo Is this even necessary with virtualkeys?
     */
    LoadKeyboardLayout(TEXT("00000409"), KLF_SUBSTITUTE_OK);

    // We'll be needing the VKey to DDKey translation table.
    initVKeyToDDKeyTlat();

    // And the down key array;
    vKeyDown = M_Calloc(sizeof(byte) * 256);

    hcInput = GetStdHandle(STD_INPUT_HANDLE);
    if(hcInput == INVALID_HANDLE_VALUE)
        Con_Error("Sys_ConInit: Bad input handle\n");

    conInputInited = true;
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
                evbuf[n].type = (key->bKeyDown? IKE_DOWN : IKE_UP);

                // Use the table to translate the vKey to a DDKEY.
                evbuf[n].ddkey = vKeyToDDKey(key->wVirtualKeyCode);
                evbuf[n].native = 0;

                memset(evbuf[n].text, 0, sizeof(evbuf[n].text));
#ifdef UNICODE
                wctomb(evbuf[n].text, key->uChar.UnicodeChar);
#else
                evbuf[n].text[0] = key->uChar.AsciiChar;
#endif
/*
                if(evbuf[n].ddkey >= ' ' && evbuf[n].ddkey < 128)
                {
                    // Printable ASCII.
                    evbuf[n].text[0] = evbuf[n].ddkey;
                }
                /// @todo Shift? Other modifiers?*/

                // Record the new state of this vKey.
                vKeyDown[key->wVirtualKeyCode] = (key->bKeyDown? true : false);
                n++;
            }
        }
    }

    return n;
}
