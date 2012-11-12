/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2009-2012 Daniel Swanson <danij@dengine.net>
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

/**
 * sys_sock.c: TCP/IP Sockets
 */

// HEADER FILES ------------------------------------------------------------

#ifdef WIN32
#   include <winsock.h>
#endif

#ifdef UNIX
#   include "de_platform.h"
#   include <unistd.h>
#   include <sys/socket.h>
#   include <sys/types.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <netdb.h>
#endif

#include <stdarg.h>
#include <stdio.h>

#include "de_base.h"
#include "sys_sock.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Called from N_Init().
 */
void N_SockInit(void)
{
#ifdef WIN32
    WSADATA wsaData;

    WSAStartup(MAKEWORD(1, 1), &wsaData);
    // \fixme Check the result... (who cares?)
#endif
}

/*
 * Called from N_Shutdown().
 */
void N_SockShutdown(void)
{
#ifdef WIN32
    WSACleanup();
#endif
}

/**
 * Don't print too long messages with one call.
 */
void N_SockPrintf(socket_t s, const char *format, ...)
{
#define BUFF_SIZE           512

    char        buf[BUFF_SIZE];
    int         length;
    va_list     args;

    // Print the message into the buffer.
    va_start(args, format);
    length = dd_vsnprintf(buf, BUFF_SIZE, format, args);
    va_end(args);

    if(!length)
    {   // Overflow or encoding error. Should we still be sending it anyway?
        length = BUFF_SIZE;
    }

    // Send it.
    send(s, buf, length, 0);

#undef BUFF_SIZE
}

struct hostent *N_SockGetHost(const char *hostName)
{
    struct sockaddr_in addr;
    struct hostent *host;

    // Clear the socket address.
    memset(&addr, 0, sizeof(addr));

    // Is it an IP address or name?
    addr.sin_addr.s_addr = inet_addr(hostName);

    // Get host information.
    if(addr.sin_addr.s_addr == INADDR_NONE)
    {
        // Try to get using DNS.
        if((host = gethostbyname(hostName)) == NULL)
        {
            //perror("Could not get host by name");
            return NULL;
        }
    }
    else
    {
        // Try to get using the IP address.
        if((host =
            gethostbyaddr((const char *) &addr.sin_addr,
                          sizeof(struct sockaddr_in), AF_INET)) == NULL)
        {
            //perror("Could not get host by address");
            return NULL;
        }
    }
    return host;
}

socket_t N_SockNewStream(void)
{
    return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
}

boolean N_SockConnect(socket_t s, struct hostent *host, unsigned short port)
{
    struct sockaddr_in addr;

    // The address to connect to.
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = *((struct in_addr *) *host->h_addr_list);
    // 80 is the default port.
    addr.sin_port = htons(port);

    // Let's try connecting.
    return connect(s, (struct sockaddr *) &addr,
                   sizeof(struct sockaddr)) != SOCKET_ERROR;
}

void N_SockClose(socket_t s)
{
#ifdef WIN32
    closesocket(s);
#elif defined UNIX
    close(s);
#endif
}
