/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/c_wrapper.h"
#include "de/LegacyCore"
#include "de/LegacyNetwork"
#include "de/Address"
#include "de/ByteRefArray"
#include "de/Block"
#include <cstring>

#define DENG2_LEGACYNETWORK()   de::LegacyCore::instance().network()

LegacyCore* LegacyCore_New(int* argc, char** argv)
{
    return reinterpret_cast<LegacyCore*>(new de::LegacyCore(*argc, argv));
}

int LegacyCore_RunEventLoop(LegacyCore* lc, void (*loopFunc)(void))
{
    DENG2_SELF(LegacyCore, lc);
    return self->runEventLoop(loopFunc);
}

void LegacyCore_Stop(LegacyCore *lc, int exitCode)
{
    DENG2_SELF(LegacyCore, lc);
    self->stop(exitCode);
}

void LegacyCore_Delete(LegacyCore* lc)
{
    if(lc)
    {
        // It gets stopped automatically.
        delete reinterpret_cast<de::LegacyCore*>(lc);
    }
}

int LegacyNetwork_OpenServerSocket(unsigned short port)
{
    try
    {
        return DENG2_LEGACYNETWORK().openServerSocket(port);
    }
    catch(const de::Error& er)
    {
        LOG_AS("LegacyNetwork_OpenServerSocket");
        LOG_WARNING(er.asText());
        return 0;
    }
}

int LegacyNetwork_Accept(int serverSocket)
{
    return DENG2_LEGACYNETWORK().accept(serverSocket);
}

int LegacyNetwork_Open(const char* ipAddress, unsigned short port)
{
    return DENG2_LEGACYNETWORK().open(de::Address(ipAddress, port));
}

void LegacyNetwork_GetPeerAddress(int socket, char* host, int hostMaxSize, unsigned short* port)
{
    de::Address peer = DENG2_LEGACYNETWORK().peerAddress(socket);
    std::memset(host, 0, hostMaxSize);
    std::strncpy(host, peer.host().toString().toAscii().constData(), hostMaxSize - 1);
    if(port) *port = peer.port();
}

void LegacyNetwork_Close(int socket)
{
    DENG2_LEGACYNETWORK().close(socket);
}

int LegacyNetwork_Send(int socket, const unsigned char* data, int size)
{
    return DENG2_LEGACYNETWORK().sendBytes(socket, de::ByteRefArray(data, size));
}

unsigned char* LegacyNetwork_Receive(int socket, int *size)
{
    de::Block data;
    if(DENG2_LEGACYNETWORK().receiveBlock(socket, data))
    {
        // Successfully got a block of data. Return a copy of it.
        unsigned char* buffer = new unsigned char[data.size()];
        std::memcpy(buffer, data.constData(), data.size());
        *size = data.size();
        return buffer;
    }
    else
    {
        // We did not receive anything.
        *size = 0;
        return 0;
    }
}

void LegacyNetwork_FreeBuffer(unsigned char* buffer)
{
    delete [] buffer;
}

int LegacyNetwork_IsDisconnected(int socket)
{
    if(!socket) return 0;
    return !DENG2_LEGACYNETWORK().isOpen(socket);
}

int LegacyNetwork_BytesReady(int socket)
{
    if(!socket) return 0;
    return DENG2_LEGACYNETWORK().incomingForSocket(socket);
}

int LegacyNetwork_NewSocketSet()
{
    return DENG2_LEGACYNETWORK().newSocketSet();
}

void LegacyNetwork_DeleteSocketSet(int set)
{
    DENG2_LEGACYNETWORK().deleteSocketSet(set);
}

void LegacyNetwork_SocketSet_Add(int set, int socket)
{
    DENG2_LEGACYNETWORK().addToSet(set, socket);
}

void LegacyNetwork_SocketSet_Remove(int set, int socket)
{
    DENG2_LEGACYNETWORK().removeFromSet(set, socket);
}

int LegacyNetwork_SocketSet_Activity(int set)
{
    if(!set) return 0;
    return DENG2_LEGACYNETWORK().checkSetForActivity(set);
}
