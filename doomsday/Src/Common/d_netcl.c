#ifdef __JDOOM__
#include "../JDoom/doomdef.h"
#include "../JDoom/doomstat.h"
#include "../JDoom/dstrings.h"
#include "../JDoom/p_local.h"
#include "../JDoom/s_sound.h"
#include "../JDoom/d_config.h"
#include "../JDoom/g_game.h"
#include "../JDoom/st_stuff.h"
#include "../JDoom/wi_stuff.h"
#include "../JDoom/am_map.h"
#endif

#ifdef __JHERETIC__
#include "../JHeretic/DoomDef.h"
#include "../JHeretic/P_local.h"
#include "../JHeretic/soundst.h"
#include "../JHeretic/settings.h"
#endif

#ifdef __JHEXEN__
#include "../JHexen/H2def.h"
#include "../JHexen/P_local.h"
#include "../JHexen/Settings.h"
#endif

#include "p_saveg.h"
#include "d_Net.h"
#include "f_infine.h"

// External Data ---------------------------------------------------------

extern char msgbuff[256];

// Private Data ----------------------------------------------------------

static byte *readbuffer;

// Code ------------------------------------------------------------------

// Mini-Msg routines.
void NetCl_SetReadBuffer(byte *data)
{
	readbuffer = data;
}

byte NetCl_ReadByte()
{
	return *readbuffer++;
}

short NetCl_ReadShort()
{
	readbuffer += 2;
	return *(short*) (readbuffer-2);
}

int NetCl_ReadLong()
{
	readbuffer += 4;
	return *(int*) (readbuffer-4);
}

void NetCl_Read(byte *buf, int len)
{
	memcpy(buf, readbuffer, len);
	readbuffer += len;
}
//-------

#ifdef __JDOOM__
int NetCl_IsCompatible(int other, int us)
{
	char comp[5][5] =	// [other][us]
	{
		{ 1, 1, 0, 1, 0 },
		{ 0, 1, 0, 1, 0 },
		{ 0, 0, 1, 0, 0 },
		{ 0, 0, 0, 1, 0 },
		{ 0, 0, 0, 0, 0 }
	};
/*  shareware,	// DOOM 1 shareware, E1, M9
	registered,	// DOOM 1 registered, E3, M27
	commercial,	// DOOM 2 retail, E1 M34
	retail,		// DOOM 1 retail, E4, M36
*/
	return comp[other][us];
}
#endif

