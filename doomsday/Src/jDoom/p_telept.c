// Emacs style mode select   -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log$
// Revision 1.8  2005/01/01 22:58:52  skyjake
// Resolved a bunch of compiler warnings
//
// Revision 1.7  2004/05/30 08:42:42  skyjake
// Tweaked indentation style
//
// Revision 1.6  2004/05/29 09:53:29  skyjake
// Consistent style (using GNU Indent)
//
// Revision 1.5  2004/05/28 19:52:58  skyjake
// Finished switch from branch-1-7 to trunk, hopefully everything is fine
//
// Revision 1.2.2.2  2004/05/16 10:01:37  skyjake
// Merged good stuff from branch-nix for the final 1.7.15
//
// Revision 1.2.2.1.2.1  2003/11/19 17:07:14  skyjake
// Modified to compile with gcc and -DUNIX
//
// Revision 1.2.2.1  2003/10/05 10:09:40  skyjake
// Cleanup
//
// Revision 1.2  2003/06/30 00:05:04  skyjake
// Use fixmom
//
// Revision 1.1  2003/02/26 19:22:04  skyjake
// Initial checkin
//
// Revision 1.1  2002/09/29 01:11:47  Jaakko
// Added Doomsday sources
//
//
// DESCRIPTION:
//  Teleportation.
//
//-----------------------------------------------------------------------------

#include "doomdef.h"

#include "s_sound.h"

#include "p_local.h"

// State.
#include "r_state.h"

//
// TELEPORTATION
//
int EV_Teleport(line_t *line, int side, mobj_t *thing)
{
	int     i;
	int     tag;
	mobj_t *m;
	mobj_t *fog;
	unsigned an;
	thinker_t *thinker;
	sector_t *sector;
	fixed_t oldx;
	fixed_t oldy;
	fixed_t oldz;

	// don't teleport missiles
	if(thing->flags & MF_MISSILE)
		return 0;

	// Don't teleport if hit back of line,
	//  so you can get out of teleporter.
	if(side == 1)
		return 0;

	tag = line->tag;
	for(i = 0; i < numsectors; i++)
	{
		if(sectors[i].tag == tag)
		{
			thinker = thinkercap.next;
			for(thinker = thinkercap.next; thinker != &thinkercap;
				thinker = thinker->next)
			{
				// not a mobj
				if(thinker->function != P_MobjThinker)
					continue;

				m = (mobj_t *) thinker;

				// not a teleportman
				if(m->type != MT_TELEPORTMAN)
					continue;

				sector = m->subsector->sector;
				// wrong sector
				if(sector - sectors != i)
					continue;

				oldx = thing->x;
				oldy = thing->y;
				oldz = thing->z;

				if(!P_TeleportMove(thing, m->x, m->y))
					return 0;

				thing->z = thing->floorz;	//fixme: not needed?
				if(thing->player)
					thing->dplayer->viewz =
						thing->z + thing->dplayer->viewheight;

				// spawn teleport fog at source and destination
				fog = P_SpawnMobj(oldx, oldy, oldz, MT_TFOG);
				S_StartSound(sfx_telept, fog);
				an = m->angle >> ANGLETOFINESHIFT;
				fog =
					P_SpawnMobj(m->x + 20 * finecosine[an],
								m->y + 20 * finesine[an], thing->z, MT_TFOG);

				// emit sound, where?
				S_StartSound(sfx_telept, fog);

				thing->angle = m->angle;
				thing->momx = thing->momy = thing->momz = 0;

				// don't move for a bit
				if(thing->player)
				{
					thing->reactiontime = 18;
					thing->dplayer->clAngle = thing->angle;
					thing->dplayer->clLookDir = 0;
					thing->dplayer->lookdir = 0;
					thing->dplayer->flags |=
						DDPF_FIXANGLES | DDPF_FIXPOS | DDPF_FIXMOM;
				}
				return 1;
			}
		}
	}
	return 0;
}
