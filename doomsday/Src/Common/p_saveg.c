
//**************************************************************************
//**
//** P_SAVEG.C
//**
//** New SaveGame I/O.
//** Utilizes LZSS compression.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <LZSS.h>

#ifdef __JHERETIC__
#include "Doomdef.h"
#include "Dstrings.h"
#endif

#ifdef __JDOOM__
#include "doomdef.h"
#include "dstrings.h"
#include "p_local.h"
#include "g_game.h"
#include "doomstat.h"
#include "r_state.h"
#endif 

#include "f_infine.h"
#include "p_oldsvg.h"
#include "d_net.h"
#include "p_svtexarc.h"

// MACROS ------------------------------------------------------------------

#ifdef __JDOOM__
#define MY_SAVE_MAGIC			0x1DEAD666
#define MY_CLIENT_SAVE_MAGIC	0x2DEAD666
#define MY_SAVE_VERSION			4
#define SAVESTRINGSIZE			24
#define CONSISTENCY				0x2c
#define SAVEGAMENAME			"DoomSav"
#define CLIENTSAVEGAMENAME		"DoomCl"
#define SAVEGAMEEXTENSION		"dsg"
#endif

#ifdef __JHERETIC__
#define MY_SAVE_MAGIC			0x7D9A12C5
#define MY_CLIENT_SAVE_MAGIC	0x1062AF43
#define MY_SAVE_VERSION			4
#define SAVESTRINGSIZE			24
#define CONSISTENCY				0x9d
#define SAVEGAMENAME			"HticSav"
#define CLIENTSAVEGAMENAME		"HticCl"
#define SAVEGAMEEXTENSION		"hsg"
#endif

#define MAX_ARCHIVED_THINGS		1024

// TYPES -------------------------------------------------------------------

typedef enum lineclass_e {
	lc_normal,
	lc_xg1
} lineclass_t;

typedef struct
{
	int magic;
	int version;
	int gamemode;		
	char description[SAVESTRINGSIZE];
	byte skill;
	byte episode;
	byte map;
	byte deathmatch;
	byte nomonsters;
	byte respawn;
	int leveltime;
	byte players[MAXPLAYERS];
	unsigned int gameid;
} saveheader_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

LZFILE *savefile;
char save_path[128]; /* = "savegame\\"; */
char client_save_path[128]; /* = "savegame\\client\\"; */

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static saveheader_t hdr;
static mobj_t *thing_archive[MAX_ARCHIVED_THINGS];
static int SaveToRealPlayer[MAXPLAYERS];

// CODE --------------------------------------------------------------------

// Must be called before saving or loading any data.
void SV_InitThingArchive(void)
{
	memset(thing_archive, 0, sizeof(thing_archive));
}

// Called by the write code to get archive numbers.
// If the mobj is already archived, the existing number is returned.
// Number zero is not used.
unsigned short SV_ThingArchiveNum(mobj_t *mo)
{
	int i;
	int first_empty = -1;

	// Null things always have the number zero.
	if(!mo) return 0;

	for(i=0; i<MAX_ARCHIVED_THINGS; i++)
	{
		if(!thing_archive[i] && first_empty < 0)
		{
			first_empty = i;
			continue;
		}		
		if(thing_archive[i] == mo) return i + 1;
	}
	if(first_empty < 0) return 0;	// No number available!
	// OK, place it in an empty pos.
	thing_archive[first_empty] = mo;
	return first_empty + 1;
}

// Used by the read code when mobjs are read.
void SV_SetArchiveThing(mobj_t *mo, int num)
{
	if(!num) return;
	thing_archive[num-1] = mo;
}

mobj_t *SV_GetArchiveThing(int num)
{
	if(!num) return NULL;
	return thing_archive[num-1];				
}

unsigned int SV_GameID(void)
{
	return Sys_GetRealTime() + (leveltime << 24);
}

void SV_Write(void *data, int len)
{
	lzWrite(data, len, savefile);
}

void SV_WriteByte(byte val)
{
	lzPutC(val, savefile);
}

void SV_WriteShort(short val)
{
	lzPutW(val, savefile);	
}

void SV_WriteLong(long val)
{
	lzPutL(val, savefile);
}

void SV_WriteFloat(float val)
{
	lzPutL(*(long*) &val, savefile);
}

void SV_Read(void *data, int len)
{
	lzRead(data, len, savefile);
}

byte SV_ReadByte()
{
	return lzGetC(savefile);
}

short SV_ReadShort()
{
	return lzGetW(savefile);
}

long SV_ReadLong()
{
	return lzGetL(savefile);
}

float SV_ReadFloat()
{
	long val = lzGetL(savefile);
	return *(float*) &val;
}

