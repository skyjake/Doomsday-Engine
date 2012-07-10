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

#ifndef LIBDENG2_APP_H
#define LIBDENG2_APP_H

#include "../libdeng2.h"
#include "../CommandLine"
#include <QApplication>

/**
 * Macro for conveniently accessing the de::App singleton instance.
 */
#define DENG2_APP   (static_cast<de::App*>(qApp))

namespace de
{
    /**
     * Application whose event loop is protected against uncaught exceptions.
     * Catches the exception and shuts down the app cleanly.
     */
    class DENG2_PUBLIC App : public QApplication
    {
        Q_OBJECT

    public:
        App(int& argc, char** argv, bool useGUI);

        bool notify(QObject* receiver, QEvent* event);

        /**
         * Returns the command line used to start the application.
         */
        CommandLine& commandLine();

        /**
         * Returns the absolute path of the application executable.
         */
        de::String executablePath() const;

    signals:
        void uncaughtException(QString message);

    private:
        CommandLine _cmdLine;

        /// Path of the application executable.
        de::String _appPath;
    };
}

#endif // LIBDENG2_APP_H
