
//**************************************************************************
//**
//** P_XGLINE.C
//**
//** Extended Generalized Line Types.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <time.h>
#include <stdarg.h>

#ifdef __JDOOM__
#include "doomdef.h"
#include "p_local.h"
#include "doomstat.h"
#include "d_config.h"
#include "s_sound.h"
#include "m_random.h"
#include "p_inter.h"
#include "r_defs.h"
#include "g_game.h"
#endif

#ifdef __JHERETIC__
#include "Doomdef.h"
#include "P_local.h"
#include "settings.h"
#include "Soundst.h"
#endif

#include "d_net.h"
#include "p_xgline.h"
#include "p_xgsec.h"

// MACROS ------------------------------------------------------------------

#define XLTIMER_STOPPED	-1	// Timer stopped.

#define	EVTYPESTR(evtype) (evtype == XLE_CHAIN? "CHAIN" \
		: evtype == XLE_CROSS? "CROSS" \
		: evtype == XLE_USE? "USE" \
		: evtype == XLE_SHOOT? "SHOOT" \
		: evtype == XLE_HIT? "HIT" \
		: evtype == XLE_TICKER? "TICKER" : "???")

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void XL_ChangeTexture(line_t *line, int sidenum, int section, int texture);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

struct mobj_s		dummything;
int					xgDev;		// Print dev messages.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static linetype_t	typebuffer;
static char			msgbuf[80];

// CODE --------------------------------------------------------------------

/*
 * Debug message printer.
 */
void XG_Dev(const char *format, ...)
{
	static char buffer[2000];
	va_list args;

	if(!xgDev) return;
	va_start(args, format);
	vsprintf(buffer, format, args);
	strcat(buffer, "\n");
	Con_Message(buffer);
	va_end(args);
}

// Init XG data for the level.
void XG_Init(void)
{
	XL_Init();		// Init lines.
	XS_Init();		// Init sectors.
}

void XG_Ticker(void)
{
	XS_Ticker();	// Think for sectors.
	
	// Clients rely on the server, they don't do XG themselves.
	if(IS_CLIENT) return;

	XL_Ticker();	// Think for lines.
}

/*
 * This is called during an engine reset. Disables all XG functionality!
 */
void XG_Update(void)
{
	// Clients rely on the server, they don't do XG themselves.
	if(IS_CLIENT) return;

	XG_ReadTypes();
	XS_Update();
	XL_Update();
}

// Returns true if the type is defined.
linetype_t *XL_GetType(int id)
{
	linetype_t *ptr;

	// Try finding it from the DDXGDATA lump.
	ptr = XG_GetLumpLine(id);
	if(ptr) 
	{
		// Got it!
		memcpy(&typebuffer, ptr, sizeof(*ptr));
		return &typebuffer;	
	}
	// Does Doomsday have a definition for this?
	if(Def_Get(DD_DEF_LINE_TYPE, (char*) id, &typebuffer))
		return &typebuffer;
	// A definition was not found.
	return NULL;
}

int XG_RandomInt(int min, int max)
{
	float x;

	if(max == min) return max;
	x = M_Random() / 256.0f; // Never reaches 1.
	return (int) (min + x*(max - min) + x);
}

float XG_RandomPercentFloat(float value, int percent)
{
	float i = (2*M_Random()/255.0f - 1) * percent/100.0f;
	return value * (1 + i);
}

void XL_SetLineType(line_t *line, int id)
{
	if(XL_GetType(id))
	{
		XG_Dev("XL_SetLineType: Line %i, type %i.", line - lines, id);

		line->special = id;
		// Allocate memory for the line type data.
		if(!line->xg)
			line->xg = Z_Malloc(sizeof(xgline_t), PU_LEVEL, 0);					
		// Init the extended line state.
		line->xg->disabled = false;
		line->xg->timer = 0;
		line->xg->ticker_timer = 0;
		memcpy(&line->xg->info, &typebuffer, sizeof(linetype_t));
		// Initial active state.
		line->xg->active = (typebuffer.flags & LTF_ACTIVE) != 0;
		line->xg->activator = &dummything;
	}
	else if(id)
	{
		XG_Dev("XL_SetLineType: Line %i, type %i NOT DEFINED.", 
			line - lines, id);
	}
}

// Initialize extended lines for the map.
void XL_Init(void)
{
	int i;

	memset(&dummything, 0, sizeof(dummything));

	// Clients rely on the server, they don't do XG themselves.
	if(IS_CLIENT) return;

	for(i = 0; i < numlines; i++)
	{
		lines[i].xg = 0;
		XL_SetLineType(lines + i, lines[i].special);
	}
}

