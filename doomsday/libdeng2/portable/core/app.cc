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

#include "de/App"
#include "de/Zone"
#include "de/LibraryFile"
#include "de/Library"
#include "de/DirectoryFeed"
#include "de/ISubsystem"
#include "de/Video"
#include "de/Audio"
#include "../sdl.h"

using namespace de;

/// Name of the default video subsystem.
static const std::string DEFAULT_VIDEO = "sdlopengl";

/// Name of the default audio subsystem.
static const std::string DEFAULT_AUDIO = "fmod";

// This will be set when the app is constructed.
App* App::singleton_ = 0;

App::App(const CommandLine& commandLine, const std::string& defaultVideo, const std::string& defaultAudio)
    : commandLine_(commandLine), 
      memory_(0), 
      fs_(0), 
      gameLib_(0), 
      video_(0), 
      defaultVideo_(defaultVideo), 
      audio_(0),
      defaultAudio_(defaultAudio),
      runMainLoop_(true), 
      firstIteration_(true), 
      exitCode_(0)
{
    if(singleton_)
    {
        /// @throw TooManyInstancesError Attempted to construct a new instance of App while
        /// one already exists. There can only be one instance of App per process.
        throw TooManyInstancesError("App::App", "Only one instance allowed");
    }
    singleton_ = this;

    // Start by initializing SDL.
    if(SDL_Init(SDL_INIT_TIMER) == -1)
    {
        /// @throw SDLError SDL initialization failed.
        throw SDLError("App::App", SDL_GetError());
    }
    if(SDLNet_Init() == -1)
    {
        throw SDLError("App::App", SDLNet_GetError());
    }

    try
    {
        // The memory zone.
        std::auto_ptr<Zone> memoryPtr(new Zone());
        memory_ = memoryPtr.get();

#ifdef MACOSX
        // When the application is started through Finder, we get a special command
        // line argument. The working directory needs to be changed.
        if(commandLine_.count() >= 2 && String(commandLine_.at(1)).beginsWith("-psn"))
        {
            DirectoryFeed::changeWorkingDir(String::fileNamePath(commandLine_.at(0)) + "/..");
        }
#endif
    
        // Now we can proceed with the members.
        std::auto_ptr<FS> fsPtr(new FS());
        fs_ = fsPtr.get();
        fs_->refresh();
        
        // Load the basic plugins.
        loadPlugins();

        // Successful construction without errors, so drop our guard.
        memoryPtr.release();
        fsPtr.release();
        
        std::cout << "libdeng2 App " << LIBDENG2_VERSION << " initialized.\n";
    }
    catch(const Error& err)
    {
        // Make sure the subsystems that were created during plugin loading are deleted.
        clearSubsystems();
        err.raise();
    }
}

App::~App()
{
    clearSubsystems();
    
    // Deleting the file system will unload everything owned by the files, including 
    // all plugin libraries.
    delete fs_;
    
    delete memory_;
 
    // Shut down SDL.
    SDLNet_Quit();
    SDL_Quit();

    singleton_ = 0;
}

void App::loadPlugins()
{
    // Names of preferred plugins.
    std::string gameName = "doom"; /// @todo There is no default game, really...
    commandLine_.getParameter("--game", gameName);
    
    std::string videoName = defaultVideo_.empty()? DEFAULT_VIDEO : defaultVideo_;
    commandLine_.getParameter("--video", videoName);
    
    std::string audioName = defaultAudio_.empty()? DEFAULT_AUDIO : defaultAudio_;
    commandLine_.getParameter("--audio", audioName);

    // Get the index of libraries.
    const FS::Index& index = fs_->indexFor(TYPE_NAME(LibraryFile));

    // Check all libraries we have access to and see what can be done with them.
    for(FS::Index::const_iterator i = index.begin(); i != index.end(); ++i)
    {
        LibraryFile& libFile = *static_cast<LibraryFile*>(i->second);
        if(libFile.name().contains("dengplugin_"))
        {
            // Initialize the plugin.
            std::string type = libFile.library().type();

            // What kind of a library do we have?
            if(type == "deng-plugin/game")
            {
                if(libFile.hasUnderscoreName(gameName) && !gameLib_)
                {
                    // This is the right game.
                    gameLib_ = &libFile;
                    std::cout << "App::loadPlugins() located the game " << libFile.path() << "\n";
                }
                else
                {
                    // Not the right game.
                    libFile.unload();
                    continue;
                }
            }
            else if(type == "deng-plugin/video")
            {
                if(videoName != "none" && libFile.hasUnderscoreName(videoName) && !video_)
                {
                    video_ = libFile.library().SYMBOL(deng_NewVideo)();
                    subsystems_.push_back(video_);
                    std::cout << "App::loadPlugins() constructed video subsystem " << libFile.path() << "\n";                
                }
                else
                {
                    // Not the right one.
                    libFile.unload();
                    continue;
                }
            }
            else if(type == "deng-plugin/audio")
            {
                if(audioName != "none" && libFile.hasUnderscoreName(audioName) && !audio_)
                {
                    audio_ = libFile.library().SYMBOL(deng_NewAudio)();
                    subsystems_.push_back(audio_);
                    std::cout << "App::loadPlugins() constructed audio subsystem " << libFile.path() << "\n";                
                }
                else
                {
                    // Not the right one.
                    libFile.unload();
                    continue;
                }
            }      
            
            std::cout << "App::loadPlugins() loaded " << libFile.path() << " [" << 
                libFile.library().type() << "]\n";
        }
    }
}