void SV_WritePlayer(int playernum)
{
	player_t temp, *pl = &temp;
	ddplayer_t *dpl;
	int i;

	memcpy(pl, players + playernum, sizeof(temp));		
	dpl = pl->plr;
	for(i=0; i<NUMPSPRITES; i++)
	{
		if(temp.psprites[i].state)
		{
			temp.psprites[i].state 
				= (state_t*) (temp.psprites[i].state-states);
		}
	}

	SV_WriteByte(1);		// Write a version byte.
	SV_WriteLong(pl->playerstate);
	SV_WriteLong(dpl->viewz);
	SV_WriteLong(dpl->viewheight);
	SV_WriteLong(dpl->deltaviewheight);
	SV_WriteFloat(dpl->lookdir);
	SV_WriteLong(pl->bob);
	
	SV_WriteLong(pl->health);
	SV_WriteLong(pl->armorpoints);
	SV_WriteLong(pl->armortype);

	SV_Write(pl->powers, NUMPOWERS * 4);
#ifdef __JDOOM__
	SV_Write(pl->cards, NUMCARDS * 4);
#endif
#ifdef __JHERETIC__
	SV_Write(pl->keys, NUMKEYS * 4);
#endif
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

	SV_WriteLong(pl->damagecount);
	SV_WriteLong(pl->bonuscount);
	
	SV_WriteLong(dpl->extralight);
	SV_WriteLong(dpl->fixedcolormap);
	SV_WriteLong(pl->colormap);
	SV_Write(pl->psprites, NUMPSPRITES * sizeof(pspdef_t));
	
	SV_WriteLong(pl->didsecret);

#ifdef __JHERETIC__
	SV_WriteLong(pl->messageTics);
	SV_WriteLong(pl->flyheight);
	SV_Write(pl->inventory, 4*2*14);
	SV_WriteLong(pl->readyArtifact);
	SV_WriteLong(pl->artifactCount);
	SV_WriteLong(pl->inventorySlotNum);
	SV_WriteLong(pl->chickenPeck);
	SV_WriteLong(pl->chickenTics);
	SV_WriteLong(pl->flamecount);
#endif
}

void SV_ReadPlayer(player_t *pl)
{
	ddplayer_t *dpl = pl->plr;
	int j;

	SV_ReadByte();	// The version (not used yet).

	pl->playerstate = SV_ReadLong();
	dpl->viewz = SV_ReadLong();
	dpl->viewheight = SV_ReadLong();
	dpl->deltaviewheight = SV_ReadLong();
	dpl->lookdir = SV_ReadFloat();
	pl->bob = SV_ReadLong();
	
	pl->health = SV_ReadLong();
	pl->armorpoints = SV_ReadLong();
	pl->armortype = SV_ReadLong();

	SV_Read(pl->powers, NUMPOWERS * 4);
#ifdef __JDOOM__
	SV_Read(pl->cards, NUMCARDS * 4);
#endif
#ifdef __JHERETIC__
	SV_Read(pl->keys, NUMKEYS * 4);
#endif
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

	pl->damagecount = SV_ReadLong();
	pl->bonuscount = SV_ReadLong();
	
	dpl->extralight = SV_ReadLong();
	dpl->fixedcolormap = SV_ReadLong();
	pl->colormap = SV_ReadLong();
	SV_Read(pl->psprites, NUMPSPRITES * sizeof(pspdef_t));
	
	pl->didsecret = SV_ReadLong();

#ifdef __JHERETIC__
	pl->messageTics = SV_ReadLong();
	pl->flyheight = SV_ReadLong();
	SV_Read(pl->inventory, 4*2*14);
	pl->readyArtifact = SV_ReadLong();
	pl->artifactCount = SV_ReadLong();
	pl->inventorySlotNum = SV_ReadLong();
	pl->chickenPeck = SV_ReadLong();
	pl->chickenTics = SV_ReadLong();
	pl->flamecount = SV_ReadLong();
#endif

	for(j=0; j<NUMPSPRITES; j++)
		if(pl->psprites[j].state)
		{
			pl->psprites[j].state 
				= &states[ (int) pl->psprites[j].state ];
		}

	// Mark the player for fixpos and fixangles.
	dpl->flags |= DDPF_FIXPOS | DDPF_FIXANGLES | DDPF_FIXMOM;
	pl->update = PSF_REBORN;
}

void SV_WriteMobj(mobj_t *mobj)
{
	mobj_t mo;

	// Mangle it!
	memcpy(&mo, mobj, sizeof(mo));
	mo.state = (state_t*) (mo.state - states);
	if(mo.player)
		mo.player = (player_t *)((mo.player - players) + 1);
	
	// Version.
	// 4: Added byte 'translucency'
	SV_WriteByte(4);

	// A version 2 features: archive number and target.
	SV_WriteShort(SV_ThingArchiveNum(mobj));
	SV_WriteShort(SV_ThingArchiveNum(mo.target));

    // Info for drawing: position.
	SV_WriteLong(mo.x);
	SV_WriteLong(mo.y);
	SV_WriteLong(mo.z);

    //More drawing info: to determine current sprite.
    SV_WriteLong(mo.angle);	// orientation
    SV_WriteLong(mo.sprite);	// used to find patch_t and flip value
    SV_WriteLong(mo.frame);	// might be ORed with FF_FULLBRIGHT

    // The closest interval over all contacted Sectors.
    SV_WriteLong(mo.floorz);
    SV_WriteLong(mo.ceilingz);

    // For movement checking.
    SV_WriteLong(mo.radius);
    SV_WriteLong(mo.height);	

    // Momentums, used to update position.
    SV_WriteLong(mo.momx);
    SV_WriteLong(mo.momy);
    SV_WriteLong(mo.momz);

    // If == validcount, already checked.
#ifdef __JDOOM__
    SV_WriteLong(mo.valid);
#elif defined(__JHERETIC__)
	SV_WriteLong(mo.valid);
#endif

    SV_WriteLong(mo.type);
    
    SV_WriteLong(mo.tics);	// state tic counter
    SV_WriteLong( (int) mo.state);
    SV_WriteLong(mo.flags);
    SV_WriteLong(mo.health);

    // Movement direction, movement generation (zig-zagging).
    SV_WriteLong(mo.movedir);	// 0-7
    SV_WriteLong(mo.movecount);	// when 0, select a new dir

    // Reaction time: if non 0, don't attack yet.
    // Used by player to freeze a bit after teleporting.
    SV_WriteLong(mo.reactiontime);   

    // If >0, the target will be chased
    // no matter what (even if shot)
    SV_WriteLong(mo.threshold);

    // Additional info record for player avatars only.
    // Only valid if type == MT_PLAYER
    SV_WriteLong( (int) mo.player);

    // Player number last looked for.
    SV_WriteLong(mo.lastlook);	

    // For nightmare respawn.
    SV_Write(&mo.spawnpoint, 10);	

#ifdef __JDOOM__
	SV_WriteLong(mo.intflags);		// killough $dropoff_fix: internal flags
	SV_WriteLong(mo.dropoffz);		// killough $dropoff_fix
	SV_WriteLong(mo.gear);			// killough used in torque simulation
#endif

#ifdef __JHERETIC__
	SV_WriteLong(mo.damage);
	SV_WriteLong(mo.flags2);
	SV_WriteLong(mo.special1);
	SV_WriteLong(mo.special2);
#endif

	SV_WriteByte(mo.translucency);
}