void NetCl_UpdateGameState(byte *data)
{
	fixed_t grav;
	packet_gamestate_t *gs = (packet_gamestate_t*) data;

	// Demo game state changes are only effective during demo playback.
	if(gs->flags & GSF_DEMO && !Get(DD_PLAYBACK))
		return;

#ifdef __JDOOM__
	if(!NetCl_IsCompatible(gs->gamemode, gamemode))
	{
		// Wrong game mode! This is highly irregular!
		Con_Message("NetCl_UpdateGameState: Game mode mismatch!\n");
		// Stop the demo if one is being played.
		Con_Execute("stopdemo", false);
		return;
	}
#endif

	deathmatch = gs->deathmatch;
	nomonsters = !gs->monsters;
	respawnparm = gs->respawn;
	//cfg.jumpEnabled = gs->jumping;
	grav = gs->gravity << 8;

	// Some statistics.
#if __JHEXEN__
	Con_Message("Game state: Map=%i Skill=%i %s\n",
		gs->map, gs->skill, deathmatch==1? "Deathmatch" 
		: deathmatch==2? "Deathmatch2" : "Co-op");
	Con_Message("  Respawn=%s Monsters=%s Gravity=%.1f\n",
		respawnparm? "yes" : "no", !nomonsters? "yes" : "no", 
		FIX2FLT(grav));
#else
	Con_Message("Game state: Map=%i Episode=%i Skill=%i %s\n",
		gs->map, gs->episode, gs->skill, deathmatch==1? "Deathmatch" 
		: deathmatch==2? "Deathmatch2" : "Co-op");
	Con_Message("  Respawn=%s Monsters=%s Jumping=%s Gravity=%.1f\n",
		respawnparm? "yes" : "no", !nomonsters? "yes" : "no", 
		gs->jumping? "yes" : "no", FIX2FLT(grav));
#endif
	
#ifdef __JHERETIC__
	prevmap = gamemap;
#endif

	// Start reading after the GS packet.
	NetCl_SetReadBuffer(data + sizeof(*gs));

	// Do we need to change the map?
	if(gs->flags & GSF_CHANGE_MAP)
	{
		G_InitNew(gs->skill, gs->episode, gs->map);
	}
	else
	{
		gameskill = gs->skill;
		gameepisode = gs->episode;
		gamemap = gs->map;
	}

	// Set gravity.
	Set(DD_GRAVITY, grav);

	// Camera init included?
	if(gs->flags & GSF_CAMERA_INIT)
	{
		player_t *pl = players + consoleplayer;
		mobj_t *mo;
		//G_DoReborn(consoleplayer);
		//if(!pl->plr->mo) G_DummySpawnPlayer(consoleplayer);
		mo = pl->plr->mo;
		//NetCl_SetReadBuffer(data + sizeof(*gs));
		P_UnsetThingPosition(mo);
		mo->x = NetCl_ReadShort() << 16;
		mo->y = NetCl_ReadShort() << 16;
		mo->z = NetCl_ReadShort() << 16;
		P_SetThingPosition(mo);
		pl->plr->clAngle = mo->angle = NetCl_ReadShort() << 16;
		pl->plr->viewz = mo->z + pl->plr->viewheight;
		// Update floorz and ceilingz.
#ifdef __JDOOM__
		P_CheckPosition2(mo, mo->x, mo->y, mo->z);
#else
		P_CheckPosition(mo, mo->x, mo->y);
#endif
		mo->floorz = tmfloorz;
		mo->ceilingz = tmceilingz;
	}
}

#if 0
void NetCl_SpawnMissile(int type, fixed_t x, fixed_t y, fixed_t z, 
						fixed_t momx, fixed_t momy, fixed_t momz)
{
	mobj_t *mis = P_SpawnMobj(x, y, z, type);

	mis->angle = R_PointToAngle2(x, y, x+momx, y+momy);
	mis->momx = momx;
	mis->momy = momy;
	mis->momz = momz;

	// Also play seesound.
	if(mis->info->seesound)
	{
		S_StartSound(mis, mis->info->seesound);
	}

#ifdef __JHERETIC__
	if(type == MT_MACEFX1)
	{
		mis->special1 = 16; // tics till dropoff
	}
#endif
}
#endif

//===========================================================================
// NetCl_UpdatePlayerState2
//===========================================================================
void NetCl_UpdatePlayerState2(byte *data, int plrNum)
{
	player_t *pl = &players[plrNum];
	unsigned int flags;
	int i, k;

	if(!Get(DD_GAME_READY)) return;

	NetCl_SetReadBuffer(data);
	flags = NetCl_ReadLong();

	if(flags & PSF2_OWNED_WEAPONS)
	{
		k = NetCl_ReadShort();
		for(i = 0; i < NUMWEAPONS; i++) 
			pl->weaponowned[i] = (k & (1<<i)) != 0;
	}
}

