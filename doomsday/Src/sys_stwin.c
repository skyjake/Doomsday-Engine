/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
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
#include "de_console.h"
#include "sys_stwin.h"
#include "resource.h"

// MACROS ------------------------------------------------------------------

#define CREF_BACKGROUND		0
#define CREF_PROGRESS		0xc08080
#define CREF_TEXT			0xffc0c0

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HINSTANCE	hInstApp;
extern HWND			hWndMain;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HWND			msgWnd = NULL;
static HBRUSH		progressBrush, bgBrush;
static int			barPos, barMax;

// CODE --------------------------------------------------------------------

//===========================================================================
// SW_DialogProc
//===========================================================================
BOOL CALLBACK SW_DialogProc
	(HWND dlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char buf[300];
	static int cleared = false;
	HWND ed;

	switch(uMsg)
	{
	case WM_CTLCOLORSTATIC:
		// Set the background and text color of the messages edit box.
		ed = GetDlgItem(dlg, IDC_MESSAGES);
		if( (HWND) lParam != ed) return FALSE;
		SetBkColor( (HDC) wParam, CREF_BACKGROUND);
		SetTextColor( (HDC) wParam, CREF_TEXT);
		// The first time text appears, clear the whole box.
		if(!cleared && GetWindowTextLength(ed))
		{
			RECT rect;
			cleared = true;
			GetClientRect(ed, &rect);
			FillRect( (HDC) wParam, &rect, bgBrush);
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

//===========================================================================
// SW_ReplaceNewlines
//	Replaces all \n with \r\n.
//===========================================================================
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

//===========================================================================
// SW_Printf
//===========================================================================
void SW_Printf(const char *format, ...)
{
	char buf[2048], rep[2048];
	va_list args;

	if(!msgWnd) return;

	va_start(args, format);
	vsprintf(buf, format, args);
	va_end(args);

	SW_ReplaceNewlines(buf, rep);
	SendDlgItemMessage(msgWnd, IDC_MESSAGES, EM_REPLACESEL, 0, (LPARAM) rep);
}

//===========================================================================
// SW_IsActive
//===========================================================================
int SW_IsActive(void)
{
	return msgWnd != NULL;
}

//===========================================================================
// SW_Init
//===========================================================================
void SW_Init(void)
{
	if(msgWnd) return; // Already initialized.

	msgWnd = CreateDialog(hInstApp, MAKEINTRESOURCE(IDD_STARTUP_WINDOW),
		hWndMain, SW_DialogProc);
	progressBrush = CreateSolidBrush(CREF_PROGRESS);
	bgBrush = CreateSolidBrush(CREF_BACKGROUND);
	Con_Message("SW_Init: Startup message window opened.\n");
}

//===========================================================================
// SW_Shutdown
//===========================================================================
void SW_Shutdown(void)
{
	if(!msgWnd) return;	// Not initialized.
	DestroyWindow(msgWnd);
	DeleteObject(progressBrush);
	DeleteObject(bgBrush);
	msgWnd = NULL;
	progressBrush = NULL;
	bgBrush = NULL;
}

//===========================================================================
// SW_DrawBar
//===========================================================================
void SW_DrawBar(void)
{
	HWND prog;
	HDC dc;
	RECT rect;

	if(!msgWnd || !barMax) return;

	prog = GetDlgItem(msgWnd, IDC_PROGRESS);
	dc = GetDC(prog);
	GetClientRect(prog, &rect);
	rect.right = (rect.right - rect.left)*barPos/barMax;
	FillRect(dc, &rect, progressBrush);
	ReleaseDC(prog, dc);
}

//===========================================================================
// SW_SetBarPos
//===========================================================================
void SW_SetBarPos(int pos)
{
	barPos = pos;
	SW_DrawBar();
}

//===========================================================================
// SW_SetBarMax
//===========================================================================
void SW_SetBarMax(int max)
{
	HWND prog;

	if(!msgWnd) return;
	prog = GetDlgItem(msgWnd, IDC_PROGRESS);
	InvalidateRect(prog, NULL, TRUE);
	UpdateWindow(prog);
	barMax = max;
}

