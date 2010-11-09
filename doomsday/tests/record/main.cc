/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "../testapp.h"

#include <iostream>
#include <sstream>

using namespace de;

int deng_Main(int argc, char** argv)
{
    try
    {
        TestApp app(CommandLine(argc, argv));
        
        Record rec;
        
        LOG_MESSAGE("Empty record:\n") << rec;
        
        rec.add(new Variable("hello", new TextValue("World!")));
        LOG_MESSAGE("With one variable:\n") << rec;
        
        rec.add(new Variable("size", new NumberValue(1024)));
        LOG_MESSAGE("With two variables:\n") << rec;
        
        Record rec2;
        Block b;
        Writer(b) << rec;
        LOG_MESSAGE("Serialized record to ") << b.size() << " bytes.";
        
        std::ostringstream str;
        for(duint i = 0; i < b.size(); ++i)
        {
            str << dint(b.data()[i]) << " ";
        }
        LOG_MESSAGE("") << str.str();
        
        Reader(b) >> rec2;        
        LOG_MESSAGE("After being (de)serialized:\n") << rec2;
    }
    catch(const Error& err)
    {
        std::cerr << err.asText() << "\n";
    }

    std::cout << "Exiting deng_Main()...\n";
    return 0;        
}
