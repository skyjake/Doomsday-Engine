/** @file doomsdayapp_bindings.cpp  Script bindings for libdoomsday.
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/DoomsdayApp"
#include "doomsday/world/mobj.h"
#include "doomsday/world/thinkerdata.h"

#include <de/Context>
#include <de/Folder>
#include <de/ScriptSystem>

using namespace de;

static Value *Function_App_Download(Context &, Function::ArgumentValues const &args)
{
    String const packageId = args.first()->asText();
    DoomsdayApp::packageDownloader().download(StringList({ packageId }), [packageId] ()
    {
        LOG().beginInteractive();

        LOG_SCR_MSG("Package \"%s\" downloaded.") << packageId;

        LOG().endInteractive();
    });
    return nullptr;
}

//-------------------------------------------------------------------------------------------------

static int playerIndex(const Context &ctx)
{
    return ctx.selfInstance().geti("__id__", 0);
}

static Value *Function_Player_Id(Context &ctx, const Function::ArgumentValues &)
{
    return new NumberValue(playerIndex(ctx));
}

static Value *Function_Player_Thing(Context &ctx, const Function::ArgumentValues &)
{
    const int plrNum = playerIndex(ctx);
    if (const mobj_t *mo = DoomsdayApp::players().at(plrNum).publicData().mo)
    {
        return new RecordValue(THINKER_NS(mo->thinker));
    }
    return nullptr;
}

//-------------------------------------------------------------------------------------------------

static Value *Function_FS_RefreshPackageFolders(Context &, const Function::ArgumentValues &)
{
    LOG_SCR_MSG("Initializing package folders...");
    Folder::afterPopulation([]() {
        DoomsdayApp::app().initWadFolders();
        DoomsdayApp::app().initPackageFolders();
    });
    return nullptr;
}

void DoomsdayApp::initBindings(Binder &binder)
{
    auto &scr       = ScriptSystem::get();
    auto &appModule = scr["App"];

    binder.init(appModule)
        << DENG2_FUNC(App_Download, "download", "packageId");

    // Value constants for App.getInteger() and App.setInteger().
    {
        appModule.addNumber("NO_VIDEO", DD_NOVIDEO).setReadOnly();
        appModule.addNumber("NETGAME", DD_NETGAME).setReadOnly();
        appModule.addNumber("SERVER", DD_SERVER).setReadOnly();
        appModule.addNumber("CLIENT", DD_CLIENT).setReadOnly();
        appModule.addNumber("CONSOLE_PLAYER", DD_CONSOLEPLAYER).setReadOnly();
        appModule.addNumber("DISPLAY_PLAYER", DD_DISPLAYPLAYER).setReadOnly();
        appModule.addNumber("GOT_FRAME", DD_GOTFRAME).setReadOnly();
        appModule.addNumber("NUM_SOUNDS", DD_NUMSOUNDS).setReadOnly();
        appModule.addNumber("SERVER_ALLOW_FRAMES", DD_SERVER_ALLOW_FRAMES).setReadOnly();
        appModule.addNumber("RENDER_FULLBRIGHT", DD_RENDER_FULLBRIGHT).setReadOnly();
        appModule.addNumber("GAME_READY", DD_GAME_READY).setReadOnly();
        appModule.addNumber("CLIENT_PAUSED", DD_CLIENT_PAUSED).setReadOnly();
        appModule.addNumber("WEAPON_OFFSET_SCALE_Y", DD_WEAPON_OFFSET_SCALE_Y).setReadOnly();
        appModule.addNumber("GAME_DRAW_HUD_HINT", DD_GAME_DRAW_HUD_HINT).setReadOnly();
        appModule.addNumber("FIXEDCOLORMAP_ATTENUATE", DD_FIXEDCOLORMAP_ATTENUATE).setReadOnly();
    }

    // Player
    {
        Record &player = appModule.addSubrecord("Player");
        binder.init(player)
            << DENG2_FUNC_NOARG(Player_Id, "id")
            << DENG2_FUNC_NOARG(Player_Thing, "thing");
    }

    binder.init(scr["FS"])
        << DENG2_FUNC_NOARG(FS_RefreshPackageFolders, "refreshPackageFolders");
}