void SV_ReadMobj(mobj_t *mo)
{
	int ver = SV_ReadByte();

	if(ver >= 2)
	{
		// Version 2 has mobj archive numbers.
		SV_SetArchiveThing(mo, SV_ReadShort());
		// The reference will be updated after all mobjs are loaded.
		mo->target = (mobj_t*)(int) SV_ReadShort();
	}

    // Info for drawing: position.
	mo->x = SV_ReadLong();
	mo->y = SV_ReadLong();
	mo->z = SV_ReadLong();

    //More drawing info: to determine current sprite.
    mo->angle = SV_ReadLong();	// orientation
    mo->sprite = SV_ReadLong();	// used to find patch_t and flip value
    mo->frame = SV_ReadLong();	// might be ORed with FF_FULLBRIGHT

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
#ifdef __JDOOM__
    mo->valid = SV_ReadLong();
#elif defined(__JHERETIC__)
	mo->valid = SV_ReadLong();
#endif

    mo->type = SV_ReadLong();
    
    mo->tics = SV_ReadLong();	// state tic counter
    mo->state = (state_t*) SV_ReadLong();
    mo->flags = SV_ReadLong();
    mo->health = SV_ReadLong();

    // Movement direction, movement generation (zig-zagging).
    mo->movedir = SV_ReadLong();	// 0-7
    mo->movecount = SV_ReadLong();	// when 0, select a new dir

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

#ifdef __JDOOM__
	if(ver >= 3)
	{
		mo->intflags = SV_ReadLong();		// killough $dropoff_fix: internal flags
		mo->dropoffz = SV_ReadLong();		// killough $dropoff_fix
		mo->gear = SV_ReadLong();			// killough used in torque simulation
	}
#endif

#ifdef __JHERETIC__
	mo->damage = SV_ReadLong();
	mo->flags2 = SV_ReadLong();
	mo->special1 = SV_ReadLong();
	mo->special2 = SV_ReadLong();
#endif

	// Version 4 has the translucency byte.
	if(ver >= 4)
	{
		mo->translucency = SV_ReadByte();
	}

	// Restore! (unmangle)
	mo->state = &states[(int)mo->state];
	if(mo->player)
	{
		// Saved players might be in a different order than at the moment.
		int pnum = SaveToRealPlayer[(int) mo->player - 1];
		mo->player = &players[pnum];
		mo->dplayer = mo->player->plr;
		mo->dplayer->mo = mo;
		mo->dplayer->clAngle = mo->angle;
		mo->dplayer->clLookDir = 0;
	}
	mo->visangle = mo->angle >> 16;
}

enum 
{
	sc_normal,
	sc_ploff,	// plane offset
	sc_xg1		
} 
sectorclass_e;

void SV_WriteSector(sector_t *sec)
{
	int type;

	// Determine type.
	if(sec->xg)
		type = sc_xg1;
	else if(sec->flooroffx 
		|| sec->flooroffy 
		|| sec->ceiloffx 
		|| sec->ceiloffy)
		type = sc_ploff;
	else
		type = sc_normal;

	// Type byte.
	SV_WriteByte(type);	

	SV_WriteShort(sec->floorheight >> FRACBITS);
	SV_WriteShort(sec->ceilingheight >> FRACBITS);
	SV_WriteShort(SV_FlatArchiveNum(sec->floorpic));
	SV_WriteShort(SV_FlatArchiveNum(sec->ceilingpic));
	SV_WriteByte(sec->lightlevel);
	SV_Write(sec->rgb, 3);
	SV_WriteShort(sec->special);
	SV_WriteShort(sec->tag);

	if(type == sc_xg1 || type == sc_ploff)
	{
		SV_WriteFloat(sec->flooroffx);
		SV_WriteFloat(sec->flooroffy);
		SV_WriteFloat(sec->ceiloffx);
		SV_WriteFloat(sec->ceiloffy);
	}

	if(sec->xg) // Extended General?
	{
		SV_WriteXGSector(sec);
	}
}

