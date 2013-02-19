/** @file remoteuser.cpp  User that is communicating with the server over a network socket.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "remoteuser.h"
#include "serversystem.h"
#include "network/net_buf.h"
#include "network/net_msg.h"
#include "network/net_event.h"
#include "server/sv_def.h"

#include <de/memory.h>
#include <de/Message>
#include <de/ByteRefArray>

#include <QCryptographicHash>

using namespace de;

DENG2_PIMPL(RemoteUser)
{
    enum State {
        Disconnected,
        Unjoined,
        Joined
    };

    Id id;
    Socket *socket;
    int protocolVersion;
    Address address;
    bool isFromLocal;
    State state;
    String name;

    Instance(Public *i, Socket *sock)
        : Base(i),
          socket(sock),
          state(Unjoined)
    {
        DENG2_ASSERT(socket != 0);

        QObject::connect(socket, SIGNAL(disconnected()), thisPublic, SLOT(socketDisconnected()));
        QObject::connect(socket, SIGNAL(messagesReady()), thisPublic, SLOT(handleIncomingPackets()));

        address = socket->peerAddress();
        isFromLocal = socket->isLocal();

        LOG_VERBOSE("New remote user %s from socket %s (local:%b)")
                << id << address << isFromLocal;
    }

    ~Instance()
    {
        delete socket;
    }

    void notifyClientExit()
    {
        netevent_t netEvent;
        netEvent.type = NE_CLIENT_EXIT;
        netEvent.id = id;
        N_NEPost(&netEvent);
    }

    void disconnect()
    {
        if(state == Disconnected) return;

        LOG_INFO("Closing connection to remote user %s (from %s)") << id << address;
        DENG2_ASSERT(socket->isOpen());

        if(state == Joined)
        {
            // Send a msg notifying of disconnection.
            Msg_Begin(PSV_SERVER_CLOSE);
            Msg_End();
            Net_SendBuffer(N_IdentifyPlayer(id), 0);

            // This causes a network event.
            notifyClientExit();
        }

        state = Disconnected;

        if(socket && socket->isOpen())
        {
            socket->close();
        }
    }

    /**
     * Validate and process the command, which has been sent by a remote agent.
     * If the command is invalid, the node is immediately closed.
     *
     * @return @c false to stop processing further incoming messages (for now).
     */
    bool handleRequest(Block const &command)
    {
        LOG_AS("handleRequest");

        serverinfo_t info;
        ddstring_t msg;

        int length = command.size();

        // If the command is too long, it'll be considered invalid.
        if(length >= 256)
        {
            self.deleteLater();
            return false;
        }

        // Status query?
        if(command == "Info?")
        {
            Sv_GetInfo(&info);
            Str_Init(&msg);
            Str_Appendf(&msg, "Info\n");
            Sv_InfoToString(&info, &msg);

            LOG_DEBUG("Info reply:\n%s") << Str_Text(&msg);

            self << ByteRefArray(Str_Text(&msg), Str_Length(&msg));

            Str_Free(&msg);
        }
        else if(length >= 5 && command.startsWith("Shell"))
        {
            if(length == 5)
            {
                // Password is not required for connections from the local computer.
                if(strlen(netPassword) > 0 && !isFromLocal)
                {
                    // Need to ask for a password, too.
                    self << ByteRefArray("Psw?", 4);
                    return true;
                }
            }
            else if(length > 5)
            {
                // A password was included.
                QByteArray supplied = command.mid(5);
                QByteArray pwd(netPassword, strlen(netPassword));
                if(supplied != QCryptographicHash::hash(pwd, QCryptographicHash::Sha1))
                {
                    // Wrong!
                    self.deleteLater();
                    return false;
                }
            }

            // This node will switch to shell mode: ownership of the socket is
            // passed to a ShellUser.
            App_ServerSystem().convertToShellUser(thisPublic);
            return false;
        }
        else if(length >= 10 && command.startsWith("Join ") && command[9] == ' ')
        {
            protocolVersion = command.mid(5, 4).toInt(0, 16);

            // Read the client's name and convert the network node into an actual
            // client. Here we also decide if the client's protocol is compatible
            // with ours.
            name = String::fromUtf8(Block(command.mid(10)));

            if(App_ServerSystem().isUserAllowedToJoin(self))
            {              
                state = Joined;

                // Successful! Send a reply.
                self << ByteRefArray("Enter", 5);

                // Inform the higher levels of this occurence.
                netevent_t netEvent;
                netEvent.type = NE_CLIENT_ENTRY;
                netEvent.id = id;
                N_NEPost(&netEvent);
            }
            else
            {
                // Couldn't join the game, so close the connection.
                self.deleteLater();
                return false;
            }
        }
        else
        {
            // Too bad, scoundrel! Goodbye.
            LOG_WARNING("Received an invalid request from %s.") << id;
            self.deleteLater();
            return false;
        }

        // Everything was OK.
        return true;
    }
};

RemoteUser::RemoteUser(Socket *socket) : d(new Instance(this, socket))
{}

RemoteUser::~RemoteUser()
{
    d->disconnect();

    delete d;
}

Id RemoteUser::id() const
{
    return d->id;
}

String RemoteUser::name() const
{
    return d->name;
}

Socket *RemoteUser::takeSocket()
{
    Socket *sock = d->socket;
    d->socket = 0;
    d->state = Instance::Disconnected; // not signaled
    return sock;
}

void RemoteUser::send(IByteArray const &data)
{
    if(d->state != Instance::Disconnected && d->socket->isOpen())
    {
        d->socket->send(data);
    }
}

void RemoteUser::handleIncomingPackets()
{
    LOG_AS("RemoteUser");
    forever
    {
        QScopedPointer<Message> packet(d->socket->receive());
        if(packet.isNull()) break;

        switch(d->state)
        {
        case Instance::Unjoined:
            // Let's see if it is a command we recognize.
            if(!d->handleRequest(*packet)) return;
            break;

        case Instance::Joined: {
            /// @todo Refactor the incoming message queue (net_buf) into something nicer.

            // Post the data into the queue.
            netmessage_t *msg = (netmessage_t *) M_Calloc(sizeof(netmessage_t));

            msg->sender = d->id;
            msg->data = new byte[packet->size()];
            memcpy(msg->data, packet->data(), packet->size());
            msg->size = packet->size();
            msg->handle = msg->data; // needs delete[]

            // The message queue will handle the message from now on.
            N_PostMessage(msg);
            break; }

        default:
            // Ignore the message.
            break;
        }
    }
}

void RemoteUser::socketDisconnected()
{
    d->state = Instance::Disconnected;
    d->notifyClientExit();

    deleteLater();
}

bool RemoteUser::isJoined() const
{
    return d->state == Instance::Joined;
}
