
//**************************************************************************
//**
//** $Id$
//**
//** Communication with the master server, using TCP and HTTP.
//** The HTTP requests run in their own threads.
//** Sockets were initialized by sys_network.
//**
//** $Log$
//** Revision 1.2  2003/05/23 22:16:15  skyjake
//** DP8, various net-related improvements
//**
//** Revision 1.1  2003/03/09 16:03:51  skyjake
//** New master server mechanism
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <winsock.h>

#include "de_base.h"
#include "de_network.h"
#include "de_system.h"
#include "de_console.h"
#include "de_misc.h"

#include "r_world.h"

// MACROS ------------------------------------------------------------------

// Communication with the master is done at 'below normal' priority.
#define MST_PRIORITY	-1

#define VALID_LABEL_LEN	16
#define	TOKEN_LEN		128

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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static const char *responseOK = "HTTP/1.1 200";

// This variable will be true while a communication is in progress.
static boolean communicating;

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
	unsigned int length, i;

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
	//sprintf(buf, "addr:%s\n", info->address);
	sprintf(buf, "port:%i\n", info->port);
	Str_Append(&msg, buf);
	sprintf(buf, "name:%s\n", info->name);
	Str_Append(&msg, buf);
	sprintf(buf, "info:%s\n", info->description);
	Str_Append(&msg, buf);
	sprintf(buf, "ver:%i\n", info->version);
	Str_Append(&msg, buf);
	sprintf(buf, "game:%s\n", info->game);
	Str_Append(&msg, buf);
	sprintf(buf, "mode:%s\n", info->gameMode);
	Str_Append(&msg, buf);
	sprintf(buf, "setup:%s\n", info->gameConfig);
	Str_Append(&msg, buf);
	sprintf(buf, "iwad:%s\n", info->iwad);
	Str_Append(&msg, buf);
	sprintf(buf, "wcrc:%i\n", info->wadNumber);
	Str_Append(&msg, buf);
	sprintf(buf, "pwads:%s\n", info->pwads);
	Str_Append(&msg, buf);
	sprintf(buf, "map:%s\n", info->map);
	Str_Append(&msg, buf);
	sprintf(buf, "nump:%i\n", info->players);
	Str_Append(&msg, buf);
	sprintf(buf, "maxp:%i\n", info->maxPlayers);
	Str_Append(&msg, buf);
	sprintf(buf, "open:%i\n", info->canJoin);
	Str_Append(&msg, buf);
	sprintf(buf, "plrn:%s\n", info->clientNames);
	Str_Append(&msg, buf);
	for(i = 0; i < sizeof(info->data)/sizeof(info->data[0]); i++)
	{
		sprintf(buf, "data%i:%x\n", i, info->data[i]);
		Str_Append(&msg, buf);
	}
	length = Str_Length(&msg);

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
 * N_MasterGetLine
 */
char *N_MasterGetLine(char *pos, ddstring_t *line)
{
	char buf[2];

	// We'll append the chars one by one.
	memset(buf, 0, sizeof(buf));
	
	for(Str_Clear(line); *pos && *pos != '\n'; pos++) 
	{
		buf[0] = *pos;
		Str_Append(line, buf);
	}

	// Strip whitespace around the line.
	Str_Strip(line);

	// The newline is excluded.
	if(*pos == '\n') pos++;
	return pos;
}

/*
 * N_MasterDecodeChunked
 *	Response is an HTTP response with the chunked transfer encoding.
 *	Output is the plain body of the response.
 */
void N_MasterDecodeChunked(ddstring_t *response, ddstring_t *out)
{
	ddstring_t line;
	char *pos = Str_Text(response);
	boolean foundChunked = false;
	int length;

	// Skip to the body.
	Str_Init(&line);
	while(*pos)
	{
		pos = N_MasterGetLine(pos, &line);

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
			pos = N_MasterGetLine(pos, &line);
			length = strtol(Str_Text(&line), 0, 16);
			if(!length) break; // No more chunks.

			// Append the chunk data to the output.
			Str_PartAppend(out, pos, 0, length);
			pos += length;

			// A newline ends the chunk.
			pos = M_SkipLine(pos);
		}
	}
	Str_Free(&line);
}

