/** @file sv_infine.cpp Server-side InFine.
 * @ingroup server
 *
 * @authors Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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

#include "de_network.h"
#include "de_infine.h"

/**
 * The actual script is sent to the clients. @a script can be NULL.
 */
void Sv_Finale(finaleid_t id, int flags, const char* script)
{
    size_t scriptLen = 0;

    if(isClient) return;

    // How much memory do we need?
    if(script)
    {
        flags |= FINF_SCRIPT;
        scriptLen = strlen(script);
    }

    // First the flags.
    Msg_Begin(PSV_FINALE);
    Writer_WriteByte(msgWriter, flags);
    Writer_WriteUInt32(msgWriter, id); // serverside Id

    if(script)
    {
        // Then the script itself.
        Writer_WriteUInt32(msgWriter, scriptLen);
        Writer_Write(msgWriter, script, scriptLen);
    }

    Msg_End();

    Net_SendBuffer(NSP_BROADCAST, 0);
}
