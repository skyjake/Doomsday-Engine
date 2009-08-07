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

Session::Session() : world_(0)
{
    // Create a blank world.
    world_ = App::game().SYMBOL(deng_NewWorld)();
}

Session::~Session()
{
    if(!users_.empty())
    {
        RecordPacket sessionEnded("session.ended");
        for(Users::iterator i = users_.begin(); i != users_.end(); ++i)
        {
            i->second->client().link().observers.remove(this);
            i->second->setSession(0);
            
            // Inform that the session has ended.
            i->second->client().updates() << sessionEnded;
            
            delete i->second;
        }
        users_.clear();
    }
    std::cout << "Deleting world\n";
    delete world_;
}

void Session::processCommand(Client& sender, const de::CommandPacket& packet)
{
    try
    {
        std::cout << "Processing '" << packet.command() << "' with args:\n" << packet.arguments();
        
        if(packet.command() == "session.new")
        {
            // Initialize the session with the provided settings.
            world_->loadMap(packet.arguments().value<TextValue>("map"));

            // Respond.
            Record* reply = new Record();
            reply->addText("id", id_);
            App::protocol().reply(sender, Protocol::OK, reply);
        }
        else if(packet.command() == "session.join")
        {
            // Request to join this session?
            if(Id(packet.arguments().value<TextValue>("id")) != id_)
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
        Writer(welcome.record().addBlock("worldState").value<BlockValue>()) << *world_;
        Record& userStateRec = welcome.record().addRecord("users");
        for(Users::iterator i = users_.begin(); i != users_.end(); ++i)
        {
            Writer(userStateRec.addBlock(i->second->user().id()).value<BlockValue>())
                << i->second->user();
        }
        client.updates() << welcome;

        RemoteUser* remote = new RemoteUser(client, this);
        std::cout << "Id of new remote user: " << remote->id() << "\n";
        users_[remote->id()] = remote;

        // Start observing when this link closes.
        client.link().observers.add(this);

        return *remote;
    }
}

void Session::demote(RemoteUser& remoteUser)
{
    users_.erase(remoteUser.id());
    remoteUser.setSession(0);
    
    // Stop observing.
    remoteUser.client().link().observers.remove(this);

    // Update the others.
    RecordPacket userLeft("user.left");
    userLeft.record().addText("id", Id(remoteUser.id()));
    broadcast() << userLeft;
}

RemoteUser& Session::userByAddress(const de::Address& address) const
{
    for(Users::const_iterator i = users_.begin(); i != users_.end(); ++i)
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
    for(Users::iterator i = users_.begin(); i != users_.end(); ++i)
    {
        if(&i->second->client().link() == &link)
        {
            // This user's link has been closed. The remote user will disappear.
            delete i->second;
            users_.erase(i);
            return;
        }
    }
    std::cout << "Session::linkBeingDeleted: " << link.peerAddress() << " not used by any user\n";
}

void Session::describe(de::Record& record) const
{
    // User names and identifiers in a dictionary.
    DictionaryValue& dict = record.addDictionary("users").value<DictionaryValue>();
    for(Users::const_iterator i = users_.begin(); i != users_.end(); ++i)
    {
        dict.add(new TextValue(i->first), new TextValue(i->second->user().name()));
    }
}

void Session::Broadcast::send(const de::IByteArray& data)
{
    for(Users::iterator i = session_.users_.begin(); i != session_.users_.end(); ++i)
    {
        if(i->second != exclude_)
        {
            i->second->client().updates() << data;
        }
    }
}
