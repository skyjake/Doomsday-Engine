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

#include "usersession.h"
#include "clientapp.h"
#include <de/core.h>
#include <de/data.h>
#include <de/types.h>
#include <de/net.h>

using namespace de;

UserSession::UserSession(de::MuxLink* link, const de::Id& id)
    : _link(link), _sessionId(id), _world(0), _user(0)
{
    // Create a blank user and world. The user is configured jointly
    // from configuration and by the game. The world is mirrored from
    // the server's session when we join.
    _user = App::game().SYMBOL(deng_NewUser)();
    _world = App::game().SYMBOL(deng_NewWorld)();
    
    // The server will tell our id.
    _user->setId(Id::NONE);
    
    // Ask to join the session.
    CommandPacket join("session.join");
    join.arguments().addText("id", id);

    // Include our initial user state in the arguments.
    Writer(join.arguments().addBlock("userState").value<BlockValue>()) << *_user;

    RecordPacket* response;
    App::app().protocol().decree(*_link, join, &response);

    // Get the user id.
    _user->setId(response->valueAsText("userId"));
}   

UserSession::~UserSession()
{
    if(_sessionId)
    {
        // Inform that we are leaving.
        _link->base() << CommandPacket("session.leave");
    }
    clearOthers();

    delete _user;
    delete _world;    
    delete _link;
}

void UserSession::processPacket(const de::Packet& packet)
{
    LOG_AS("processPacket");
    
    const RecordPacket* record = dynamic_cast<const RecordPacket*>(&packet);
    if(record)
    {
        const Record& rec = record->record();
        LOG_DEBUG("%s\n") << record->label() << rec;
        LOG_AS_STRING(record->label());

        if(record->label() == "user.welcome")
        {
            // State of the world.
            Reader(rec.value<BlockValue>("worldState")) >> *_world;
            
            // State of existing users.
            clearOthers();
            const Record& existingUsers = rec.subrecord("users");
            FOR_EACH(i, existingUsers.members(), Record::Members::const_iterator)
            {
                User* remoteUser = App::game().SYMBOL(deng_NewUser)();
                _others[Id(i->first)] = remoteUser;                
                Reader(i->second->value<BlockValue>()) >> *remoteUser;
            }
        }
        else if(record->label() == "user.joined")
        {
            User* remoteUser = App::game().SYMBOL(deng_NewUser)();
            _others[Id(record->valueAsText("id"))] = remoteUser;
            
            // State of the new user.
            Reader(rec.value<BlockValue>("userState")) >> *remoteUser;
        }
        else if(record->label() == "user.left")
        {
            Others::iterator found = _others.find(Id(rec.value<TextValue>("id")));
            if(found != _others.end())
            {
                delete found->second;
                _others.erase(found);
            }
        }
        else if(record->label() == "session.ended")
        {
            LOG_INFO("The session ended!");
            _sessionId = Id::NONE;
            /// @throw SessionEndedError The user session is no longer valid due to
            /// the serverside session having ended.
            throw SessionEndedError("UserSession::processPacket", 
                "Serverside session ended");
        }
    }
}

void UserSession::listenForUpdates()
{
    FOREVER
    {
        std::auto_ptr<Message> message(_link->updates().receive());
        if(!message.get())
        {
            // That was all.
            break;
        }
        std::auto_ptr<Packet> packet(App::protocol().interpret(*message));
        if(packet.get())
        {
            // It's always from the server.
            packet.get()->setFrom(message->address());
            processPacket(*packet.get());
        }
    }
}

void UserSession::listen()
{
    try
    {
        listenForUpdates();
    }
    catch(const ISerializable::DeserializationError&)
    {
        // Malformed packet!
        LOG_WARNING("Server sent nonsense.");
    }
}

void UserSession::clearOthers()
{
    FOR_EACH(i, _others, Others::iterator)
    {
        delete i->second;
    }
    _others.clear();
}