void App::clearSubsystems()
{
    for(Subsystems::iterator i = subsystems_.begin(); i != subsystems_.end(); ++i)
    {
        delete *i;
    }
    subsystems_.clear();
}

void App::unloadGame()
{
    // Unload the game first.
    if(gameLib_)
    {
        gameLib_->unload();
        gameLib_ = 0;
    }
}

void App::unloadPlugins()
{
    clearSubsystems();
    unloadGame();
    
    // Get the index of libraries.
    const FS::Index& index = fs_->indexFor(TYPE_NAME(LibraryFile));
    
    for(FS::Index::const_iterator i = index.begin(); i != index.end(); ++i)
    {
        LibraryFile& libFile = *static_cast<LibraryFile*>(i->second);
        if(libFile.name().contains("dengplugin_"))
        {
            // Initialize the plugin.
            libFile.unload();
            std::cout << "App::unloadPlugins() unloaded " << libFile.path() << "\n";
        }
    }
}

dint App::mainLoop()
{
    // Now running the main loop.
    runMainLoop_ = true;
    firstIteration_ = true;
    
    while(runMainLoop_)
    {
        // Determine elapsed time.
        Time::Delta elapsed = 0;
        currentTime_ = Time();
        if(firstIteration_)
        {
            firstIteration_ = false;
            elapsed = 0;
        }
        else
        {
            elapsed = currentTime_ - lastTime_;
        }
        lastTime_ = currentTime_;

        // Do the loop iteration.
        iterate();
        
        // Update subsystems (draw graphics, update sounds, etc.).
        for(Subsystems::iterator i = subsystems_.begin(); i != subsystems_.end(); ++i)
        {
            (*i)->update(elapsed);
        }
    }
    
    return exitCode_;
}

void App::stop(dint code)
{
    runMainLoop_ = false;
    setExitCode(code);
}

App& App::app()
{
    if(!singleton_)
    {
        /// @throw NoInstanceError No App instance is currently available.
        throw NoInstanceError("App::app", "App has not been constructed yet");
    }
    return *singleton_;
}

Version App::version()
{
    Version ver;
    ver.label = LIBDENG2_RELEASE_LABEL;
    ver.major = LIBDENG2_MAJOR_VERSION;
    ver.minor = LIBDENG2_MINOR_VERSION;
    ver.patchlevel = LIBDENG2_PATCHLEVEL;
    return ver;
}

CommandLine& App::commandLine() 
{ 
    return app().commandLine_; 
}

Zone& App::memory()
{
    App& self = app();
    assert(self.memory_ != 0);
    return *self.memory_;
}

FS& App::fileSystem() 
{ 
    App& self = app();
    assert(self.fs_ != 0);
    return *self.fs_; 
}

Protocol& App::protocol()
{
    return app().protocol_;
}

Library& App::game()
{
    App& self = app();
    if(!self.gameLib_)
    {
        /// @throw NoGameError No game library is currently available.
        throw NoGameError("App::game", "No game library located");
    }
    return self.gameLib_->library();
}

Video& App::video()
{
    App& self = app();
    if(!self.video_)
    {
        /// @throw NoVideoError The video subsystem is not available. 
        throw NoVideoError("App::video", "No video subsystem available");
    }
    return *self.video_;
}

bool App::hasGame() 
{ 
    return app().gameLib_ != 0;
}

bool App::hasVideo() 
{ 
    return app().video_ != 0; 
}

Time::Delta App::uptime()
{
    return app().initializedAt_.since();
}
