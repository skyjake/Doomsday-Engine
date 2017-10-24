/** @file remote/nativelink.cpp
 *
 * @authors Copyright (c) 2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/filesys/NativeLink"

#include "de/App"
#include "de/Message"
#include "de/RemoteFeedProtocol"
#include "de/Socket"

namespace de {
namespace filesys {

DENG2_PIMPL(NativeLink)
{
    RemoteFeedProtocol protocol;
    Socket socket;

    Impl(Public *i) : Base(i) {}

    void receiveMessages()
    {
        while (socket.hasIncoming())
        {
            try
            {
                DENG2_ASSERT_IN_MAIN_THREAD();

                std::unique_ptr<Message> response(socket.receive());
                std::unique_ptr<Packet>  packet  (protocol.interpret(*response));

                if (!packet) continue;

                switch (protocol.recognize(*packet))
                {
                case RemoteFeedProtocol::Metadata: {
                    auto const &md = packet->as<RemoteFeedMetadataPacket>();
                    self().metadataReceived(md.id(), md.metadata());
                    break; }

                case RemoteFeedProtocol::FileContents: {
                    auto const &fc = packet->as<RemoteFeedFileContentsPacket>();
                    self().chunkReceived(fc.id(), fc.startOffset(), fc.data(), fc.fileSize());
                    break; }

                default:
                    break;
                }
            }
            catch (Error const &er)
            {
                LOG_NET_ERROR("Error when handling remote feed response: %s")
                        << er.asText();
            }
        }
        self().cleanupQueries();
    }
};

NativeLink::NativeLink(String const &address)
    : Link(address)
    , d(new Impl(this))
{
    QObject::connect(&d->socket, &Socket::connected,     [this] () { wasConnected(); });
    QObject::connect(&d->socket, &Socket::disconnected,  [this] () { wasDisconnected(); });
    QObject::connect(&d->socket, &Socket::error,         [this] (QString msg) { handleError(msg); });

    QObject::connect(&d->socket, &Socket::messagesReady, [this] () { d->receiveMessages(); });

    d->socket.open(address);
}

void NativeLink::wasConnected()
{
    d->socket << ByteRefArray("RemoteFeed", 10);
    Link::wasConnected();
}

void NativeLink::transmit(Query const &query)
{
    DENG2_ASSERT(query.isValid());

    RemoteFeedQueryPacket packet;
    packet.setId(query.id);
    packet.setPath(query.path);
    if (query.fileList)
    {
        packet.setQuery(RemoteFeedQueryPacket::ListFiles);
    }
    else if (query.fileContents)
    {
        packet.setQuery(RemoteFeedQueryPacket::FileContents);
    }
    d->socket.sendPacket(packet);
}

} // namespace filesys
} // namespace de
