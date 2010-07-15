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

static byte *readbuffer;

// Mini-Msg routines.
static void SetReadBuffer(byte *data)
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
    return SHORT( *(short *) (readbuffer - 2) );
}

static int ReadLong(void)
{
    readbuffer += 4;
    return LONG( *(int *) (readbuffer - 4) );
}

static void Read(byte *buf, size_t len)
{
    memcpy(buf, readbuffer, len);
    readbuffer += len;
}

/**
 * This is where clients start their InFine sequences.
 */
void Cl_Finale(int packetType, byte* data)
{
    int gameState = 0;
    void* extraData = 0;
    byte flags, extraDataFlags = 0;
    boolean haveExtraData = false;
    byte* script = 0;

    SetReadBuffer(data);
    flags = ReadByte();

    if(packetType == PSV_FINALE2)
    {
        // Read extra data.
        extraDataFlags = ReadByte();

        haveExtraData = (extraDataFlags > 0? true:false);
        if(haveExtraData)
        {
            ddhook_finale_script_deserialize_extradata_t p;

            extraData = (void*) malloc(FINALE_SCRIPT_EXTRADATA_SIZE);

            memset(&p, 0, sizeof(p));
            p.inBuf = readbuffer;
            p.gameState = &gameState;
            p.extraData = extraData;
            p.bytesRead = 0;

            if(Plug_DoHook(HOOK_FINALE_SCRIPT_DESERIALIZE_EXTRADATA, extraDataFlags, &p))
            {
                readbuffer += FINALE_SCRIPT_EXTRADATA_SIZE;
            }
            else
            {
                haveExtraData = false;
            }
        }
    }

    if(flags & FINF_SCRIPT)
    {
        /**
         * @fixme: Need to rethink memory management here.
         *
         * Read the script into map-scope memory. It will be freed
         * when the next map is loaded.
         */
        script = Z_Malloc(strlen((char*)readbuffer) + 1, PU_MAP, 0);
        strcpy((char*)script, (char*)readbuffer);

        if(flags & FINF_BEGIN)
        {   // Start the script.
            finale_mode_t mode = ((flags & FINF_AFTER) ? FIMODE_AFTER : (flags & FINF_OVERLAY) ? FIMODE_OVERLAY : FIMODE_BEFORE);
            FI_ScriptBegin((const char*)script, mode, gameState, extraData);
        }
    }

    if(flags & FINF_END)
    {   // Stop InFine.
        FI_ScriptTerminate();
    }

    if(flags & FINF_SKIP)
    {
        FI_SkipRequest();
    }

    if(extraData)
        free(extraData);
}
