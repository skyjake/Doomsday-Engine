/** @file cl_infine.cpp  Clientside InFine.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "client/cl_infine.h"
#include "network/net_main.h"
#include "network/net_msg.h"
#include "ui/infine/finaleinterpreter.h"

#include <doomsday/network/protocol.h>
#include <de/legacy/memory.h>

static finaleid_t currentFinale;
static finaleid_t remoteFinale;

finaleid_t Cl_CurrentFinale()
{
    return currentFinale;
}

void Cl_Finale(Reader1 *msg)
{
    LOG_AS("Cl_Finale");

    byte *script = 0;

    const int flags           = Reader_ReadByte(msg);
    const finaleid_t finaleId = Reader_ReadUInt32(msg);

    if (flags & FINF_SCRIPT)
    {
        // Read the script into map-scope memory. It will be freed
        // when the next map is loaded.
        int len = Reader_ReadUInt32(msg);
        script = (byte *) M_Malloc(len + 1);
        Reader_Read(msg, script, len);
        script[len] = 0;
    }

    if ((flags & FINF_SCRIPT) && (flags & FINF_BEGIN))
    {
        // Start the script.
        currentFinale = FI_Execute((const char*)script, FF_LOCAL);
        remoteFinale = finaleId;
        LOGDEV_NET_MSG("Started finale %i (remote id %i)") << currentFinale << remoteFinale;
    }

    /// @todo Wouldn't hurt to make sure that the server is talking about the
    /// same finale as before... (check remoteFinale)

    if ((flags & FINF_END) && currentFinale)
    {
        FI_ScriptTerminate(currentFinale);
        currentFinale = 0;
        remoteFinale = 0;
    }

    if ((flags & FINF_SKIP) && currentFinale)
    {
        FI_ScriptRequestSkip(currentFinale);
    }

    if (script) M_Free(script);
}

void Cl_RequestFinaleSkip()
{
    // First the flags.
    Msg_Begin(PCL_FINALE_REQUEST);
    Writer_WriteUInt32(msgWriter, remoteFinale);
    Writer_WriteUInt16(msgWriter, 1); // skip
    Msg_End();

    LOGDEV_NET_MSG("Requesting skip on finale %i") << remoteFinale;

    Net_SendBuffer(0, 0);
}
