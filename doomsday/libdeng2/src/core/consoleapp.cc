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

#include "de/ConsoleApp"
#include "de/Error"

#include <QDebug>

using namespace de;

namespace de
{
    namespace internal
    {
        CoreApplication::CoreApplication(int argc, char** argv)
            : QCoreApplication(argc, argv)
        {}

        bool CoreApplication::notify(QObject* receiver, QEvent* event)
        {
            try
            {
                return QCoreApplication::notify(receiver, event);
            }
            catch(const std::exception& error)
            {
                LOG_AS("ConsoleApp::notify");
                LOG_ERROR(error.what());
                LOG_INFO("Application will quit.");
                quit();
            }
            catch(...)
            {
                LOG_AS("ConsoleApp::notify");
                LOG_ERROR("Uncaught exception.");
                LOG_INFO("Application will quit.");
                quit();
            }
            return false;
        }
    }
}

ConsoleApp::ConsoleApp(int argc, char** argv,
                       const String& configPath,
                       const String& homeSubFolder,
                       Log::LogLevel defaultLogLevel)
    : App(CommandLine(argc, argv), configPath, homeSubFolder, defaultLogLevel),
      _coreApp(argc, argv)
{}
