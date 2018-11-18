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
    binder.init(ScriptSystem::get()["App"])
            << DENG2_FUNC(App_Download, "download", "packageId");

    binder.init(ScriptSystem::get()["FS"])
            << DENG2_FUNC_NOARG(FS_RefreshPackageFolders, "refreshPackageFolders");
}
