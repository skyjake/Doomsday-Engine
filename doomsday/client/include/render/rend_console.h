/**\file rend_console.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * Console Rendering.
 */

#ifndef LIBDENG_CONSOLE_RENDER_H
#define LIBDENG_CONSOLE_RENDER_H

#include <de/point.h>

#ifdef __cplusplus
extern "C" {
#endif

extern byte consoleShowFPS;

void Rend_ConsoleRegister(void);

/**
 * Initialize the console renderer (allows re-init).
 */
void Rend_ConsoleInit(void);

void Rend_ConsoleTicker(timespan_t time);

/**
 * Try to fulfill any pending console resize request.
 * To be called after a resolution change to resize the console.
 *
 * @param force  @c true= Force a new resize request.
 * @return  @c true= A resize is still pending.
 */
boolean Rend_ConsoleResize(boolean force);

void Rend_ConsoleOpen(int yes);
void Rend_ConsoleMove(int numLines);
void Rend_ConsoleToggleFullscreen(void);
void Rend_ConsoleCursorResetBlink(void);
void Rend_ConsoleUpdateTitle(void);

void Rend_Console(void);

/// @param origin  Origin of the display (top right) in screen-space coordinates.
void Rend_ConsoleFPS(const Point2Raw* origin);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_CONSOLE_RENDER_H */
