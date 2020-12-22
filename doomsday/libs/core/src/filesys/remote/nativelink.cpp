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

#include "de/filesys/nativelink.h"

#include "de/app.h"
#include "de/filesystem.h"
#include "de/loop.h"
#include "de/message.h"
#include "de/remotefeed.h"
#include "de/remotefeedprotocol.h"
#include "de/socket.h"

namespace de { namespace filesys {

const char *NativeLink::URL_SCHEME = "doomsday:";

static const char *PATH_SERVER_REPOSITORY_ROOT = "/sys/server/public"; // serverside folder

DE_PIMPL(NativeLink)
, DE_OBSERVES(Socket, StateChange)
, DE_OBSERVES(Socket, Message)
, DE_OBSERVES(Socket, Error)
{
    RemoteFeedProtocol protocol;
    Socket             socket;

    Impl(Public *i) : Base(i) {}

    void socketStateChanged(Socket &, Socket::SocketState state) override
    {
        switch (state)
        {
        case Socket::Connected:
            self().wasConnected();
            break;

        case Socket::Disconnected:
            self().wasDisconnected();
            break;

        default:
            break;
        }
    }

    void error(Socket &, const String &errorMessage) override
    {
        self().handleError(errorMessage);
    }

    void messagesIncoming(Socket &) override
    {
        Loop::mainCall([this]() { receiveMessages(); });
    }

    void receiveMessages()
    {
        while (socket.hasIncoming())
        {
            try
            {
                DE_ASSERT_IN_MAIN_THREAD();

                std::unique_ptr<Message> response(socket.receive());
                std::unique_ptr<Packet>  packet  (protocol.interpret(*response));

                if (!packet) continue;

                switch (protocol.recognize(*packet))
                {
                case RemoteFeedProtocol::Metadata: {
                    const auto &md = packet->as<RemoteFeedMetadataPacket>();
                    self().metadataReceived(md.id(), md.metadata());
                    break; }

                case RemoteFeedProtocol::FileContents: {
                    const auto &fc = packet->as<RemoteFeedFileContentsPacket>();
                    self().chunkReceived(fc.id(), fc.startOffset(), fc.data(), fc.fileSize());
                    break; }

                default:
                    break;
                }
            }
            catch (const Error &er)
            {
                LOG_NET_ERROR("Error when handling remote feed response: %s")
                        << er.asText();
            }
        }
        self().cleanupQueries();
    }
};

NativeLink::NativeLink(const String &address)
    : Link(address)
    , d(new Impl(this))
{
    d->socket.audienceForStateChange() += d;
    d->socket.audienceForError() += d;
    d->socket.audienceForMessage() += d;

    DE_ASSERT(address.beginsWith(URL_SCHEME));
    d->socket.open(address.substr(BytePos(strlen(URL_SCHEME))));
}

Link *NativeLink::construct(const String &address)
{
    if (address.beginsWith(URL_SCHEME))
    {
        return new NativeLink(address);
    }
    return nullptr;
}

void NativeLink::setLocalRoot(const String &rootPath)
{
    Link::setLocalRoot(rootPath);

    auto &root = localRoot();
    root.attach(new RemoteFeed(address(), PATH_SERVER_REPOSITORY_ROOT));
    root.populate(Folder::PopulateAsyncFullTree);
}

PackagePaths NativeLink::locatePackages(const StringList &packageIds) const
{
    PackagePaths remotePaths;
    for (const String &pkg : packageIds)
    {
        // Available packages have been populated as remote files.
        if (File *rem = FS::tryLocate<File>(DE_STR("/remote/server") / pkg))
        {
            // Remote path not needed because local folders are fully populated with
            // remote files.
            remotePaths.insert(pkg, RepositoryPath(*this, rem->path(), ""));
        }
    }
    return remotePaths;
}

LoopResult filesys::NativeLink::forPackageIds(std::function<LoopResult (const String &)> func) const
{
    return FS::locate<Folder>(DE_STR("/remote/server"))
        .forContents([&func](String name, File &) -> LoopResult {
            if (auto result = func(name))
            {
                return result;
            }
            return LoopContinue;
        });
}

void NativeLink::wasConnected()
{
    d->socket << ByteRefArray("RemoteFeed", 10);
    Link::wasConnected();
}

void NativeLink::transmit(const Query &query)
{
    DE_ASSERT(query.isValid());

    RemoteFeedQueryPacket packet;
    packet.setId(query.id);
    packet.setPath(query.path);
    if (query.fileMetadata)
    {
        packet.setQuery(RemoteFeedQueryPacket::ListFiles);
    }
    else if (query.fileContents)
    {
        packet.setQuery(RemoteFeedQueryPacket::FileContents);
    }
    d->socket.sendPacket(packet);
}

}} // namespace de::filesys
