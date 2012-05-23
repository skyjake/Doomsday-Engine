/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * @file c_wrapper.h
 * libdeng2 C wrapper.
 *
 * Defines a C wrapper API for (some of the) libdeng2 classes. Legacy code
 * can use this wrapper API to access libdeng2 functionality. Note that the
 * identifiers in this file are _not_ in the de namespace.
 *
 * @note The basic de data types (e.g., dint32) are not available for the C
 * API; instead, only the standard C data types should be used.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LegacyCore
 */
DENG2_OPAQUE(LegacyCore)

// Log levels (see de::Log for description).
typedef enum legacycore_loglevel_e {
    DE2_LOG_TRACE,
    DE2_LOG_DEBUG,
    DE2_LOG_VERBOSE,
    DE2_LOG_MESSAGE,
    DE2_LOG_INFO,
    DE2_LOG_WARNING,
    DE2_LOG_ERROR,
    DE2_LOG_CRITICAL
} legacycore_loglevel_t;

DENG2_PUBLIC LegacyCore* LegacyCore_New(void* dengApp);
DENG2_PUBLIC void LegacyCore_Delete(LegacyCore* lc);
DENG2_PUBLIC void LegacyCore_SetLoopRate(LegacyCore* lc, int freqHz);
DENG2_PUBLIC void LegacyCore_SetLoopFunc(LegacyCore* lc, void (*callback)(void));
DENG2_PUBLIC void LegacyCore_PushLoop(LegacyCore* lc);
DENG2_PUBLIC void LegacyCore_PopLoop(LegacyCore* lc);
DENG2_PUBLIC void LegacyCore_PauseLoop(LegacyCore* lc);
DENG2_PUBLIC void LegacyCore_ResumeLoop(LegacyCore* lc);
DENG2_PUBLIC int LegacyCore_RunEventLoop(LegacyCore* lc);
DENG2_PUBLIC void LegacyCore_Stop(LegacyCore* lc, int exitCode);
DENG2_PUBLIC void LegacyCore_Timer(LegacyCore* lc, unsigned int milliseconds, void (*callback)(void));
DENG2_PUBLIC int LegacyCore_SetLogFile(LegacyCore* lc, const char* filePath);
DENG2_PUBLIC const char* LegacyCore_LogFile(LegacyCore* lc);
DENG2_PUBLIC void LegacyCore_PrintLogFragment(LegacyCore* lc, const char* text);
DENG2_PUBLIC void LegacyCore_PrintfLogFragmentAtLevel(LegacyCore* lc, legacycore_loglevel_t level, const char* format, ...);
DENG2_PUBLIC void LegacyCore_SetTerminateFunc(LegacyCore* lc, void (*func)(const char*));

/*
 * CommandLine
 */
DENG2_PUBLIC void CommandLine_Alias(const char* longname, const char* shortname);
DENG2_PUBLIC int CommandLine_Count(void);
DENG2_PUBLIC const char* CommandLine_At(int i);
DENG2_PUBLIC const char* CommandLine_Next(void);
DENG2_PUBLIC int CommandLine_Check(const char* check);
DENG2_PUBLIC int CommandLine_CheckWith(const char* check, int num);
DENG2_PUBLIC int CommandLine_Exists(const char* check);
DENG2_PUBLIC int CommandLine_IsOption(int i);
DENG2_PUBLIC int CommandLine_IsMatchingAlias(const char* original, const char* originalOrAlias);

/*
 * LogBuffer
 */
DENG2_PUBLIC void LogBuffer_EnableStandardOutput(int enable);
DENG2_PUBLIC void LogBuffer_Flush(void);

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

DENG2_PUBLIC int LegacyNetwork_Send(int socket, const void *data, int size);
DENG2_PUBLIC unsigned char* LegacyNetwork_Receive(int socket, int* size);
DENG2_PUBLIC void LegacyNetwork_FreeBuffer(unsigned char* buffer);
DENG2_PUBLIC int LegacyNetwork_BytesReady(int socket);

DENG2_PUBLIC int LegacyNetwork_NewSocketSet();
DENG2_PUBLIC void LegacyNetwork_DeleteSocketSet(int set);
DENG2_PUBLIC void LegacyNetwork_SocketSet_Add(int set, int socket);
DENG2_PUBLIC void LegacyNetwork_SocketSet_Remove(int set, int socket);
DENG2_PUBLIC int LegacyNetwork_SocketSet_Activity(int set);

/*
 * Info
 */
DENG2_OPAQUE(Info)

DENG2_PUBLIC Info* Info_NewFromString(const char* utf8text);
DENG2_PUBLIC Info* Info_NewFromFile(const char* nativePath);
DENG2_PUBLIC void Info_Delete(Info* info);
DENG2_PUBLIC int Info_FindValue(Info* info, const char* path, char* buffer, size_t bufSize);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG2_C_WRAPPER_H
