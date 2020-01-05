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

static Value *Function_App_Download(Context &, const Function::ArgumentValues &args)
{
    const String packageId = args.first()->asText();
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
            << DE_FUNC(App_Download, "download", "packageId");

    // Player
    {
        Record &player = appModule.addSubrecord("Player");
        binder.init(player)
            << DE_FUNC_NOARG(Player_Id,     "id")
            << DE_FUNC_NOARG(Player_Thing,  "thing");
    }

    binder.init(scr["FS"])
        << DE_FUNC_NOARG(FS_RefreshPackageFolders, "refreshPackageFolders");
}
