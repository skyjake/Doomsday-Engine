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

#include <de/CommandLine>
#include <de/DirectoryFeed>
#include <de/EscapeParser>
#include <de/FS>
#include <de/LogBuffer>
#include <de/NativeFile>
#include <de/Process>
#include <de/Script>
#include <de/TextApp>

using namespace de;

int main(int argc, char **argv)
{
    init_Foundation();
    if (argc < 2) return -1;
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
        NativeFile inputFile(inputFn.fileName(), inputFn);
        inputFile.setStatus(DirectoryFeed::fileStatus(inputFn));

        Script testScript(inputFile);
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

    debug("Exiting main()...");
    return 0;
}
