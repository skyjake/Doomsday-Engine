/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
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
 * Client-side InFine.
 */
#include "de_base.h"
#include "de_network.h"
#include "de_infine.h"

static const byte* readbuffer;

// Mini-Msg routines.
static void SetReadBuffer(const byte* data)
{
    readbuffer = data;
}

static byte ReadByte(void)
{
    return *readbuffer++;
}

static short ReadShort(void)
{
    readbuffer += 2;
    return SHORT( *(const short*) (readbuffer - 2) );
}

static int ReadLong(void)
{
    readbuffer += 4;
    return LONG( *(const int*) (readbuffer - 4) );
}

static void Read(byte* buf, size_t len)
{
    memcpy(buf, readbuffer, len);
    readbuffer += len;
}

/**
 * This is where clients start their InFine sequences.
 */
void Cl_Finale(int packetType, const byte* data)
{
    byte flags;

    SetReadBuffer(data);
    flags = ReadByte();

    if(flags & (FINF_SCRIPT|FINF_BEGIN))
    {   // Start the script.
#pragma message("WARNING: Cl_Finale does not presently read the state condition flags")
        FI_Execute((const char*)readbuffer, FF_LOCAL);
    }

    if(flags & FINF_END)
    {
#pragma message("WARNING: Cl_Finale does not presently respond to FINF_END")
        //FI_ScriptTerminate();
    }

    if(flags & FINF_SKIP)
    {
#pragma message("WARNING: Cl_Finale does not presently respond to FINF_SKIP")
        //FI_ScriptRequestSkip();
    }
}
