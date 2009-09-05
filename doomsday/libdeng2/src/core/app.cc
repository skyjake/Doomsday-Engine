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
#include "de/Config"
#include "de/ArrayValue"
#include "de/DirectoryFeed"
#include "de/Library"
#include "de/LibraryFile"
#include "de/LogBuffer"
#include "de/Module"
#include "de/ISubsystem"
#include "de/Audio"
#include "de/Video"
#include "de/Zone"
#include "de/Thinker"
#include "de/Object"
#include "de/Map"
#include "../sdl.h"

using namespace de;

const duint DEFAULT_LOG_BUFFER_MAX_ENTRY_COUNT = 1000;

// This will be set when the app is constructed.
App* App::_singleton = 0;

App::App(const CommandLine& commandLine, const String& configPath, const String& homeSubFolder,
    LogLevel defaultLogLevel)
    : _commandLine(commandLine), 
      _logBuffer(0),
      _memory(0), 
      _fs(0), 
      _config(0),
      _gameLib(0),
      _currentMap(0), 
      _video(0), 
      _audio(0),
      _runMainLoop(true), 
      _firstIteration(true), 
      _exitCode(0)
{
    if(_singleton)
    {
        /// @throw TooManyInstancesError Attempted to construct a new instance of App while
        /// one already exists. There can only be one instance of App per process.
        throw TooManyInstancesError("App::App", "Only one instance allowed");
    }
    _singleton = this;

#ifdef _DEBUG
    // Enable all log entries by default in debug builds.
    defaultLogLevel = DEBUG;
#endif

    // Create a buffer for log entries.
    _logBuffer = new LogBuffer(DEFAULT_LOG_BUFFER_MAX_ENTRY_COUNT);
    _logBuffer->enable(defaultLogLevel);

    if(commandLine.has("--stdout"))
    {
        _logBuffer->enableStandardOutput();
    }

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
        // Define built-in constructors.
        Thinker::define(Thinker::fromReader);
        Thinker::define(Object::fromReader);
        
        // The memory zone.
        std::auto_ptr<Zone> memoryPtr(new Zone());
        _memory = memoryPtr.get();

#if defined(MACOSX)
        // When the application is started through Finder, we get a special command
        // line argument. The working directory needs to be changed.
        if(_commandLine.count() >= 2 && String(_commandLine.at(1)).beginsWith("-psn"))
        {
            DirectoryFeed::changeWorkingDir(_commandLine.at(0).fileNamePath() + "/..");
        }
#endif
    
        // Now we can proceed with the members.
        std::auto_ptr<FS> fsPtr(new FS());
        _fs = fsPtr.get();

        // Initialize the built-in folders.
#if defined(MACOSX)
        _fs->getFolder("/bin").attach(new DirectoryFeed("MacOS"));
        _fs->getFolder("/data").attach(new DirectoryFeed("Resources"));
        _fs->getFolder("/config").attach(new DirectoryFeed("Resources/config"));
        //fs_->getFolder("/modules").attach(new DirectoryFeed("Resources/modules"));
#endif

#if defined(UNIX) && !defined(MACOSX)
        _fs->getFolder("/bin").attach(new DirectoryFeed("bin"));
        _fs->getFolder("/data").attach(new DirectoryFeed("data"));
        _fs->getFolder("/config").attach(new DirectoryFeed("data/config"));
        //fs_->getFolder("/modules").attach(new DirectoryFeed("data/modules"));
#endif

#ifdef WIN32
        _fs->getFolder("/bin").attach(new DirectoryFeed("bin"));
        _fs->getFolder("/data").attach(new DirectoryFeed("data"));
        _fs->getFolder("/config").attach(new DirectoryFeed("data\\config"));
        //fs_->getFolder("/modules").attach(new DirectoryFeed("data\\modules"));
#endif

        /// @todo  /home should really be under the native user home dir (~/.deng2, 
        /// C:\\Documents and Settings\\..., ~/Library/Application Support/Doomsday2/)
        _fs->getFolder("/home").attach(new DirectoryFeed(
            String("home").concatenateNativePath(homeSubFolder), 
            DirectoryFeed::ALLOW_WRITE | DirectoryFeed::CREATE_IF_MISSING));

        _fs->refresh();
        
        // The configuration.
        std::auto_ptr<Config> configPtr(new Config(configPath));
        _config = configPtr.get();
        _config->read();
        
        // Update the log buffer max entry count.
        _logBuffer->setMaxEntryCount(_config->getui("deng.log.bufferSize"));
        
        // Set the log output file.
        _logBuffer->setOutputFile(_config->gets("deng.log.file"));
        
        // The level of enabled messages.
        _logBuffer->enable(LogLevel(_config->getui("deng.log.level")));
        
        // Load the basic plugins.
        loadPlugins();

        // Successful construction without errors, so drop our guard.
        memoryPtr.release();
        fsPtr.release();
        configPtr.release();
        
        LOG_VERBOSE("libdeng2::App ") << LIBDENG2_VERSION << " initialized.";
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

    delete _config;
    _config = 0;

    clearModules();

    // Deleting the file system will unload everything owned by the files, including 
    // all remaining plugin libraries.
    delete _fs;
    _fs = 0;
    
    delete _memory;
    _memory = 0;
 
    // Shut down SDL.
    SDLNet_Quit();
    SDL_Quit();

    delete _logBuffer;
    _logBuffer = 0;

    _singleton = 0;
}

