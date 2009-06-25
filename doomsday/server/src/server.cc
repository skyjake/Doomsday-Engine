/*
 * The Doomsday Engine Project -- dengsv
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

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "server.h"
#include "doomsday.h"

using namespace de;

Server::Server(const CommandLine& commandLine)
    : App(commandLine)
{}

Server::~Server()
{}

dint Server::mainLoop()
{
    // For our testing purposes, let's modify the command line to launch Doom1 E1M1 
    // in dedicated server mode.
    
    CommandLine& args = commandLine();
    
    args.append("-dedicated");
    args.append("-game");
#ifdef WIN32
    args.append("plugins\\deng_doom.dll");
#else
#ifdef MACOSX
    args.append("libdeng_doom.dylib");
#else
	args.append("deng_doom");
#endif
#endif
    args.append("-file");
    args.append("../../data/doomsday.pk3");
    args.append("../../data/doom.pk3");
    args.append("-cmd");
    args.append("net-port-control 13209; net-port-data 13210; after 30 \"net init\"; after 40 \"net server start\"");
    args.append("-userdir");
    args.append("serverdir");
    args.append("-libdir");
    args.append("../plugins");
    
    return DD_Entry(0, NULL);
}