//===========================================================================
// NetCl_UpdatePlayerState
//===========================================================================
void NetCl_UpdatePlayerState(byte *data, int plrNum)
{
	player_t *pl = &players[plrNum];
	byte b;
	unsigned short flags;
	int i;
	int oldstate = pl->playerstate;
	unsigned short s;

	if(!Get(DD_GAME_READY)) return;

	NetCl_SetReadBuffer(data);
	flags = NetCl_ReadShort();

	//Con_Printf("NetCl_UpdPlrState: fl=%x\n", flags);

	if(flags & PSF_STATE) // and armor type (the same bit)
	{
		b = NetCl_ReadByte();
		pl->playerstate = b & 0xf;
#if !__JHEXEN__
		pl->armortype = b >> 4;
#endif
		// Set or clear the DEAD flag for this player.
		if(pl->playerstate == PST_LIVE)
			pl->plr->flags &= ~DDPF_DEAD;
		else
			pl->plr->flags |= DDPF_DEAD;
	}
	if(flags & PSF_HEALTH) 
	{
		pl->health = NetCl_ReadByte();
		pl->plr->mo->health = pl->health;
	}
	if(flags & PSF_ARMOR_POINTS) 
	{
#if __JHEXEN__
		for(i=0; i<NUMARMOR; i++)
			pl->armorpoints[i] = NetCl_ReadByte();
#else
		pl->armorpoints = NetCl_ReadByte();
#endif
	}

#if __JHERETIC__ || __JHEXEN__
	if(flags & PSF_INVENTORY)
	{
		pl->inventorySlotNum = NetCl_ReadByte();
		pl->artifactCount = 0;
		for(i = 0; i < NUMINVENTORYSLOTS; i++)
		{
			if(i >= pl->inventorySlotNum)
			{
				pl->inventory[i].type = arti_none;
				pl->inventory[i].count = 0;
				continue;
			}
			s = NetCl_ReadShort();
			pl->inventory[i].type = s & 0xff;
			pl->inventory[i].count = s >> 8;
			if(pl->inventory[i].type != arti_none)
				pl->artifactCount += pl->inventory[i].count;
		}
#if __JHERETIC__
		if(plrNum == consoleplayer)	P_CheckReadyArtifact();
#endif
	}
#endif

	if(flags & PSF_POWERS)
	{
		b = NetCl_ReadByte();
		// Only the non-zero powers are included in the message.
#if __JHEXEN__
		for(i = 0; i < NUMPOWERS - 1; i++)
			if(b & (1 << i))
				pl->powers[i + 1] = NetCl_ReadByte() * 35;
			else
				pl->powers[i + 1] = 0;
#else
		for(i = 0; i < NUMPOWERS; i++)
		{
#if __JDOOM__
			if(i == pw_ironfeet || i == pw_strength) continue;
#endif
			if(b & (1 << i))
				pl->powers[i] = NetCl_ReadByte() * 35;		
			else
				pl->powers[i] = 0;
		}
#endif
	}
	if(flags & PSF_KEYS)
	{
		b = NetCl_ReadByte();
#ifdef __JDOOM__
		for(i=0; i<NUMCARDS; i++) pl->cards[i] = (b & (1<<i)) != 0;
#endif

#ifdef __JHERETIC__
		for(i=0; i<NUMKEYS; i++) pl->keys[i] = (b & (1<<i)) != 0;
#endif
	}
	if(flags & PSF_FRAGS)
	{
		memset(pl->frags, 0, sizeof(pl->frags));
		// First comes the number of frag counts included.
		for(i = NetCl_ReadByte(); i > 0; i--)
		{
			s = NetCl_ReadShort();
			pl->frags[s >> 12] = s & 0xfff;
		}		
		
		/*// A test...
		Con_Printf("Frags update: ");
		for(i=0; i<4; i++)
			Con_Printf("%i ", pl->frags[i]);
		Con_Printf("\n");*/	
	}
	if(flags & PSF_OWNED_WEAPONS)
	{
		b = NetCl_ReadByte();
		for(i = 0; i < NUMWEAPONS; i++) 
			pl->weaponowned[i] = (b & (1<<i)) != 0;
	}
	if(flags & PSF_AMMO)
	{
#if __JHEXEN__
		for(i=0; i<NUMMANA; i++) pl->mana[i] = NetCl_ReadByte();
#else
		for(i=0; i<NUMAMMO; i++) pl->ammo[i] = NetCl_ReadShort();
#endif
	}
	if(flags & PSF_MAX_AMMO)
	{
#if !__JHEXEN__ // Hexen has no use for max ammo.
		for(i=0; i<NUMAMMO; i++) pl->maxammo[i] = NetCl_ReadShort();
#endif
	}
	if(flags & PSF_COUNTERS)
	{
		pl->killcount = NetCl_ReadShort();
		pl->itemcount = NetCl_ReadByte();
		pl->secretcount = NetCl_ReadByte();

		/*Con_Printf( "plr%i: kills=%i items=%i secret=%i\n", pl-players,
			pl->killcount, pl->itemcount, pl->secretcount);*/
	}
	if(flags & PSF_PENDING_WEAPON || flags & PSF_READY_WEAPON)
	{
		b = NetCl_ReadByte();
		if(flags & PSF_PENDING_WEAPON) 
		{
			pl->pendingweapon = b & 0xf;
		}
		if(flags & PSF_READY_WEAPON) 
		{
#ifdef __JHERETIC__
			if(pl->readyweapon == wp_beak)
				P_PostChickenWeapon(pl, b >> 4);
			else
#endif
			pl->readyweapon = b >> 4;

#if _DEBUG
			Con_Message("NetCl_UpdPlSt: rdyw=%i\n", pl->readyweapon);
#endif
		}
	}
	if(flags & PSF_VIEW_HEIGHT)
	{
		pl->plr->viewheight = NetCl_ReadByte() << 16;
	}

#if __JHERETIC__
	if(flags & PSF_CHICKEN_TIME)
	{
		pl->chickenTics = NetCl_ReadByte() * 35;
	}
#endif

#if __JHEXEN__
	if(flags & PSF_MORPH_TIME)
	{
		pl->morphTics = NetCl_ReadByte() * 35;
	}
	if(flags & PSF_LOCAL_QUAKE)
	{
		localQuakeHappening[plrNum] = NetCl_ReadByte();
	}
#endif

	if(oldstate != pl->playerstate && oldstate == PST_DEAD)
	{
		P_SetupPsprites(pl);
	}
}

