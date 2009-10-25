/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2009 Daniel Swanson <danij@dengine.net>
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
 * cl_frame.c: Obsolete Frame Reception
 *
 * This file contains obsolete delta routines. They are preserved so that
 * backwards compatibility is retained with older versions of the network
 * protocol.
 *
 * These routines should be considered FROZEN. Do not change them.
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"

// MACROS ------------------------------------------------------------------

#define BIT(x)              (1 << (x))
#define MAX_ACKS            40

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int      gotFrame;
extern int      predicted_tics;
extern boolean  gotFirstFrame;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

/*
static int num_acks;
static byte acks[MAX_ACKS];
*/

// CODE --------------------------------------------------------------------

/**
 * Read and ack a PSV_FRAME delta set.
 *
 * THIS FUNCTION IS OBSOLETE (PSV_FRAME is no longer used)
 */
void Cl_ReadDeltaSet(void)
{
#if 0
    byte        present = Msg_ReadByte();
    byte        set = Msg_ReadByte();

    // Add the set number to the list of acks.
    if(num_acks < MAX_ACKS)
        acks[num_acks++] = set;

    if(present & BIT(DT_MOBJ))
        while(Cl_ReadMobjDelta());
    if(present & BIT(DT_PLAYER))
        while(Cl_ReadPlayerDelta());
    if(present & BIT(DT_LUMP))
        while(Cl_ReadLumpDelta());
    if(present & BIT(DT_SECTOR_R6))
        while(Cl_ReadSectorDelta());
    if(present & BIT(DT_SIDE_R6))
        while(Cl_ReadSideDelta());
    if(present & BIT(DT_POLY))
        while(Cl_ReadPolyDelta());
#endif
}

/**
 * Reads a PSV_FRAME packet from the message buffer and applies the deltas
 * in it. Also acks the sets in the packet immediately.
 *
 * THIS FUNCTION IS OBSOLETE (PSV_FRAME is no longer used)
 */
void Cl_FrameReceived(void)
{
#if 0
    int         i, frametime;

    gotFrame = true;
    gotFirstFrame = true;

#if _DEBUG
if(!gameReady)
    Con_Message("Got frame but GAME NOT READY!\n");
#endif

    // Frame time, lowest byte of gametic.
    frametime = Msg_ReadByte();

    num_acks = 0;
    while(!Msg_End())
        Cl_ReadDeltaSet();

    // Acknowledge all sets.
    Msg_Begin(PCL_ACK_SETS);
    for(i = 0; i < num_acks; ++i)
        Msg_WriteByte(acks[i]);
    Net_SendBuffer(0, 0);

    // Reset the predict counter.
    predicted_tics = 0;
#endif
}