int XL_TraversePlanes(line_t *line, int reftype, int ref, int data,
					  void *context, int (*func)(sector_t *sector, 
					  boolean ceiling, int data, void *context))
{
	int i;

	XG_Dev("XL_TraversePlanes: Line %i, ref (%i, %i)",
		line - lines, reftype, ref);

	if(reftype == LPREF_NONE) return false; // This is not a reference!

	if(reftype == LPREF_MY_FLOOR)
		return func(line->frontsector, false, data, context);
	if(reftype == LPREF_MY_CEILING)
		return func(line->frontsector, true, data, context);
	if(reftype == LPREF_INDEX_FLOOR)
		return func(sectors + ref, false, data, context);
	if(reftype == LPREF_INDEX_CEILING)
		return func(sectors + ref, true, data, context);

	for(i = 0; i < numsectors; i++)
	{
		if(reftype == LPREF_ALL_FLOORS
			|| reftype == LPREF_ALL_CEILINGS)
		{
			if(!func(sectors + i, reftype == LPREF_ALL_CEILINGS, 
				data, context)) return false;
		}
		if((reftype == LPREF_TAGGED_FLOORS 
			|| reftype == LPREF_TAGGED_CEILINGS) 
			&& sectors[i].tag == ref)
		{
			if(!func(sectors + i, reftype == LPREF_TAGGED_CEILINGS, 
				data, context)) return false;
		}
		if((reftype == LPREF_LINE_TAGGED_FLOORS
			|| reftype == LPREF_LINE_TAGGED_CEILINGS)
			&& sectors[i].tag == line->tag)
		{
			if(!func(sectors + i, reftype == LPREF_LINE_TAGGED_CEILINGS,
				data, context)) return false;
		}
		if((reftype == LPREF_ACT_TAGGED_FLOORS 
			|| reftype == LPREF_ACT_TAGGED_CEILINGS) 
			&& sectors[i].xg 
			&& sectors[i].xg->info.act_tag == ref)
		{
			if(!func(sectors + i, reftype == LPREF_ACT_TAGGED_CEILINGS, 
				data, context)) return false;
		}
	}
	return true;
}

/*
 * XL_TraverseLines
 *	Returns false if func returns false, otherwise true.
 *	Stops checking when false is returned.
 */
int XL_TraverseLines(line_t *line, int reftype, int ref, int data, 
					 void *context, int (*func)(line_t *line, int data,
					 void *context))
{
	int i;

	XG_Dev("XL_TraverseLines: Line %i, ref (%i, %i)",
		line - lines, reftype, ref);

	// Traversing self is simple.
	if(reftype == LREF_SELF)
		return func(line, data, context);
	if(reftype == LREF_INDEX)
		return func(lines + ref, data, context);
	if(reftype == LREF_ALL)
	{
		for(i=0; i<numlines; i++)
			if(!func(lines + i, data, context))
				return false;
	}
	if(reftype == LREF_TAGGED)
	{
		for(i=0; i<numlines; i++)
			if(lines[i].tag == ref)
				if(!func(lines + i, data, context))
					return false;
	}
	if(reftype == LREF_LINE_TAGGED)
	{
		// Ref is true if line itself should be excluded.
		for(i = 0; i < numlines; i++)
			if(lines[i].tag == line->tag && (!ref || lines+i != line))
				if(!func(lines + i, data, context))
					return false;
	}
	if(reftype == LREF_ACT_TAGGED)
	{
		for(i=0; i<numlines; i++)
			if(lines[i].xg && lines[i].xg->info.act_tag == ref)
				if(!func(lines + i, data, context))
					return false;
	}
	return true;
}

int XLTrav_ChangeLineType(line_t *line, int data, void *context)
{
	XL_SetLineType(line, data);
	return true; // Keep looking.
}

int XLTrav_ChangeWallTexture(line_t *line, int data, void *context)
{
	linetype_t *info = context;
	side_t *side;

	// i2: sidenum
	// i3: top texture (zero if no change)
	// i4: mid texture (zero if no change)
	// i5: bottom texture (zero if no change)
	// i6: (true/false) set midtexture even if previously zero

	// Is there a sidedef?
	if(line->sidenum[info->iparm[2]] < 0) return true;

	XG_Dev("XLTrav_ChangeWallTexture: Line %i", line - lines);

	side = sides + line->sidenum[info->iparm[2]];

	if(info->iparm[3])
		XL_ChangeTexture(line, info->iparm[2], LWS_UPPER, info->iparm[3]);
	if(info->iparm[4] && (side->midtexture || info->iparm[6]))
		XL_ChangeTexture(line, info->iparm[2], LWS_MID, info->iparm[4]);
	if(info->iparm[5])
		XL_ChangeTexture(line, info->iparm[2], LWS_LOWER, info->iparm[5]);
	
	return true;
}

int XLTrav_Activate(line_t *line, int data, void *context)
{
	XL_LineEvent(XLE_CHAIN, 0, line, 0, context);
	return true; // Keep looking.
}

//===========================================================================
// XLTrav_SmartActivate
//	If data=true, the line will receive a chain event if not activate.
//	If data=false, then ... if active.
//===========================================================================
int XLTrav_SmartActivate(line_t *line, int data, void *context)
{
	if(data != line->xg->active)
		XL_LineEvent(XLE_CHAIN, 0, line, 0, context);
	return true;
}

int XLTrav_LineCount(line_t *line, int data, void *context)
{
	if(!line->xg) return true; // Must have extended type.
	if(context)
		line->xg->info.act_count = data;
	else
		line->xg->info.act_count += data;
	return true;
}

int XLTrav_DisableLine(line_t *line, int data, void *context)
{
	if(!line->xg) return true;
	line->xg->disabled = data;
	return true; // Keep looking.
}

int XLTrav_QuickActivate(line_t *line, int data, void *context)
{
	if(!line->xg) return true;
	line->xg->active = data;
	line->xg->timer = XLTIMER_STOPPED;	// Stop timer.
	return true;
}

// Returns true if the line is active.
int XLTrav_CheckLine(line_t *line, int data, void *context)
{
	if(!line->xg) return false; // Stop checking!
	return (line->xg->active != 0) == (data != 0);
}

// Checks if the given lines are active or inactive.
// Returns true if all are in the specified state.
boolean XL_CheckLineStatus(line_t *line, int reftype, int ref, int active)
{
	return XL_TraverseLines(line, reftype, ref, active, 0, 
		XLTrav_CheckLine);
}

