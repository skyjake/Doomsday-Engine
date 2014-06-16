/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/TextApp>
#include <de/LogBuffer>
#include <de/Script>
#include <de/NativeFile>
#include <de/FS>
#include <de/Process>
#include <de/DirectoryFeed>
#include <QDebug>

using namespace de;

int main(int argc, char **argv)
{
    if(argc < 2) return -1;
    try
    {
        TextApp app(argc, argv);
        app.setApplicationName("Doomsday Script");
        app.setConfigScript("");
        LogBuffer::appBuffer().enableStandardOutput();
        app.initSubsystems(App::DisablePlugins | App::DisablePersistentData);

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
    catch(Error const &err)
    {
        qWarning() << err.asText();
    }

    qDebug("Exiting main()...");
    return 0;        
}
