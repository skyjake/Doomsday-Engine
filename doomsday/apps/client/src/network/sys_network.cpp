/** @file sys_network.cpp  Low-level network socket routines (deprecated).
 *
 * @todo Remove this source file entirely once dependent code is revised.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "network/sys_network.h"

#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include "clientapp.h"
#include "network/net_buf.h"

using namespace de;

void N_Register(void)
{}

ServerLink &Net_ServerLink(void)
{
    return ClientApp::app().serverLink();
}

/**
 * Called from "net info" (client-side).
 */
void N_PrintNetworkStatus(void)
{
    if (isClient)
    {
        LOG_NET_NOTE(_E(b) "CLIENT: " _E(.) "Connected to server at %s") << Net_ServerLink().address();
    }
    else
    {
        LOG_NET_NOTE(_E(b) "OFFLINE: " _E(.) "Single-player mode");
    }
    N_PrintBufferInfo();
}
