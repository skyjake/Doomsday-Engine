/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sys_master.c: Communication with the Master Server
 *
 * Communication with the master server, using TCP and HTTP.
 * The HTTP requests run in their own threads.
 * Sockets were initialized by sys_network.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#ifdef WIN32
#	include <winsock.h>
#endif

#ifdef UNIX
#	include <sys/types.h>
#	include <sys/socket.h>
#endif

#include "de_base.h"
#include "de_network.h"
#include "de_system.h"
#include "de_console.h"
#include "de_misc.h"

#include "r_world.h"

// MACROS ------------------------------------------------------------------

// Communication with the master is done at 'below normal' priority.
#define MST_PRIORITY	-1

// TYPES -------------------------------------------------------------------

typedef struct serverlist_s {
	struct serverlist_s *next;
	serverinfo_t info;
} serverlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Master server info. Hardcoded defaults.
char	    *masterAddress = "www.doomsdayhq.com"; 
int			masterPort     = 0; // Uses 80 by default.
char	    *masterPath    = "/master.php";
boolean		masterAware    = false;		

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *responseOK = "HTTP/1.1 200";

// This variable will be true while a communication is in progress.
static volatile boolean communicating;

// A linked list of servers retrieved from the master.
static serverlist_t *servers;
static int numServers;

// CODE --------------------------------------------------------------------

/*
 * N_MasterClearList
 */
void N_MasterClearList(void)
{
	numServers = 0;
	while(servers)
	{
		serverlist_t *next = servers->next;
		free(servers);
		servers = next;
	}
}

/*
 * N_MasterNewServer
 */
serverlist_t *N_MasterNewServer(void)
{
	serverlist_t *node;

	numServers++;
	node = calloc(sizeof(serverlist_t), 1);
	node->next = servers;
	servers = node;
	return node;
}

/*
 * N_MasterSendAnnouncement
 *	A thread. Announcement info received as the parameter. The info must be
 *	allocated from the heap. We will free it when it's no longer needed.
 */
int N_MasterSendAnnouncement(void *parm)
{
	serverinfo_t *info = parm;
    socket_t s;
    struct hostent *host;
	ddstring_t msg;
	char buf[256];
	unsigned int length;

	// Get host information.
	if((host = N_SockGetHost(masterAddress)) == NULL)
	{
		// Could not find the host.
		return false;
	}

	// Create a socket.
	if((s = N_SockNewStream()) == INVALID_SOCKET)
	{
		// Couldn't create a new socket.
		return false;
	}

	// Connect.
	if(!N_SockConnect(s, host, masterPort? masterPort : 80))
	{
		// Connection failed.
		return false;
	}

	// Convert the serverinfo into plain text.
	Str_Init(&msg);
	length = Sv_InfoToString(info, &msg);

	// Free the serverinfo, it's no longer needed. 
	free(info);

	// Write an HTTP POST request with our info.
	N_SockPrintf(s, "POST %s HTTP/1.1\n", masterPath);
	N_SockPrintf(s, "Host: %s\n", masterAddress);
	N_SockPrintf(s, "Connection: close\n");
	N_SockPrintf(s, "Content-Type: application/x-deng-announce\n");
	N_SockPrintf(s, "Content-Length: %i\n\n", length);
	send(s, Str_Text(&msg), length, 0);
	Str_Free(&msg);

	// Wait for a response.
	length = recv(s, buf, sizeof(buf), 0);
/*	
	memset(buf, 0, sizeof(buf));
	while(recv(s, buf, sizeof(buf) - 1, 0) > 0)
	{
		Con_Printf(buf);
		memset(buf, 0, sizeof(buf));
	}
*/
	N_SockClose(s);

	// The communication ends.
	communicating = false;

	// If the master responds "OK" return true, otherwise false.
	return length >= strlen(responseOK) 
		&& !strncmp(buf, responseOK, strlen(responseOK));
}

/*
 * N_MasterDecodeChunked
 *	Response is an HTTP response with the chunked transfer encoding.
 *	Output is the plain body of the response.
 */
void N_MasterDecodeChunked(ddstring_t *response, ddstring_t *out)
{
	ddstring_t line;
	const char *pos = Str_Text(response);
	boolean foundChunked = false;
	int length;

	// Skip to the body.
	Str_Init(&line);
	while(*pos)
	{
		pos = Str_GetLine(&line, pos);

		// Let's make sure the encoding is chunked.
		// RFC 2068 says that HTTP/1.1 clients must ignore encodings
		// they don't understand.
		if(!stricmp(Str_Text(&line), "Transfer-Encoding: chunked"))
			foundChunked = true;

		// The first break indicates the end of the headers.
		if(!Str_Length(&line)) break;
	}
	if(foundChunked)
	{
		// Decode the body.
		while(*pos)
		{
			// The first line of the chunk is the length.
			pos = Str_GetLine(&line, pos);
			length = strtol(Str_Text(&line), 0, 16);
			if(!length) break; // No more chunks.

			// Append the chunk data to the output.
			Str_PartAppend(out, pos, 0, length);
			pos += length;

			// A newline ends the chunk.
			pos = (const char*) M_SkipLine((char*)pos);
		}
	}
	Str_Free(&line);
}