void SV_ReadSector(sector_t *sec)
{
	int type;

	// Read the type.
	type = SV_ReadByte();

	sec->floorheight = SV_ReadShort() << FRACBITS;
	sec->ceilingheight = SV_ReadShort() << FRACBITS;
	sec->floorpic = SV_ReadShort();
	sec->ceilingpic = SV_ReadShort();
	
	if(hdr.version >= 4)
	{
		// The flat numbers are actually archive numbers.
		sec->floorpic = SV_GetArchiveFlat(sec->floorpic);
		sec->ceilingpic = SV_GetArchiveFlat(sec->ceilingpic);
	}

	sec->lightlevel = SV_ReadByte();
	SV_Read(sec->rgb, 3);
	sec->special = SV_ReadShort();
	sec->tag = SV_ReadShort();

	if(type == sc_xg1 || type == sc_ploff)
	{
		sec->flooroffx = SV_ReadFloat();
		sec->flooroffy = SV_ReadFloat();
		sec->ceiloffx = SV_ReadFloat();
		sec->ceiloffy = SV_ReadFloat();
	}

	if(type == sc_xg1) SV_ReadXGSector(sec);
}

void SV_WriteLine(line_t *li)
{
	lineclass_t type = li->xg? lc_xg1 : lc_normal;
	int i;
	side_t *si;

	SV_WriteByte(type);

	SV_WriteShort(li->flags);
	SV_WriteShort(li->special);
	SV_WriteShort(li->tag);
	for(i=0; i<2; i++)
	{
		if(li->sidenum[i] < 0) continue;

		si = &sides[li->sidenum[i]];

		SV_WriteShort(si->textureoffset >> FRACBITS);
		SV_WriteShort(si->rowoffset >> FRACBITS);
		SV_WriteShort(SV_TextureArchiveNum(si->toptexture));
		SV_WriteShort(SV_TextureArchiveNum(si->bottomtexture));
		SV_WriteShort(SV_TextureArchiveNum(si->midtexture));
	}

	// Extended General?
	if(li->xg)
	{
		SV_WriteXGLine(li);
	}
}

void SV_ReadLine(line_t *li)
{
	enum lineclass_e type;
	int i;
	side_t *si;

	type = SV_ReadByte();

	li->flags = SV_ReadShort();
	li->special = SV_ReadShort();
	li->tag = SV_ReadShort();

	for(i=0; i<2; i++)
	{
		if(li->sidenum[i] < 0) continue;

		si = &sides[li->sidenum[i]];

		si->textureoffset = SV_ReadShort() << FRACBITS;
		si->rowoffset = SV_ReadShort() << FRACBITS;
		si->toptexture = SV_ReadShort();
		si->bottomtexture = SV_ReadShort();
		si->midtexture = SV_ReadShort();

		if(hdr.version >= 4)
		{
			// The texture numbers are only archive numbers.
			si->toptexture = SV_GetArchiveTexture(si->toptexture);
			si->bottomtexture = SV_GetArchiveTexture(si->bottomtexture);
			si->midtexture = SV_GetArchiveTexture(si->midtexture);
		}
	}

	// Extended General?
	if(type == lc_xg1) SV_ReadXGLine(li);
}

//
// P_ArchivePlayers
//
void P_ArchivePlayers(void)
{
    int			i;
	
    for(i=0; i<MAXPLAYERS; i++)
    {
		if(!players[i].plr->ingame) continue;
		SV_WriteLong(Net_GetPlayerID(i));
		SV_WritePlayer(i);
    }
}

//
// P_UnArchivePlayers
//
void P_UnArchivePlayers(boolean *infile, boolean *loaded)
{
    int		i;
    int		j;
	unsigned int pid;
	player_t dummy_player;
	ddplayer_t dummy_ddplayer;
	player_t *player;
	
	// Setup the dummy.
	dummy_player.plr = &dummy_ddplayer;

    for(i=0; i<MAXPLAYERS; i++)
    {
		// By default a saved player translates to nothing.
		SaveToRealPlayer[i] = -1;

		if(!infile[i]) continue;
		
		// The ID number will determine which player this actually is.
		pid = SV_ReadLong();
		for(player=0, j=0; j<MAXPLAYERS; j++)
			if(Net_GetPlayerID(j) == pid)
			{
				// This is our guy.
				player = players + j;
				loaded[j] = true;
				// Later references to the player number 'i' must be
				// translated!
				SaveToRealPlayer[i] = j;
#ifdef _DEBUG
				Con_Printf("P_UnArchivePlayers: Saved %i is now %i.\n", i, j);
#endif
				break;
			}
		if(!player)
		{
			// We have a missing player. Use a dummy to load the data.
			player = &dummy_player;
		}
		SV_ReadPlayer(player);
		// Will be set when unarc thinker.
		player->plr->mo = NULL;	
		player->message = NULL;
		player->attacker = NULL;
    }
}

//
// P_ArchiveWorld
//
void P_ArchiveWorld (void)
{
    int			i;
    sector_t*	sec;
    line_t*		li;

	//
	// Only write world in the new format
	//

	// Write the texture archives.
	SV_WriteTextureArchive();

	for(i=0, sec = sectors; i<numsectors; i++, sec++)
		SV_WriteSector(sec);

	for(i=0, li = lines; i<numlines; i++, li++)
		SV_WriteLine(li);
}



