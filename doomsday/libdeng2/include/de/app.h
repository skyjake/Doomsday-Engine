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

namespace de
{
    /**
     * The application. 
     *
     * @note This is a singleton class. Only one instance per process is allowed.
     */
    class PUBLIC_API App
    {
    public:
        DEFINE_ERROR(TooManyInstancesError);
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
         * Main loop of the application. To be defined by a derived class.
         *
         * @return Zero on successful exit from the main loop. Nonzero on error.
         */
        virtual dint mainLoop() = 0;

        /**
         * Returns the singleton App instance. With this the App can be accessed
         * anywhere.
         */
        static App& the();
        
    private:
        CommandLine commandLine_;
        
        static App* singleton_;
    };
};

#endif /* LIBDENG2_APP_H */
