/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2010-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/App"
#include "de/Log"
#include <QDebug>

using namespace de;

App::App(int& argc, char** argv, bool useGUI)
    : QApplication(argc, argv, useGUI),
      _cmdLine(argc, argv)
{
    _appPath = applicationFilePath();
}

bool App::notify(QObject* receiver, QEvent* event)
{
    try
    {
        return QApplication::notify(receiver, event);
    }
    catch(const std::exception& error)
    {
        emit uncaughtException(error.what());
    }
    catch(...)
    {
        emit uncaughtException("de::App caught exception of unknown type.");
    }
    return false;
}

CommandLine& App::commandLine()
{
    return _cmdLine;
}

String App::executablePath() const
{
    return _appPath;
}
