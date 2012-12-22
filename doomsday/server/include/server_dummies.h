/** @file server_dummies.h Dummy functions for the server.
 * @ingroup server
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef SERVER_DUMMIES_H
#define SERVER_DUMMIES_H

#include <de/libdeng.h>
#include "map/sector.h"

#ifndef __SERVER__
#  error Attempted to include server's header in a non-server build
#endif

DENG_EXTERN_C void Con_TransitionRegister();
DENG_EXTERN_C void Con_TransitionTicker(timespan_t t);

DENG_EXTERN_C void GL_Shutdown();
DENG_EXTERN_C void GL_EarlyInitTextureManager();
DENG_EXTERN_C void GL_PruneTextureVariantSpecifications();
DENG_EXTERN_C void GL_SetFilter(int f);

DENG_EXTERN_C void R_InitViewWindow(void);

DENG_EXTERN_C void FR_Init(void);

DENG_EXTERN_C void Rend_DecorInit();
DENG_EXTERN_C void Rend_ConsoleInit();
DENG_EXTERN_C void Rend_ConsoleResize(int force);
DENG_EXTERN_C void Rend_ConsoleOpen(int yes);
DENG_EXTERN_C void Rend_ConsoleToggleFullscreen();
DENG_EXTERN_C void Rend_ConsoleMove(int y);
DENG_EXTERN_C void Rend_ConsoleCursorResetBlink();

DENG_EXTERN_C void Models_Init();
DENG_EXTERN_C void Models_Shutdown();

DENG_EXTERN_C void LG_SectorChanged(Sector* sector);

DENG_EXTERN_C void Cl_InitPlayers(void);

DENG_EXTERN_C void UI_Init();
DENG_EXTERN_C void UI_Ticker(timespan_t t);
DENG_EXTERN_C void UI2_Ticker(timespan_t t);
DENG_EXTERN_C void UI_Shutdown();

#endif // SERVER_DUMMIES_H