//
// P_UnArchiveWorld
//
void P_UnArchiveWorld (void)
{
    int			i;
    int			j;
    sector_t*	sec;
    line_t*		li;
    side_t*		si;

	if(hdr.version > 1)
	{
		if(hdr.version >= 4)
		{
			// Read the texture archives.
			SV_ReadTextureArchive();
		}

		// Load sectors.
		for(i=0, sec = sectors; i<numsectors; i++, sec++)
			SV_ReadSector(sec);
		
		// Load lines.
		for(i=0, li = lines; i<numlines; i++, li++)
			SV_ReadLine(li);
	}
	else // Old version 1 world.
	{
		int firstflat = W_CheckNumForName("F_START") + 1;

		// do sectors
		for (i=0, sec = sectors ; i<numsectors ; i++,sec++)
		{
			sec->floorheight = SV_ReadShort() << FRACBITS;
			sec->ceilingheight = SV_ReadShort() << FRACBITS;
			sec->floorpic = SV_ReadShort() + firstflat;
			sec->ceilingpic = SV_ReadShort() + firstflat;
			sec->lightlevel = SV_ReadShort();
			sec->special = SV_ReadShort();		// needed?
			sec->tag = SV_ReadShort();		// needed?
			sec->specialdata = 0;
			sec->soundtarget = 0;
		}
		
		// do lines
		for (i=0, li = lines ; i<numlines ; i++,li++)
		{
			li->flags = SV_ReadShort();
			li->special = SV_ReadShort();
			li->tag = SV_ReadShort();
			for (j=0 ; j<2 ; j++)
			{
				if (li->sidenum[j] == -1)
					continue;
				si = &sides[li->sidenum[j]];
				si->textureoffset = SV_ReadShort() << FRACBITS;
				si->rowoffset = SV_ReadShort() << FRACBITS;
				si->toptexture = SV_ReadShort();
				si->bottomtexture = SV_ReadShort();
				si->midtexture = SV_ReadShort();
			}
		}    
	}
}


//
// Thinkers
//
typedef enum
{
    tc_end,
    tc_mobj,
	tc_xgmover
} 
thinkerclass_t;


//
// P_ArchiveThinkers
//
void P_ArchiveThinkers (void)
{
    thinker_t*	th;
    	
    // save off the current thinkers
    for(th = thinkercap.next ; th != &thinkercap ; th=th->next)
    {
		if(th->function == P_MobjThinker)
		{
			SV_WriteByte(tc_mobj);
			SV_WriteMobj((mobj_t*) th);
		}
		else if(th->function == XS_PlaneMover)
		{
			SV_WriteByte(tc_xgmover);
			SV_WriteXGPlaneMover(th);
		}
		// Con_Error ("P_ArchiveThinkers: Unknown thinker function");
    }
	
    // add a terminating marker
    SV_WriteByte(tc_end);	
}



//
// P_UnArchiveThinkers
//
void P_UnArchiveThinkers (void)
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
		tclass = SV_ReadByte();
		switch (tclass)
		{
		case tc_end:
			goto end_of_thinkers; // end of list
			
		case tc_mobj:
			mobj = Z_Malloc (sizeof(*mobj), PU_LEVEL, NULL);
			memset(mobj, 0, sizeof(*mobj));
			SV_ReadMobj(mobj);
			if(mobj->dplayer && !mobj->dplayer->ingame)
			{
				// This mobj doesn't belong to anyone any more.
				mobj->dplayer->mo = NULL;
				Z_Free(mobj);
				break;
			}
			P_SetThingPosition (mobj);
			mobj->info = &mobjinfo[mobj->type];
			mobj->floorz = mobj->subsector->sector->floorheight;
			mobj->ceilingz = mobj->subsector->sector->ceilingheight;
			mobj->thinker.function = P_MobjThinker;
			P_AddThinker (&mobj->thinker);
			break;

		case tc_xgmover:
			SV_ReadXGPlaneMover();
			break;
			
		default:
			Con_Error("P_UnArchiveThinkers: Unknown tclass %i in savegame.", 
				tclass);
		}
    }

end_of_thinkers:

	// Update references to things.
	for(currentthinker = thinkercap.next; currentthinker != &thinkercap;
		currentthinker = currentthinker->next)
	{
		if(currentthinker->function != P_MobjThinker) continue;
		// Update target.
		mobj = (mobj_t*) currentthinker;
		mobj->target = SV_GetArchiveThing((int)mobj->target);
	}

	// The activator mobjs must be set.
	XL_UnArchiveLines();
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
} 
specials_e;	



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
void P_ArchiveSpecials (void)
{
    thinker_t*		th;
    ceiling_t		ceiling;
    vldoor_t		door;
    floormove_t		floor;
    plat_t			plat;
    lightflash_t	flash;
    strobe_t		strobe;
    glow_t			glow;
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
				SV_WriteByte(tc_ceiling);
				memcpy(&ceiling, th, sizeof(ceiling));
				ceiling.sector = (sector_t *)(ceiling.sector - sectors);
				SV_Write(&ceiling, sizeof(ceiling));
			}
			continue;
		}
		
		if (th->function == T_MoveCeiling)
		{
			SV_WriteByte(tc_ceiling);
			memcpy(&ceiling, th, sizeof(ceiling));
			ceiling.sector = (sector_t *)(ceiling.sector - sectors);
			SV_Write(&ceiling, sizeof(ceiling));
			continue;
		}
		
		if (th->function == T_VerticalDoor)
		{
			SV_WriteByte(tc_door);
			memcpy(&door, th, sizeof(door));
			door.sector = (sector_t *)(door.sector - sectors);
			SV_Write(&door, sizeof(door));
			continue;
		}
		
		if (th->function == T_MoveFloor)
		{
			SV_WriteByte(tc_floor);
			memcpy(&floor, th, sizeof(floor));
			floor.sector = (sector_t *)(floor.sector - sectors);
			SV_Write(&floor, sizeof(floor));
			continue;
		}
		
		if (th->function == T_PlatRaise)
		{
			SV_WriteByte(tc_plat);
			memcpy(&plat, th, sizeof(plat));
			plat.sector = (sector_t *)(plat.sector - sectors);
			SV_Write(&plat, sizeof(plat));
			continue;
		}
		
		if (th->function == T_LightFlash)
		{
			SV_WriteByte(tc_flash);
			memcpy(&flash, th, sizeof(flash));
			flash.sector = (sector_t *)(flash.sector - sectors);
			SV_Write(&flash, sizeof(flash));
			continue;
		}
		
		if (th->function == T_StrobeFlash)
		{
			SV_WriteByte(tc_strobe);
			memcpy(&strobe, th, sizeof(strobe));
			strobe.sector = (sector_t *)(strobe.sector - sectors);
			SV_Write(&strobe, sizeof(strobe));
			continue;
		}
		
		if (th->function == T_Glow)
		{
			SV_WriteByte(tc_glow);
			memcpy(&glow, th, sizeof(glow));
			glow.sector = (sector_t *)(glow.sector - sectors);
			SV_Write(&glow, sizeof(glow));
			continue;
		}
    }
	
    // add a terminating marker
    SV_WriteByte(tc_endspecials);	
}