/*
 * N_MasterTokenize
 *	Extracts the label and value from a string.
 *	'max' is the maximum allowed length of a token, including terminating \0.
 */
boolean N_MasterTokenize(ddstring_t *line, char *label, char *value, int max)
{
	char *src = Str_Text(line);
	char *colon = strchr(src, ':');

	// The colon must exist near the beginning.
	if(!colon || colon - src >= VALID_LABEL_LEN) return false;

	// Copy the label.
	memset(label, 0, max);
	strncpy(label, src, MIN_OF(colon - src, max - 1));

	// Copy the value.
	memset(value, 0, max);
	strncpy(value, colon + 1, 
		MIN_OF(Str_Length(line) - (colon - src + 1), max - 1));

	// Everything is OK.
	return true;
}

/*
 * N_MasterParseResponse
 *	Parses a list of servers from the response.
 */
void N_MasterParseResponse(ddstring_t *response)
{
	ddstring_t msg, line;
	char *pos, label[TOKEN_LEN], value[TOKEN_LEN];
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
		pos = N_MasterGetLine(pos, &line);

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

		// Extract the label and value. The maximum length of a value is
		// TOKEN_LEN. Labels are returned in lower case.
		if(!N_MasterTokenize(&line, label, value, sizeof(value)))
		{
			// Badly formed lines are ignored.
			continue;
		}

		if(!strcmp(label, "at"))
		{
			strncpy(info->address, value, sizeof(info->address) - 1);
		}
		else if(!strcmp(label, "port"))
		{
			info->port = strtol(value, 0, 0);
		}
		else if(!strcmp(label, "ver"))
		{
			info->version = strtol(value, 0, 0);
		}
		else if(!strcmp(label, "map"))
		{
			strncpy(info->map, value, sizeof(info->map) - 1);
		}
		else if(!strcmp(label, "game"))
		{
			strncpy(info->game, value, sizeof(info->game) - 1);
		}
		else if(!strcmp(label, "name"))
		{
			strncpy(info->name, value, sizeof(info->name) - 1);
		}
		else if(!strcmp(label, "info"))
		{
			strncpy(info->description, value, sizeof(info->description) - 1);
		}
		else if(!strcmp(label, "nump"))
		{
			info->players = strtol(value, 0, 0);
		}
		else if(!strcmp(label, "maxp"))
		{
			info->maxPlayers = strtol(value, 0, 0);
		}
		else if(!strcmp(label, "open"))
		{
			info->canJoin = strtol(value, 0, 0);
		}
		else if(!strcmp(label, "mode"))
		{
			strncpy(info->gameMode, value, sizeof(info->gameMode) - 1);
		}
		else if(!strcmp(label, "setup"))
		{
			strncpy(info->gameConfig, value, sizeof(info->gameConfig) - 1);
		}
		else if(!strcmp(label, "iwad"))
		{
			strncpy(info->iwad, value, sizeof(info->iwad) - 1);
		}
		else if(!strcmp(label, "wcrc"))
		{
			info->wadNumber = strtol(value, 0, 0);
		}
		else if(!strcmp(label, "pwads"))
		{
			strncpy(info->pwads, value, sizeof(info->pwads) - 1);
		}
		else if(!strcmp(label, "plrn"))
		{
			strncpy(info->clientNames, value, sizeof(info->clientNames) - 1);
		}
		else if(!strcmp(label, "data0"))
		{
			info->data[0] = strtol(value, 0, 16);
		}
		else if(!strcmp(label, "data1"))
		{
			info->data[1] = strtol(value, 0, 16);
		}
		else if(!strcmp(label, "data2"))
		{
			info->data[2] = strtol(value, 0, 16);
		}
		// Unknown labels are ignored.
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