
//**************************************************************************
//**
//** P_OLDSVG.C
//**
//** Doom v1.9 (Doom II, Ultimate Doom, Final Doom) compatible savegames.
//** Loading only.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "p_local.h"
#include "doomstat.h"
#include "r_state.h"
#include "g_game.h"
#include "m_misc.h"

// MACROS ------------------------------------------------------------------

#define PADSAVEP()		save_p += (4 - ((save_p - savebuffer) & 3)) & 3
#define SAVESTRINGSIZE	24
#define VERSIONSIZE		16 

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

byte *save_p;
byte *savebuffer;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*void SV_Write(void *data, int len)
{
	memcpy(save_p, data, len);
	save_p += len;
}

void SV_WriteLong(unsigned int val)
{
	SV_Write(&val, 4);	
}*/

static void SV_Read(void *data, int len)
{
	memcpy(data, save_p, len);
	save_p += len;
}

static int SV_ReadLong()
{
	save_p += 4;
	return *(int*) (save_p-4);
}

/*void SV_WritePlayer(player_t *pl)
{
	int temp[3] = { 0, 0, 0 };

	SV_WriteLong(0);	// mo
	SV_WriteLong(pl->playerstate);
	SV_Write(temp, 8);
	SV_WriteLong(pl->plr->viewz);
	SV_WriteLong(pl->plr->viewheight);
	SV_WriteLong(pl->plr->deltaviewheight);
	SV_WriteLong(pl->bob);
	
	SV_WriteLong(pl->health);
	SV_WriteLong(pl->armorpoints);
	SV_WriteLong(pl->armortype);

	SV_Write(pl->powers, NUMPOWERS * 4);
	SV_Write(pl->cards, NUMCARDS * 4);
	SV_WriteLong(pl->backpack);

	SV_Write(pl->frags, 4 * 4);
	SV_WriteLong(pl->readyweapon);
	SV_WriteLong(pl->pendingweapon);

	SV_Write(pl->weaponowned, NUMWEAPONS * 4);
	SV_Write(pl->ammo, NUMAMMO * 4);
	SV_Write(pl->maxammo, NUMAMMO * 4);

	SV_WriteLong(pl->attackdown);
	SV_WriteLong(pl->usedown);

	SV_WriteLong(pl->cheats);

	SV_WriteLong(pl->refire);

	SV_WriteLong(pl->killcount);
	SV_WriteLong(pl->itemcount);
	SV_WriteLong(pl->secretcount);

	SV_WriteLong(0);	// message

	SV_WriteLong(pl->damagecount);
	SV_WriteLong(pl->bonuscount);
	
	SV_WriteLong(0);	// attacker

	SV_WriteLong(pl->plr->extralight);
	SV_WriteLong(pl->plr->fixedcolormap);
	SV_WriteLong(pl->colormap);
	SV_Write(pl->psprites, NUMPSPRITES * sizeof(pspdef_t));
	
	SV_WriteLong(pl->didsecret);
}*/

static void SV_ReadPlayer(player_t *pl)
{
	int temp[3];

	SV_ReadLong();
	pl->playerstate = SV_ReadLong();
	SV_Read(temp, 8);
	pl->plr->viewz = SV_ReadLong();
	pl->plr->viewheight = SV_ReadLong();
	pl->plr->deltaviewheight = SV_ReadLong();
	pl->bob = SV_ReadLong();
	
	pl->health = SV_ReadLong();
	pl->armorpoints = SV_ReadLong();
	pl->armortype = SV_ReadLong();

	SV_Read(pl->powers, NUMPOWERS * 4);
	SV_Read(pl->cards, NUMCARDS * 4);
	pl->backpack = SV_ReadLong();

	SV_Read(pl->frags, 4 * 4);
	pl->readyweapon = SV_ReadLong();
	pl->pendingweapon = SV_ReadLong();

	SV_Read(pl->weaponowned, NUMWEAPONS * 4);
	SV_Read(pl->ammo, NUMAMMO * 4);
	SV_Read(pl->maxammo, NUMAMMO * 4);

	pl->attackdown = SV_ReadLong();
	pl->usedown = SV_ReadLong();

	pl->cheats = SV_ReadLong();

	pl->refire = SV_ReadLong();

	pl->killcount = SV_ReadLong();
	pl->itemcount = SV_ReadLong();
	pl->secretcount = SV_ReadLong();

	SV_ReadLong();

	pl->damagecount = SV_ReadLong();
	pl->bonuscount = SV_ReadLong();
	
	SV_ReadLong();

	pl->plr->extralight = SV_ReadLong();
	pl->plr->fixedcolormap = SV_ReadLong();
	pl->colormap = SV_ReadLong();
	SV_Read(pl->psprites, NUMPSPRITES * sizeof(pspdef_t));
	
	pl->didsecret = SV_ReadLong();
}