boolean XL_CheckMobjGone(int thingtype)
{
	thinker_t *th;
	mobj_t *mo;

	for(th = thinkercap.next; th != &thinkercap; th = th->next)
	{
		// Only find mobjs.
		if(th->function != P_MobjThinker)
			continue;
		
		mo = (mobj_t*) th;
		if(mo->type == thingtype && mo->health > 0)
		{
			// Not dead.
			XG_Dev("XL_CheckMobjGone: Thing type %i: Found mo id=%i, "
				"health=%i, pos=(%i,%i)", thingtype, 
				mo->thinker.id, mo->health, 
				mo->x >> FRACBITS, mo->y >> FRACBITS);
			return false;
		}
	}

	XG_Dev("XL_CheckMobjGone: Thing type %i is gone", thingtype);
	return true;
}

boolean XL_SwitchSwap(short *tex)
{
	char *name = R_TextureNameForNum(*tex);
	char buf[10];

	strncpy(buf, name, 8);
	buf[8] = 0;

	if(!strnicmp(buf, "SW1", 3))
	{
		XG_Dev("XL_SwitchSwap: Changing texture '%s'", name);

		buf[2] = '2';
		*tex = R_TextureNumForName(buf);
		return true;
	}
	if(!strnicmp(buf, "SW2", 3))
	{
		XG_Dev("XL_SwitchSwap: Changing texture '%s'", name);

		buf[2] = '1';
		*tex = R_TextureNumForName(buf);
		return true;
	}
	return false;
}

void XL_SwapSwitchTextures(line_t *line, int snum)
{
	int sidenum = line->sidenum[snum];
	side_t *side;

	if(sidenum < 0) return;

	XG_Dev("XL_SwapSwitchTextures: Line %i, side %i", line - lines, 
		sidenum);
	
	side = sides + sidenum;
	XL_SwitchSwap(&side->midtexture);
	XL_SwitchSwap(&side->toptexture);
	XL_SwitchSwap(&side->bottomtexture);
}

// Changes texture of the given line.
void XL_ChangeTexture(line_t *line, int sidenum, int section, int texture)
{
	side_t *side = sides + line->sidenum[sidenum];

	if(line->sidenum[sidenum] < 0) return;

	XG_Dev("XL_ChangeTexture: Line %i, side %i, section %i, texture %i",
		line - lines, sidenum, section, texture);

	if(section == LWS_MID)
		side->midtexture = texture;
	if(section == LWS_UPPER)
		side->toptexture = texture;
	if(section == LWS_LOWER)
		side->bottomtexture = texture;
}