//
// P_UnArchiveSpecials
//
void P_UnArchiveSpecials (void)
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
		tclass = SV_ReadByte();
		switch (tclass)
		{
		case tc_endspecials:
			return;	// end of list
			
		case tc_ceiling:
			ceiling = Z_Malloc (sizeof(*ceiling), PU_LEVEL, NULL);
			SV_Read(ceiling, sizeof(*ceiling));
#ifdef _DEBUG
			if((int)ceiling->sector >= numsectors
				|| (int)ceiling->sector < 0)
				Con_Error("tc_ceiling: bad sector number\n");
#endif
			ceiling->sector = &sectors[(int)ceiling->sector];
			ceiling->sector->specialdata = ceiling;
			
			if (ceiling->thinker.function)
				ceiling->thinker.function = T_MoveCeiling;
			
			P_AddThinker (&ceiling->thinker);
			P_AddActiveCeiling(ceiling);
			break;
			
		case tc_door:
			door = Z_Malloc (sizeof(*door), PU_LEVEL, NULL);
			SV_Read(door, sizeof(*door));
			door->sector = &sectors[(int)door->sector];
			door->sector->specialdata = door;
			door->thinker.function = T_VerticalDoor;
			P_AddThinker (&door->thinker);
			break;
			
		case tc_floor:
			floor = Z_Malloc (sizeof(*floor), PU_LEVEL, NULL);
			SV_Read(floor, sizeof(*floor));
			floor->sector = &sectors[(int)floor->sector];
			floor->sector->specialdata = floor;
			floor->thinker.function = T_MoveFloor;
			P_AddThinker (&floor->thinker);
			break;
			
		case tc_plat:
			plat = Z_Malloc (sizeof(*plat), PU_LEVEL, NULL);
			SV_Read(plat, sizeof(*plat));
			plat->sector = &sectors[(int)plat->sector];
			plat->sector->specialdata = plat;
			
			if (plat->thinker.function)
				plat->thinker.function = T_PlatRaise;
			
			P_AddThinker (&plat->thinker);
			P_AddActivePlat(plat);
			break;
			
		case tc_flash:
			flash = Z_Malloc (sizeof(*flash), PU_LEVEL, NULL);
			SV_Read(flash, sizeof(*flash));
			flash->sector = &sectors[(int)flash->sector];
			flash->thinker.function = T_LightFlash;
			P_AddThinker (&flash->thinker);
			break;
			
		case tc_strobe:
			strobe = Z_Malloc (sizeof(*strobe), PU_LEVEL, NULL);
			SV_Read(strobe, sizeof(*strobe));
			strobe->sector = &sectors[(int)strobe->sector];
			strobe->thinker.function = T_StrobeFlash;
			P_AddThinker (&strobe->thinker);
			break;
			
		case tc_glow:
			glow = Z_Malloc (sizeof(*glow), PU_LEVEL, NULL);
			SV_Read(glow, sizeof(*glow));
			glow->sector = &sectors[(int)glow->sector];
			glow->thinker.function = T_Glow;
			P_AddThinker (&glow->thinker);
			break;
			
		default:
			Con_Error("P_UnArchiveSpecials: Unknown tclass %i "
				"in savegame.",tclass);
		}
    }
}

#ifdef __JDOOM__
void P_ArchiveBrain(void)
{
	int i;

	SV_WriteByte(numbraintargets);
	SV_WriteByte(braintargeton);
	// Write the mobj references using the mobj archive.
	for(i=0; i<numbraintargets; i++)
		SV_WriteShort(SV_ThingArchiveNum(braintargets[i]));
}

void P_UnArchiveBrain(void)
{
	int i;

	if(hdr.version < 3) return;	// No brain data before version 3.
	
	numbraintargets = SV_ReadByte();
	braintargeton = SV_ReadByte();
	for(i=0; i<numbraintargets; i++)
		braintargets[i] = SV_GetArchiveThing(SV_ReadShort());
}
#endif

