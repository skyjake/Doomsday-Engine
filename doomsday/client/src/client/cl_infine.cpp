/** @file
 *
 * @authors Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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

/**
 * Client-side InFine.
 */
#include "de_base.h"
#include "de_console.h"
#include "de_infine.h"

#include "client/cl_infine.h"
#include "network/net_main.h"
#include "network/net_msg.h"

static finaleid_t currentFinale = 0;
static finaleid_t remoteFinale = 0;

finaleid_t Cl_CurrentFinale(void)
{
    return currentFinale;
}

void Cl_Finale(Reader* msg)
{
    int flags = Reader_ReadByte(msg);
    byte* script = 0;
    int len;
    finaleid_t finaleId = Reader_ReadUInt32(msg);

    if(flags & FINF_SCRIPT)
    {
        // Read the script into map-scope memory. It will be freed
        // when the next map is loaded.
        len = Reader_ReadUInt32(msg);
        script = (byte *) malloc(len + 1);
        Reader_Read(msg, script, len);
        script[len] = 0;
    }

    if((flags & FINF_SCRIPT) && (flags & FINF_BEGIN))
    {
        // Start the script.
        currentFinale = FI_Execute((const char*)script, FF_LOCAL);
        remoteFinale = finaleId;
#ifdef _DEBUG
        Con_Message("Cl_Finale: Started finale %i (remote id %i).\n", currentFinale, remoteFinale);
#endif
    }

    /// @todo Wouldn't hurt to make sure that the server is talking about the
    /// same finale as before... (check remoteFinale)

    if((flags & FINF_END) && currentFinale)
    {
        FI_ScriptTerminate(currentFinale);
        currentFinale = 0;
        remoteFinale = 0;
    }

    if((flags & FINF_SKIP) && currentFinale)
    {       
        FI_ScriptRequestSkip(currentFinale);
    }

    if(script) free(script);
}

void Cl_RequestFinaleSkip(void)
{
    // First the flags.
    Msg_Begin(PCL_FINALE_REQUEST);
    Writer_WriteUInt32(msgWriter, remoteFinale);
    Writer_WriteUInt16(msgWriter, 1); // skip
    Msg_End();

#ifdef _DEBUG
    Con_Message("Cl_RequestFinaleSkip: Requesting skip on finale %i.\n", remoteFinale);
#endif

    Net_SendBuffer(0, 0);
}
