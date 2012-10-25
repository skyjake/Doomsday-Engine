/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/App>
#include <de/LogBuffer>
#include <de/Script>
#include <de/FS>
#include <de/Process>
#include <QDebug>

using namespace de;

int main(int argc, char** argv)
{
    try
    {
        App app(argc, argv, App::GUIDisabled);
        app.initSubsystems(App::DisablePlugins);

        Script testScript(app.fileSystem().find("kitchen_sink.de"));
        Process proc(testScript);
        LOG_MSG("Script parsing is complete! Executing...");
        LOG_MSG("------------------------------------------------------------------------------");

        proc.execute();

        LOG_MSG("------------------------------------------------------------------------------");
        LOG_MSG("Final result value is: ") << proc.context().evaluator().result().asText();
    }
    catch(const Error& err)
    {
        qWarning() << err.asText();
    }

    qDebug("Exiting main()...");
    return 0;        
}
