/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006 Jaakko Keränen <skyjake@dengine.net>
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

/*
 * dd_uinit.h: Unix Initialization
 */

#ifndef __DOOMSDAY_UINIT_H__
#define __DOOMSDAY_UINIT_H__

#include "dd_pinit.h"

extern uint windowIDX;   // Main window.

uint            DD_CreateWindow(application_t *app, uint parent,
                                int x, int y, int w, int h, int bpp, int flags,
                                const char *title, int cmdShow);
void            DD_DestroyWindow(uint idx);
boolean         DD_GetWindowDimensions(uint idx, int *x, int *y, int *w, int *h);
boolean         DD_GetWindowBPP(uint idx);
boolean         DD_IsWindowFullscreen(uint idx);
boolean         DD_SetWindowVisibility(uint idx, boolean show);

void            DD_Shutdown(void);

#endif