void NetCl_UpdatePSpriteState(byte *data)
{
	unsigned short s;

	NetCl_SetReadBuffer(data);
	s = NetCl_ReadShort();
	P_SetPsprite(&players[consoleplayer], ps_weapon, s);
}

#if 0
void NetCl_PlaySectorSound(byte *data)
{
	int sound, sector;

	NetCl_SetReadBuffer(data);
	sound = NetCl_ReadByte();
	sector = NetCl_ReadShort();
	S_StartSound( (mobj_t*) &sectors[sector].soundorg, sound);
//	Con_Printf("NetCl_PlaySectorSound: id=%i\n", sound);
}
#endif

void NetCl_Intermission(byte *data)
{
	int	flags;
	extern int interstate;
	extern int intertime;
#if __JHEXEN__
	extern int LeaveMap, LeavePosition;
#endif

	NetCl_SetReadBuffer(data);
	flags = NetCl_ReadByte();

	//Con_Printf( "NetCl_Intermission: flags=%x\n", flags);

#ifdef __JDOOM__
	if(flags & IMF_BEGIN)
	{
		wminfo.maxkills = NetCl_ReadShort();
		wminfo.maxitems = NetCl_ReadShort();
		wminfo.maxsecret = NetCl_ReadShort();
		wminfo.next = NetCl_ReadByte();
		wminfo.last = NetCl_ReadByte();
		wminfo.didsecret = NetCl_ReadByte();

		G_PrepareWIData();

		gamestate = GS_INTERMISSION;
		viewactive = false; 
		if(automapactive) AM_Stop(); 

		WI_Start(&wminfo);
	}
	if(flags & IMF_END)
	{
		WI_End();
	}
	if(flags & IMF_STATE)
	{
		WI_SetState(NetCl_ReadByte());
	}
#endif

#ifdef __JHERETIC__
	if(flags & IMF_STATE) interstate = (int) NetCl_ReadByte();
	if(flags & IMF_TIME) intertime = NetCl_ReadShort();
	if(flags & IMF_BEGIN) 
	{
		gamestate = GS_INTERMISSION;
		IN_Start();
	}
	if(flags & IMF_END) 
	{
		IN_Stop();
	}
#endif

#if __JHEXEN__
	if(flags & IMF_BEGIN)
	{
		LeaveMap = NetCl_ReadByte();
		LeavePosition = NetCl_ReadByte();
		gamestate = GS_INTERMISSION;
		IN_Start();
	}
	if(flags & IMF_END)
	{
		IN_Stop();
	}
	if(flags & IMF_STATE) interstate = (int) NetCl_ReadByte();
#endif
}

