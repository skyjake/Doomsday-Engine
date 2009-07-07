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

#include "de/app.h"
#include "de/libraryfile.h"
#include "de/library.h"
#include "de/directoryfeed.h"

using namespace de;

// This will be set when the app is constructed.
App* App::singleton_ = 0;

App::App(const CommandLine& commandLine)
    : commandLine_(commandLine), fs_(0), game_(0)
{
    if(singleton_)
    {
        throw TooManyInstancesError("App::App", "Only one instance allowed");
    }
    
    singleton_ = this;

#ifdef MACOSX
    // When the application is started through Finder, we get a special command
    // line argument. The working directory needs to be changed.
    if(commandLine_.count() >= 2 && String(commandLine_.at(1)).beginsWith("-psn"))
    {
        DirectoryFeed::changeWorkingDir(String::fileNamePath(commandLine_.at(0)) + "/..");
    }
#endif
    
    // Now we can proceed with the members.
    fs_ = new FS();
    fs_->refresh();
    
    // Load the basic plugins.
    loadPlugins();
}

App::~App()
{
    delete fs_;
    
    singleton_ = 0;
}

FS& App::fileSystem() 
{ 
    assert(fs_ != 0);
    return *fs_; 
}

Library* App::game()
{
    return game_;
}

void App::loadPlugins()
{
    // Get the index of libraries.
    const FS::Index& index = fs_->indexFor(TYPE_NAME(LibraryFile));
    
    for(FS::Index::const_iterator i = index.begin(); i != index.end(); ++i)
    {
        LibraryFile& libFile = *static_cast<LibraryFile*>(i->second);
        if(libFile.name().contains("dengplugin_"))
        {
            // Initialize the plugin.
            libFile.library();
            std::cout << "App::loadPlugins() loaded " << libFile.path() << "\n";
        }
    }
    
    // Also load the specified game plugin, if there is one.
    std::string gameName = "doom";
    dint pos = commandLine_.check("-game", 1);
    if(pos)
    {
        gameName = commandLine_.at(pos + 1);
    }
    
    std::cout << "Looking for game '" << gameName << "'\n";
        
    for(FS::Index::const_iterator i = index.begin(); i != index.end(); ++i)
    {
        LibraryFile& libFile = *static_cast<LibraryFile*>(i->second);
        if(libFile.name().contains("_" + gameName + "."))
        {
            // This is the one.
            game_ = &libFile.library();
            std::cout << "App::loadPlugins() loaded the game " << libFile.path() << "\n";
            break;
        }
    }
}

App& App::app()
{
    if(!singleton_)
    {
        throw NoInstanceError("App::app", "App has not been constructed yet");
    }
    return *singleton_;
}
