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

#include "session.h"
#include "serverapp.h"
#include "remoteuser.h"
#include <de/core.h>
#include <de/data.h>
#include <de/net.h>

using namespace de;

Session::Session() : _world(0)
{
    // Create a blank world.
    _world = App::game().SYMBOL(deng_NewWorld)();
}

Session::~Session()
{
    LOG_AS("Session::~Session");
    
    if(!_users.empty())
    {
        RecordPacket sessionEnded("session.ended");
        for(Users::iterator i = _users.begin(); i != _users.end(); ++i)
        {
            i->second->client().link().audienceForDeletion.remove(this);
            i->second->setSession(0);
            
            // Inform that the session has ended.
            i->second->client().updates() << sessionEnded;
            
            delete i->second;
        }
        _users.clear();
    }
    LOG_DEBUG("Deleting the world");
    delete _world;
}

void Session::processCommand(Client& sender, const de::CommandPacket& packet)
{
    LOG_AS("Session::processCommand");
    
    try
    {
        LOG_DEBUG("Processing '%s' with args:\n%s") << packet.command() << packet.arguments();
        
        if(packet.command() == "session.new")
        {
            // Initialize the session with the provided settings.
            _world->loadMap(packet.arguments().value<TextValue>("map"));

            LOG_DEBUG("Replying with session id.");

            // Respond.
            Record* reply = new Record();
            reply->addText("id", _id);
            App::protocol().reply(sender, Protocol::OK, reply);
        }
        else if(packet.command() == "session.join")
        {
            // Request to join this session?
            if(Id(packet.arguments().value<TextValue>("id")) != _id)
            {
                // Not intended for this session.
                return;
            }
            RemoteUser& newUser = promote(sender);
     
            // The sender has provided us with the initial state.
            Reader(packet.arguments().value<BlockValue>("userState")) >> newUser.user();
     
            // Update the others.
            RecordPacket userJoined("user.joined");
            userJoined.record().addText("id", newUser.id());
            Writer(userJoined.record().addBlock("userState").value<BlockValue>()) 
                << newUser.user();
            broadcast().exclude(&newUser) << userJoined;
     
            // Reply with the official user id.
            Record* reply = new Record();
            reply->addText("userId", newUser.id());
            App::protocol().reply(sender, Protocol::OK, reply);
        }
        else if(packet.command() == "session.leave")
        {
            delete &userByAddress(packet.from());
            // No reply necessary.
        }
    }
    catch(const Error& err)
    {
        // No go, pal.
        App::protocol().reply(sender, Protocol::FAILURE, err.asText());
    }
}

RemoteUser& Session::promote(Client& client)
{
    LOG_AS("Session::promote");
    
    try
    {
        userByAddress(client.peerAddress());
        /// @throw AlreadyPromotedError  The client already is a user in the session.
        throw AlreadyPromotedError("Session::promote", "Client from " + 
            client.peerAddress().asText() + " already is a user");
    }
    catch(const UnknownAddressError&)
    {
        // Compose a welcome packet for the new user.
        RecordPacket welcome("user.welcome");
        Writer(welcome.record().addBlock("worldState").value<BlockValue>()) << *_world;
        Record& userStateRec = welcome.record().addRecord("users");
        for(Users::iterator i = _users.begin(); i != _users.end(); ++i)
        {
            Writer(userStateRec.addBlock(i->second->user().id()).value<BlockValue>())
                << i->second->user();
        }
        client.updates() << welcome;

        RemoteUser* remote = new RemoteUser(client, this);
        _users[remote->id()] = remote;

        LOG_VERBOSE("Id of new remote user: ") << remote->id();

        // Start observing when this link closes.
        client.link().audienceForDeletion.add(this);

        return *remote;
    }
}

void Session::demote(RemoteUser& remoteUser)
{
    _users.erase(remoteUser.id());
    remoteUser.setSession(0);
    
    // Stop observing.
    remoteUser.client().link().audienceForDeletion.remove(this);

    // Update the others.
    RecordPacket userLeft("user.left");
    userLeft.record().addText("id", Id(remoteUser.id()));
    broadcast() << userLeft;
}

RemoteUser& Session::userByAddress(const de::Address& address) const
{
    for(Users::const_iterator i = _users.begin(); i != _users.end(); ++i)
    {
        if(i->second->address() == address)
        {
            return *i->second;
        }
    }
    /// @throw UnknownAddressError  No user has the address @a address.
    throw UnknownAddressError("Session::userByAddress", "No one has address " + address.asText());
}

void Session::linkBeingDeleted(de::Link& link)
{
    LOG_AS("Session::linkBeingDeleted");
    
    for(Users::iterator i = _users.begin(); i != _users.end(); ++i)
    {
        if(&i->second->client().link() == &link)
        {
            // This user's link has been closed. The remote user will disappear.
            delete i->second;
            _users.erase(i);
            return;
        }
    }
    LOG_DEBUG("%s not used by any user") << link.peerAddress();
}

void Session::describe(de::Record& record) const
{
    // User names and identifiers in a dictionary.
    DictionaryValue& dict = record.addDictionary("users").value<DictionaryValue>();
    for(Users::const_iterator i = _users.begin(); i != _users.end(); ++i)
    {
        dict.add(new TextValue(i->first), new TextValue(i->second->user().name()));
    }
}

void Session::Broadcast::send(const de::IByteArray& data)
{
    for(Users::iterator i = _session._users.begin(); i != _session._users.end(); ++i)
    {
        if(i->second != _exclude)
        {
            i->second->client().updates() << data;
        }
    }
}
