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
 * sys_console.c: Text-Mode Console
 *
 * Standalone console window handling.  Used in dedicated mode.
 * FIXME: Some lazy programming in here...
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_console.h"

#include <curses.h>

// MACROS ------------------------------------------------------------------

#define LINELEN 256		// Lazy: This is the max acceptable window width.

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static WINDOW *winTitle, *winText, *winCommand;

static int cx, cy;
static int needNewLine = false;

// CODE --------------------------------------------------------------------

void Sys_ConUpdateTitle(void)
{
	char title[256];

	DD_MainWindowTitle(title);

	// The background will also be in reverse.
	wbkgdset(winTitle, ' ' | A_REVERSE);

	// First clear the whole line.
	wmove(winTitle, 0, 0);
	wclrtoeol(winTitle);

	// Center the title.
	wmove(winTitle, 0, getmaxx(winTitle)/2 - strlen(title)/2);
	waddstr(winTitle, title);
	wrefresh(winTitle);
}

void Sys_ConInit(void)
{
	int maxPos[2];
	
	// Initialize curses.
	initscr();
	cbreak();
	noecho();
	nonl();

	// The current size of the screen.
	getmaxyx(stdscr, maxPos[VY], maxPos[VX]);
	
	// Create the three windows we will be using.
	winTitle = newwin(1, maxPos[VX], 0, 0);
	winText = newwin(maxPos[VY] - 2, maxPos[VX], 1, 0);
	winCommand = newwin(1, maxPos[VX], maxPos[VY] - 1, 0);

	// Set attributes.
	wattrset(winTitle, A_REVERSE);
	wattrset(winText, A_NORMAL);
	wattrset(winCommand, A_BOLD);

	Sys_ConUpdateTitle();

	scrollok(winText, TRUE);
	wclear(winText);
	wrefresh(winText);

	keypad(winCommand, TRUE);
	nodelay(winCommand, TRUE);
	Sys_ConUpdateCmdLine("");	
}

void Sys_ConShutdown(void)
{
	// Delete windows and shut down curses.
	delwin(winTitle);
	delwin(winText);
	delwin(winCommand);
	endwin();

	winTitle = winText = winCommand = NULL;
}

int Sys_ConTranslateKey(int key)
{
	if(key >= 32 && key <= 127) return key;

	//fprintf(outFile, "key=%i\n", key);
	
	switch(key)
	{
	case '\r':
	case '\n':
	case KEY_ENTER:
		return DDKEY_ENTER;

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

void Sys_ConPostEvents(void)
{
	event_t ev;
	int key;

	// Get all keys that are waiting.
	for(key = wgetch(winCommand); key != ERR; key = wgetch(winCommand))
	{
/*		if(key == KEY_RESIZE)
		{
			int maxPos[2];

			// The current size of the screen.
			getmaxyx(stdscr, maxPos[VY], maxPos[VX]);

			// Resize the windows.
			wresize(winTitle, 1, maxPos[VX]);
			wresize(winText, maxPos[VY] - 2, maxPos[VX]);
			wresize(winCommand, 1, maxPos[VX]);

			mvwin(winCommand, maxPos[VY] - 1, 0);
			
			// Let's redraw everything.
			Sys_ConUpdateTitle();
			wrefresh(winText);
			Sys_ConUpdateCmdLine(NULL);
			continue;
			}*/
		
		ev.type = ev_keydown;
		ev.data1 = Sys_ConTranslateKey(key);
		DD_PostEvent(&ev);

		// Release immediately.
		ev.type = ev_keyup;
		DD_PostEvent(&ev);
	}
}

void Sys_ConScrollLine(void)
{
	scroll(winText);
}

void Sys_ConSetAttrib(int flags)
{
	if(flags & (CBLF_YELLOW | CBLF_LIGHT))
		wattrset(winText, A_BOLD);
	else
		wattrset(winText, A_NORMAL);
}

// Writes the text in winText at (cx,cy).
void Sys_ConWriteText(const char *line, int len)
{
	wmove(winText, cy, cx);
	waddnstr(winText, line, len);
	wclrtoeol(winText);
}

int Sys_ConGetScreenSize(int axis)
{
	int x, y;

	getmaxyx(winText, y, x);
	return axis == VX? x : y;
}

void Sys_ConPrint(int clflags, char *text)
{
	char line[LINELEN];
	int count = strlen(text), lineStart, bPos;
	char *ptr = text, ch;
	int maxPos[2];

	// Determine the size of the text window.
	getmaxyx(winText, maxPos[VY], maxPos[VX]);

	if(needNewLine)
	{
		// Need to make some room.
		cx = 0;
		cy++;
		if(cy >= maxPos[VY])
		{
			cy--;
			Sys_ConScrollLine();
		}
		needNewLine = false;
	}
	bPos = lineStart = cx;

	Sys_ConSetAttrib(clflags);

	for(; count > 0; count--, ptr++)
	{
		ch = *ptr;
		// Ignore carriage returns.
		if(ch == '\r') continue;
		if(ch != '\n' && bPos < maxPos[VX])
		{
			line[bPos] = ch;
			bPos++;
		}
		// Time for newline?
		if(ch == '\n' || bPos >= maxPos[VX])
		{
			Sys_ConWriteText(line + lineStart, bPos - lineStart);
			cx += bPos - lineStart;
			bPos = 0;
			lineStart = 0;
			if(count > 1)	// Not the last character?
			{
				needNewLine = false;
				cx = 0;
				cy++;
				if(cy == maxPos[VY])
				{
					Sys_ConScrollLine();
					cy--;
				}
			}
			else needNewLine = true;
		}
	}
	
	// Something in the buffer?
	if(bPos - lineStart)
	{
		Sys_ConWriteText(line + lineStart, bPos - lineStart);
		cx += bPos - lineStart;
	}

	wrefresh(winText);

	// Move the cursor back onto the command line.
	Sys_ConUpdateCmdLine(NULL);
}

void Sys_ConUpdateCmdLine(char *text)
{
	char line[LINELEN], *ch;
	unsigned int i, numCols;

	if(text == NULL)
	{
		int y, x;
		// Just move the cursor into the command line window.
		getyx(winCommand, y, x);
		wmove(winCommand, y, x);
	}
	else
	{
		memset(line, 0, sizeof(line));
		line[0] = '>';
		for(i = 0, ch = line + 1; i < LINELEN - 1; i++, ch++)
		{
			if(i < strlen(text))
				*ch = text[i];
			else
				*ch = 0;
		}
		wmove(winCommand, 0, 0);
		// Can't print longer than the window.
		waddnstr(winCommand, line, numCols - 1);
		wclrtoeol(winCommand);
	}
	wrefresh(winCommand);
}	
