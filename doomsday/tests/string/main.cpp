/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/math.h>
#include <QDebug>

using namespace de;

int main(int argc, char **argv)
{
    try
    {
        TextApp app(argc, argv);
        app.initSubsystems(App::DisablePlugins);

        LOG_MSG("Escaped %%: arg %i") << 1;
        LOG_MSG("Escaped %%: arg %%%i%%") << 1;
        //LOG_MSG("Error: %") << 1; // incomplete formatting
        //LOG_MSG("Error: %i %i") << 1; // ran out of arguments

        LOG_MSG("String: '%s'") << "Hello World";
        LOG_MSG(" Min width 8:  '%8s'") << "Hello World";
        LOG_MSG(" Max width .8: '%.8s'") << "Hello World";
        LOG_MSG(" Left align:   '%-.8s'") << "Hello World";
        LOG_MSG("String: '%s'") << "Hello";
        LOG_MSG(" Min width 8:  '%8s'") << "Hello";
        LOG_MSG(" Max width .8: '%.8s'") << "Hello";
        LOG_MSG(" Left align:   '%-8s'") << "Hello";

        LOG_MSG("Integer (64-bit signed): %i") << 0x1000000000;
        LOG_MSG("Integer (64-bit signed): %d") << 0x1000000000;
        LOG_MSG("Integer (64-bit unsigned): %u") << 0x123456789abc;
        LOG_MSG("Boolean: %b %b") << true << false;
        LOG_MSG("16-bit Unicode character: %c") << 0x44;
        LOG_MSG("Hexadecimal (64-bit): %x") << 0x123456789abc;
        LOG_MSG("Hexadecimal (64-bit): %X") << 0x123456789abc;
        LOG_MSG("Pointer: %p") << &app;
        LOG_MSG("Double precision floating point: %f") << PI;
        LOG_MSG("Decimal places .4: %.4f") << PI;
        LOG_MSG("Decimal places .10: %.10f") << PI;
    }
    catch(Error const &err)
    {
        qWarning() << err.asText() << "\n";
    }

    qDebug() << "Exiting main()...\n";
    return 0;        
}
