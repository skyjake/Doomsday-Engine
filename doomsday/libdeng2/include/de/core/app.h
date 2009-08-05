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

#include "../deng.h"
#include "../IClock"
#include "../CommandLine"
#include "../FS"
#include "../Protocol"

#include <list>

/**
 * @defgroup core Core
 *
 * These classes contain the core functionality of libdeng2.
 */

namespace de
{
    class Audio;
    class Config;
    class ISubsystem;
    class Library;
    class LibraryFile;
    class Module;
    class Video;
    class Zone;
    
    /**
     * The application. This is an abstract class. Subclasses will need to define
     * the iterate() method that performs tasks while the main loop is running.
     *
     * @note This is a singleton class. Only one instance per process is allowed.
     *
     * @ingroup core
     */
    class LIBDENG2_API App : public IClock
    {
    public:
        /// Only one instance of App is allowed. @ingroup errors
        DEFINE_ERROR(TooManyInstancesError);

        /// The App instance has not been created but someone is trying to access it. 
        /// @ingroup errors
        DEFINE_ERROR(NoInstanceError);
        
        /// An attempt is made to access the game library while one is not loaded.
        /// @ingroup errors
        DEFINE_ERROR(NoGameError);
        
        /// An attempt is made to access the video subsystem while on is not available.
        /// @ingroup errors
        DEFINE_ERROR(NoVideoError);

        /// There was a problem with SDL. Contains the SDL error message. @ingroup errors
        DEFINE_ERROR(SDLError);
        
        /// The object or resource that was being looked for was not found. @ingroup errors
        DEFINE_ERROR(NotFoundError);
        
    public:
        /**
         * Constructs the application.
         * 
         * @param commandLine    Command line arguments.
         * @param configPath     Path of the configuration file. The application's 
         *                       configuration must <tt>import deng</tt>.
         * @param homeSubFolder  Folder under <tt>/home</tt> where the app will store the data
         *                       it writes. (Note that <tt>/data</tt> is read-only.)
         */
        App(const CommandLine& commandLine, const String& configPath, const String& homeSubFolder = "");
            
        virtual ~App();

        /**
         * Loads the basic plugins (named "dengplugin_").
         */
        void loadPlugins();

        /**
         * Unloads the game plugin.
         */
        void unloadGame();
        
        /**
         * Unloads all loaded plugins.
         */
        void unloadPlugins();
        
        /**
         * Main loop of the application.
         *
         * @return  Application exit code.
         *
         * @see iterate(), setExitCode()
         */
        virtual dint mainLoop();
        
        /**
         * Called on every iteration of the main loop.
         */
        virtual void iterate() = 0;
        
        /** 
         * Determines whether the main loop should be running.
         *
         * @return @c true if main loop running.
         */
        bool runningMainLoop() const { return runMainLoop_; }
        
        /**
         * Sets the code to be returned from the deng_Main() function to the
         * operating system upon quitting.
         *
         * @param exitCode  Exit code.
         */
        virtual void setExitCode(dint exitCode) { exitCode_ = exitCode; }

        /**
         * Returns the current exit code.
         *
         * @return  Exit code for operating system.
         *
         * @see setExitCode()
         */
        dint exitCode() const { return exitCode_; }
        
        /**
         * Stops the main loop.
         *
         * @param code  Exit code for the application.
         *
         * @see setExitCode()
         */
        virtual void stop(dint code = 0);
        
        /**
         * Deletes all loaded modules.
         */
        void clearModules();
        
        // Implements IClock.
        const Time& now() const { return currentTime_; }
        
    protected:
        /**
         * Delete all the subsystems currently in use.
         */
        void clearSubsystems();
        
    public:
        /**
         * Returns the singleton App instance. With this the App can be accessed
         * anywhere.
         */
        static App& app();
        
        /**
         * Returns the version number of the library.
         */
        static Version version();

        /**
         * Returns the command line arguments specified at the start of the application.
         */
        static CommandLine& commandLine();

        /** 
         * Returns the file system.
         */
        static FS& fileSystem();
        
        /**
         * Returns the root folder of the file system.
         */
        static Folder& fileRoot();

        /**
         * Returns the configuration.
         */
        static Config& config();

        /**
         * Returns the network protocol.
         */
        static Protocol& protocol();

        /**
         * Returns the game library.
         */
        static Library& game();
        
        /**
         * Returns the memory zone.
         */
        static Zone& memory();
        
        /**
         * Returns the video subsystem.
         */
        static Video& video();
        
        /**
         * Determines whether a game library is currently available.
         */
        static bool hasGame();
        
        /**
         * Determines whether a video subsystem is currently available.
         */
        static bool hasVideo();
        
        /**
         * Returns the amount of time since the creation of the App.
         */
        static Time::Delta uptime();
        
        /**
         * Imports a script module that is located on the import path.
         *
         * @param name      Name of the module.
         * @param fromPath  Absolute path of the script doing the importing.
         * 
         * @return  The imported module.
         */
        static Record& importModule(const String& name, const String& fromPath = "");

    private:
        CommandLine commandLine_;
        
        /// Time when the App was initialized.
        Time initializedAt_;
        
        /// The memory zone.
        Zone* memory_;
        
        /// The file system.
        FS* fs_;

        /// The configuration.
        Config* config_;
        
        /// Protocol for incoming packets.
        Protocol protocol_;

        /// The game library.
        LibraryFile* gameLib_;
        
        /// Subsystems.
        typedef std::list<ISubsystem*> Subsystems;
        Subsystems subsystems_;
        
        /// The video subsystem. Can be NULL.
        Video* video_;
        
        /// The audio subsystem. Can be NULL.
        Audio* audio_;

        /// Modules.
        typedef std::map<String, Module*> Modules;
        Modules modules_;
        
        /// @c true while the main loop is running.
        bool runMainLoop_;

        /// Time in effect for the current main loop iteration.
        Time currentTime_;
        
        bool firstIteration_;
        Time lastTime_;
        
        /// Exit code passed to the operating system when quitting.
        dint exitCode_;
        
        /// The singleton instance of the App.
        static App* singleton_;
    };
};

#endif /* LIBDENG2_APP_H */
