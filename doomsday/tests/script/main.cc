/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <dengmain.h>
#include <de/data.h>
#include <de/types.h>
#include <de/fs.h>
#include <de/script.h>
#include "../testapp.h"

#include <iostream>

using namespace de;

int deng_Main(int argc, char** argv)
{
    try
    {
        CommandLine args(argc, argv);
        TestApp app(args);

        Script testScript(app.fileSystem().findSingle("kitchen_sink.de"));
        Process proc(testScript);
        LOG_MESSAGE("Script parsing is complete! Executing...");
        LOG_MESSAGE("------------------------------------------------------------------------------");

        proc.execute();
        
        LOG_MESSAGE("------------------------------------------------------------------------------");
        LOG_MESSAGE("Final result value is: ") << proc.context().evaluator().result().asText();
    }
    catch(const Error& err)
    {
        std::cerr << err.asText() << "\n";
    }

    std::cout << "Exiting deng_Main()...\n";
    return 0;        
}