/*void SV_WriteMobj(mobj_t *mo)
{
    // List: thinker links.
	SV_WriteLong(0);
	SV_WriteLong(0);
	SV_WriteLong(0);

    // Info for drawing: position.
	SV_WriteLong(mo->x);
	SV_WriteLong(mo->y);
	SV_WriteLong(mo->z);

    // More list: links in sector (if needed)
    SV_WriteLong(0);
    SV_WriteLong(0);

    //More drawing info: to determine current sprite.
    SV_WriteLong(mo->angle);	// orientation
    SV_WriteLong(mo->sprite);	// used to find patch_t and flip value
    SV_WriteLong(mo->frame);	// might be ORed with FF_FULLBRIGHT

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    SV_WriteLong(0);
    SV_WriteLong(0);
    
    SV_WriteLong(0);

    // The closest interval over all contacted Sectors.
    SV_WriteLong(mo->floorz);
    SV_WriteLong(mo->ceilingz);

    // For movement checking.
    SV_WriteLong(mo->radius);
    SV_WriteLong(mo->height);	

    // Momentums, used to update position.
    SV_WriteLong(mo->momx);
    SV_WriteLong(mo->momy);
    SV_WriteLong(mo->momz);

    // If == validcount, already checked.
    SV_WriteLong(mo->Validcount);

    SV_WriteLong(mo->type);
    SV_WriteLong(0);	// &mobjinfo[mobj->type]
    
    SV_WriteLong(mo->tics);	// state tic counter
    SV_WriteLong( (int) mo->state);
    SV_WriteLong(mo->flags);
    SV_WriteLong(mo->health);

    // Movement direction, movement generation (zig-zagging).
    SV_WriteLong(mo->movedir);	// 0-7
    SV_WriteLong(mo->movecount);	// when 0, select a new dir

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    SV_WriteLong(0);

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    SV_WriteLong(mo->reactiontime);   

    // If >0, the target will be chased
    // no matter what (even if shot)
    SV_WriteLong(mo->threshold);

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    SV_WriteLong( (int) mo->player);

    // Player number last looked for.
    SV_WriteLong(mo->lastlook);	

    // For nightmare respawn.
    SV_Write(&mo->spawnpoint, 10);	

    // Thing being chased/attacked for tracers.
    SV_WriteLong(0);
}*/

static void SV_ReadMobj(mobj_t *mo)
{
    // List: thinker links.
	SV_ReadLong();
	SV_ReadLong();
	SV_ReadLong();

    // Info for drawing: position.
	mo->x = SV_ReadLong();
	mo->y = SV_ReadLong();
	mo->z = SV_ReadLong();

    // More list: links in sector (if needed)
    SV_ReadLong();
    SV_ReadLong();

    //More drawing info: to determine current sprite.
    mo->angle = SV_ReadLong();	// orientation
    mo->sprite = SV_ReadLong();	// used to find patch_t and flip value
    mo->frame = SV_ReadLong();	// might be ORed with FF_FULLBRIGHT

    // Interaction info, by BLOCKMAP.
    // Links in blocks (if needed).
    SV_ReadLong();
    SV_ReadLong();
    SV_ReadLong();

    // The closest interval over all contacted Sectors.
    mo->floorz = SV_ReadLong();
    mo->ceilingz = SV_ReadLong();

    // For movement checking.
    mo->radius = SV_ReadLong();
    mo->height = SV_ReadLong();	

    // Momentums, used to update position.
    mo->momx = SV_ReadLong();
    mo->momy = SV_ReadLong();
    mo->momz = SV_ReadLong();

    // If == validcount, already checked.
    mo->valid = SV_ReadLong();

    mo->type = SV_ReadLong();
    SV_ReadLong();	// &mobjinfo[mobj->type]
    
    mo->tics = SV_ReadLong();	// state tic counter
    mo->state = (state_t*) SV_ReadLong();
    mo->flags = SV_ReadLong();
    mo->health = SV_ReadLong();

    // Movement direction, movement generation (zig-zagging).
    mo->movedir = SV_ReadLong();	// 0-7
    mo->movecount = SV_ReadLong();	// when 0, select a new dir

    // Thing being chased/attacked (or NULL),
    // also the originator for missiles.
    SV_ReadLong();

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    mo->reactiontime = SV_ReadLong();   

    // If >0, the target will be chased
    // no matter what (even if shot)
    mo->threshold = SV_ReadLong();

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    mo->player = (player_t*) SV_ReadLong();

    // Player number last looked for.
    mo->lastlook = SV_ReadLong();	

    // For nightmare respawn.
    SV_Read(&mo->spawnpoint, 10);	

    // Thing being chased/attacked for tracers.
    SV_ReadLong();
}

