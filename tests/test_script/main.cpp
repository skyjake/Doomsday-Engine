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
#include <de/filesystem.h>
#include <de/scripting/script.h>
#include <de/scripting/process.h>
#include <de/escapeparser.h>

#include <iostream>

using namespace de;

int main(int argc, char **argv)
{
    init_Foundation();
    using namespace std;
    try
    {
        TextApp app(makeList(argc, argv));
        app.initSubsystems();
        cout << FS::locate<const Folder>("/data").correspondingNativePath().toString() << endl;

#if 1
        Script testScript(app.fileSystem().find("kitchen_sink.ds"));
#endif
#if 0
        Script testScript("def returnValue(a): return a\n"
                          "returnValue(True) and returnValue(True)\n");
#endif
#if 0
        Script testScript("print 'Dictionary:', {'a':'A', 'b':'B'} - 'a'\n");
#endif
        Process proc(testScript);
        LOG_MSG("Script parsing is complete! Executing...");
        LOG_MSG("------------------------------------------------------------------------------");

        proc.execute();

        LOG_MSG("------------------------------------------------------------------------------");
        LOG_MSG("Final result value is: ") << proc.context().evaluator().result().asText();
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
