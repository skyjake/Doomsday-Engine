/** @file sys_network.cpp Low-level network socket routines.
 * @ingroup network
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#ifndef __CLIENT__
#  error "sys_network.cpp is only for __CLIENT__"
#endif

#include <errno.h>

#ifndef WIN32
#include <signal.h>
#endif

#include "de_platform.h"
#include "de_console.h"
#include "de_system.h"
#include "de_misc.h"

#include "dd_main.h"
#include "dd_def.h"
#include "network/net_main.h"
#include "network/net_event.h"
#include "network/net_demo.h"
#include "network/protocol.h"
#include "network/serverlink.h"
#include "client/cl_def.h"
#include "map/p_players.h"

#include <de/Address>
#include <de/Socket>
#include <de/shell/ServerFinder>

using namespace de;

char *nptIPAddress = (char *) ""; ///< Address to connect to by default (cvar).
int   nptIPPort = 0;              ///< Port to connect to by default (cvar).

static ServerLink *svLink;

void N_Register(void)
{
    C_VAR_CHARPTR("net-ip-address", &nptIPAddress, 0, 0, 0);
    C_VAR_INT("net-ip-port", &nptIPPort, CVF_NO_MAX, 0, 0);

#ifdef _DEBUG
    C_CMD("netfreq", NULL, NetFreqs);
#endif
}

ServerLink &Net_ServerLink(void)
{
    return *svLink;
}

void N_SystemInit(void)
{
    svLink = new ServerLink;
}

/**
 * Shut down the low-level network interface. Called during engine
 * shutdown (not before).
 */
void N_SystemShutdown(void)
{
    //svLink.disconnect();

    /*
    if(netGame)
    {
        // We seem to be shutting down while a netGame is running.
        Con_Execute(CMDS_DDAY, "net disconnect", true, false);
    }*/

    // Any queued messages will be destroyed.
    N_ClearMessages();

    // Let's forget about servers found earlier.
    //located.clear();

    delete svLink;
    svLink = 0;
}

boolean N_GetHostInfo(int index, struct serverinfo_s *info)
{
    QList<Address> const listed = svLink->foundServers();
    if(index < 0 || index >= listed.size())
        return false;
    svLink->foundServerInfo(listed[index], info);
    return true;
}

int N_GetHostCount(void)
{
    return svLink->foundServerCount();
}

/**
 * Called from "net info" (client-side).
 */
void N_PrintNetworkStatus(void)
{
    if(isClient)
    {
        Con_Message("CLIENT: Connected to server at %s.\n",
                    svLink->address().asText().toAscii().constData());
    }
    else
    {
        Con_Message("OFFLINE: Single-player mode.\n");
    }

    N_PrintBufferInfo();
}