/*
 * N_MasterParseResponse
 *	Parses a list of servers from the response.
 */
void N_MasterParseResponse(ddstring_t *response)
{
	ddstring_t msg, line;
	const char *pos;
	serverinfo_t *info = NULL;

	Str_Init(&msg);
	Str_Init(&line);	

	// Clear the list of servers.
	N_MasterClearList();

	// The response must be OK.
	if(strncmp(Str_Text(response), responseOK, strlen(responseOK)))
	{
		// This is not valid.
		return;
	}

	// Extract the body of the response.
	N_MasterDecodeChunked(response, &msg);

	// The syntax of the response is simple:
	// label:value
	// One or more empty lines separate servers.
	pos = Str_Text(&msg);
	while(*pos)
	{
		pos = Str_GetLine(&line, pos);

		if(Str_Length(&line) && !info)
		{
			// A new server begins.
			info = &N_MasterNewServer()->info;
		}
		else if(!Str_Length(&line) && info)
		{
			// No more current server.
			info = NULL;
			continue;
		}

		// If there is no current server, skip everything.
		if(!info) continue;

		Sv_StringToInfo(Str_Text(&line), info);
	}
	
	Str_Free(&line);
	Str_Free(&msg);
}

/*
 * N_MasterSendRequest
 *	A thread.
 */
int N_MasterSendRequest(void *parm)
{
    struct hostent *host;
    socket_t s;
	ddstring_t response;
	char buf[128];

	// Get host information.
	if((host = N_SockGetHost(masterAddress)) == NULL)
	{
		// Could not find the host.
		return false;
	}

	// Create a TCP stream socket.
	if((s = N_SockNewStream()) == INVALID_SOCKET)
	{
		// Couldn't create a new socket.
		return false;
	}

	// Connect.
	if(!N_SockConnect(s, host, masterPort? masterPort : 80))
	{
		// Connection failed.
		return false;
	}

	// Write a HTTP GET request.
	N_SockPrintf(s, "GET %s?list HTTP/1.1\n", masterPath);
	N_SockPrintf(s, "Host: %s\n", masterAddress);
	// We want the connection to close as soon as everything has been 
	// received.
	N_SockPrintf(s, "Connection: close\n\n\n");

	// Receive the entire list.
	Str_Init(&response);
	memset(buf, 0, sizeof(buf));
	while(recv(s, buf, sizeof(buf) - 1, 0) > 0)
	{
		Str_Append(&response, buf);
		memset(buf, 0, sizeof(buf));
	}
	N_SockClose(s);

	// Let's parse the message.
	N_MasterParseResponse(&response);

	// We're done with the parsing.
	Str_Free(&response);
	communicating = false;
	return true;
}

/*
 * N_MasterInit
 */
void N_MasterInit(void)
{
	communicating = false;
}

/*
 * N_MasterShutdown
 */
void N_MasterShutdown(void)
{
	// Free the server list. (What if communicating?)
	N_MasterClearList();
}

/*
 * N_MasterAnnounceServer
 *	Sends a server announcement to the master. The announcement includes
 *	our IP address and other information.
 */
void N_MasterAnnounceServer(boolean isOpen)
{
	serverinfo_t *info;

	if(isClient) return; // Must be a server.

	// Are we already communicating with the master at the moment?
	if(communicating)
	{
		if(verbose) Con_Printf("N_MasterAnnounceServer: Request already in "
			"progress.\n");
		return;
	}

	// The communication begins.
	communicating = true;

	// The info is not freed in this function, but in 
	// N_MasterSendAnnouncement().
	info = calloc(sizeof(serverinfo_t), 1);

	// Let's figure out what we want to tell about ourselves.
	Sv_GetInfo(info);

	if(!isOpen) info->canJoin = false;

	// The announcement thread runs at 'below normal' priority.
	Sys_StartThread(N_MasterSendAnnouncement, info, MST_PRIORITY);
}

/*
 * N_MasterRequestList
 *	Requests the list of open servers from the master.
 */
void N_MasterRequestList(void)
{
	// Are we already communicating with the master at the moment?
	if(communicating)
	{
		if(verbose) Con_Printf("N_MasterRequestList: Request already "
			"in progress.\n");
		return;
	}

	// The communication begins. Will be cleared when the list is ready
	// for use.
	communicating = true;

	// Start a new thread for the request.
	Sys_StartThread(N_MasterSendRequest, NULL, MST_PRIORITY);
}

/*
 * N_MasterGet
 *	Returns information about the server #N. If 'info' is NULL, returns
 *	the number of known servers. Otherwise returns true if the server
 *	index was valid and data was returned. 
 *
 *	Returns negative if a communication is in progress.
 */
int N_MasterGet(int index, serverinfo_t *info)
{
	serverlist_t *it;

	if(communicating) return -1;
	if(!info) return numServers;

	// Find the index'th entry.
	for(it = servers; index > 0 && it; index--, it = it->next);
	
	// Was the index valid?
	if(!it) 
	{
		memset(info, 0, sizeof(*info));
		return false; 
	}

	// Make a copy of the info.
	memcpy(info, &it->info, sizeof(*info));
	return true;
}
