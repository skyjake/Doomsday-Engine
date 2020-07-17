/** @file sv_infine.cpp  Server-side InFine.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "server/sv_infine.h"

#include "network/net_main.h"
#include "network/net_msg.h"
#include "ui/infine/finale.h"

#include <doomsday/network/protocol.h>

using namespace de;

void Sv_Finale(finaleid_t id, dint flags, const char *script)
{
    if (netState.isClient) return;

    // How much memory do we need?
    dsize scriptLen = 0;
    if (script)
    {
        flags |= FINF_SCRIPT;
        scriptLen = strlen(script);
    }

    // First the flags.
    Msg_Begin(PSV_FINALE);
    Writer_WriteByte(::msgWriter, flags);
    Writer_WriteUInt32(::msgWriter, id); // serverside Id

    if (script)
    {
        // Then the script itself.
        Writer_WriteUInt32(::msgWriter, scriptLen);
        Writer_Write(::msgWriter, script, scriptLen);
    }

    Msg_End();

    Net_SendBuffer(NSP_BROADCAST, 0);
}
