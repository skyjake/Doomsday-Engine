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
#include "../testapp.h"

using namespace de;

int deng_Main(int argc, char** argv)
{
    using std::cout;
    using std::endl;
    
    try
    {
        Record rec;
        
        std::cout << "Empty record:\n";
        cout << rec;
        
        rec.add(new Variable("hello", new TextValue("World!")));
        std::cout << "\nWith one variable:\n" << rec;
        
        rec.add(new Variable("size", new NumberValue(1024)));
        std::cout << "\nWith two variables:\n" << rec;
        
        Record rec2;
        Block b;
        Writer(b) << rec;
        std::cout << "Serialized record to " << b.size() << " bytes.\n";
        
        for(duint i = 0; i < b.size(); ++i)
        {
            std::cout << dint(b.data()[i]) << " ";
        }
        std::cout << "\n";
        
        Reader(b) >> rec2;        
        std::cout << "\nAfter being (de)serialized:\n" << rec2;
    }
    catch(const Error& err)
    {
        std::cout << err.asText() << "\n";
    }

    std::cout << "Exiting deng_Main()...\n";
    return 0;        
}