// Apply the function defined by the line's class and parameters.
void XL_DoFunction(linetype_t *info, line_t *line, int sidenum, 
				   mobj_t *act_thing)
{
	xgline_t *xg = line->xg;
	player_t *activator = act_thing->player;	
	int i;

	XG_Dev("XL_DoFunction: Line %i, side %i, activator id %i",
		line - lines, sidenum, act_thing? act_thing->thinker.id : 0);
	XG_Dev("  Executing class 0x%X...", info->line_class);	

	switch(info->line_class)
	{
	case LTC_CHAIN_SEQUENCE:
		// Line data defines the current position in the sequence.
		// i0: flags (loop, deact when done)
		// i1..i19: line types
		// f0: interval randomness (100 means real interval can be 0%..200%).
		// f1..f19: intervals (seconds)
		xg->chidx = 1;			// This is the first.
		// Start counting the first interval.
		xg->chtimer = XG_RandomPercentFloat(info->fparm[1], 
			info->fparm[0]);	
		break;

	case LTC_PLANE_MOVE:
		// i0 + i1: (plane ref) plane to move.
		// i2: destination type (zero, relative to current, surrounding 
		//     highest/lowest floor/ceiling)
		// i3: flags (PMF_*)
		// i4: start sound
		// i5: end sound
		// i6: move sound
		// i7: start texture origin (uses same ids as i2)
		// i8: start texture index (used with PMD_ZERO).
		// i9: end texture origin (uses same ids as i2)
		// i10: end texture (used with PMD_ZERO)
		// i11 + i12: (plane ref) start sector type (spec: use i12)
		// i13 + i14: (plane ref) end sector type (spec: use i14)
		// f0: move speed (units per tic).
		// f1: crush speed (units per tic).
		// f2: destination offset
		// f3: move sound min interval (seconds)
		// f4: move sound max interval (seconds)
		// f5: time to wait before starting the move
		// f6: wait increment for each plane that gets moved
		line->xg->fdata = info->fparm[5]; // fdata keeps track of wait time
		line->xg->idata = true;	// play sound
		XL_TraversePlanes(line, info->iparm[0], info->iparm[1],
			(int) line, info, XSTrav_MovePlane);
		break;

	case LTC_BUILD_STAIRS:
		// i0 + i1: (plane ref) plane to start from
		// i2: (true/false) stop when texture changes
		// i3: (true/false) spread build? 
		// i4: start build sound (doesn't wait)
		// i5: step start sound
		// i6: step end sound
		// i7: step move sound		
		// f0: build speed
		// f1: step size (signed)
		// f2: time to wait before starting the first step
		// f3: time to wait between steps 
		// f4: move sound min interval (seconds)
		// f5: move sound max interval (seconds)
		// f6: build speed delta per step
		XS_InitStairBuilder();
		XL_TraversePlanes(line, info->iparm[0], info->iparm[1],
			(int) line, info, XSTrav_BuildStairs);
		break;

	case LTC_DAMAGE:
		// Iparm2 and 3 define how far we're allowed to go.
		// i0: min damage
		// i1: max damage
		// i2: min limit
		// i3: max limit
		if(act_thing->health > info->iparm[2]) 
		{
			// Iparms define the min and max damage to inflict.
			// The real amount is random.
			i = XG_RandomInt(info->iparm[0], info->iparm[1]);
			if(i > 0)
				P_DamageMobj(act_thing, 0, 0, i);
			else if(i < 0)
			{
				act_thing->health -= i;				
				// Don't go above a given level.
				if(act_thing->health > info->iparm[3])
					act_thing->health = info->iparm[3];
				if(activator)
				{
					// This is player.
					activator->health = act_thing->health;
					activator->update |= PSF_HEALTH;
				}
			}
		}
		break;

	case LTC_POWER: 
		// i0: min power delta
		// i1: max power delta
		// i2: min limit
		// i3: max limit
		if(!activator) break; // Must be a player.
		activator->armorpoints += XG_RandomInt(info->iparm[0], 
			info->iparm[1]);
		if(activator->armorpoints < info->iparm[2])
			activator->armorpoints = info->iparm[2];
		if(activator->armorpoints > info->iparm[3])
			activator->armorpoints = info->iparm[3];
		break;

	case LTC_SECTOR_TYPE:
		// i0 + i1: plane ref
		// i2: new type (zero or xg sector)
		XL_TraversePlanes(line, info->iparm[0], info->iparm[1],
			info->iparm[2], 0, XSTrav_SectorType);
		break;

	case LTC_SECTOR_LIGHT:
		// Change the light level of the target sector(s).
		XL_TraversePlanes(line, info->iparm[0], info->iparm[1],
			(int) line, info, XSTrav_SectorLight);
		break;

	case LTC_LINE_TYPE:
		// Change target line type. (i0 + i1: line ref, i2: new type)
		XL_TraverseLines(line, info->iparm[0], info->iparm[1],
			info->iparm[2], 0, XLTrav_ChangeLineType);
		break;

	case LTC_KEY:
		if(!activator) break; // Must be a player.
		// i0 give keys, i1 take keys away.
#ifdef __JDOOM__
		for(i=0; i<6; i++)
		{
			if(info->iparm[0] & (1<<i)) P_GiveCard(activator, i);
			if(info->iparm[1] & (1<<i))
				activator->cards[i] = false;
		}
#elif defined __JHERETIC__
		for(i=0; i<3; i++)
		{
			if(info->iparm[0] & (1<<i)) P_GiveKey(activator, i);
			if(info->iparm[1] & (1<<i))
				activator->keys[i] = false;
		}
#endif
		break;

	case LTC_ACTIVATE:
		// i0 + i1: line ref.
		// Sends a chain event to all the referenced lines.
		// Act_thing is used as the activator for each.
		XL_TraverseLines(line, info->iparm[0], info->iparm[1], 
			0, act_thing, XLTrav_Activate);
		break;

	case LTC_MUSIC:
/*#ifdef __JDOOM__
		S_ChangeMusic(info->iparm[0], info->iparm[1]);
#else if defined __JHERETIC__
		S_StartSong(info->iparm[0], info->iparm[1]);
#endif*/
		S_StartMusicNum(info->iparm[0], info->iparm[1]);
		break;

	case LTC_SOUND:
		XL_TraversePlanes(line, info->iparm[0], info->iparm[1],
			info->iparm[2], 0, XSTrav_SectorSound);
		break;

	case LTC_LINE_COUNT:
		// i0 + i1: line ref.
		// i2: (true/false) is i3 an absolute value
		// i3: value
		XL_TraverseLines(line, info->iparm[0], info->iparm[1],
			info->iparm[3], (void*) info->iparm[2], XLTrav_LineCount);
		break;

	case LTC_END_LEVEL:
		// Quite simply just ends the level. Secret flag set in i0.
		if(info->iparm[0])
			G_SecretExitLevel();
		else
			G_ExitLevel();
		break;

	case LTC_DISABLE_IF_ACTIVE:
		// i0 + i1: line ref.
		XL_TraverseLines(line, info->iparm[0], info->iparm[1],
			xg->active, 0, XLTrav_DisableLine);
		break;

	case LTC_ENABLE_IF_ACTIVE:
		// i0 + i1: line ref.
		XL_TraverseLines(line, info->iparm[0], info->iparm[1],
			!xg->active, 0, XLTrav_DisableLine);
		break;

	case LTC_EXPLODE:
		if(!act_thing) break;
		P_ExplodeMissile(act_thing);
		break;

	case LTC_PLANE_TEXTURE:
		// i0 + i1: plane ref
		// i2: (spref) texture origin
		// i3: texture number (flat), used with SPREF_NONE
		XL_TraversePlanes(line, info->iparm[0], info->iparm[1],
			(int) line, info, XSTrav_PlaneTexture);
		break;

	case LTC_WALL_TEXTURE:
		// i0 + i1: line ref
		// i2: sidenum
		// i3: top texture (zero if no change)
		// i4: mid texture (zero if no change)
		// i5: bottom texture (zero if no change)
		XL_TraverseLines(line, info->iparm[0], info->iparm[1],
			0, info, XLTrav_ChangeWallTexture);
		break;

	case LTC_COMMAND:
		// s0: console command to execute
		Con_Execute(info->sparm[0], true);
		break;

	case LTC_MIMIC_SECTOR:
		// i0 + i1: sector ref
		// i2: (spref) sector to mimic
		XL_TraversePlanes(line, info->iparm[0], info->iparm[1],
			(int) line, info, XSTrav_MimicSector);
		break;
	}
}

