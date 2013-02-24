/** @file sys_network.cpp Low-level network socket routines.
 * @ingroup network
 *
 * @todo Remove this source file entirely once dependent code is revised.
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

#include "clientapp.h"
#include "de_console.h"

using namespace de;

char *nptIPAddress = (char *) ""; ///< Address to connect to by default (cvar).
int   nptIPPort = 0;              ///< Port to connect to by default (cvar).

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
    return ClientApp::app().serverLink();
}

boolean N_GetHostInfo(int index, struct serverinfo_s *info)
{
    QList<Address> const listed = Net_ServerLink().foundServers();
    if(index < 0 || index >= listed.size())
        return false;
    Net_ServerLink().foundServerInfo(listed[index], info);
    return true;
}

int N_GetHostCount(void)
{
    return Net_ServerLink().foundServerCount();
}

/**
 * Called from "net info" (client-side).
 */
void N_PrintNetworkStatus(void)
{
    if(isClient)
    {
        Con_Message("CLIENT: Connected to server at %s.\n",
                    Net_ServerLink().address().asText().toAscii().constData());
    }
    else
    {
        Con_Message("OFFLINE: Single-player mode.\n");
    }

    N_PrintBufferInfo();
}
