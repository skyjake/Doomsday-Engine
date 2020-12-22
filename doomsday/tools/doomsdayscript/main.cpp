/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <de/commandline.h>
#include <de/directoryfeed.h>
#include <de/escapeparser.h>
#include <de/filesystem.h>
#include <de/logbuffer.h>
#include <de/nativefile.h>
#include <de/dscript.h>
#include <de/textapp.h>

using namespace de;

int main(int argc, char **argv)
{
    if (argc < 2) return -1;
    init_Foundation();
    try
    {
        TextApp app(makeList(argc, argv));
        {
            Record &amd = app.metadata();
            amd.set(App::APP_NAME, "Doomsday Script");
            amd.set(App::CONFIG_PATH, "");
        }
        LogBuffer::get().enableStandardOutput();
        app.initSubsystems(App::DisablePersistentData);

        app.commandLine().makeAbsolutePath(1);
        NativePath inputFn = app.commandLine().at(1);

        // Allow access to all files in the same folder in case the script does imports of
        // files from the same directory.
        FS::get().makeFolderWithFeed(
            "/src", new DirectoryFeed(inputFn.fileNamePath(), DirectoryFeed::OnlyThisFolder));
        FS::waitForIdle();

        Script testScript(FS::locate<const File>("/src" / inputFn.fileName()));
        Process proc(testScript);
        LOG_MSG("Script parsing is complete! Executing...");
        LOG_MSG("------------------------------------------------------------------------------");

        proc.execute();

        LOG_MSG("------------------------------------------------------------------------------");
        LOG_MSG("Final result value is: ") << proc.context().evaluator().result().asText();
    }
    catch (const Error &er)
    {
        er.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
