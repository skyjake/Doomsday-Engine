/** @file
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
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

#include "de_platform.h"
#include "de_network.h"
#include "de_console.h"

#include <QList>

Writer* msgWriter;
Reader* msgReader;

/// An ongoing writer is pushed here if a new one is started before the
/// earlier one is finished.
static QList<Writer *> pendingWriters;

void Msg_Begin(int type)
{
    if(msgReader)
    {
        // End reading the netbuffer automatically.
        Msg_EndRead();
    }

    // An ongoing writer will have to wait.
    if(msgWriter)
    {
        pendingWriters.prepend(msgWriter);
        msgWriter = 0;
    }

    // Allocate a new writer.
    msgWriter = Writer_NewWithDynamicBuffer(1 /*type*/ + NETBUFFER_MAXSIZE);
    Writer_WriteByte(msgWriter, type);
}

boolean Msg_BeingWritten(void)
{
    return msgWriter != NULL;
}

void Msg_End(void)
{
    DENG_ASSERT(msgWriter != 0);

    // Finalize the netbuffer.
    // Message type is included as the first byte.
    netBuffer.length = Writer_Size(msgWriter) - 1 /*type*/;
    memcpy(&netBuffer.msg, Writer_Data(msgWriter), Writer_Size(msgWriter));
    Writer_Delete(msgWriter);
    msgWriter = 0;

    // Pop a pending writer off the stack.
    if(!pendingWriters.isEmpty())
    {
        msgWriter = pendingWriters.takeFirst();
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
