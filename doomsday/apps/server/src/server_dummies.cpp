/** @file server_dummies.c Dummy functions for the server.
 * @ingroup server
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "Texture"

void BusyMode_FreezeGameForBusyMode()
{}

DENG_DECLARE_API(Busy) =
{
    { DE_API_BUSY },
    BusyMode_FreezeGameForBusyMode
};

void Con_TransitionTicker(timespan_t)
{}

void Con_SetProgress(int)
{}

void GL_Shutdown()
{}

void R_RenderPlayerView(int)
{}

void R_SetBorderGfx(Uri const *const *)
{}

void R_SkyParams(int, int, void *)
{}

void R_InitSvgs()
{}

void R_ShutdownSvgs()
{}

struct font_s *R_CreateFontFromDef(ded_compositefont_t *)
{
    return nullptr;
}

void Rend_CacheForMobjType(int)
{}

void Rend_ConsoleInit()
{}

void Rend_ConsoleOpen(int)
{}

void Rend_ConsoleMove(int)
{}

void Rend_ConsoleResize(int)
{}

void Rend_ConsoleToggleFullscreen()
{}

void Rend_ConsoleCursorResetBlink()
{}

void Cl_InitPlayers()
{}

void UI_Ticker(timespan_t)
{}

void Sys_MessageBox(messageboxtype_t, char const *, char const *, char const *)
{}

void Sys_MessageBox2(messageboxtype_t, char const *, char const *, char const *, char const *)
{}

void Sys_MessageBoxf(messageboxtype_t, char const *, char const *, ...)
{}

int Sys_MessageBoxWithButtons(messageboxtype_t, char const *, char const *, char const *, char const **)
{
    return 0;
}

void Sys_MessageBoxWithDetailsFromFile(messageboxtype_t, char const *, char const *,
    char const *, char const *)
{}

DENG_EXTERN_C void R_ProjectSprite(struct mobj_s *)
{}

dd_bool S_StartMusicNum(int, dd_bool)
{
    // We don't play music locally on server side.
    return false;
}

void S_StopMusic()
{
    // We don't play music locally on server side.
}

void S_PauseMusic(dd_bool)
{
    // We don't play music locally on server side.
}

void S_LocalSoundAtVolumeFrom(int, struct mobj_s *, coord_t *, float)
{
    // We don't play sounds locally on server side.
}

void S_LocalSoundAtVolume(int, struct mobj_s *, float)
{
    // We don't play sounds locally on server side.
}

void S_LocalSound(int, struct mobj_s *)
{
    // We don't play sounds locally on server side.
}

void S_LocalSoundFrom(int, coord_t *)
{
    // We don't play sounds locally on server side.
}
