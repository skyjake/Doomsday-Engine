/** @file server_dummies.c Dummy functions for the server.
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

#include "server_dummies.h"

void Con_TransitionRegister()
{}

void Con_TransitionTicker(timespan_t t)
{
    DENG_UNUSED(t);
}

void GL_Shutdown()
{}

void GL_EarlyInitTextureManager()
{}

void GL_PruneTextureVariantSpecifications()
{}

void GL_SetFilter(int f)
{
    DENG_UNUSED(f);
}

void FR_Init(void)
{}

void R_InitViewWindow(void)
{}

void Rend_DecorInit()
{}

void Rend_ConsoleInit()
{}

void Rend_ConsoleOpen(int yes)
{
    DENG_UNUSED(yes);
}

void Rend_ConsoleMove(int y)
{
    DENG_UNUSED(y);
}

void Rend_ConsoleResize(int force)
{
    DENG_UNUSED(force);
}

void Rend_ConsoleToggleFullscreen()
{}

void Rend_ConsoleCursorResetBlink()
{}

void Models_Init()
{}

void Models_Shutdown()
{}

void LG_SectorChanged(Sector* sector)
{
    DENG_UNUSED(sector);
}

void Cl_InitPlayers(void)
{}

void UI_Ticker(timespan_t t)
{
    DENG_UNUSED(t);
}

void UI2_Ticker(timespan_t t)
{
    DENG_UNUSED(t);
}

void UI_Init()
{}

void UI_Shutdown()
{}