void App::loadPlugins()
{
    LOG_AS("App::loadPlugins");
    
    // Names of preferred plugins.
    String gameName = "doom"; /// @todo There is no default game, really...
    _commandLine.getParameter("--game", gameName);
    
    String videoName = config().gets("deng.video");
    _commandLine.getParameter("--video", videoName);
    
    String audioName = config().gets("deng.audio");
    _commandLine.getParameter("--audio", audioName);

    // Get the index of libraries.
    const FS::Index& index = _fs->indexFor(TYPE_NAME(LibraryFile));

    // Check all libraries we have access to and see what can be done with them.
    FOR_EACH(i, index, FS::Index::const_iterator)
    {
        LibraryFile& libFile = *static_cast<LibraryFile*>(i->second);
        if(libFile.name().contains("dengplugin_"))
        {
            // Initialize the plugin.
            String type = libFile.library().type();

            // What kind of a library do we have?
            if(type == "deng-plugin/game")
            {
                if(libFile.hasUnderscoreName(gameName) && !_gameLib)
                {
                    // This is the right game.
                    _gameLib = &libFile;
                    LOG_VERBOSE("Located the game ") << libFile.path();
                }
                else
                {
                    // Not the right game.
                    libFile.clear();
                    continue;
                }
            }
            else if(type == "deng-plugin/video")
            {
                if(videoName != "none" && libFile.hasUnderscoreName(videoName) && !_video)
                {
                    _video = libFile.library().SYMBOL(deng_NewVideo)();
                    _subsystems.push_back(_video);
                    LOG_MESSAGE("Video subsystem ") << libFile.path();
                }
                else
                {
                    // Not the right one.
                    libFile.clear();
                    continue;
                }
            }
            else if(type == "deng-plugin/audio")
            {
                if(audioName != "none" && libFile.hasUnderscoreName(audioName) && !_audio)
                {
                    _audio = libFile.library().SYMBOL(deng_NewAudio)();
                    _subsystems.push_back(_audio);
                    LOG_MESSAGE("Audio subsystem ") << libFile.path();
                }
                else
                {
                    // Not the right one.
                    libFile.clear();
                    continue;
                }
            }      
            
            LOG_VERBOSE("Loaded ") << libFile.path() << " [" << libFile.library().type() << "]";
        }
    }
}

void App::clearModules()
{
    FOR_EACH(i, _modules, Modules::iterator)
    {
        delete i->second;
    }
    _modules.clear();
}

void App::clearSubsystems()
{
    FOR_EACH(i, _subsystems, Subsystems::iterator)
    {
        delete *i;
    }
    _subsystems.clear();
}

void App::unloadGame()
{
    // Unload the game first.
    if(_gameLib)
    {
        _gameLib->clear();
        _gameLib = 0;
    }
}

void App::unloadPlugins()
{
    LOG_AS("App::unloadPlugins");
    
    clearSubsystems();
    unloadGame();
    
    // Get the index of libraries.
    const FS::Index& index = _fs->indexFor(TYPE_NAME(LibraryFile));
    
    FOR_EACH(i, index, FS::Index::const_iterator)
    {
        LibraryFile& libFile = *static_cast<LibraryFile*>(i->second);
        if(libFile.name().contains("dengplugin_"))
        {
            libFile.clear();
            LOG_VERBOSE("Unloaded ") << libFile.path();
        }
    }
}

dint App::mainLoop()
{
    try
    {
        // Now running the main loop.
        _runMainLoop = true;
        _firstIteration = true;
    
        while(_runMainLoop)
        {
            // Determine elapsed time.
            Time::Delta elapsed = 0;
            _currentTime = Time();
            if(_firstIteration)
            {
                _firstIteration = false;
                elapsed = 0;
            }
            else
            {
                elapsed = _currentTime - _lastTime;
            }
            _lastTime = _currentTime;

            // Do the loop iteration.
            iterate();
        
            // Update subsystems (draw graphics, update sounds, etc.).
            FOR_EACH(i, _subsystems, Subsystems::iterator)
            {
                (*i)->update(elapsed);
            }
        }
    }
    catch(const Error& err)
    {
        LOG_ERROR("Main loop stopped: ") << err.what();        
    }
    return _exitCode;
}