void XL_Message(mobj_t *act, char *msg, boolean global)
{
	player_t *pl;
	int i;

	if(!msg || !msg[0]) return;

	if(global)
	{
		XG_Dev("XL_Message: GLOBAL '%s'", msg);
		// Send to all players in the game.
		for(i=0; i<MAXPLAYERS; i++)
			if(players[i].plr->ingame)
#ifdef __JDOOM__
				P_SetMessage(players+i, msg);
#elif defined __JHERETIC__
				P_SetMessage(players+i, msg, true);
#endif
		return;
	}

	if(act->player) 
	{
		pl = act->player;
	}
	else if(act->flags & MF_MISSILE 
		&& act->target && act->target->player)
	{
		// Originator of the missile.
		pl = act->target->player;
	}
	else
	{
		// We don't know whom to send the message.
		XG_Dev("XL_Message: '%s'", msg);
		XG_Dev("  NO DESTINATION, MESSAGE DISCARDED");
		return;
	}
#ifdef __JDOOM__
	P_SetMessage(pl, msg);
#elif defined __JHERETIC__
	P_SetMessage(pl, msg, true);
#endif
}

/*
 * XL_ActivateLine
 */
void XL_ActivateLine(boolean activating, linetype_t *info, line_t *line,
					 int sidenum, mobj_t *data)
{
	xgline_t *xg = line->xg;
	mobj_t *activator_thing = (mobj_t*) data;
//	player_t *activator = activator_thing->player;
	degenmobj_t *soundorg;

	XG_Dev("XL_ActivateLine: %s line %i, side %i", activating?
		"Activating" : "Deactivating", line - lines, sidenum);

	if(xg->disabled) 
	{
		XG_Dev("  LINE DISABLED, ABORTING");		
		return; // The line is disabled.
	}

	if((activating && xg->active) || (!activating && !xg->active)) 
	{
		XG_Dev("  Line is ALREADY %s, ABORTING", activating? "ACTIVE"
			: "INACTIVE");
		return; // Do nothing (can't activate if already active!).
	}

	// Activation should happen on the front side.
	if(line->frontsector)
		soundorg = &line->frontsector->soundorg;

	// Let the line know who's activating it.
	xg->activator = data;

	// Process (de)activation chains. Chains always pass as an activation
	// method, but the other requirements of the chained type must be met.
	// Also play sounds.
	if(activating) 
	{
		XL_Message(activator_thing, info->act_msg, 
			(info->flags2 & LTF2_GLOBAL_A_MSG) != 0);

		if(info->act_sound) S_StartSound(info->act_sound, (mobj_t*)soundorg);
		// Change the texture of the line if asked to.
		if(info->wallsection && info->act_tex)
			XL_ChangeTexture(line, sidenum, info->wallsection, info->act_tex);
		if(info->act_chain)
			XL_LineEvent(XLE_CHAIN, info->act_chain, line, sidenum, data);
	}
	else
	{
		XL_Message(activator_thing, info->deact_msg, 
			(info->flags2 & LTF2_GLOBAL_D_MSG) != 0);

		if(info->deact_sound) S_StartSound(info->deact_sound, (mobj_t*)soundorg);
		// Change the texture of the line if asked to.
		if(info->wallsection && info->deact_tex)
			XL_ChangeTexture(line, sidenum, info->wallsection, info->deact_tex);
		if(info->deact_chain)
			XL_LineEvent(XLE_CHAIN, info->deact_chain, line, sidenum, data);
	}	

	// Automatically swap any SW* textures.
	if(xg->active != activating)
		XL_SwapSwitchTextures(line, sidenum);

	// Change the state of the line.
	xg->active = activating;
	xg->timer = 0;				// Reset timer.

	// Activate lines with a matching tag with Group Activation.
	if((activating && info->flags2 & LTF2_GROUP_ACT)
		|| (!activating && info->flags2 & LTF2_GROUP_DEACT))
	{
		XL_TraverseLines(line, LREF_LINE_TAGGED, true, activating, 0,
			XLTrav_SmartActivate);
	}

	// For lines flagged Multiple, quick-(de)activate other lines that have
	// the same line tag.
	if(info->flags2 & LTF2_MULTIPLE)
	{
		XL_TraverseLines(line, LREF_LINE_TAGGED, true, activating, 0,
			XLTrav_QuickActivate);
	}

	// Should we apply the function of the line? Functions are defined by
	// the class of the line type.
	if(((activating && info->flags2 & LTF2_WHEN_ACTIVATED)
		|| (!activating && info->flags2 & LTF2_WHEN_DEACTIVATED))
	   && (!(info->flags2 & LTF2_WHEN_LAST) || info->act_count == 1))
	{
		XL_DoFunction(info, line, sidenum, activator_thing);
	}
	else
	{
		XG_Dev("  Line %i FUNCTION TEST FAILED", line - lines);
	}
}

/*
 * XL_CheckKeys
 */
boolean XL_CheckKeys(mobj_t *mo, int flags2)
{
	player_t *act = mo->player;
#ifdef __JDOOM__
	int num = 6;
	char *keystr[] = { "BLUE KEYCARD", "YELLOW KEYCARD", "RED KEYCARD",
		"BLUE SKULL KEY", "YELLOW SKULL KEY", "RED SKULL KEY" };
	int *keys = (int*) act->cards;
	int badsound = sfx_oof;
#elif defined __JHERETIC__
	int num = 3;
	char *keystr[] = { "YELLOW KEY", "GREEN KEY", "BLUE KEY" };
	boolean *keys = act->keys;
	int badsound = sfx_plroof;
#endif
	int i;

	for(i=0; i<num; i++)
	{
		if(flags2 & LTF2_KEY(i) && !keys[i])
		{
			// This key is missing! Show a message.
			sprintf(msgbuf, "YOU NEED A %s.", keystr[i]);
			XL_Message(mo, msgbuf, false);
			S_ConsoleSound(badsound, mo, act - players);
			return false;
		}
	}
	return true;
}

