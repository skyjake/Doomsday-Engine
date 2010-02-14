/*
 * The Doomsday Engine Project -- dengcl
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

#include "clientapp.h"
#include "localserver.h"
#include "usersession.h"
#include <de/net.h>
#include <de/data.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "doomsday.h"

using namespace de;

ClientApp::ClientApp(const de::CommandLine& arguments)
    : App(arguments, "/config/client/client.de", "client"), _localServer(0), _session(0)
{        
    CommandLine& args = commandLine();
    
#ifdef MACOSX
    args.append("-iwad");
    args.append("/Users/jaakko/IWADs/Doom.wad");
#endif

    args.append("-file");
#if defined(MACOSX)
    args.append("}Resources/doomsday.pk3");
    args.append("}Resources/doom.pk3");
#elif defined(WIN32)
    args.append("..\\..\\data\\doomsday.pk3");
    args.append("..\\..\\data\\doom.pk3");
#else
    args.append("../../data/doomsday.pk3");
    args.append("../../data/doom.pk3");
#endif

    const duint16 SERVER_PORT = duint16(config().getui("net.localServer.listenPort"));

    std::auto_ptr<LocalServer> svPtr(new LocalServer(SERVER_PORT));
    _localServer = svPtr.get();

    /*
    args.append("-cmd");
    args.append("net-port-control 13211; net-port-data 13212; after 30 \"net init\"; after 50 \"connect localhost:13209\"");
    */
    
    args.append("-userdir");
    args.append("clientdir");

    // Initialize the engine.
    DD_SetInteger(DD_DENG2_CLIENT, true);
    DD_Entry(0, NULL);
    
    LOG_MESSAGE("Opening link to server.");

    // DEVEL: Join the game session.
    // Query the on-going sessions.
    MuxLink* link = new MuxLink(Address("localhost", SERVER_PORT));

    LOG_MESSAGE("Creating new session.");

    CommandPacket createSession("session.new");
    createSession.arguments().addText("map", "E1M1");
    protocol().decree(*link, createSession);
    
    link->base() << CommandPacket("status");
    std::auto_ptr<RecordPacket> status(link->base().receivePacket<RecordPacket>());
    LOG_MESSAGE("The server said '%s':\n%s") << status->label() << status->record();

    Id sessionToJoin(status->record().subrecord("sessions").subrecords().begin()->first);
    LOG_MESSAGE("Going to join session ") << sessionToJoin;

    // Join the session.
    _session = new UserSession(link, sessionToJoin);

    //Link(Address("localhost", SERVER_PORT)) << CommandPacket("quit");
    
    // Good to go.
    svPtr.release();
}

ClientApp::~ClientApp()
{
    delete _session;
    delete _localServer;
    
    // Shutdown the engine.
    DD_Shutdown();
}

void ClientApp::iterate(const Time::Delta& elapsed)
{
    try
    {
        if(_session)
        {
            _session->listen();
        }
        
        // Perform thinking for the current map.
        if(hasCurrentMap())
        {
            currentMap().think(elapsed);
        }
    }
    catch(const UserSession::SessionEndedError& err)
    {
        LOG_INFO("Session ended: ") << err.asText();
        delete _session;
        _session = 0;
    }
    catch(const Link::DisconnectedError& err)
    {
        LOG_INFO("Disconnected from server: ") << err.asText();
        delete _session;
        _session = 0;
    }
    
    // libdeng main loop tasks.
    DD_GameLoop();
}
