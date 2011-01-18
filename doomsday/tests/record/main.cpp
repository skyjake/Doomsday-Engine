/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009, 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/data.h>
#include "../testapp.h"

#include <QDebug>
#include <QTextStream>

using namespace de;

int main(int argc, char** argv)
{
    try
    {
        TestApp app(argc, argv);
        
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
        
        String str;
        QTextStream os(&str);
        for(duint i = 0; i < b.size(); ++i)
        {
            os << dint(b.data()[i]) << " ";
        }
        LOG_MESSAGE("") << str;
        
        Reader(b) >> rec2;        
        LOG_MESSAGE("After being (de)serialized:\n") << rec2;
    }
    catch(const Error& err)
    {
        qWarning() << err.asText();
    }

    qDebug() << "Exiting main()...";
    return 0;        
}
