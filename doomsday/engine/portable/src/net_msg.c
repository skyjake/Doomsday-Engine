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
 * net_msg.c: Network Messaging
 *
 * Buffer overflow checks *ARE NOT* made ifndef _DEBUG
 * The caller must know what it's doing.
 * The data is stored using little-endian ordering.
 *
 * Note that negative values are not good for the packed write/read routines,
 * as they always have the high bits set.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"
#include "de_console.h"

Writer* msgWriter;
Reader* msgReader;

void Msg_Begin(int type)
{
    if(msgReader)
    {
        // End reading the netbuffer automatically.
        Msg_EndRead();
    }

    // The previous write must have been ended by now.
    assert(msgWriter == NULL);

    // Allocate a new writer.
    msgWriter = Writer_NewWithNetworkBuffer();
    netBuffer.msg.type = type;
}

boolean Msg_BeingWritten(void)
{
    return msgWriter != NULL;
}

void Msg_End(void)
{
    if(msgWriter)
    {
        // Finalize the netbuffer.
        netBuffer.length = Writer_Size(msgWriter);
        Writer_Delete(msgWriter);
        msgWriter = 0;
    }
    else
    {
        Con_Error("Msg_End: No message being written.\n");
    }
}

void Msg_BeginRead(void)
{
    if(msgWriter)
    {
        // End writing the netbuffer automatically.
        Msg_End();
    }

    // Start reading from the netbuffer.
    assert(msgReader == NULL);
    msgReader = Reader_NewWithNetworkBuffer();
}

void Msg_EndRead(void)
{
    if(msgReader)
    {
        Reader_Delete(msgReader);
        msgReader = 0;
    }
}