//
// P_ArchivePlayers
//
/*void P_ArchivePlayers (void)
{
    int			i;
    int			j;
	player_t	dest;
	
    for(i=0; i<4; i++)
    {
		if (!players[i].plr->ingame)
			continue;
		
		PADSAVEP();
		
		//dest = (saveplayer_t*) save_p;
//		P_PlayerConverter(&players[i], dest, true);
		memcpy(&dest, players + i, sizeof(dest));		
		//save_p += sizeof(saveplayer_t);
		for(j=0; j<NUMPSPRITES; j++)
		{
			if(dest.psprites[j].state)
			{
				dest.psprites[j].state 
					= (state_t*) (dest.psprites[j].state-states);
			}
		}
		SV_WritePlayer(&dest);
    }
}

*/

//
// P_UnArchivePlayers
//
void P_v19_UnArchivePlayers (void)
{
    int		i;
    int		j;
	
    for (i=0 ; i<4 ; i++)
    {
		if (!players[i].plr->ingame)
			continue;
		
		PADSAVEP();
		
		//memcpy (&players[i],save_p, sizeof(player_t));

		//P_PlayerConverter(&players[i], (saveplayer_t*) save_p, false);
		SV_ReadPlayer(players + i);
		//save_p += sizeof(saveplayer_t);
		
		// will be set when unarc thinker
		players[i].plr->mo = NULL;	
		players[i].message = NULL;
		players[i].attacker = NULL;
		
		for(j=0; j<NUMPSPRITES; j++)
		{
			if(players[i].psprites[j].state)
			{
				players[i].psprites[j].state 
					= &states[ (int) players[i].psprites[j].state ];
			}
		}
    }
}


//
// P_ArchiveWorld
//
/*void P_ArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*	sec;
    line_t*		li;
    side_t*		si;
    short*		put;
	
    put = (short*) save_p;
    
    // do sectors
    for(i=0, sec = sectors; i<numsectors; i++, sec++)
    {
		*put++ = sec->floorheight >> FRACBITS;
		*put++ = sec->ceilingheight >> FRACBITS;
		*put++ = sec->floorpic;
		*put++ = sec->ceilingpic;
		*put++ = sec->lightlevel;
		*put++ = sec->special;		// needed?
		*put++ = sec->tag;		// needed?
    }
    
    // do lines
    for(i=0, li = lines; i<numlines; i++, li++)
    {
		*put++ = li->flags;
		*put++ = li->special;
		*put++ = li->tag;
		for(j=0; j<2; j++)
		{
			if (li->sidenum[j] == -1)
				continue;
			
			si = &sides[li->sidenum[j]];
			
			*put++ = si->textureoffset >> FRACBITS;
			*put++ = si->rowoffset >> FRACBITS;
			*put++ = si->toptexture;
			*put++ = si->bottomtexture;
			*put++ = si->midtexture;	
		}
    }
    save_p = (byte*) put;
}
*/


//
// P_UnArchiveWorld
//
void P_v19_UnArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*	sec;
    line_t*		li;
    side_t*		si;
    short*		get;
	int			firstflat = W_CheckNumForName("F_START") + 1;
	
    get = (short*) save_p;
    
    // do sectors
    for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
    {
		sec->floorheight = *get++ << FRACBITS;
		sec->ceilingheight = *get++ << FRACBITS;
		sec->floorpic = *get++ + firstflat;
		sec->ceilingpic = *get++ + firstflat;
		sec->lightlevel = *get++;
		sec->special = *get++;		// needed?
		sec->tag = *get++;		// needed?
		sec->specialdata = 0;
		sec->soundtarget = 0;
    }
    
    // do lines
    for (i=0, li = lines ; i<numlines ; i++,li++)
    {
		li->flags = *get++;
		li->special = *get++;
		li->tag = *get++;
		for (j=0 ; j<2 ; j++)
		{
			if (li->sidenum[j] == -1)
				continue;
			si = &sides[li->sidenum[j]];
			si->textureoffset = *get++ << FRACBITS;
			si->rowoffset = *get++ << FRACBITS;
			si->toptexture = *get++;
			si->bottomtexture = *get++;
			si->midtexture = *get++;
		}
    }
    save_p = (byte *)get;	
}





