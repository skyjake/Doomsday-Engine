/** @file bossbrain.cpp  Playsim "Boss Brain" management.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 1999 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman (PrBoom 2.2.6)
 * @authors Copyright © 1999-2000 Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze (PrBoom 2.2.6)
 * @authors Copyright © 1993-1996 id Software, Inc.
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

#include "jdoom.h"
#include "bossbrain.h"

#include "p_saveg.h"

void P_BrainInitForMap()
{
    bossBrain.easy = 0; // Always init easy to 0.
    // Calling shutdown rather than clear allows us to free up memory.
    P_BrainShutdown();
}

void P_BrainClearTargets()
{
    bossBrain.numTargets = 0;
    bossBrain.targetOn = 0;
}

void P_BrainWrite(Writer *writer)
{
    DENG_ASSERT(writer != 0);

    // Not for us?
    if(!IS_SERVER) return;

    Writer_WriteByte(writer, 1); // Write a version byte.

    Writer_WriteInt16(writer, bossBrain.numTargets);
    Writer_WriteInt16(writer, bossBrain.targetOn);
    Writer_WriteByte(writer, bossBrain.easy!=0? 1:0);

    // Write the mobj references using the mobj archive.
    for(int i = 0; i < bossBrain.numTargets; ++i)
    {
        Writer_WriteInt16(writer, SV_ThingArchiveId(bossBrain.targets[i]));
    }
}

void P_BrainRead(Reader *reader, int mapVersion)
{
    DENG_ASSERT(reader != 0);

    // Not for us?
    if(!IS_SERVER) return;

    // No brain data before version 3.
    if(mapVersion < 3) return;

    P_BrainClearTargets();

    int ver = (mapVersion >= 8? Reader_ReadByte(reader) : 0);
    int numTargets;
    if(ver >= 1)
    {
        numTargets         = Reader_ReadInt16(reader);
        bossBrain.targetOn = Reader_ReadInt16(reader);
        bossBrain.easy     = (dd_bool)Reader_ReadByte(reader);
    }
    else
    {
        numTargets         = Reader_ReadByte(reader);
        bossBrain.targetOn = Reader_ReadByte(reader);
        bossBrain.easy     = false;
    }

    for(int i = 0; i < numTargets; ++i)
    {
        P_BrainAddTarget(SV_GetArchiveThing((int) Reader_ReadInt16(reader), 0));
    }
}

void P_BrainShutdown()
{
    Z_Free(bossBrain.targets); bossBrain.targets = 0;
    bossBrain.numTargets = 0;
    bossBrain.maxTargets = -1;
    bossBrain.targetOn = 0;
}

void P_BrainAddTarget(mobj_t *mo)
{
    DENG_ASSERT(mo != 0);

    if(bossBrain.numTargets >= bossBrain.maxTargets)
    {
        // Do we need to alloc more targets?
        if(bossBrain.numTargets == bossBrain.maxTargets)
        {
            bossBrain.maxTargets *= 2;
            bossBrain.targets = (mobj_t **)Z_Realloc(bossBrain.targets, bossBrain.maxTargets * sizeof(*bossBrain.targets), PU_APPSTATIC);
        }
        else
        {
            bossBrain.maxTargets = 32;
            bossBrain.targets = (mobj_t **)Z_Malloc(bossBrain.maxTargets * sizeof(*bossBrain.targets), PU_APPSTATIC, NULL);
        }
    }

    bossBrain.targets[bossBrain.numTargets++] = mo;
}
