/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * sys_sock.h: TCP/IP Sockets
 */

#ifndef __DOOMSDAY_SYSTEM_SOCKET_H__
#define __DOOMSDAY_SYSTEM_SOCKET_H__

#include "dd_types.h"

// DJS - this was unsigned, however socket() will return a signed
// value depending on the outcome. Which we check against a signed
// value in several places in the code.
typedef int socket_t;

void            N_SockInit(void);
void            N_SockShutdown(void);
void            N_SockPrintf(socket_t s, const char *format, ...) PRINTF_F(2,3);
struct hostent *N_SockGetHost(const char *hostName);
socket_t        N_SockNewStream(void);
boolean         N_SockConnect(socket_t s, struct hostent *host,
                              unsigned short port);
void            N_SockClose(socket_t s);

#endif