//
// Thinkers
//
typedef enum
{
    tc_end,
    tc_mobj

} thinkerclass_t;



//
// P_ArchiveThinkers
//
/*void P_ArchiveThinkers (void)
{
    thinker_t*	th;
    mobj_t		smobj;
	
    // save off the current thinkers
    for(th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
		if(th->function == P_MobjThinker)
		{
			*save_p++ = tc_mobj;
			PADSAVEP();
			//smobj = (savemobj_t*) save_p;
			//memcpy (mobj, th, sizeof(*mobj));
			//P_MobjConverter( (mobj_t*) th, smobj, true);
			//save_p += sizeof(*smobj);
			memcpy(&smobj, th, sizeof(smobj));
			smobj.state = (state_t*) (smobj.state - states);
			if(smobj.player)
				smobj.player = (player_t *)((smobj.player-players) + 1);
			SV_WriteMobj(&smobj);
			continue;
		}
		
		// Con_Error ("P_ArchiveThinkers: Unknown thinker function");
    }
	
    // add a terminating marker
    *save_p++ = tc_end;	
}
*/


//
// P_UnArchiveThinkers
//
void P_v19_UnArchiveThinkers (void)
{
    byte		tclass;
    thinker_t*	currentthinker;
    thinker_t*	next;
    mobj_t*		mobj;
    
    // remove all the current thinkers
    currentthinker = thinkercap.next;
    while (currentthinker != &thinkercap)
    {
		next = currentthinker->next;
		
		if (currentthinker->function == P_MobjThinker)
			P_RemoveMobj ((mobj_t *)currentthinker);
		else
			Z_Free (currentthinker);
		
		currentthinker = next;
    }
    P_InitThinkers ();
	
    // read in saved thinkers
    while (1)
    {
		tclass = *save_p++;
		switch (tclass)
		{
		case tc_end:
			return; 	// end of list
			
		case tc_mobj:
			PADSAVEP();
			mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
			memset(mobj, 0, sizeof(*mobj));
			//memcpy (mobj, save_p, sizeof(*mobj));
			//P_MobjConverter(mobj, (savemobj_t*) save_p, false);
			//memcpy(mobj, save_p, sizeof(*mobj));
			SV_ReadMobj(mobj);
			//save_p += sizeof(savemobj_t);
			mobj->state = &states[(int)mobj->state];
			mobj->target = NULL;
			if (mobj->player)
			{
				int pnum = (int) mobj->player - 1;
				mobj->player = &players[pnum];
				mobj->dplayer = mobj->player->plr;
				mobj->dplayer->mo = mobj;
				mobj->dplayer->clAngle = mobj->angle;
				mobj->dplayer->clLookDir = 0;
			}
			P_SetThingPosition (mobj);
			mobj->info = &mobjinfo[mobj->type];
			mobj->floorz = mobj->subsector->sector->floorheight;
			mobj->ceilingz = mobj->subsector->sector->ceilingheight;
			mobj->thinker.function = P_MobjThinker;
			P_AddThinker (&mobj->thinker);
			break;
			
		default:
			Con_Error ("Unknown tclass %i in savegame",tclass);
		}
    }
}


//
// P_ArchiveSpecials
//
enum
{
    tc_ceiling,
    tc_door,
    tc_floor,
    tc_plat,
    tc_flash,
    tc_strobe,
    tc_glow,
    tc_endspecials

} specials_e;	



