/**\file sys_master.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * Communication with the Master Server.
 *
 * Communication with the master server, using TCP and HTTP.
 * The HTTP requests run in their own threads.
 * Sockets were initialized by sys_network.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"

#ifdef WIN32
# define CURL_STATICLIB
#endif
#include <curl/curl.h>

#include "de_base.h"
#include "de_network.h"
#include "de_system.h"
#include "de_console.h"
#include "de_misc.h"

#include "r_world.h"

// MACROS ------------------------------------------------------------------

// Maximum time allowed time for a master server operation to take (seconds).
#define RESPONSE_TIMEOUT    15

// TYPES -------------------------------------------------------------------

typedef struct serverlist_s {
    struct serverlist_s *next;
    serverinfo_t info;
} serverlist_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

static void N_MasterClearList(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Master server info. Hardcoded defaults.
char   *masterAddress = "www.dengine.net";
int     masterPort = 0;         // Uses 80 by default.
char   *masterPath = "/master.php";
boolean masterAware = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// This variable will be true while a communication is in progress.
static volatile boolean communicating;

// A linked list of servers created from info retrieved from the master.
static serverlist_t *servers;
static int numServers;

// CODE --------------------------------------------------------------------

/**
 * Called by N_Init() while initializing the low-level network subsystem.
 */
void N_MasterInit(void)
{
	// Initialize libcurl.
	curl_global_init(CURL_GLOBAL_WIN32);

    communicating = false;
}

/**
 * Called by N_Shutdown() during engine shutdown.
 */
void N_MasterShutdown(void)
{
    // Free the server list. (What if communicating?)
    N_MasterClearList();

	// Clean up libcurl.
	curl_global_cleanup();
}

/**
 * Clears our copy of the server list returned by the master.
 */
static void N_MasterClearList(void)
{
    numServers = 0;
    while(servers)
    {
        serverlist_t *next = servers->next;

        M_Free(servers);
        servers = next;
    }
}

/**
 * Creates a new server and links it into our copy of the server list.
 *
 * @return                  Ptr to the newly created server.
 */
static serverlist_t *N_MasterNewServer(void)
{
    serverlist_t *node;

    numServers++;
    node = M_Calloc(sizeof(serverlist_t));
    node->next = servers;
    servers = node;
    return node;
}

/**
 * Callback function that sends outgoing data with libcurl.
 */
static size_t C_DECL N_MasterReadCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    ddstring_t *msg = stream;
    size_t bytes = size * nmemb;

    // Don't copy too much.
    bytes = MIN_OF(bytes, (unsigned)Str_Length(msg));
    memcpy(ptr, msg->str, bytes);

    // Remove the sent portion from the buffer.
    Str_Set(msg, msg->str + bytes);

    // Number of bytes written to the buffer.
    return bytes;
}

/**
 * Callback function that receives incoming data from libcurl.
 */
static size_t C_DECL N_MasterWriteCallback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	ddstring_t* response = stream;
	size_t bytes = size * nmemb;

	// Append the new data to the response.
	Str_Reserve(response, Str_Length(response) + bytes);
	memcpy(response->str, ptr, bytes);

	return bytes;
}

static void N_MasterGetUrl(char* url)
{
    sprintf(url, "http://%s:%i%s", masterAddress,
            (masterPort? masterPort : 80), masterPath);
#ifdef _DEBUG
	printf("%s\n", url);
#endif
}

/**
 * Attempts to parses a list of servers from the given text string.
 *
 * @param response          The string to be parsed.
 *
 * @return                  @c true, if successful.
 */
static int N_MasterParseResponse(ddstring_t *msg)
{
    ddstring_t  line;
    const char *pos;
    serverinfo_t *info = NULL;

    Str_Init(&line);

    // Clear the list of servers.
    N_MasterClearList();

    // The syntax of the response is simple:
    // label:value
    // One or more empty lines separate servers.
    pos = Str_Text(msg);
    if(*pos)
    {
        boolean isDone = false;
        while(!isDone)
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
            }

            if(info)
                Sv_StringToInfo(Str_Text(&line), info);

            if(!*pos)
                isDone = true;
        }
    }

    Str_Free(&line);
    return true;
}

/**
 * Send a request for the list of currently available servers.
 * This function runs as a thread.
 *
 * @param parm              Not used.
 *
 * @return                  @c true, if the request was sent
 *                          successfully.
 */
