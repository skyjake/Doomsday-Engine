/**
 * @file sys_network.h
 * Low-level network socket routines. @ingroup network
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_SYSTEM_NETWORK_H
#define LIBDENG_SYSTEM_NETWORK_H

#ifndef __CLIENT__
#  error "sys_network.h requires __CLIENT__"
#endif

#include "dd_share.h"
#include "net_buf.h"
#include "net_main.h"
#include "monitor.h"
#include "serverlink.h"

extern boolean  allowSending;
extern int      maxQueuePackets;

extern char    *nptIPAddress;
extern int      nptIPPort;

ServerLink &Net_ServerLink(void);

void    N_Register(void);

void    N_SystemInit(void);
void    N_SystemShutdown(void);
void    N_PrintInfo(void);
int     N_GetHostCount(void);
boolean N_GetHostInfo(int index, struct serverinfo_s *info);
void    N_PrintNetworkStatus(void);

#endif /* LIBDENG_SYSTEM_NETWORK_H */
