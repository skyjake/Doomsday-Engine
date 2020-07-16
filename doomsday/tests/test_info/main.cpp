/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/textapp.h>
#include <de/logbuffer.h>
#include <de/dscript.h>
#include <de/escapeparser.h>
#include <de/filesystem.h>

using namespace de;

int main(int argc, char **argv)
{
    init_Foundation();
    try
    {
        TextApp app(makeList(argc, argv));
        app.initSubsystems(App::DisablePersistentData);

        ScriptedInfo dei;
        dei.parse(app.fileSystem().find("test_info.dei"));
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
