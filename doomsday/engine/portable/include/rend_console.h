/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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
 * rend_console.h: Console Rendering.
 */

#ifndef __DOOMSDAY_CONSOLE_RENDER_H__
#define __DOOMSDAY_CONSOLE_RENDER_H__

extern byte consoleShowFPS, consoleShadowText;

void    Rend_ConsoleRegister(void);

void    Rend_ConsoleInit(void);
void    Rend_ConsoleTicker(timespan_t time);
void    Rend_Console(void);

void    Rend_ConsoleFPS(int x, int y);
void    Rend_ConsoleOpen(int yes);
void    Rend_ConsoleMove(int numLines);
void    Con_DrawRuler(int y, int lineHeight, float alpha);
void    Con_InitUI(void);

void    Rend_ConsoleToggleFullscreen(void);
void    Rend_ConsoleCursorResetBlink(void);

#endif
