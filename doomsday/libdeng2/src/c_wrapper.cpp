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
    return DENG2_LEGACYNETWORK().openServerSocket(port);
}

int LegacyNetwork_Accept(int serverSocket)
{
    return DENG2_LEGACYNETWORK().accept(serverSocket);
}

int LegacyNetwork_Open(const char* ipAddress, unsigned short port)
{
    return DENG2_LEGACYNETWORK().open(de::Address(ipAddress, port));
}

void LegacyNetwork_Close(int socket)
{
    DENG2_LEGACYNETWORK().close(socket);
}

int LegacyNetwork_Send(int socket, const unsigned char* data, int size)
{
    return DENG2_LEGACYNETWORK().sendBytes(socket, de::ByteRefArray(data, size));
}

int LegacyNetwork_Receive(int socket, unsigned char* data, int size)
{
    de::ByteRefArray dest(data, size);
    return DENG2_LEGACYNETWORK().waitToReceiveBytes(socket, dest);
}

int LegacyNetwork_BytesReady(int socket)
{
    return DENG2_LEGACYNETWORK().bytesReadyForSocket(socket);
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

int LegacyNetwork_SocketSet_Activity(int set, int waitMs)
{
    return DENG2_LEGACYNETWORK().checkSetForActivity(set, de::Time::Delta::fromMilliSeconds(waitMs));
}