static int C_DECL N_MasterSendRequest(void *parm)
{
    ddstring_t  response;
	char		masterUrl[200];
    char        errorBuf[CURL_ERROR_SIZE];
    boolean		success = true;
	CURL	   *session;

	N_MasterGetUrl(masterUrl);
	strcat(masterUrl, "?list");

	Str_Init(&response);

	// Prepare the curl session for our HTTP GET request.
	session = curl_easy_init();
#ifdef _DEBUG
    curl_easy_setopt(session, CURLOPT_VERBOSE, true);
#endif
	curl_easy_setopt(session, CURLOPT_HEADER, false);
	curl_easy_setopt(session, CURLOPT_WRITEFUNCTION, N_MasterWriteCallback);
	curl_easy_setopt(session, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(session, CURLOPT_URL, masterUrl);
	curl_easy_setopt(session, CURLOPT_TIMEOUT, RESPONSE_TIMEOUT);
	curl_easy_setopt(session, CURLOPT_ERRORBUFFER, errorBuf);

	// Perform the operation.
	if(!curl_easy_perform(session))
	{
#ifdef _DEBUG
		printf("%s", Str_Text(&response));
#endif

		// Let's parse the message.
		N_MasterParseResponse(&response);
	}
	else
	{
		success = false;
		fprintf(outFile, "N_MasterSendRequest: %s\n", errorBuf);
	}

	// Cleanup the curl session.
	curl_easy_cleanup(session);
	session = NULL;

    // We're done with the parsing.
    Str_Free(&response);
    communicating = false;
    return success;
}

/**
 * This function runs as a thread.
 *
 * NOTE: The info must be allocated from the heap. We will free it when it's
 * no longer needed.
 *
 * @param parm              The announcement info to be sent.
 *
 * @return                  @c true, if the announcement was sent
 *                          successfully and the master responds "OK".
 */
static int C_DECL N_MasterSendAnnouncement(void *parm)
{
    serverinfo_t *info = parm;
    ddstring_t  msg;
	char		masterUrl[200];
    char        errorBuf[CURL_ERROR_SIZE];
    boolean		success = true;
	CURL	   *session;
    struct curl_slist* headers = 0;

    headers = curl_slist_append(0, "Content-Type: application/x-deng-announce");

	// Post a server announcement.
	N_MasterGetUrl(masterUrl);

    // Convert the serverinfo into plain text.
    Str_Init(&msg);
    Sv_InfoToString(info, &msg);
    // Free the serverinfo, it's no longer needed.
    M_Free(info);

	// Prepare the curl session for our HTTP POST request.
	session = curl_easy_init();
#ifdef _DEBUG
    curl_easy_setopt(session, CURLOPT_VERBOSE, true);
#endif
    curl_easy_setopt(session, CURLOPT_POST, true);
    curl_easy_setopt(session, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(session, CURLOPT_HEADER, false);
	curl_easy_setopt(session, CURLOPT_READFUNCTION, N_MasterReadCallback);
	curl_easy_setopt(session, CURLOPT_READDATA, &msg);
    curl_easy_setopt(session, CURLOPT_POSTFIELDSIZE, Str_Length(&msg));
	curl_easy_setopt(session, CURLOPT_URL, masterUrl);
	curl_easy_setopt(session, CURLOPT_TIMEOUT, RESPONSE_TIMEOUT);
	curl_easy_setopt(session, CURLOPT_ERRORBUFFER, errorBuf);

    if(curl_easy_perform(session))
    {
        // Failure.
		success = false;
		fprintf(outFile, "N_MasterSendAnnouncement: %s\n", errorBuf);
    }

    // Write an HTTP POST request with our info.
    /*
     N_SockPrintf(s, "POST %s HTTP/1.1\n", masterPath);
     N_SockPrintf(s, "Host: %s\n", masterAddress);
     N_SockPrintf(s, "Connection: close\n");
     N_SockPrintf(s, \n");
     N_SockPrintf(s, "Content-Length: %i\n\n", length);
     send(s, Str_Text(&msg), length, 0);
     */
    Str_Free(&msg);

	// Cleanup the curl session.
    curl_slist_free_all(headers);
    curl_easy_cleanup(session);
    session = NULL;

    // The communication ends.
    communicating = false;

    // If the master responds "OK" return true, otherwise false.
    return success;
}

/**
 * Sends a server announcement to the master. The announcement includes our
 * IP address and other information.
 *
 * @param isOpen            If @c true, then the server will be
 *                          visible on the server list for other clients to
 *                          find by querying the server list.
 */
void N_MasterAnnounceServer(boolean isOpen)
{
    serverinfo_t *info;

    if(isClient)
        return;                 // Must be a server.

    // Are we already communicating with the master at the moment?
    if(communicating)
    {
        if(verbose)
            Con_Printf("N_MasterAnnounceServer: Request already in "
                       "progress.\n");
        return;
    }

    // The communication begins.
    communicating = true;

    // The info is not freed in this function, but in
    // N_MasterSendAnnouncement().
    info = M_Calloc(sizeof(serverinfo_t));

    // Let's figure out what we want to tell about ourselves.
    Sv_GetInfo(info);

    if(!isOpen)
        info->canJoin = false;

    Sys_StartThread(N_MasterSendAnnouncement, info);
}

/**
 * Requests the list of open servers from the master.
 */
void N_MasterRequestList(void)
{
    // Are we already communicating with the master at the moment?
    if(communicating)
    {
        if(verbose)
            Con_Printf("N_MasterRequestList: Request already "
                       "in progress.\n");
        return;
    }

    // The communication begins. Will be cleared when the list is ready
    // for use.
    communicating = true;

    // Start a new thread for the request.
    Sys_StartThread(N_MasterSendRequest, NULL);
}

/**
 * Returns information about the server #N.
 *
 * @return                  @c 0, if communication with the master
 *                          is currently in progress.
 *                          If param info is @c NULL,, will return
 *                          the number of known servers ELSE, will return
 *                          @c not zero, if param index was valid
 *                          and the master returned info on the requested
 *                          server.
 */
int N_MasterGet(int index, serverinfo_t *info)
{
    serverlist_t *it;

    if(communicating)
        return -1;
    if(!info)
        return numServers;

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