//===========================================================================
// NetCl_Finale
//	This is where clients start their InFine interludes.
//===========================================================================
void NetCl_Finale(byte *data)
{
	int flags;
	int len;
	byte *script = NULL;

	NetCl_SetReadBuffer(data);
	flags = NetCl_ReadByte();
	if(flags & FINF_SCRIPT)
	{
		// Read the script into level-scope memory. It will be freed 
		// when the next level is loaded.
		len = strlen(readbuffer);
		script = Z_Malloc(len + 1, PU_LEVEL, 0);
		strcpy(script, readbuffer);
	}
	if(flags & FINF_BEGIN && script)
	{
		// Start the script.
		FI_StartScript(script, (flags & FINF_AFTER) != 0);
	}
	if(flags & FINF_END)
	{
		// Stop InFine.
		FI_End();
	}
	if(flags & FINF_SKIP)
	{
		FI_SkipRequest();
	}
}

//===========================================================================
// NetCl_UpdatePlayerInfo
//	Clients have other players' info, but it's only "FYI"; they don't
//	really need it.
//===========================================================================
void NetCl_UpdatePlayerInfo(byte *data)
{
	int num;

	NetCl_SetReadBuffer(data);
	num = NetCl_ReadByte();
	cfg.PlayerColor[num] = NetCl_ReadByte();
#if __JHEXEN__
	cfg.PlayerClass[num] = NetCl_ReadByte();
	players[num].class = cfg.PlayerClass[num];
	if(num == consoleplayer) SB_SetClassData();
#endif
#if __JDOOM__
	ST_updateGraphics();
#endif

#if !__JHEXEN__
	Con_Printf( "NetCl_UpdatePlayerInfo: pl=%i color=%i\n", num, 
		cfg.PlayerColor[num]);
#else
	Con_Printf( "NetCl_UpdatePlayerInfo: pl=%i color=%i class=%i\n", num, 
		cfg.PlayerColor[num], cfg.PlayerClass[num]);
#endif
}

// Send consoleplayer's settings to the server.
void NetCl_SendPlayerInfo()
{
	byte buffer[10], *ptr = buffer;
	
	if(!IS_CLIENT) return;

	*ptr++ = cfg.netColor;
#if __JHEXEN__
	*ptr++ = cfg.netClass;
#endif
	Net_SendPacket(DDSP_RELIABLE, GPT_PLAYER_INFO, buffer, ptr-buffer);
}

void NetCl_SaveGame(void *data)
{
	if(Get(DD_PLAYBACK)) return;
	SV_SaveClient(*(unsigned int*) data);
#ifdef __JDOOM__
	P_SetMessage(&players[consoleplayer], GGSAVED);
#endif
}

void NetCl_LoadGame(void *data)
{
	if(!IS_CLIENT) return;
	if(Get(DD_PLAYBACK)) return;
	SV_LoadClient(*(unsigned int*) data);
	//	Net_SendPacket(DDSP_RELIABLE, GPT_LOAD, &con, 1);
#ifdef __JDOOM__
	P_SetMessage(&players[consoleplayer], GET_TXT(TXT_CLNETLOAD));
#endif
}

/*
 * Pause or unpause the game. 
 */
void NetCl_Paused(boolean setPause)
{
	paused = (setPause != 0);
	DD_SetInteger(DD_CLIENT_PAUSED, paused);
}
