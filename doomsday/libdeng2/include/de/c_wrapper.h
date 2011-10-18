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

#ifndef LIBDENG2_C_WRAPPER_H
#define LIBDENG2_C_WRAPPER_H

#include "libdeng2.h"

/**
 * @file Defines the C wrapper API for libdeng2 classes. Legacy code can use
 * this wrapper API to access libdeng2 functionality. Note that the identifiers
 * in this file are not in the de namespace.
 *
 * @note The basic de data types (e.g., dint32) are not available for the C API;
 * instead, only the standard C data types should be used.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LegacyCore
 */
DENG2_OPAQUE(LegacyCore)

DENG2_PUBLIC LegacyCore* LegacyCore_New(int* argc, char** argv);
DENG2_PUBLIC int LegacyCore_RunEventLoop(LegacyCore* lc, void (*loopFunc)(void));
DENG2_PUBLIC void LegacyCore_Stop(LegacyCore* lc, int exitCode);
DENG2_PUBLIC void LegacyCore_Delete(LegacyCore* lc);

/*
 * LegacyNetwork
 */
DENG2_OPAQUE(LegacyNetwork)

DENG2_PUBLIC int LegacyNetwork_OpenServerSocket(unsigned short port);
DENG2_PUBLIC int LegacyNetwork_Accept(int serverSocket);
DENG2_PUBLIC int LegacyNetwork_Open(const char* ipAddress, unsigned short port);
DENG2_PUBLIC void LegacyNetwork_GetPeerAddress(int socket, char* host, int hostMaxSize, unsigned short* port);
DENG2_PUBLIC int LegacyNetwork_IsDisconnected(int socket);
DENG2_PUBLIC void LegacyNetwork_Close(int socket);

DENG2_PUBLIC int LegacyNetwork_Send(int socket, const unsigned char* data, int size);
DENG2_PUBLIC unsigned char* LegacyNetwork_Receive(int socket, int* size);
DENG2_PUBLIC void LegacyNetwork_FreeBuffer(unsigned char* buffer);
DENG2_PUBLIC int LegacyNetwork_BytesReady(int socket);

DENG2_PUBLIC int LegacyNetwork_NewSocketSet();
DENG2_PUBLIC void LegacyNetwork_DeleteSocketSet(int set);
DENG2_PUBLIC void LegacyNetwork_SocketSet_Add(int set, int socket);
DENG2_PUBLIC void LegacyNetwork_SocketSet_Remove(int set, int socket);
DENG2_PUBLIC int LegacyNetwork_SocketSet_Activity(int set);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG2_C_WRAPPER_H
