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

#ifndef LIBDENG2_GUIAPP_H
#define LIBDENG2_GUIAPP_H

#include "../App"

#include <QApplication>

namespace de
{
    /**
     * Application with a graphical user interface.
     */
    class GUIApp : public QApplication, public App
    {
    public:
        GUIApp(int argc, char** argv,
               const String& configPath,
               const String& homeSubFolder = "",
               Log::LogLevel defaultLogLevel = Log::MESSAGE);
    };
}

#endif // LIBDENG2_GUIAPP_H
