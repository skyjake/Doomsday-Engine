/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_CONSOLEAPP_H
#define LIBDENG2_CONSOLEAPP_H

#include "../App"

#include <QCoreApplication>

namespace de
{
    /**
     * Console application.
     */
    class ConsoleApp : public QCoreApplication, public App
    {
    public:
        ConsoleApp(int argc, char** argv,
                   const String& configPath,
                   const String& homeSubFolder = "",
                   Log::LogLevel defaultLogLevel = Log::MESSAGE);
    };
}

#endif // LIBDENG2_CONSOLEAPP_H
