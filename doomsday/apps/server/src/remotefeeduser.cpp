/** @file remotefeeduser.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "remotefeeduser.h"

#include <de/FileSystem>
#include <de/Folder>
#include <de/Message>
#include <de/RemoteFeedProtocol>

using namespace de;

DENG2_PIMPL(RemoteFeedUser)
{
    std::unique_ptr<Socket> socket;
    RemoteFeedProtocol protocol;

    Impl(Public *i, Socket *s) : Base(i), socket(s)
    {
        LOG_NET_MSG("Setting up RemoteFeedUser %p") << thisPublic;
        QObject::connect(s, &Socket::messagesReady, [this] () { receiveMessages(); });
        QObject::connect(s, &Socket::disconnected, [this] ()
        {
            DENG2_FOR_PUBLIC_AUDIENCE(Disconnect, i)
            {
                i->userDisconnected(self());
            }
        });

        // We took over an open socket, there may already be messages waiting.
        receiveMessages();
    }

    void receiveMessages()
    {
        LOG_AS("RemoteFeedUser");
        while (socket->hasIncoming())
        {
            try
            {
                std::unique_ptr<Message> message(socket->receive());
                std::unique_ptr<Packet>  packet(protocol.interpret(*message));

                LOG_NET_MSG("received packet '%s'") << packet->type();

                if (protocol.recognize(*packet) == RemoteFeedProtocol::Query)
                {
                    handleQuery(packet->as<RemoteFeedQueryPacket>());
                }
            }
            catch (Error const &er)
            {
                LOG_NET_ERROR("Error during query: %s") << er.asText();
            }
        }
    }

    void handleQuery(RemoteFeedQueryPacket const &query)
    {
        std::unique_ptr<RemoteFeedMetadataPacket> response(new RemoteFeedMetadataPacket);
        response->setId(query.id());

        switch (query.query())
        {
        case RemoteFeedQueryPacket::ListFiles:
            if (auto const *folder = FS::tryLocate<Folder const>(query.path()))
            {
                folder->forContents([&response] (String, File &file)
                {
                    response->addFile(file);
                    return LoopContinue;
                });
            }
            else
            {
                LOG_NET_WARNING("%s not found!") << query.path();
            }
            LOG_NET_MSG("%s") << response->metadata().asText();
            break;

        case RemoteFeedQueryPacket::FileContents:

            break;
        }

        LOG_NET_MSG("Sending response %i") << query.id();
        socket->sendPacket(*response);
    }
};

RemoteFeedUser::RemoteFeedUser(Socket *socket)
    : d(new Impl(this, socket))
{}

Address RemoteFeedUser::address() const
{
    DENG2_ASSERT(d->socket);
    return d->socket->peerAddress();
}
