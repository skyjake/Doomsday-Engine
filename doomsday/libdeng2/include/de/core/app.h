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
#include "../FS"
#include "../Module"
#include "../Config"
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
        /// The object or resource that was being looked for was not found. @ingroup errors
        DENG2_ERROR(NotFoundError);

        enum GUIMode {
            GUIDisabled = 0,
            GUIEnabled = 1
        };

    public:
        App(int& argc, char** argv, GUIMode guiMode);

        /**
         * Initializes all the application's subsystems. This includes Config and FS.
         */
        void initSubsystems();

        bool notify(QObject* receiver, QEvent* event);

        static App& app();

        /**
         * Returns the command line used to start the application.
         */
        static CommandLine& commandLine();

        /**
         * Returns the absolute path of the application executable.
         */
        static String executablePath();

        /**
         * Returns the application's file system.
         */
        static FS& fileSystem();

        /**
         * Returns the root folder of the file system.
         */
        static Folder& fileRoot();

        /**
         * Returns the /home folder.
         */
        static Folder& homeFolder();

        /**
         * Returns the configuration.
         */
        static Config& config();

        /**
         * Imports a script module that is located on the import path.
         *
         * @param name      Name of the module.
         * @param fromPath  Absolute path of the script doing the importing.
         *
         * @return  The imported module.
         */
        static Record& importModule(const String& name, const String& fromPath = "");

    signals:
        void uncaughtException(QString message);

    private:
        CommandLine _cmdLine;

        /// Path of the application executable.
        String _appPath;

        /// The file system.
        FS _fs;

        /// The configuration.
        Config* _config;

        /// Resident modules.
        typedef std::map<String, Module*> Modules;
        Modules _modules;
    };
}

#endif // LIBDENG2_APP_H
