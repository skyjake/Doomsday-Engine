/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * sv_sound.c: Serverside Sound Management
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_audio.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Tell clients to play a sound with full volume.
 */
void Sv_Sound(int sound_id, mobj_t *origin, int toPlr)
{
	Sv_SoundAtVolume(sound_id, origin, 1, toPlr);
}

/*
 * Finds the sector/polyobj to whom the origin mobj belong.
 */
void Sv_IdentifySoundOrigin(mobj_t **origin, int *sector, int *poly)
{
	*sector = *poly = -1;

	if(*origin && !(*origin)->thinker.id)
	{
		// No mobj ID => it's not a real mobj.
		if((*poly = PO_GetNumForDegen(*origin)) < 0)
		{
			// It wasn't a polyobj degenmobj, try the sectors instead.
			*sector = R_GetSectorNumForDegen(*origin);
		}
#ifdef _DEBUG
		if(*poly < 0 && *sector < 0)
		{
			Con_Error("Sv_IdentifySoundOrigin: Bad mobj.\n");
		}
#endif
		*origin = NULL;
	}
}

/*
 * Tell clients to play a sound.
 */
void Sv_SoundAtVolume
	(int sound_id_and_flags, mobj_t *origin, float volume, int toPlr)
{
	int sound_id = (sound_id_and_flags & ~DDSF_FLAG_MASK);
	int sector, poly;

	if(isClient || !sound_id) return;

	Sv_IdentifySoundOrigin(&origin, &sector, &poly);

	Sv_NewSoundDelta(sound_id, origin, sector, poly, volume, 
		(sound_id_and_flags & DDSF_REPEAT) != 0,
		toPlr & SVSF_TO_ALL? -1 : (toPlr & 0xf));
}

/*
 * This is called when the server needs to tell clients to stop 
 * a sound.
 */
void Sv_StopSound(int sound_id, mobj_t *origin)
{
	int sector, poly;	

	if(isClient) return;

	Sv_IdentifySoundOrigin(&origin, &sector, &poly);

/*#ifdef _DEBUG
	Con_Printf("Sv_StopSound: id=%i origin=%i(%p) sec=%i poly=%i\n",
		sound_id, origin? origin->thinker.id : 0, origin,
		sector, poly);
#endif*/

	// Send the stop sound delta to everybody.
	// Volume zero means silence.
	Sv_NewSoundDelta(sound_id, origin, sector, poly, 0, false, -1);
}