void App::stop(dint code)
{
    _runMainLoop = false;
    setExitCode(code);
}

App& App::app()
{
    if(!_singleton)
    {
        /// @throw NoInstanceError No App instance is currently available.
        throw NoInstanceError("App::app", "App has not been constructed yet");
    }
    return *_singleton;
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
    return app()._commandLine; 
}

LogBuffer& App::logBuffer()
{
    App& self = app();
    assert(self._logBuffer != 0);
    return *self._logBuffer;
}

Zone& App::memory()
{
    App& self = app();
    assert(self._memory != 0);
    return *self._memory;
}

FS& App::fileSystem() 
{ 
    App& self = app();
    assert(self._fs != 0);
    return *self._fs; 
}

Folder& App::fileRoot()
{
    App& self = app();
    assert(self._fs != 0);
    return self._fs->root();
}

Folder& App::homeFolder()
{
    App& self = app();
    assert(self._fs != 0);
    return self._fs->root().locate<Folder>("/home");
}

Config& App::config()
{
    App& self = app();
    assert(self._config != 0);
    return *self._config; 
}

Protocol& App::protocol()
{
    return app()._protocol;
}

Library& App::game()
{
    App& self = app();
    if(!self._gameLib)
    {
        /// @throw NoGameError No game library is currently available.
        throw NoGameError("App::game", "No game library located");
    }
    return self._gameLib->library();
}

Map& App::currentMap()
{
    App& self = app();
    if(!self._currentMap)
    {
        /// @throw NoCurrentMapError  No map is currently active.
        throw NoCurrentMapError("App::currentMap", "No map is currently active");
    }
    return *self._currentMap;
}

void App::setCurrentMap(Map* map)
{
    App& self = app();
    self._currentMap = map;
    
    if(map)
    {
        LOG_VERBOSE("Current map set: ") << map->name();
    }
    else
    {
        LOG_VERBOSE("No more current map.");
    }
}

Video& App::video()
{
    App& self = app();
    if(!self._video)
    {
        /// @throw NoVideoError The video subsystem is not available. 
        throw NoVideoError("App::video", "No video subsystem available");
    }
    return *self._video;
}

bool App::hasGame() 
{ 
    return app()._gameLib != 0;
}

bool App::hasVideo() 
{ 
    return app()._video != 0; 
}

Time::Delta App::uptime()
{
    return app()._initializedAt.since();
}

static int sortFilesByModifiedAt(const File* a, const File* b)
{
    return cmp(a->status().modifiedAt, b->status().modifiedAt);
}

Record& App::importModule(const String& name, const String& fromPath)
{
    LOG_AS("App::importModule");
    
    App& self = app();
    
    // There are some special modules.
    if(name == "Config")
    {
        return self.config().names();
    }
    
    // Maybe we already have this module?
    Modules::iterator found = self._modules.find(name);
    if(found != self._modules.end())
    {
        return found->second->names();
    }
    
    /// @todo  Move this path searching logic to FS.

    // Fall back on the default if the libdeng2 module hasn't been imported yet.
    std::auto_ptr<ArrayValue> defaultImportPath(new ArrayValue);
    defaultImportPath->add("");    
    defaultImportPath->add("*"); // Newest module with a matching name.
    ArrayValue* importPath = defaultImportPath.get();
    try
    {
        importPath = &config().names()["deng.importPath"].value<ArrayValue>();
    }
    catch(const Record::NotFoundError&)
    {}
    
    // Search the import path (array of paths).
    FOR_EACH(i, importPath->elements(), ArrayValue::Elements::const_iterator)
    {
        String dir = (*i)->asText();
        String p;
        FS::FoundFiles matching;
        File* found = 0;
        if(dir.empty())
        {
            if(!fromPath.empty())
            {
                // Try the local folder.
                p = fromPath.fileNamePath() / name;            
            }
            else
            {
                continue;
            }
        }
        else if(dir == "*")
        {
            fileSystem().find(name + ".de", matching);
            if(matching.empty())
            {
                continue;
            }
            matching.sort(sortFilesByModifiedAt);
            found = matching.back();
            LOG_VERBOSE("Chose ") << found->path() << " out of " << matching.size() << " candidates.";
        }
        else
        {
            p = dir / name;
        }
        if(!found)
        {
            found = fileRoot().tryLocateFile(p + ".de");
        }
        if(found)
        {
            Module* module = new Module(*found);
            self._modules[name] = module;
            return module->names();
        }
    }
    
    throw NotFoundError("App::importModule", "Cannot find module '" + name + "'");
}
