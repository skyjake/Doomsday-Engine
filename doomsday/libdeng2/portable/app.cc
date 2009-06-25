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

using namespace de;

// This will be set when the app is constructed.
App* App::singleton_ = 0;

App::App(const CommandLine& commandLine)
    : commandLine_(commandLine)
{
    if(singleton_)
    {
        throw TooManyInstancesError("App::App", "Only one instance allowed");
    }
    
    singleton_ = this;
}

App::~App()
{
    singleton_ = 0;
}

App& App::the()
{
    if(!singleton_)
    {
        throw NoInstanceError("App::the", "App has not been constructed yet");
    }
    return *singleton_;
}