// Decides if the event leads to (de)activation.
// Returns true if the event is processed.
// Line must be extended.
// Most conditions use AND (act method, game mode and difficult use OR).
int XL_LineEvent(int evtype, int linetype, line_t *line, int sidenum, 
				 void *data)
{
	xgline_t *xg = line->xg;
	linetype_t *info = &xg->info;
	boolean active = xg->active;
	mobj_t *activator_thing = (mobj_t*) data; 
	player_t *activator = activator_thing->player;
	int i;

	// Clients rely on the server, they don't do XG themselves.
	if(IS_CLIENT) return false;

	XG_Dev("XL_LineEvent: %s line %i, side %i (chained type %i)",
		EVTYPESTR(evtype), line - lines, sidenum, linetype);

	if(xg->disabled) 
	{
		XG_Dev("  LINE IS DISABLED, ABORTING EVENT");
		return false;	// The line is disabled.
	}

	// This is a chained event.
	if(linetype)
	{
		if(!XL_GetType(linetype)) return false;
		info = &typebuffer;
	}

	// Process chained event first. It takes precedence.
	if(info->ev_chain)
	{
		if(XL_LineEvent(evtype, info->ev_chain, line, sidenum, data))
		{
			XG_Dev("  Event %s, line %i, side %i OVERRIDDEN BY EVENT CHAIN %i",
				EVTYPESTR(evtype), line - lines, sidenum, info->ev_chain);
			return true;		
		}
	}

	// Check restrictions and conditions that will prevent processing
	// the event.
	if((active && info->act_type == LTACT_COUNTED_OFF)
		|| (!active && info->act_type == LTACT_COUNTED_ON))
	{
		// Can't be processed at this time.
		XG_Dev("  Line %i: Active=%i, type=%i ABORTING EVENT", 
			line - lines, active, info->act_type);
		return false;
	}
	// Check the type of the event vs. the requirements of the line.
	if(evtype == XLE_CHAIN) goto type_passes;
	if(evtype == XLE_USE 
		&& ((info->flags & LTF_PLAYER_USE_A && activator && !active)
			|| (info->flags & LTF_OTHER_USE_A && !activator && !active)
			|| (info->flags & LTF_PLAYER_USE_D && activator && active)
			|| (info->flags & LTF_OTHER_USE_D && !activator && active)))
		goto type_passes;
	if(evtype == XLE_SHOOT 
		&& ((info->flags & LTF_PLAYER_SHOOT_A && activator && !active)
			|| (info->flags & LTF_OTHER_SHOOT_A && !activator && !active)
			|| (info->flags & LTF_PLAYER_SHOOT_D && activator && active)
			|| (info->flags & LTF_OTHER_SHOOT_D && !activator && active)))
		goto type_passes;
	if(evtype == XLE_CROSS
		&& ((info->flags & LTF_PLAYER_CROSS_A && activator && !active)
			|| (info->flags & LTF_MONSTER_CROSS_A 
				&& activator_thing->flags & MF_COUNTKILL && !active)
			|| (info->flags & LTF_MISSILE_CROSS_A
				&& activator_thing->flags & MF_MISSILE && !active)
			|| (info->flags & LTF_ANY_CROSS_A && !active)
			|| (info->flags & LTF_PLAYER_CROSS_D && activator && active)
			|| (info->flags & LTF_MONSTER_CROSS_D
				&& activator_thing->flags & MF_COUNTKILL && active)
			|| (info->flags & LTF_MISSILE_CROSS_D
				&& activator_thing->flags & MF_MISSILE && active)
			|| (info->flags & LTF_ANY_CROSS_D && active)))
		goto type_passes;
	if(evtype == XLE_HIT
		&& ((info->flags & LTF_PLAYER_HIT_A && activator && !active)
			|| (info->flags & LTF_OTHER_HIT_A && !activator && !active)
			|| (info->flags & LTF_MONSTER_HIT_A 
				&& activator_thing->flags & MF_COUNTKILL && !active)
			|| (info->flags & LTF_MISSILE_HIT_A
				&& activator_thing->flags & MF_MISSILE && !active)
			|| (info->flags & LTF_ANY_HIT_A && !active)
			|| (info->flags & LTF_PLAYER_HIT_D && activator && active)
			|| (info->flags & LTF_OTHER_HIT_D && !activator && active)
			|| (info->flags & LTF_MONSTER_HIT_D 
				&& activator_thing->flags & MF_COUNTKILL && active)
			|| (info->flags & LTF_MISSILE_HIT_D
				&& activator_thing->flags & MF_MISSILE && active)
			|| (info->flags & LTF_ANY_HIT_D && active)))
		goto type_passes;
	if(evtype == XLE_TICKER
	   && ((info->flags & LTF_TICKER_A && !active) ||
		   (info->flags & LTF_TICKER_D && active)))
		goto type_passes;
		
	// Type doesn't pass, sorry.
	XG_Dev("  Line %i: ACT REQUIREMENTS NOT FULFILLED, ABORTING EVENT",
		line - lines);
	return false;

type_passes:

	if(info->flags & LTF_NO_OTHER_USE_SECRET)
	{
		// Non-players can't use this line if line is flagged secret.
		if(evtype == XLE_USE && !activator && line->flags & ML_SECRET)
		{
			XG_Dev("  Line %i: ABORTING EVENT due to no_other_use_secret",
				line - lines);
			return false;
		}
	}
	if(info->flags & LTF_MOBJ_GONE)
	{
		if(!XL_CheckMobjGone(info->aparm[9]))
			return false;
	}
	if(info->flags & LTF_ACTIVATOR_TYPE)
	{
		// Check the activator's type.
		if(!activator_thing || activator_thing->type != info->aparm[9])
		{
			XG_Dev("  Line %i: ABORTING EVENT due to activator type",
				line - lines);
			return false;
		}
	}
	if((evtype == XLE_USE || evtype == XLE_SHOOT || evtype == XLE_CROSS)
		&& !(info->flags2 & LTF2_TWOSIDED))
	{
		// Only allow (de)activation from the front side.
		if(sidenum != 0) 
		{
			XG_Dev("  Line %i: ABORTING EVENT due to line side test",
				line - lines);
			return false;		
		}
	}

	// Check counting.
	if(!info->act_count) 
	{
		XG_Dev("  Line %i: ABORTING EVENT due to Count = 0", line - lines);
		return false;
	}

	// More requirements.
	if(info->flags2 & LTF2_HEALTH_ABOVE 
		&& activator_thing->health <= info->aparm[0]) return false;
	if(info->flags2 & LTF2_HEALTH_BELOW
		&& activator_thing->health >= info->aparm[1]) return false;
	if(info->flags2 & LTF2_POWER_ABOVE 
		&& (!activator || activator->armorpoints <= info->aparm[2])) 
		return false;
	if(info->flags2 & LTF2_POWER_BELOW 
		&& (!activator || activator->armorpoints >= info->aparm[3]))
		return false;
	if(info->flags2 & LTF2_LINE_ACTIVE)
		if(!XL_CheckLineStatus(line, info->aparm[4], info->aparm[5], true)) 
		{
			XG_Dev("  Line %i: ABORTING EVENT due to line_active test",
				line - lines);
			return false;
		}
	if(info->flags2 & LTF2_LINE_INACTIVE)
		if(!XL_CheckLineStatus(line, info->aparm[6], info->aparm[7], false)) 
		{
			XG_Dev("  Line %i: ABORTING EVENT due to line_inactive test",
				line - lines);
			return false;
		}
	// Check game mode.
	if(IS_NETGAME)
	{
		if(!(info->flags2 & (LTF2_COOPERATIVE | LTF2_DEATHMATCH)))
		{
			XG_Dev("  Line %i: ABORTING EVENT due to netgame mode", 
				line - lines);
			return false;
		}
	}
	else
	{
		if(!(info->flags2 & LTF2_SINGLEPLAYER)) 
		{
			XG_Dev("  Line %i: ABORTING EVENT due to game mode (1p)", 
				line - lines);
			return false;
		}
	}
	// Check skill level.
	if(gameskill < 1) i = 1;
	else if(gameskill > 3) i = 4;
	else i = 1 << (gameskill-1);
	i <<= LTF2_SKILL_SHIFT;
	if(!(info->flags2 & i)) 
	{
		XG_Dev("  Line %i: ABORTING EVENT due to skill level (%i)", 
			line - lines, gameskill);
		return false;
	}
	// Check activator color.
	if(info->flags2 & LTF2_COLOR)
	{
		if(!activator) return false;
		if(cfg.PlayerColor[activator - players] != info->aparm[8])
		{
			XG_Dev("  Line %i: ABORTING EVENT due to activator color (%i)",
				line - lines, cfg.PlayerColor[activator - players]);
			return false;
		}
	}
	// Keys require that the activator is a player.
	if(info->flags2 & (LTF2_KEY1 | LTF2_KEY2 | LTF2_KEY3 | LTF2_KEY4 
		| LTF2_KEY5 | LTF2_KEY6))
	{
		// Check keys.
		if(!activator) 
		{
			XG_Dev("  Line %i: ABORTING EVENT due to missing key "
				"(no activator)", line - lines);
			return false;
		}
		// Check that all the flagged keys are present.
		if(!XL_CheckKeys(activator_thing, info->flags2))
		{
			XG_Dev("  Line %i: ABORTING EVENT due to missing key",
				line - lines);
			return false;	// Keys missing!
		}
	}
	
	// All tests passed, use this event.
	if(info->act_count > 0 && evtype != XLE_CHAIN)	
	{
		// Decrement counter.
		info->act_count--;

		XG_Dev("  Line %i: Decrementing counter, now %i", line - lines,
			info->act_count);
	}
	XL_ActivateLine(!active, info, line, sidenum, activator_thing);
	return true;
}

