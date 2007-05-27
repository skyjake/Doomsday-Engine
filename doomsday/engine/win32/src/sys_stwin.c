/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * sys_stwin.c: Startup Window
 *
 * Startup message and progress bar window.
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "de_base.h"
#include "de_misc.h"
#include "de_console.h"
#include "sys_stwin.h"
#include "resource.h"

// MACROS ------------------------------------------------------------------

#define CREF_BACKGROUND     0
#define CREF_PROGRESS       0xc08080
#define CREF_TEXT           0xffc0c0

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HINSTANCE hInstApp;
extern HWND hWndMain;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HWND msgWnd = NULL;
static HBRUSH progressBrush, bgBrush;
static int barPos, barMax;

// CODE --------------------------------------------------------------------

BOOL CALLBACK SW_DialogProc(HWND dlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    char        buf[300];
    static int cleared = false;
    HWND        ed;

    switch(uMsg)
    {
    case WM_CTLCOLORSTATIC:
        // Set the background and text color of the messages edit box.
        ed = GetDlgItem(dlg, IDC_MESSAGES);
        if((HWND) lParam != ed)
            return FALSE;

        SetBkColor((HDC) wParam, CREF_BACKGROUND);
        SetTextColor((HDC) wParam, CREF_TEXT);
        // The first time text appears, clear the whole box.
        if(!cleared && GetWindowTextLength(ed))
        {
            RECT    rect;

            cleared = true;
            GetClientRect(ed, &rect);
            FillRect((HDC) wParam, &rect, bgBrush);
        }
        return TRUE;

    case WM_INITDIALOG:
        // Get the title from the main window.
        GetWindowText(hWndMain, buf, 300);
        SetWindowText(dlg, buf);
        return TRUE;
    }
    // The message was not processed.
    return FALSE;
}

/**
 * Replaces all \n with \r\n.
 */
void SW_ReplaceNewlines(const char *in, char *out)
{
    for(; *in; in++)
    {
        if(*in == '\n')
        {
            *out++ = '\r';
            *out++ = '\n';
        }
        else
        {
            *out++ = *in;
        }
    }
    *out = 0;
}

void SW_Printf(const char *format, ...)
{
    static int  printedChars = 0;
    char        buf[16384], rep[16384];
    va_list     args;
    boolean     clearBox = false;

    if(!msgWnd)
        return;

    va_start(args, format);
    printedChars += vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if(printedChars > 32768)
    {
        // Windows hack: too much stuff printed, clear the text box and
        // start over.
        clearBox = true;
        printedChars = 0;
    }

    SW_ReplaceNewlines(buf, rep);
    SendDlgItemMessage(msgWnd, IDC_MESSAGES,
        clearBox? WM_SETTEXT : EM_REPLACESEL, 0, (LPARAM) rep);
}

int SW_IsActive(void)
{
    return msgWnd != NULL;
}

void SW_Init(void)
{
    if(msgWnd || ArgCheck("-nostwin"))
        return;                 // Already initialized.

    msgWnd =
        CreateDialog(hInstApp, MAKEINTRESOURCE(IDD_STARTUP_WINDOW), hWndMain,
                     SW_DialogProc);
    progressBrush = CreateSolidBrush(CREF_PROGRESS);
    bgBrush = CreateSolidBrush(CREF_BACKGROUND);
    Con_Message("SW_Init: Startup message window opened.\n");
}

void SW_Shutdown(void)
{
    if(!msgWnd)
        return;                 // Not initialized.
    DestroyWindow(msgWnd);
    DeleteObject(progressBrush);
    DeleteObject(bgBrush);
    msgWnd = NULL;
    progressBrush = NULL;
    bgBrush = NULL;
}

void SW_DrawBar(void)
{
    HWND        prog;
    HDC         dc;
    RECT        rect;

    if(!msgWnd || !barMax)
        return;

    prog = GetDlgItem(msgWnd, IDC_PROGRESS);
    dc = GetDC(prog);
    GetClientRect(prog, &rect);
    rect.right = (rect.right - rect.left) * barPos / barMax;
    FillRect(dc, &rect, progressBrush);
    ReleaseDC(prog, dc);
}

void SW_SetBarPos(int pos)
{
    barPos = pos;
    SW_DrawBar();
}

void SW_SetBarMax(int max)
{
    HWND        prog;

    if(!msgWnd)
        return;
    prog = GetDlgItem(msgWnd, IDC_PROGRESS);
    InvalidateRect(prog, NULL, TRUE);
    UpdateWindow(prog);
    barMax = max;
}