/*
 * Initialize the savegame directories. If the directories do not 
 * exist, they are created.
 */
void SV_Init(void)
{
	if(ArgCheckWith("-savedir", 1))
    {
		strcpy(save_path, ArgNext());
		// Add a trailing backslash is necessary.
		if(save_path[strlen(save_path)-1] != '\\')
			strcat(save_path, "\\");
    }
	else
	{
		// Use the default path.
		sprintf(save_path, "savegame\\%s\\", G_Get(DD_GAME_MODE));
	}

	// Build the client save path.
	strcpy(client_save_path, save_path);
	strcat(client_save_path, "client\\");

	// Check that the save paths exist.
	M_CheckPath(save_path);
	M_CheckPath(client_save_path);
	M_TranslatePath(save_path, save_path);
	M_TranslatePath(client_save_path, client_save_path);
}

void SV_SaveGameFile(int slot, char *str)
{
	sprintf(str, "%s"SAVEGAMENAME"%i."SAVEGAMEEXTENSION, save_path, slot);	
}

void SV_ClientSaveGameFile(unsigned int game_id, char *str)
{
	sprintf(str, "%s"CLIENTSAVEGAMENAME"%08X.dsg", client_save_path, game_id);
}

int SV_SaveGame(char *filename, char *description)
{
    int	i; 
	
	// Open the file.
	savefile = lzOpen(filename, "wp");
	if(!savefile)
	{
		Con_Message("P_SaveGame: couldn't open \"%s\" for writing.\n", 
			filename);
		return false;	// No success.
	}

	SV_InitThingArchive();
	SV_InitTextureArchives();

	// Write the header.
	hdr.magic = MY_SAVE_MAGIC;
	hdr.version = MY_SAVE_VERSION;
#ifdef __JDOOM__
	hdr.gamemode = gamemode;
#elif defined(__JHERETIC__)
	hdr.gamemode = 0;
#endif

	strncpy(hdr.description, description, SAVESTRINGSIZE);
	hdr.description[SAVESTRINGSIZE-1] = 0;
	hdr.skill = gameskill;
#ifdef __JDOOM__
	if(fastparm) hdr.skill |= 0x80; // Set high byte.
#endif
	hdr.episode = gameepisode;
	hdr.map = gamemap;
	hdr.deathmatch = deathmatch;
	hdr.nomonsters = nomonsters;
	hdr.respawn = respawnparm;
	hdr.leveltime = leveltime;
	hdr.gameid = SV_GameID();
	for(i=0; i<MAXPLAYERS; i++) 
		hdr.players[i] = players[i].plr->ingame;
	lzWrite(&hdr, sizeof(hdr), savefile);

	// In netgames the server tells the clients to save their games.
	NetSv_SaveGame(hdr.gameid);

    P_ArchivePlayers (); 
    P_ArchiveWorld (); 
    P_ArchiveThinkers (); 
    P_ArchiveSpecials (); 

#ifdef __JDOOM__
	// Doom saves the brain data, too. (It's a part of the world.)
	P_ArchiveBrain();
#endif
	
	// To be absolutely sure...
	SV_WriteByte(CONSISTENCY);
	
	lzClose(savefile);
	return true;
}

int SV_GetSaveDescription(char *filename, char *str)
{
	savefile = lzOpen(filename, "rp");
	if(!savefile) 
	{
		// It might still be a v19 savegame.
		savefile = lzOpen(filename, "r");
		if(!savefile) return false;	// It just doesn't exist.
		lzRead(str, SAVESTRINGSIZE, savefile);
		str[SAVESTRINGSIZE-1] = 0;
		lzClose(savefile);
		return true;
	}
	// Read the header.
	lzRead(&hdr, sizeof(hdr), savefile);
	lzClose(savefile);
	// Check the magic.
	if(hdr.magic != MY_SAVE_MAGIC)
		// This isn't a proper savegame file.
		return false;
	strcpy(str, hdr.description);	
	return true;
}