// Returns true if the event was processed.
int XL_CrossLine(line_t *line, int sidenum, mobj_t *thing)
{
	if(!line->xg) return false;
	return XL_LineEvent(XLE_CROSS, 0, line, sidenum, thing);
}

// Returns true if the event was processed.
int XL_UseLine(line_t *line, int sidenum, mobj_t *thing)
{
	if(!line->xg) return false;
	return XL_LineEvent(XLE_USE, 0, line, sidenum, thing);
}

// Returns true if the event was processed.
int XL_ShootLine(line_t *line, int sidenum, mobj_t *thing)
{
	if(!line->xg) return false;
	return XL_LineEvent(XLE_SHOOT, 0, line, sidenum, thing);
}

int XL_HitLine(line_t *line, int sidenum, mobj_t *thing)
{
	if(!line->xg) return false;
	return XL_LineEvent(XLE_HIT, 0, line, sidenum, thing);
}

void XL_DoChain(line_t *line, int chain, boolean activating, mobj_t *act_thing)
{
	line_t dummy;
	xgline_t dummyxg;

	XG_Dev("XL_DoChain: Line %i, chained type %i", line - lines, chain);
	XG_Dev("  (dummy line will show up as %i)", &dummy - lines);

	// We'll use a dummy line for the chain.
	memcpy(&dummy, line, sizeof(*line));
	memcpy(&dummyxg, line->xg, sizeof(*line->xg));
	dummy.sidenum[0] = -1;
	dummy.sidenum[1] = -1;
	dummy.xg = &dummyxg;
	dummyxg.active = !activating;

	XL_LineEvent(XLE_CHAIN, chain, &dummy, 0, act_thing);
}