//
// Things to handle:
//
// T_MoveCeiling, (ceiling_t: sector_t * swizzle), - active list
// T_VerticalDoor, (vldoor_t: sector_t * swizzle),
// T_MoveFloor, (floormove_t: sector_t * swizzle),
// T_LightFlash, (lightflash_t: sector_t * swizzle),
// T_StrobeFlash, (strobe_t: sector_t *),
// T_Glow, (glow_t: sector_t *),
// T_PlatRaise, (plat_t: sector_t *), - active list
//
/*void P_ArchiveSpecials (void)
{
    thinker_t*		th;
    ceiling_t*		ceiling;
    vldoor_t*		door;
    floormove_t*	floor;
    plat_t*			plat;
    lightflash_t*	flash;
    strobe_t*		strobe;
    glow_t*			glow;
    int				i;
	
    // save off the current thinkers
    for (th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
		if (th->function == NULL)
		{
			for (i = 0; i < MAXCEILINGS;i++)
				if (activeceilings[i] == (ceiling_t *)th)
					break;
				
				if (i<MAXCEILINGS)
				{
					*save_p++ = tc_ceiling;
					PADSAVEP();
					ceiling = (ceiling_t *)save_p;
					memcpy (ceiling, th, sizeof(*ceiling));
					save_p += sizeof(*ceiling);
					ceiling->sector = (sector_t *)(ceiling->sector - sectors);
				}
				continue;
		}
		
		if (th->function == T_MoveCeiling)
		{
			*save_p++ = tc_ceiling;
			PADSAVEP();
			ceiling = (ceiling_t *)save_p;
			memcpy (ceiling, th, sizeof(*ceiling));
			save_p += sizeof(*ceiling);
			ceiling->sector = (sector_t *)(ceiling->sector - sectors);
			continue;
		}
		
		if (th->function == T_VerticalDoor)
		{
			*save_p++ = tc_door;
			PADSAVEP();
			door = (vldoor_t *)save_p;
			memcpy (door, th, sizeof(*door));
			save_p += sizeof(*door);
			door->sector = (sector_t *)(door->sector - sectors);
			continue;
		}
		
		if (th->function == T_MoveFloor)
		{
			*save_p++ = tc_floor;
			PADSAVEP();
			floor = (floormove_t *)save_p;
			memcpy (floor, th, sizeof(*floor));
			save_p += sizeof(*floor);
			floor->sector = (sector_t *)(floor->sector - sectors);
			continue;
		}
		
		if (th->function == T_PlatRaise)
		{
			*save_p++ = tc_plat;
			PADSAVEP();
			plat = (plat_t *)save_p;
			memcpy (plat, th, sizeof(*plat));
			save_p += sizeof(*plat);
			plat->sector = (sector_t *)(plat->sector - sectors);
			continue;
		}
		
		if (th->function == T_LightFlash)
		{
			*save_p++ = tc_flash;
			PADSAVEP();
			flash = (lightflash_t *)save_p;
			memcpy (flash, th, sizeof(*flash));
			save_p += sizeof(*flash);
			flash->sector = (sector_t *)(flash->sector - sectors);
			continue;
		}
		
		if (th->function == T_StrobeFlash)
		{
			*save_p++ = tc_strobe;
			PADSAVEP();
			strobe = (strobe_t *)save_p;
			memcpy (strobe, th, sizeof(*strobe));
			save_p += sizeof(*strobe);
			strobe->sector = (sector_t *)(strobe->sector - sectors);
			continue;
		}
		
		if (th->function == T_Glow)
		{
			*save_p++ = tc_glow;
			PADSAVEP();
			glow = (glow_t *)save_p;
			memcpy (glow, th, sizeof(*glow));
			save_p += sizeof(*glow);
			glow->sector = (sector_t *)(glow->sector - sectors);
			continue;
		}
    }
	
    // add a terminating marker
    *save_p++ = tc_endspecials;	
}
*/

