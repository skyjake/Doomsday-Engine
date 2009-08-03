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
 
#include "de/Config"
#include "de/App"
#include "de/Folder"

using namespace de;

Config::Config() : configPath_("/config")
{
    // Read the configuration. Everything under the config path.
    Folder& f = App::fileSystem().getFolder(configPath_);
    for(Folder::Contents::const_iterator i = f.contents().begin(); i != f.contents().end(); ++i)
    {
        if(i->second->name().fileNameExtension() == ".de")
        {
            Script script(*i->second);
            config_.run(script);
            config_.execute();
        }
    }
}

Record& Config::names()
{
    return config_.globals();
}