void XL_ChainSequenceThink(line_t *line)
{
	xgline_t *xg = line->xg;
	linetype_t *info = &xg->info;

	// Only process active chain sequences.
	if(info->line_class != LTC_CHAIN_SEQUENCE || !xg->active) return;

	xg->chtimer -= TIC2FLT(1);

	// idata = current pos
	// fdata = count down seconds

	// i1..i19: line types
	// f0: interval randomness (100 means real interval can be 0%..200%).
	// f1..f19: intervals (seconds)

	// If the counter goes to zero, it's time to execute the chain.
	if(xg->chtimer < 0)
	{
		XG_Dev("XL_ChainSequenceThink: Line %i, executing...", 
			line - lines);

		// Are there any more chains?
		if(xg->chidx < DDLT_MAX_PARAMS && info->iparm[xg->chidx])
		{
			// Only send activation events.
			XL_DoChain(line, info->iparm[xg->chidx], true, xg->activator);

			// Advance to the next one.
			xg->chidx++;

			// Are we out of chains?
			if((xg->chidx == DDLT_MAX_PARAMS || !info->iparm[xg->chidx])
				&& info->iparm[0] & CHSF_LOOP)
			{
				// Loop back to beginning.
				xg->chidx = 1;
			}
			// If there are more chains, get the next interval.
			if(xg->chidx < DDLT_MAX_PARAMS && info->iparm[xg->chidx])
			{
				// Start counting it down.
				xg->chtimer = XG_RandomPercentFloat(info->fparm[xg->chidx],
					info->fparm[0]);
			}
		}
		else if(info->iparm[0] & CHSF_DEACTIVATE_WHEN_DONE)
		{
			// The sequence has been completed.
			XL_ActivateLine(false, info, line, 0, xg->activator);
		}
	}
}

// Called once a tic for each XG line.
void XL_Think(line_t *line)
{
	xgline_t *xg = line->xg;
	linetype_t *info = &xg->info;
	float levtime = TIC2FLT(leveltime);
	fixed_t xoff, yoff, spd;
	side_t *sid;
	int i;

	if(xg->disabled) return;	// Disabled, do nothing.

	// Increment time.
	if(xg->timer >= 0)
	{
		xg->timer++;
		xg->ticker_timer++;
	}

	// Activation by ticker.
	if((info->ticker_end <= 0 || (levtime >= info->ticker_start 
		&& levtime <= info->ticker_end))
		&& xg->ticker_timer > info->ticker_interval)
	{
		if(info->flags & LTF_TICKER)
		{
			xg->ticker_timer = 0;
			XL_LineEvent(XLE_TICKER, 0, line, 0, &dummything);
		}
		// How about some forced functions?
		if(((info->flags2 & LTF2_WHEN_ACTIVE && xg->active)
			|| (info->flags2 & LTF2_WHEN_INACTIVE && !xg->active))
			&& (!(info->flags2 & LTF2_WHEN_LAST) || info->act_count == 1))
		{
			XL_DoFunction(info, line, 0, xg->activator);
		}
	}
	
	XL_ChainSequenceThink(line);

	// Check for automatical (de)activation.
	if(((info->act_type == LTACT_COUNTED_OFF ||
		 info->act_type == LTACT_FLIP_COUNTED_OFF) && xg->active)
	   || ((info->act_type == LTACT_COUNTED_ON ||
			info->act_type == LTACT_FLIP_COUNTED_ON) && !xg->active))
	{
		if(info->act_time >= 0 && xg->timer > FLT2TIC(info->act_time))
		{
			XG_Dev("XL_Think: Line %i, timed to go %s",
				line - lines, xg->active? "INACTIVE" : "ACTIVE");

			// Swap line state without any checks.
			XL_ActivateLine(!xg->active, info, line, 0, &dummything);
		}
	}

	if(info->texmove_speed)
	{
		// The texture should be moved. Calculate the offsets.
		angle_t ang = ((angle_t)(ANGLE_MAX * (info->texmove_angle/360)))
			>> ANGLETOFINESHIFT;
		spd = FRACUNIT * info->texmove_speed;
		xoff = -FixedMul(finecosine[ang], spd);
		yoff = FixedMul(finesine[ang], spd);
		// Apply to both sides of the line.
		for(i = 0; i < 2; i++)
		{
			if(line->sidenum[i] < 0) continue;
			sid = sides + line->sidenum[i];
			sid->textureoffset += xoff;
			sid->rowoffset += yoff;
		}
	}
}

// Think for each extended line.
void XL_Ticker(void)
{
	int i;

	for(i=0; i<numlines; i++)
	{
		if(!lines[i].xg) continue;	// Not an extended line.
		XL_Think(lines + i);
	}
}

/*
 * During update, definitions are re-read, so the pointers need to be
 * updated. However, this is a bit messy operation, prone to errors.
 * Instead, we just disable XG.
 */
void XL_Update(void)
{
	int i;

	// It's all PU_LEVEL memory, so we can just lose it.
	for(i = 0; i < numlines; i++)
		if(lines[i].xg)
		{
			lines[i].xg = NULL;
			lines[i].special = 0;
		}
}

