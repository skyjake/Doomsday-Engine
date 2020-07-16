/** @file server_dummies.c Dummy functions for the server.
 * @ingroup server
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "ui/nativeui.h"
#include <doomsday/res/texture.h>

void BusyMode_FreezeGameForBusyMode()
{}

DE_DECLARE_API(Busy) =
{
    { DE_API_BUSY },
    BusyMode_FreezeGameForBusyMode
};

void Con_TransitionTicker(timespan_t t)
{
    DE_UNUSED(t);
}

void Con_SetProgress(int progress)
{
    DE_UNUSED(progress);
}

void GL_Shutdown()
{}

void R_RenderPlayerView(int num)
{
    DE_UNUSED(num);
}

void R_SetBorderGfx(const Uri *const *paths)
{
    DE_UNUSED(paths);
}

void R_SkyParams(int layer, int param, void *data)
{
    DE_UNUSED(layer);
    DE_UNUSED(param);
    DE_UNUSED(data);
}

void R_ShutdownSvgs(void)
{}

struct font_s* R_CreateFontFromDef(ded_compositefont_t* def)
{
    DE_UNUSED(def);
    return 0;
}

void Rend_CacheForMobjType(int num)
{
    DE_UNUSED(num);
}

void Rend_ConsoleInit()
{}

void Rend_ConsoleOpen(int yes)
{
    DE_UNUSED(yes);
}

void Rend_ConsoleMove(int y)
{
    DE_UNUSED(y);
}

void Rend_ConsoleResize(int force)
{
    DE_UNUSED(force);
}

void Rend_ConsoleToggleFullscreen()
{}

void Rend_ConsoleCursorResetBlink()
{}

void Cl_InitPlayers(void)
{}

void UI_Ticker(timespan_t t)
{
    DE_UNUSED(t);
}

void Sys_MessageBox(messageboxtype_t type, const char* title, const char* msg, const char* detailedMsg)
{
    DE_UNUSED(type);
    DE_UNUSED(title);
    DE_UNUSED(msg);
    DE_UNUSED(detailedMsg);
}

void Sys_MessageBox2(messageboxtype_t /*type*/, const char * /*title*/, const char * /*msg*/, const char * /*informativeMsg*/, const char * /*detailedMsg*/)
{
}

void Sys_MessageBoxf(messageboxtype_t /*type*/, const char* /*title*/, const char* /*format*/, ...)
{
}

int Sys_MessageBoxWithButtons(messageboxtype_t /*type*/, const char* /*title*/, const char* /*msg*/,
                              const char* /*informativeMsg*/, const char** /*buttons*/)
{
    return 0;
}

void Sys_MessageBoxWithDetailsFromFile(messageboxtype_t /*type*/, const char* /*title*/, const char* /*msg*/,
                                       const char* /*informativeMsg*/, const char* /*detailsFileName*/)
{
}

DE_EXTERN_C void R_ProjectSprite(struct mobj_s *mo)
{
    DE_UNUSED(mo);
}
