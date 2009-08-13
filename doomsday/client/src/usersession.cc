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
    : link_(link), sessionId_(id), world_(0), user_(0)
{
    // Create a blank user and world. The user is configured jointly
    // from configuration and by the game. The world is mirrored from
    // the server's session when we join.
    user_ = App::game().SYMBOL(deng_NewUser)();
    world_ = App::game().SYMBOL(deng_NewWorld)();
    
    // The server will tell our id.
    user_->setId(Id::NONE);
    
    // Ask to join the session.
    CommandPacket join("session.join");
    join.arguments().addText("id", id);

    // Include our initial user state in the arguments.
    Writer(join.arguments().addBlock("userState").value<BlockValue>()) << *user_;

    RecordPacket* response;
    App::app().protocol().decree(*link_, join, &response);

    // Get the user id.
    user_->setId(response->valueAsText("userId"));
}   

UserSession::~UserSession()
{
    if(sessionId_)
    {
        // Inform that we are leaving.
        link_->base() << CommandPacket("session.leave");
    }
    clearOthers();

    delete user_;
    delete world_;    
    delete link_;
}

void UserSession::processPacket(const de::Packet& packet)
{
    const RecordPacket* record = dynamic_cast<const RecordPacket*>(&packet);
    if(record)
    {
        const Record& rec = record->record();
        std::cout << record->label() << "\n" << rec << "\n";

        if(record->label() == "user.welcome")
        {
            // State of the world.
            Reader(rec.value<BlockValue>("worldState")) >> *world_;
            
            // State of existing users.
            clearOthers();
            const Record& existingUsers = rec.subrecord("users");
            for(Record::Members::const_iterator i = existingUsers.members().begin();
                i != existingUsers.members().end(); ++i)
            {
                User* remoteUser = App::game().SYMBOL(deng_NewUser)();
                others_[Id(i->first)] = remoteUser;                
                Reader(i->second->value<BlockValue>()) >> *remoteUser;
            }
        }
        else if(record->label() == "user.joined")
        {
            User* remoteUser = App::game().SYMBOL(deng_NewUser)();
            others_[Id(record->valueAsText("id"))] = remoteUser;
            
            // State of the new user.
            Reader(rec.value<BlockValue>("userState")) >> *remoteUser;
        }
        else if(record->label() == "user.left")
        {
            Others::iterator found = others_.find(Id(rec.value<TextValue>("id")));
            if(found != others_.end())
            {
                delete found->second;
                others_.erase(found);
            }
        }
        else if(record->label() == "session.ended")
        {
            std::cout << "The session ended!\n";
            sessionId_ = Id::NONE;
            /// @throw SessionEndedError The user session is no longer valid due to
            /// the serverside session having ended.
            throw SessionEndedError("UserSession::processPacket", 
                "Serverside session ended");
        }
    }
}

void UserSession::listenForUpdates()
{
    for(;;)
    {
        std::auto_ptr<Message> message(link_->updates().receive());
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
        std::cout << "Server sent nonsense.\n";
    }
}

void UserSession::clearOthers()
{
    for(Others::iterator i = others_.begin(); i != others_.end(); ++i)
    {
        delete i->second;
    }
    others_.clear();
}
