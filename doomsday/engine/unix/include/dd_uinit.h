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

// Doomsday window flags.
#define DDWF_VISIBLE            0x01
#define DDWF_FULLSCREEN         0x02
#define DDWF_CENTER             0x04

// Flags for DD_SetWindow()
#define DDSW_NOSIZE             0x01
#define DDSW_NOMOVE             0x02
#define DDSW_NOBPP              0x04
#define DDSW_NOFULLSCREEN       0x08
#define DDSW_NOVISIBLE          0x10
#define DDSW_NOCENTER           0x20
#define DDSW_NOCHANGES          (DDSW_NOSIZE & DDSW_NOMOVE & DDSW_NOBPP & \
                                 DDSW_NOFULLSCREEN & DDSW_NOVISIBLE & \
                                 DDSW_NOCENTER)

typedef struct {
    lt_dlhandle hGame;      // Instance handle to the game library.
    lt_dlhandle hPlugins[MAX_PLUGS]; // Instance handle to all other libs.
    GETGAMEAPI GetGameAPI;
} application_t;

extern uint windowIDX;   // Main window.
extern application_t app;

uint            DD_CreateWindow(application_t *app, uint parent,
                                int x, int y, int w, int h, int bpp, int flags,
                                const char *title, int cmdShow);
boolean         DD_DestroyWindow(uint idx);

boolean         DD_GetWindowDimensions(uint idx, int *x, int *y, int *w, int *h);
boolean         DD_GetWindowBPP(uint idx, int *bpp);
boolean         DD_GetWindowFullscreen(uint idx, boolean *fullscreen);
boolean         DD_GetWindowVisibility(uint idx, boolean *show);

boolean         DD_SetWindow(uint idx, int x, int y, int w, int h, int bpp,
                             uint wflags, uint uflags);

void            DD_Shutdown(void);

#endif
