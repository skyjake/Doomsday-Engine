/*
 * The Doomsday Engine Project -- libdeng2
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

#ifndef LIBDENG2_APP_H
#define LIBDENG2_APP_H

#include <de/deng.h>
#include <de/Error>
#include <de/CommandLine>
#include <de/FS>

/**
 * @defgroup core Core
 *
 * These classes contain the core functionality of libdeng2.
 */

namespace de
{
    class Library;
    
    /**
     * The application. 
     *
     * @note This is a singleton class. Only one instance per process is allowed.
     *
     * @ingroup core
     */
    class PUBLIC_API App
    {
    public:
        /// Only one instance of App is allowed. @ingroup errors
        DEFINE_ERROR(TooManyInstancesError);

        /// The App instance has not been created but someone is trying to access it. 
        /// @ingroup errors
        DEFINE_ERROR(NoInstanceError);
        
    public:
        App(const CommandLine& commandLine);
        virtual ~App();

        /**
         * Returns the command line arguments specified at the start of the application.
         */
        const CommandLine& commandLine() const { return commandLine_; }

        /**
         * Returns the command line arguments specified at the start of the application.
         */
        CommandLine& commandLine() { return commandLine_; }

        /** 
         * Returns the file system.
         */
        FS& fileSystem();

        /**
         * Returns the game library.
         *
         * @return The game library, or @c NULL if one is not loaded at the moment.
         */
        Library* game();

        /**
         * Loads the basic plugins (named "dengplugin_").
         */
        void loadPlugins();
        
        /**
         * Main loop of the application. To be defined by a derived class.
         *
         * @return Zero on successful exit from the main loop. Nonzero on error.
         */
        virtual dint mainLoop() = 0;

    public:
        /**
         * Returns the singleton App instance. With this the App can be accessed
         * anywhere.
         */
        static App& app();
        
    private:
        CommandLine commandLine_;
        
        /// The file system.
        FS* fs_;

        // The game library.
        Library* game_;
        
        static App* singleton_;
    };
};

#endif /* LIBDENG2_APP_H */