//
// P_UnArchiveSpecials
//
void P_v19_UnArchiveSpecials (void)
{
    byte			tclass;
    ceiling_t*		ceiling;
    vldoor_t*		door;
    floormove_t*	floor;
    plat_t*			plat;
    lightflash_t*	flash;
    strobe_t*		strobe;
    glow_t*			glow;
	
	
    // read in saved thinkers
    while (1)
    {
		tclass = *save_p++;
		switch (tclass)
		{
		case tc_endspecials:
			return;	// end of list
			
		case tc_ceiling:
			PADSAVEP();
			ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, NULL);
			memcpy (ceiling, save_p, sizeof(*ceiling));
			save_p += sizeof(*ceiling);
			ceiling->sector = &sectors[(int)ceiling->sector];
			ceiling->sector->specialdata = ceiling;
			
			if (ceiling->thinker.function)
				ceiling->thinker.function = T_MoveCeiling;
			
			P_AddThinker (&ceiling->thinker);
			P_AddActiveCeiling(ceiling);
			break;
			
		case tc_door:
			PADSAVEP();
			door = Z_Malloc (sizeof(*door), PU_LEVEL, NULL);
			memcpy (door, save_p, sizeof(*door));
			save_p += sizeof(*door);
			door->sector = &sectors[(int)door->sector];
			door->sector->specialdata = door;
			door->thinker.function = T_VerticalDoor;
			P_AddThinker (&door->thinker);
			break;
			
		case tc_floor:
			PADSAVEP();
			floor = Z_Malloc (sizeof(*floor), PU_LEVEL, NULL);
			memcpy (floor, save_p, sizeof(*floor));
			save_p += sizeof(*floor);
			floor->sector = &sectors[(int)floor->sector];
			floor->sector->specialdata = floor;
			floor->thinker.function = T_MoveFloor;
			P_AddThinker (&floor->thinker);
			break;
			
		case tc_plat:
			PADSAVEP();
			plat = Z_Malloc (sizeof(*plat), PU_LEVEL, NULL);
			memcpy (plat, save_p, sizeof(*plat));
			save_p += sizeof(*plat);
			plat->sector = &sectors[(int)plat->sector];
			plat->sector->specialdata = plat;
			
			if (plat->thinker.function)
				plat->thinker.function = T_PlatRaise;
			
			P_AddThinker (&plat->thinker);
			P_AddActivePlat(plat);
			break;
			
		case tc_flash:
			PADSAVEP();
			flash = Z_Malloc (sizeof(*flash), PU_LEVEL, NULL);
			memcpy (flash, save_p, sizeof(*flash));
			save_p += sizeof(*flash);
			flash->sector = &sectors[(int)flash->sector];
			flash->thinker.function = T_LightFlash;
			P_AddThinker (&flash->thinker);
			break;
			
		case tc_strobe:
			PADSAVEP();
			strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, NULL);
			memcpy (strobe, save_p, sizeof(*strobe));
			save_p += sizeof(*strobe);
			strobe->sector = &sectors[(int)strobe->sector];
			strobe->thinker.function = T_StrobeFlash;
			P_AddThinker (&strobe->thinker);
			break;
			
		case tc_glow:
			PADSAVEP();
			glow = Z_Malloc (sizeof(*glow), PU_LEVEL, NULL);
			memcpy (glow, save_p, sizeof(*glow));
			save_p += sizeof(*glow);
			glow->sector = &sectors[(int)glow->sector];
			glow->thinker.function = T_Glow;
			P_AddThinker (&glow->thinker);
			break;
			
		default:
			Con_Error ("P_UnarchiveSpecials:Unknown tclass %i "
				"in savegame",tclass);
		}
    }
}

void SV_v19_LoadGame(char *savename)
{
    int		length; 
    int		i; 
    int		a,b,c; 
    char	vcheck[VERSIONSIZE]; 

    length = M_ReadFile(savename, &savebuffer); 
    // Skip the description field.
	save_p = savebuffer + SAVESTRINGSIZE;
	// Check version.
    memset(vcheck, 0, sizeof(vcheck)); 
    sprintf(vcheck, "version %i", SAVE_VERSION); 
    if(strcmp(save_p, vcheck)) 
	{
		int saveVer;
		sscanf(save_p, "version %i", &saveVer);
		if(saveVer >= SAVE_VERSION_BASE) 
		{
			// Must be from the wrong game.
			Con_Message("Bad savegame version.\n");
			return;
		}
		// Just give a warning.
		Con_Message("Savegame ID '%s': incompatible?\n", save_p);
	}
    save_p += VERSIONSIZE; 
			 
    gameskill = *save_p++; 
    gameepisode = *save_p++; 
    gamemap = *save_p++; 
    for(i=0; i<4; i++) 
		players[i].plr->ingame = *save_p++; 
	
    // Load a base level.
    G_InitNew(gameskill, gameepisode, gamemap); 
	
    // get the times 
    a = *save_p++; 
    b = *save_p++; 
    c = *save_p++; 
    leveltime = (a<<16) + (b<<8) + c; 
	
    // dearchive all the modifications
    P_v19_UnArchivePlayers (); 
    P_v19_UnArchiveWorld (); 
    P_v19_UnArchiveThinkers (); 
    P_v19_UnArchiveSpecials (); 
	
    if(*save_p != 0x1d)  
		Con_Error("SV_v19_LoadGame: Bad savegame (consistency test failed!)\n");
    
    // done 
    Z_Free (savebuffer); 
	savebuffer = NULL;

	// Spawn particle generators.
	R_SetupLevel("", DDSLF_AFTER_LOADING);
}
