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

#include <de/async.h>
#include <de/filesystem.h>
#include <de/folder.h>
#include <de/message.h>
#include <de/remotefeedprotocol.h>

using namespace de;

DE_PIMPL(RemoteFeedUser)
{
    using QueryId = RemoteFeedQueryPacket::Id;

    struct Transfer
    {
        QueryId queryId;
        Block data;
        duint64 position = 0;

        Transfer(QueryId id = 0) : queryId(id)
        {}
    };

    std::unique_ptr<Socket> socket;
    RemoteFeedProtocol protocol;
    LockableT<List<Transfer>> transfers;

    Impl(Public *i, Socket *s) : Base(i), socket(s)
    {
        LOG_NET_MSG("Setting up RemoteFeedUser %p") << thisPublic;

        // The RemoteFeed protocol does not require ordered messages.
        socket->setRetainOrder(false);

        s->audienceForMessage() += [this]() { receiveMessages(); };
        s->audienceForAllSent() += [this]() { continueFileTransfers(); };
        s->audienceForStateChange() += [this]() {
            if (!socket->isOpen())
            {
                DE_NOTIFY_PUBLIC_VAR(Disconnect, i) { i->userDisconnected(self()); }
            }
        };

        // We took over an open socket, there may already be messages waiting.
        receiveMessages();
    }

    void receiveMessages()
    {
        DE_ASSERT_IN_MAIN_THREAD();

        LOG_AS("RemoteFeedUser");
        while (socket->hasIncoming())
        {
            try
            {
                std::unique_ptr<Message> message { socket->receive() };
                std::shared_ptr<Packet>  packet  { protocol.interpret(*message) };

                LOG_NET_MSG("received packet '%s'") << packet->type();

                if (protocol.recognize(*packet) == RemoteFeedProtocol::Query)
                {
                    async([this, packet] ()
                    {
                        return handleQueryAsync(packet->as<RemoteFeedQueryPacket>());
                    },
                    [this] (Packet *response)
                    {
                        if (std::unique_ptr<Packet> p { response })
                        {
                            socket->sendPacket(*p);
                        }
                        else
                        {
                            continueFileTransfers();
                        }
                    });
                }
            }
            catch (const Error &er)
            {
                LOG_NET_ERROR("Error during query: %s") << er.asText();
            }
        }
    }

    void continueFileTransfers()
    {
        DE_ASSERT_IN_MAIN_THREAD();
        try
        {
            if (socket->bytesBuffered() > 0) return; // Too soon.

            std::unique_ptr<RemoteFeedFileContentsPacket> response;

            // Send the next block of the first file in the transfer queue.
            {
                DE_GUARD(transfers);

                if (transfers.value.isEmpty()) return;

                response.reset(new RemoteFeedFileContentsPacket);

                const dsize blockSize = 128 * 1024;
                auto &xfer = transfers.value.front();

                response->setId(xfer.queryId);
                response->setFileSize(xfer.data.size());
                response->setStartOffset(xfer.position);
                response->setData(xfer.data.mid(xfer.position, blockSize));

                xfer.position += response->data().size();
                if (xfer.position >= xfer.data.size())
                {
                    // That was all.
                    transfers.value.pop_front();
                }
            }

            if (response)
            {
                socket->sendPacket(*response);
            }
        }
        catch (const Error &er)
        {
            LOG_NET_ERROR("Error during file transfer to %s: %s")
                    << socket->peerAddress().asText()
                    << er.asText();
        }
    }

    Packet *handleQueryAsync(const RemoteFeedQueryPacket &query)
    {
        // Note: This is executed in a background thread.
        try
        {
            // Make sure the file system is ready for use. Waiting is ok because this is
            // called via de::async.
            FS::waitForIdle();

            std::unique_ptr<RemoteFeedMetadataPacket> response;

            switch (query.query())
            {
            case RemoteFeedQueryPacket::ListFiles:
                response.reset(new RemoteFeedMetadataPacket);
                response->setId(query.id());
                if (const auto *folder = FS::tryLocate<Folder const>(query.path()))
                {
                    response->addFolder(*folder);
                }
                else
                {
                    LOG_NET_WARNING("%s not found!") << query.path();
                }
                LOG_NET_MSG("%s") << response->metadata().asText();
                return response.release();

            case RemoteFeedQueryPacket::FileContents: {
                Transfer xfer(query.id());
                if (const auto *file = FS::tryLocate<File const>(query.path()))
                {
                    *file >> xfer.data;
                }
                else
                {
                    LOG_NET_WARNING("%s not found!") << query.path();
                }
                LOG_NET_MSG("New file transfer: %s size:%i")
                        << query.path()
                        << xfer.data.size();
                DE_GUARD(transfers);
                transfers.value.push_back(xfer);
                break; }
            }
        }
        catch (const Error &er)
        {
            LOG_NET_ERROR("Error while handling remote feed query from %s: %s")
                    << query.from().asText() << er.asText();
        }
        return nullptr;
    }
};

RemoteFeedUser::RemoteFeedUser(Socket *socket)
    : d(new Impl(this, socket))
{}

Address RemoteFeedUser::address() const
{
    DE_ASSERT(d->socket);
    return d->socket->peerAddress();
}