//===========================================================================
// SV_LoadGame
//===========================================================================
int SV_LoadGame(char *filename)
{
	int i;
	boolean infile[MAXPLAYERS], loaded[MAXPLAYERS];
	char buf[80];

	// Make sure an opening briefing is not shown.
	// (G_InitNew --> G_DoLoadLevel)
	brief_disabled = true;

	savefile = lzOpen(filename, "rp");
	if(!savefile) 
	{
#ifdef __JDOOM__
		// It might still be a v19 savegame.
		SV_v19_LoadGame(filename);
#endif
#ifdef __JHERETIC__
		SV_v13_LoadGame(filename);
#endif
		return true;
	}

	SV_InitThingArchive();

	// Read the header.
	lzRead(&hdr, sizeof(hdr), savefile);
	if(hdr.magic != MY_SAVE_MAGIC) 
	{
		Con_Message("SV_LoadGame: Bad magic.\n");
		return false;
	}
#ifdef __JDOOM__
	if(hdr.gamemode != gamemode && !ArgExists("-nosavecheck"))
	{
		Con_Message("SV_LoadGame: savegame not from gamemode %i.\n", gamemode);
		return false;
	}
#endif

	gameskill = hdr.skill & 0x7f;
#ifdef __JDOOM__
	fastparm = (hdr.skill & 0x80) != 0;
#endif
	gameepisode = hdr.episode;
	gamemap = hdr.map;
	deathmatch = hdr.deathmatch;
	nomonsters = hdr.nomonsters;
	respawnparm = hdr.respawn;
	// We don't have the right to say which players are in the game. The 
	// players that already are will continue to be. If the data for a given 
	// player is not in the savegame file, he will be notified. The data for
	// players who were saved but are not currently in the game will be
	// discarded.
	for(i = 0; i < MAXPLAYERS; i++) infile[i] = hdr.players[i];

	// Load the level.
	G_InitNew(gameskill, gameepisode, gamemap);

	// Set the time.
	leveltime = hdr.leveltime;

	// Dearchive all the data.
	memset(loaded, 0, sizeof(loaded));
	P_UnArchivePlayers(infile, loaded);
	P_UnArchiveWorld();
	P_UnArchiveThinkers();
	P_UnArchiveSpecials();

#ifdef __JDOOM__
	// Doom saves the brain data, too. (It's a part of the world.)
	P_UnArchiveBrain();
#endif

	// Check consistency.
	if(SV_ReadByte() != CONSISTENCY)
		Con_Error("SV_LoadGame: Bad savegame (consistency test failed!)\n");

	// We're done.
	lzClose(savefile);

	// Notify the players that weren't in the savegame.
	for(i=0; i<MAXPLAYERS; i++)
		if(!loaded[i] && players[i].plr->ingame)
		{
			if(!i)
#ifdef __JDOOM__
				P_SetMessage(players, GET_TXT(TXT_LOADMISSING));
#endif
#ifdef __JHERETIC__
				P_SetMessage(players, GET_TXT(TXT_LOADMISSING), true);
#endif
			else
				NetSv_SendMessage(i, GET_TXT(TXT_LOADMISSING));
			// Kick this player out, he doesn't belong here.
			sprintf(buf, "kick %i", i);
			Con_Execute(buf, false);
		}
	
	// In netgames, the server tells the clients about this.
	NetSv_LoadGame(hdr.gameid);

	// Spawn particle generators.
	R_SetupLevel("", DDSLF_AFTER_LOADING);
	return true;
}

// Saves a snapshot of the world, a still image. 
// No data of movement is included (server sends it).
void SV_SaveClient(unsigned int gameid)
{
	char name[200];
	player_t *pl = players + consoleplayer;
	mobj_t *mo = players[consoleplayer].plr->mo;

	if(!IS_CLIENT || !mo) return;

	SV_InitTextureArchives();

	SV_ClientSaveGameFile(gameid, name);
	// Open the file.
	savefile = lzOpen(name, "wp");
	if(!savefile)
	{
		Con_Message("SV_SaveClient: Couldn't open \"%s\" for writing.\n", name);
		return;
	}
	// Prepare the header.
	memset(&hdr, 0, sizeof(hdr));
	hdr.magic = MY_CLIENT_SAVE_MAGIC;
	hdr.version = MY_SAVE_VERSION;
	hdr.skill = gameskill;
	hdr.episode = gameepisode;
	hdr.map = gamemap;
	hdr.deathmatch = deathmatch;
	hdr.nomonsters = nomonsters;
	hdr.respawn = respawnparm;
	hdr.leveltime = leveltime;
	hdr.gameid = gameid;
	SV_Write(&hdr, sizeof(hdr));

	// Some important information.
	// Our position and look angles.
	SV_WriteLong(mo->x);
	SV_WriteLong(mo->y);
	SV_WriteLong(mo->z);
	SV_WriteLong(mo->floorz);
	SV_WriteLong(mo->ceilingz);
	SV_WriteLong(pl->plr->clAngle);
	SV_WriteFloat(pl->plr->clLookDir);
	SV_WritePlayer(consoleplayer);
	
	P_ArchiveWorld();
	P_ArchiveSpecials();

	lzClose(savefile);
}

// Returns zero if the game isn't found.
void SV_LoadClient(unsigned int gameid)
{
	char name[200];
	player_t *cpl = players + consoleplayer;
	mobj_t *mo = cpl->plr->mo;

	if(!IS_CLIENT || !mo) return;

	SV_ClientSaveGameFile(gameid, name);
	// Try to open the file.
	savefile = lzOpen(name, "rp");
	if(!savefile) return;

	SV_Read(&hdr, sizeof(hdr));
	if(hdr.magic != MY_CLIENT_SAVE_MAGIC) 
	{
		lzClose(savefile);
		Con_Message("SV_LoadClient: Bad magic!\n");
		return;	
	}
	gameskill = hdr.skill;
	deathmatch = hdr.deathmatch;
	nomonsters = hdr.nomonsters;
	respawnparm = hdr.respawn;
	// Do we need to change the map?
	if(gamemap != hdr.map || gameepisode != hdr.episode)
	{
		gamemap = hdr.map;
		gameepisode = hdr.episode;
		G_InitNew(gameskill, gameepisode, gamemap);
	}
	leveltime = hdr.leveltime;

	P_UnsetThingPosition(mo);
	mo->x = SV_ReadLong();
	mo->y = SV_ReadLong();
	mo->z = SV_ReadLong();
	P_SetThingPosition(mo);
	mo->floorz = SV_ReadLong();
	mo->ceilingz = SV_ReadLong();
	mo->angle = cpl->plr->clAngle = SV_ReadLong();
	cpl->plr->clLookDir = SV_ReadFloat();
	SV_ReadPlayer(cpl);
		
	P_UnArchiveWorld();
	P_UnArchiveSpecials();

	lzClose(savefile);
	return;
}
