#include <ctype.h>

#ifdef __JDOOM__
#include "../JDoom/doomdef.h"
#include "../JDoom/doomstat.h"
#include "../JDoom/p_local.h"
#include "../JDoom/d_config.h"
#include "../JDoom/s_sound.h"
#include "../JDoom/st_stuff.h"
#include "../JDoom/m_random.h"
#include "../JDoom/dstrings.h"
#endif

#ifdef __JHERETIC__
#include "../JHeretic/DoomDef.h"
#include "../JHeretic/Soundst.h"
#include "../JHeretic/P_local.h"
#include "../JHeretic/settings.h"
#endif

#ifdef __JHEXEN__
#include "../JHexen/H2def.h"
#include "../JHexen/P_local.h"
#include "../JHexen/Settings.h"
#endif

#include "d_Net.h"
#include "p_svtexarc.h"

// MACROS -----------------------------------------------------------------

#if __JHEXEN__
#define SOUND_COUNTDOWN		SFX_PICKUP_KEY
#elif __JDOOM__
#define SOUND_COUNTDOWN		sfx_getpow
#elif __JHERETIC__
#define SOUND_COUNTDOWN		sfx_keyup
#endif

#define SOUND_VICTORY		SOUND_COUNTDOWN

#define UPD_BUFFER_LEN	500

// How long is the largest possible sector update?
#define MAX_SECTORUPD	20
#define MAX_SIDEUPD		9

#define WRITE_SHORT(byteptr, val)	{(*(short*)(byteptr) = (val)); byteptr += 2;}
#define WRITE_LONG(byteptr, val)	{(*(int*)(byteptr) = (val)); byteptr += 4;}

// TYPES ------------------------------------------------------------------

typedef struct 
{
	boolean usetime, usefrags;
	int time;		// Minutes.
	int frags;		// Maximum frags for one player.
} 
maprule_t;

enum
{
	CYCLE_IDLE,
	CYCLE_TELL_RULES,
	CYCLE_COUNTDOWN
};

// EXTERNAL FUNCTIONS -----------------------------------------------------

void R_SetAllDoomsdayFlags();

#if __JHEXEN__
void SB_ChangePlayerClass(player_t *player, int newclass);
#endif

// PUBLIC DATA ------------------------------------------------------------

boolean		cycling_maps = false;
char		*map_cycle = "";
boolean		map_cycle_noexit = true;
int			netsv_allow_sendmsg = true;

// This is returned in *_Get(DD_GAME_CONFIG). It contains a combination
// of space-separated keywords.
char		gameConfigString[128];

// PRIVATE DATA -----------------------------------------------------------

static int	cycle_index;
static int	cycle_counter = -1, cycle_mode = CYCLE_IDLE;
static int	oldpals[MAXPLAYERS];
static int	oldclasses[MAXPLAYERS];

// CODE -------------------------------------------------------------------

//===========================================================================
// NetSv_UpdateGameConfig
//	Update the game config string with keywords that describe the game.
//	The string is sent out in netgames (also to the master).
//  Keywords: dm, coop, jump, nomonst, respawn, skillN
//===========================================================================
void NetSv_UpdateGameConfig(void)
{
	if(IS_CLIENT) return;

	memset(gameConfigString, 0, sizeof(gameConfigString));

	sprintf(gameConfigString, "skill%i", gameskill + 1);

	if(deathmatch > 1) 
		sprintf(gameConfigString, " dm%i", deathmatch);
	else if(deathmatch)
		strcat(gameConfigString, " dm");
	else
		strcat(gameConfigString, " coop");

	if(nomonsters) strcat(gameConfigString, " nomonst");
	if(respawnparm) strcat(gameConfigString, " respawn");
#if !__JHEXEN__
	if(cfg.jumpEnabled) strcat(gameConfigString, " jump");
#endif
}

//===========================================================================
// NetSv_ScanCycle
//	Reads through the MapCycle cvar and finds the map with the given index.
//	Rules that apply to the map are returned in 'rules'.
//===========================================================================
int NetSv_ScanCycle(int index, maprule_t *rules)
{
	char *ptr = map_cycle, *end;
	int i, pos = -1, episode, mission;
#if __JHEXEN__
	int m;
#endif
	char tmp[3], lump[10];
	boolean clear = false, has_random = false;
	maprule_t dummy;

	if(!rules) rules = &dummy;

	// By default no rules apply.
	rules->usetime = rules->usefrags = false;

	for(; *ptr; ptr++)
	{
		if(isspace(*ptr)) continue;
		if(*ptr == ',' || *ptr == '+' || *ptr == ';'
			|| *ptr == '/' || *ptr == '\\') 
		{
			// These symbols are allowed to combine "time" and "frags".
			// E.g. "Time:10/Frags:5" or "t:30, f:10"
			clear = false;
		}
		else if(!strnicmp("time", ptr, 1))
		{
			// Find the colon.
			while(*ptr && *ptr != ':') ptr++;
			if(!*ptr) return -1;
			if(clear) rules->usefrags = false;
			clear = true;
			rules->usetime = true;
			rules->time = strtol(ptr+1, &end, 0);
			ptr = end - 1;
		}
		else if(!strnicmp("frags", ptr, 1))
		{
			// Find the colon.
			while(*ptr && *ptr != ':') ptr++;
			if(!*ptr) return -1;
			if(clear) rules->usetime = false;
			clear = true;
			rules->usefrags = true;
			rules->frags = strtol(ptr+1, &end, 0);
			ptr = end - 1;
		}
		else if(*ptr == '*' || *ptr >= '0' && *ptr <= '9')
		{
			// A map identifier is here.
			pos++;
			// Read it.
			tmp[0] = *ptr++;
			tmp[1] = *ptr;
			tmp[2] = 0;
			if(strlen(tmp) < 2)
			{
				// Assume a zero is missing.
				tmp[1] = tmp[0];
				tmp[0] = '0';
			}
			if(index == pos)
			{
				if(tmp[0] == '*' || tmp[1] == '*') has_random = true;
				// This is the map we're looking for. Return it.
				// But first randomize the asterisks.
				for(i = 0; i < 100; i++) // Try many times to find a good map.
				{
					// The differences in map numbering make this harder
					// than it should be.
#if __JDOOM__
					if(gamemode == commercial)
					{
						sprintf(lump, "MAP%i%i",
							episode = tmp[0] == '*'? M_Random() % 4 : tmp[0]-'0',
							mission = tmp[1] == '*'? M_Random() % 10 : tmp[1]-'0');
					}
					else
					{
						sprintf(lump, "E%iM%i",
							episode = tmp[0] == '*'? 1 + M_Random() % 4 : tmp[0]-'0',
							mission = tmp[1] == '*'? 1 + M_Random() % 9 : tmp[1]-'0');
					}
#elif __JHERETIC__
					sprintf(lump, "E%iM%i",
						episode = tmp[0] == '*'? 1 + M_Random() % 6 : tmp[0]-'0',
						mission = tmp[1] == '*'? 1 + M_Random() % 9 : tmp[1]-'0');
#elif __JHEXEN__
					sprintf(lump, "%i%i",
						episode = tmp[0] == '*'? M_Random() % 4 : tmp[0]-'0',
						mission = tmp[1] == '*'? M_Random() % 10 : tmp[1]-'0');
					m = P_TranslateMap(atoi(lump));
					if(m < 0) continue;
					sprintf(lump, "MAP%02i", m);
#endif
					if(W_CheckNumForName(lump) >= 0)
					{
						tmp[0] = episode + '0';
						tmp[1] = mission + '0';
						break;
					}
					else if(!has_random) return -1;
				}
				// Convert to a number.
				return atoi(tmp);
			}	
		}
	}
	// Didn't find it.
	return -1;
}

//===========================================================================
// NetSv_CycleToMapNum
//===========================================================================
void NetSv_CycleToMapNum(int map)
{
	char tmp[3], cmd[80];
	
	sprintf(tmp, "%02i", map);
#if __JDOOM__
	if(gamemode == commercial)
		sprintf(cmd, "setmap 1 %i", map);
	else
		sprintf(cmd, "setmap %c %c", tmp[0], tmp[1]);
#elif __JHERETIC__
	sprintf(cmd, "setmap %c %c", tmp[0], tmp[1]);
#elif __JHEXEN__
	sprintf(cmd, "setmap %i", map);
#endif
	Con_Execute(cmd, false);

	// In a couple of seconds, send everyone the rules of this map.
	cycle_mode = CYCLE_TELL_RULES;
	cycle_counter = 3 * TICSPERSEC;
}

//===========================================================================
// NetSv_GetFrags
//	Calculates the frags of player 'pl'.
//===========================================================================
int NetSv_GetFrags(int pl)
{
	int i, frags = 0;
	for(i = 0; i < MAXPLAYERS; i++) 
	{
#if __JDOOM__
		frags += players[pl].frags[i] * (i == pl? -1 : 1);
#else
		frags += players[pl].frags[i];	
#endif
	}
	return frags;
}

//===========================================================================
// NetSv_CheckCycling
//===========================================================================
void NetSv_CheckCycling(void)
{
	int map, i, f;
	maprule_t rules;
	char msg[100], tmp[50];

	if(!cycling_maps) return;
	cycle_counter--;
	switch(cycle_mode)
	{
	case CYCLE_IDLE:
		// Check if the current map should end.
		if(cycle_counter <= 0)
		{
			// Test every ten seconds.
			cycle_counter = 10 * TICSPERSEC;

			map = NetSv_ScanCycle(cycle_index, &rules);
			if(map < 0)
			{
				map = NetSv_ScanCycle(cycle_index = 0, &rules);
				if(map < 0)
				{
					// Hmm?! Abort cycling.
					Con_Message("NetSv_CheckCycling: All of a sudden MapCycle is invalid!\n");
					Con_Execute("endcycle", false);
					return;
				}
			}
			if(rules.usetime 
				&& leveltime > (rules.time*60 - 29)*TICSPERSEC)
			{
				// Time runs out!
				cycle_mode = CYCLE_COUNTDOWN;
				cycle_counter = 31*TICSPERSEC;
			}
			if(rules.usefrags)
			{
				for(i = 0; i < MAXPLAYERS; i++)
				{
					if(!players[i].plr->ingame) continue;
					if((f = NetSv_GetFrags(i)) >= rules.frags)
					{
						sprintf(msg, "--- %s REACHES %i FRAGS ---", 
							Net_GetPlayerName(i), f);
						NetSv_SendMessage(DDSP_ALL_PLAYERS, msg);
						S_StartSound(SOUND_VICTORY, NULL);
						cycle_mode = CYCLE_COUNTDOWN;
						cycle_counter = 15*TICSPERSEC; // No msg for 15 secs.
						break;
					}
				}
			}
		}
		break;

	case CYCLE_TELL_RULES:
		if(cycle_counter <= 0)
		{
			// Get the rules of the current map.
			NetSv_ScanCycle(cycle_index, &rules);
			strcpy(msg, "MAP RULES: ");
			if(!rules.usetime && !rules.usefrags)
				strcat(msg, "NONE");
			else 
			{
				if(rules.usetime) 
				{
					sprintf(tmp, "%i MINUTES", rules.time);
					strcat(msg, tmp);
				}
				if(rules.usefrags)
				{
					sprintf(tmp, "%s%i FRAGS", rules.usetime? " OR " : "",
						rules.frags);
					strcat(msg, tmp);
				}
			}
			// Send it to all players.
			NetSv_SendMessage(DDSP_ALL_PLAYERS, msg);
			// Start checking.
			cycle_mode = CYCLE_IDLE;
		}
		break;

	case CYCLE_COUNTDOWN:
		if(cycle_counter == 30*TICSPERSEC
			|| cycle_counter == 15*TICSPERSEC
			|| cycle_counter == 10*TICSPERSEC
			|| cycle_counter == 5*TICSPERSEC)
		{
			sprintf(msg, "--- WARPING IN %i SECONDS ---", cycle_counter/TICSPERSEC);
			NetSv_SendMessage(DDSP_ALL_PLAYERS, msg);
			// Also, a warning sound.
			S_StartSound(SOUND_COUNTDOWN, NULL);
		}
		else if(cycle_counter <= 0)
		{
			// Next map, please!
			map = NetSv_ScanCycle(++cycle_index, NULL);
			if(map < 0)
			{
				// Must be past the end?
				map = NetSv_ScanCycle(cycle_index = 0, NULL);
				if(map < 0)
				{
					// Hmm?! Abort cycling.
					Con_Message("NetSv_CheckCycling: All of a sudden MapCycle is invalid!\n");
					Con_Execute("endcycle", false);
					return;
				}
			}
			// Warp to the next map. Don't bother with the intermission.
			NetSv_CycleToMapNum(map);
		}
		break;
	}
}

//===========================================================================
// CCmdMapCycle
//	Handles the console commands "startcycle" and "endcycle".
//===========================================================================
int CCmdMapCycle(int argc, char **argv)
{
	int map;

	if(!IS_SERVER)
	{
		Con_Printf("Only allowed for a server.\n");
		return false;
	}
	if(!stricmp(argv[0], "startcycle")) // (Re)start rotation?
	{
		// Find the first map in the sequence.
		map = NetSv_ScanCycle(cycle_index = 0, 0);
		if(map < 0)
		{
			Con_Printf("MapCycle \"%s\" is invalid.\n", map_cycle);
			return false;
		}
		// Warp there.
		NetSv_CycleToMapNum(map);
		cycling_maps = true;
	}
	else // OK, then we need to end it.
	{
		if(cycling_maps)
		{
			cycling_maps = false;
			NetSv_SendMessage(DDSP_ALL_PLAYERS, "MAP ROTATION ENDS");
		}
	}
	return true;
}

void P_Telefrag(mobj_t *thing)
{
	P_TeleportMove(thing, thing->x, thing->y);
}

// Server calls this when new players enter the game.
void NetSv_NewPlayerEnters(int plrnumber)
{
	player_t *plr = &players[plrnumber];
	
	Con_Message("NetSv_NewPlayerEnters: spawning player %i.\n", plrnumber);

	plr->playerstate = PST_REBORN;	// Force an init.

	// Re-deal player starts.
	P_DealPlayerStarts();

	// Spawn the player into the world.
	// FIXME: Spawn a telefog in front of the player.
	P_SpawnPlayer(&playerstarts[plr->startspot], plrnumber);
	// Get rid of anybody at the starting spot.
	P_Telefrag(plr->plr->mo);
}

//===========================================================================
// NetSv_SendMessageEx
//===========================================================================
void NetSv_SendMessageEx(int plrNum, char *msg, boolean yellow)
{
	if(IS_CLIENT || !netsv_allow_sendmsg) return;
	if(plrNum >= 0 && plrNum < MAXPLAYERS)
		if(!players[plrNum].plr->ingame) return;
	if(plrNum == DDSP_ALL_PLAYERS)
	{
		// Also show locally. No sound is played!
		D_NetMessageNoSound(msg); 
	}
	Net_SendPacket(plrNum | DDSP_ORDERED, 
		yellow? GPT_YELLOW_MESSAGE : GPT_MESSAGE,
		msg, strlen(msg) + 1);
}

//===========================================================================
// NetSv_SendMessage
//===========================================================================
void NetSv_SendMessage(int plrNum, char *msg)
{
	NetSv_SendMessageEx(plrNum, msg, false);
}

//===========================================================================
// NetSv_SendYellowMessage
//===========================================================================
void NetSv_SendYellowMessage(int plrNum, char *msg)
{
	NetSv_SendMessageEx(plrNum, msg, true);
}

//===========================================================================
// NetSv_SendPlayerState2
//	More player state information. Had to be separate because of backwards
//	compatibility.
//===========================================================================
void NetSv_SendPlayerState2
	(int srcPlrNum, int destPlrNum, int flags, boolean reliable)
{
	int pType = (srcPlrNum == destPlrNum? GPT_CONSOLEPLAYER_STATE2 
		: GPT_PLAYER_STATE2);
	player_t *pl = &players[srcPlrNum];
	byte buffer[UPD_BUFFER_LEN], *ptr = buffer;
	int i, fl;

	// Check that this is a valid call.
	if(IS_CLIENT || !pl->plr->ingame
		|| (destPlrNum >= 0 && destPlrNum < MAXPLAYERS 
		&& !players[destPlrNum].plr->ingame)) return;

	// Include the player number if necessary.
	if(pType == GPT_PLAYER_STATE2) *ptr++ = srcPlrNum;
	WRITE_LONG(ptr, flags);

	if(flags & PSF2_OWNED_WEAPONS)
	{
		// This supports up to 16 weapons.
		for(fl = 0, i = 0; i < NUMWEAPONS; i++) 
			if(pl->weaponowned[i]) fl |= 1 << i;
		WRITE_SHORT(ptr, fl);
	}

	// Finally, send the packet.
	Net_SendPacket(destPlrNum | (reliable? DDSP_ORDERED : 0),
		pType, buffer, ptr-buffer);	
}

//===========================================================================
// NetSv_SendPlayerState
//===========================================================================
void NetSv_SendPlayerState
	(int srcPlrNum, int destPlrNum, int flags, boolean reliable)
{
	int pType = srcPlrNum==destPlrNum? GPT_CONSOLEPLAYER_STATE : GPT_PLAYER_STATE;
	player_t *pl = &players[srcPlrNum];
	byte buffer[UPD_BUFFER_LEN], *ptr = buffer, fl;
	int i, k;

	if(IS_CLIENT || !pl->plr->ingame
		|| (destPlrNum >= 0 && destPlrNum < MAXPLAYERS 
		&& !players[destPlrNum].plr->ingame)) return;

	// Include the player number if necessary.
	if(pType == GPT_PLAYER_STATE) *ptr++ = srcPlrNum;

	// The first bytes contain the flags.
	WRITE_SHORT(ptr, flags);
	if(flags & PSF_STATE) 
	{
		*ptr++ = pl->playerstate |
#if !__JHEXEN__ // Hexen doesn't have armortype.
			(pl->armortype << 4);
#else
			0;
#endif
		
	}
	if(flags & PSF_HEALTH) *ptr++ = pl->health;
	if(flags & PSF_ARMOR_POINTS) 
	{
#if __JHEXEN__
		// Hexen has many types of armor points, send them all.
		for(i=0; i<NUMARMOR; i++)
			*ptr++ = pl->armorpoints[i];
#else
		*ptr++ = pl->armorpoints;
#endif
	}

#if __JHERETIC__ || __JHEXEN__
	if(flags & PSF_INVENTORY)
	{
		*ptr++ = pl->inventorySlotNum;
		for(i=0; i<pl->inventorySlotNum; i++)
			WRITE_SHORT(ptr, (pl->inventory[i].type & 0xff) 
				| ((pl->inventory[i].count & 0xff) << 8));
	}
#endif

	if(flags & PSF_POWERS) // Austin P.?
	{
		// First see which powers should be sent.
#if __JHEXEN__
		for(i = 1, *ptr = 0; i < NUMPOWERS; i++)
			if(pl->powers[i]) *ptr |= 1 << (i-1);
#else
		for(i = 0, *ptr = 0; i < NUMPOWERS; i++)
		{
#ifdef __JDOOM__
			if(i == pw_ironfeet || i == pw_strength) continue;
#endif
			if(pl->powers[i]) *ptr |= 1 << i;
		}
#endif
		ptr++;

		// Send the non-zero powers.
#if __JHEXEN__
		for(i = 1; i < NUMPOWERS; i++)
			if(pl->powers[i]) 
				*ptr++ = (pl->powers[i]+34) / 35;
#else
		for(i = 0; i < NUMPOWERS; i++)
		{
#ifdef __JDOOM__
			if(i == pw_ironfeet || i == pw_strength) continue;
#endif
			if(pl->powers[i])
				*ptr++ = (pl->powers[i]+34) / 35;	// Send as seconds.
		}
#endif
	}
	if(flags & PSF_KEYS) 
	{
		*ptr = 0;

#ifdef __JDOOM__
		for(i=0; i<NUMCARDS; i++) if(pl->cards[i]) *ptr |= 1<<i;
#endif

#ifdef __JHERETIC__
		for(i=0; i<NUMKEYS; i++) if(pl->keys[i]) *ptr |= 1<<i;
#endif

		ptr++;
	}
	if(flags & PSF_FRAGS)
	{
		byte *count = ptr++;
		// We'll send all non-zero frags. The topmost four bits of
		// the word define the player number.
		for(i=0, *count = 0; i<MAXPLAYERS; i++)
			if(pl->frags[i])
			{
				WRITE_SHORT(ptr, (i<<12) | pl->frags[i]);
				(*count)++;
			}
	}
/*	if(flags & PSF_ALL_FRAGS)
	{
		for(i=0; i<MAXPLAYERS; i++) WRITE_SHORT(ptr, pl->frags[i]);
	}*/
	if(flags & PSF_OWNED_WEAPONS)
	{
		for(k = 0, i = 0; i < NUMWEAPONS; i++) 
			if(pl->weaponowned[i]) k |= 1<<i;
		*ptr++ = k;
	}
	if(flags & PSF_AMMO)
	{
#if __JHEXEN__
		// 200 is the mana limit for Hexen.
		for(i=0; i<NUMMANA; i++) *ptr++ = pl->mana[i];
#else
		for(i=0; i<NUMAMMO; i++) WRITE_SHORT(ptr, pl->ammo[i]);
#endif
	}
	if(flags & PSF_MAX_AMMO)
	{
#if !__JHEXEN__	// Hexen has no use for max ammo.
		for(i=0; i<NUMAMMO; i++) WRITE_SHORT(ptr, pl->maxammo[i]);
#endif
	}
	if(flags & PSF_COUNTERS)
	{
		WRITE_SHORT(ptr, pl->killcount);
		*ptr++ = pl->itemcount;
		*ptr++ = pl->secretcount;
	}
	if(flags & PSF_PENDING_WEAPON || flags & PSF_READY_WEAPON)
	{
		// These two will be in the same byte.
		fl = 0;
		if(flags & PSF_PENDING_WEAPON) fl |= pl->pendingweapon & 0xf;
		if(flags & PSF_READY_WEAPON) fl |= (pl->readyweapon & 0xf) << 4;
		*ptr++ = fl;
	}
	if(flags & PSF_VIEW_HEIGHT)
	{
		*ptr++ = (pl->plr->viewheight >> 16);
	}

#if __JHERETIC__
	if(flags & PSF_CHICKEN_TIME)
	{
		*ptr++ = (pl->chickenTics+34) / 35;
	}
#endif

#if __JHEXEN__
	if(flags & PSF_MORPH_TIME)
	{
		// Send as seconds.
		*ptr++ = (pl->morphTics+34) / 35;
	}
	if(flags & PSF_LOCAL_QUAKE)
	{
		// Send the "quaking" state.
		*ptr++ = localQuakeHappening[srcPlrNum];
	}
#endif

	/*Con_Printf( "PlrUpd (%i bytes, flags=%x):\n", ptr-buffer, flags);
	for(i=0; i<ptr-buffer; i++)
	{
		Con_Printf( "%02x ", buffer[i]);
		if(i && !(i%10)) Con_Printf( "\n");
	}
	Con_Printf( "\n");*/

	// Finally, send the packet.
	Net_SendPacket(destPlrNum | (reliable? DDSP_ORDERED : 0),
		pType, buffer, ptr-buffer);	
}

void NetSv_PSpriteChange(int plrNum, int state)
{
/*	byte buffer[20], *ptr = buffer;

	if(IS_CLIENT) return;

	WRITE_SHORT(ptr, state);
	Net_SendPacket(plrNum, GPT_PSPRITE_STATE, buffer, ptr-buffer);*/
}


/*void NetSv_SectorSound(sector_t *sector, int sound_id)
{
	byte buffer[20], *ptr = buffer;

	if(IS_CLIENT) return;

	*ptr++ = sound_id;
	WRITE_SHORT(ptr, sector-sectors);
	Net_SendPacket(DDSP_ALL_PLAYERS, GPT_SECTOR_SOUND, buffer, ptr-buffer);
}*/

void NetSv_SendGameState(int flags, int to)
{
	byte buffer[256], *ptr;
	packet_gamestate_t *gs = (packet_gamestate_t*) buffer;
	int i;

	if(IS_CLIENT) return;
	if(gamestate != GS_LEVEL) return;

	// Print a short message that describes the game state.
	if(ArgExists("-verbose") || IS_DEDICATED)
	{
		Con_Printf("Game setup: ep%i map%i %s\n",
			gameepisode, gamemap, gameConfigString);
	}
/*
	// We can't init the camera if we don't know which 
	// player this packet is going to.
	if(to < 0 || to >= MAXPLAYERS) flags &= ~GSF_CAMERA_INIT;*/

	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!players[i].plr->ingame
			|| to != DDSP_ALL_PLAYERS && to != i) continue;
		
		ptr = buffer;

#ifdef __JDOOM__
		gs->gamemode = gamemode;
#endif
		gs->flags = flags;
		gs->episode = gameepisode;
		gs->map = gamemap;
		gs->skill = gameskill;
		gs->deathmatch = deathmatch;
		gs->monsters = !nomonsters;
		gs->respawn = respawnparm;
#if !__JHEXEN__
		gs->jumping = cfg.jumpEnabled;
#endif
		gs->gravity = (Get(DD_GRAVITY) >> 8) & 0xffff;
		ptr += sizeof(packet_gamestate_t);
		
		if(flags & GSF_CAMERA_INIT)
		{
			mobj_t *mo = players[i].plr->mo;
			WRITE_SHORT(ptr, mo->x >> 16);
			WRITE_SHORT(ptr, mo->y >> 16);
			WRITE_SHORT(ptr, mo->z >> 16);
			WRITE_SHORT(ptr, mo->angle >> 16);
		}
		
		// Send the packet.
		Net_SendPacket(i | DDSP_ORDERED, GPT_GAME_STATE, buffer, ptr-buffer);
	}
}

void NetSv_Intermission(int flags, int state, int time)
{
	byte buffer[32], *ptr = buffer;

	if(IS_CLIENT) return;

	*ptr++ = flags;

#ifdef __JDOOM__
	if(flags & IMF_BEGIN)
	{
		// Only include the necessary information.
		WRITE_SHORT(ptr, wminfo.maxkills);
		WRITE_SHORT(ptr, wminfo.maxitems);
		WRITE_SHORT(ptr, wminfo.maxsecret);
		*ptr++ = wminfo.next;
		*ptr++ = wminfo.last;
		*ptr++ = wminfo.didsecret;
	}
#endif

#if __JHEXEN__
	if(flags & IMF_BEGIN)
	{
		*ptr++ = state;	// LeaveMap
		*ptr++ = time;	// LeavePosition
	}
#endif

	if(flags & IMF_STATE) *ptr++ = state;
	if(flags & IMF_TIME) WRITE_SHORT(ptr, time);
	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_ORDERED, GPT_INTERMISSION,
		buffer, ptr-buffer);
}

//===========================================================================
// NetSv_Finale
//	The actual script is sent to the clients. 'script' can be NULL.
//	
//===========================================================================
void NetSv_Finale(int flags, char *script, boolean *conds, int numConds)
{
	byte *buffer, *ptr;
	int i, len;

	if(IS_CLIENT) return;

	// How much memory do we need?
	if(script)
	{
		flags |= FINF_SCRIPT;
		len = strlen(script) + 2;	// The end null and flags byte.

		// The number of conditions and their values.
		len += 1 + numConds;
	}
	else
	{
		// Just enough memory for the flags byte.
		len = 1;
	}

	ptr = buffer = Z_Malloc(len, PU_STATIC, 0);
	
	// First the flags.
	*ptr++ = flags;

	if(script) 
	{
		// The conditions.
		*ptr++ = numConds;
		for(i = 0; i < numConds; i++) *ptr++ = conds[i];

		// Then the script itself.
		strcpy(ptr, script);
	}

	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_ORDERED, GPT_FINALE2,
		buffer, len);
	Z_Free(buffer);
}

void NetSv_SendPlayerInfo(int whose, int to_whom)
{
	byte buffer[10], *ptr = buffer;

	if(IS_CLIENT) return;

	*ptr++ = whose;
	*ptr++ = cfg.PlayerColor[whose];
#if __JHEXEN__
	*ptr++ = cfg.PlayerClass[whose];
#endif
	Net_SendPacket(to_whom | DDSP_ORDERED, GPT_PLAYER_INFO,
		buffer, ptr-buffer);
}

void NetSv_ChangePlayerInfo(int from, byte *data)
{
	player_t *pl = players + from;
	int col;

	// Color is first.
	col = *data++;
	cfg.PlayerColor[from] = PLR_COLOR(from, col);
#if __JHEXEN__
	cfg.PlayerClass[from] = *data++;
	Con_Printf( "NetSv_ChangePlayerInfo: pl%i, col=%i, class=%i\n", from, 
		cfg.PlayerColor[from], cfg.PlayerClass[from]);
	// The 'colormap' variable controls the setting of the color 
	// translation flags when the player is (re)spawned (which will 
	// be done in SB_ChangePlayerClass).
	pl->colormap = cfg.PlayerColor[from];
	SB_ChangePlayerClass(pl, cfg.PlayerClass[from]);
#else
	Con_Printf( "NetSv_ChangePlayerInfo: pl%i, col=%i\n", from, 
		cfg.PlayerColor[from]);
	if(pl->plr->mo)
	{
		// Change the player's mobj's color translation flags.
		pl->plr->mo->flags &= ~MF_TRANSLATION;
		pl->plr->mo->flags |= col << MF_TRANSSHIFT;
	}
#endif

	// Re-deal start spots.
	P_DealPlayerStarts();

	// Tell the other clients about the change.
	NetSv_SendPlayerInfo(from, DDSP_ALL_PLAYERS);
}

void NetSv_SaveGame(unsigned int game_id)
{
	if(!IS_SERVER || !IS_NETGAME) return;
	// This will make the clients save their games.
	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_CONFIRM, GPT_SAVE,
		&game_id, 4);
}

void NetSv_LoadGame(unsigned int game_id)
{
	if(!IS_SERVER || !IS_NETGAME) return;
	// The clients must tell their old console numbers.
	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_CONFIRM, GPT_LOAD,
		&game_id, 4);
}

//===========================================================================
// NetSv_FragsForAll
//	Sends the frags of player 'whose' to all other players.
//===========================================================================
void NetSv_FragsForAll(player_t *player)
{
	NetSv_SendPlayerState(player - players, DDSP_ALL_PLAYERS, 
		PSF_FRAGS, true);
}

//===========================================================================
// NetSv_SendPlayerClass
//===========================================================================
void NetSv_SendPlayerClass(int pnum, char cls)
{
	Net_SendPacket(pnum | DDSP_CONFIRM, GPT_CLASS, &cls, 1);
}

//===========================================================================
// NetSv_KillMessage
//	Send one of the kill messages, depending on the weapon of the killer.
//===========================================================================
void NetSv_KillMessage(player_t *killer, player_t *fragged)
{
#if __JDOOM__
	char buf[160], *in, tmp[2];
	
	if(!cfg.killMessages || !deathmatch) return;

	buf[0] = 0;
	tmp[1] = 0;

	// Choose the right kill message template.
	in = GET_TXT(killer == fragged? TXT_KILLMSG_SUICIDE 
		: TXT_KILLMSG_WEAPON0 + killer->readyweapon);
	for(; *in; in++)
	{
		if(in[0] == '%')
		{
			if(in[1] == '1')
			{
				strcat(buf, Net_GetPlayerName(killer - players));
				in++;
				continue;
			}
			if(in[1] == '2')
			{
				strcat(buf, Net_GetPlayerName(fragged - players));
				in++;
				continue;
			}
			if(in[1] == '%') in++;
		}
		tmp[0] = *in;
		strcat(buf, tmp);
	}

	// Send the message to everybody.
	NetSv_SendMessage(DDSP_ALL_PLAYERS, buf);
#endif
}

/*
 * Inform all clients about a change in the 'pausedness' of a game.
 */
void NetSv_Paused(boolean isPaused)
{
	char setPause = (isPaused != false);

	if(!IS_SERVER || !IS_NETGAME) return;

	// This will make the clients save their games.
	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_CONFIRM, GPT_PAUSE,
		&setPause, 1);
}

//===========================================================================
// NetSv_Ticker
//===========================================================================
void NetSv_Ticker(void)
{
	player_t *plr;
	int	i, red, palette;
#if __JDOOM__
	int bz;
#endif

	// Map rotation checker.
	NetSv_CheckCycling();

	// This is done here for servers.
	R_SetAllDoomsdayFlags();

	// Set the camera filters for players.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!players[i].plr->ingame) continue;

		plr = &players[i];

		red = plr->damagecount;
#ifdef __JDOOM__
		if(plr->powers[pw_strength])
		{
			// Slowly fade the berzerk out.
			bz = 12 - (plr->powers[pw_strength] >> 6);
			if(bz > red) red = bz;
		}
#endif
		if(red)
		{
			palette = (red+7)>>3;
			if(palette >= NUMREDPALS)
			{
				palette = NUMREDPALS-1;
			}
			palette += STARTREDPALS;
		}
		else if(plr->bonuscount)
		{
			palette = (plr->bonuscount+7)>>3;
			if(palette >= NUMBONUSPALS)
			{
				palette = NUMBONUSPALS-1;
			}
			palette += STARTBONUSPALS;
		}
#ifdef __JDOOM__
	    else if(plr->powers[pw_ironfeet] > 4*32
			|| plr->powers[pw_ironfeet] & 8)
			palette = 13; //RADIATIONPAL;
#elif __JHEXEN__
		else if(plr->poisoncount)
		{
			palette = (plr->poisoncount + 7) >> 3;
			if(palette >= NUMPOISONPALS)
			{
				palette = NUMPOISONPALS-1;
			}
			palette += STARTPOISONPALS;
		}
		else if(plr->plr->mo 
			&& plr->plr->mo->flags2 & MF2_ICEDAMAGE)
		{
			palette = STARTICEPAL;
		}
#endif
		else
		{
			palette = 0;
		}
		if(oldpals[i] != palette)
		{
			// The filter changes, send it to the client.
			plr->plr->flags |= DDPF_FILTER;
			oldpals[i] = palette;
		}
#if __JDOOM__
		plr->plr->filter = D_GetFilterColor(palette);
#elif __JHERETIC__
		plr->plr->filter = H_GetFilterColor(palette);
#else /*__JHEXEN__*/
		plr->plr->filter = H2_GetFilterColor(palette);
#endif
	}

#if __JHEXEN__
	// Keep track of player class changes (fighter, cleric, mage, pig).
	// Notify clients accordingly. This is mostly just FYI (it'll update
	// pl->class on the clientside).
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!players[i].plr->ingame) continue;
		if(oldclasses[i] != players[i].class)
		{
			oldclasses[i] = players[i].class;
			NetSv_SendPlayerClass(i, players[i].class);
		}
	}
#endif

	// Send the player state updates.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		// Don't send on every tic. Also, don't send to all 
		// players at the same time.
		if((gametic+i) % 10) continue; 
		if(!players[i].plr->ingame || !players[i].update) continue;
#if !__JHEXEN__
		// Owned weapons will be sent in a new kind of packet.
		if(players[i].update & PSF_OWNED_WEAPONS)
		{
			players[i].update &= ~PSF_OWNED_WEAPONS;
			NetSv_SendPlayerState2(i, i, PSF2_OWNED_WEAPONS, true);
			// That was all?
			if(!players[i].update) continue;
		}
#endif
		// Always send reliably.
		NetSv_SendPlayerState(i, i, players[i].update, true);
		players[i].update = 0;
	}
}


/*
 * Unravel a DDPT_COMMANDS (32) packet. Returns a pointer to a static
 * buffer that contains the ticcmds (kludge to work around the parameter
 * passing from the engine).
 */
void *NetSv_ReadCommands(byte *msg, uint size)
{
#define MAX_COMMANDS 30
	static byte data[2 + sizeof(ticcmd_t) * MAX_COMMANDS];
	ticcmd_t *cmd;
	byte *end = msg + size, flags;
	ushort *count = (ushort*) data;

	memset(data, 0, sizeof(data));

	// The first two bytes of the data contain the number of commands.
	*count = 0;

	// The first command.
	cmd = (void*) (data + 2);

	while(msg < end)
	{
		// One more command.
		*count += 1;		

		// First the flags.
		flags = *msg++;

		if(flags & CMDF_FORWARDMOVE) cmd->forwardmove = *msg++;
		if(flags & CMDF_SIDEMOVE) cmd->sidemove = *msg++;
		if(flags & CMDF_ANGLE) cmd->angle = *((short*)msg)++;
		if(flags & CMDF_LOOKDIR) cmd->lookdir = *((short*)msg)++;
		if(flags & CMDF_BUTTONS) cmd->buttons = *msg++;
#ifndef __JDOOM__
		if(flags & CMDF_LOOKFLY) cmd->lookfly = *msg++;
		if(flags & CMDF_ARTI) cmd->arti = *msg++;
#endif

		// Copy to next command (only differences have been written).
		memcpy(cmd + 1, cmd, sizeof(ticcmd_t));

		// Move to next command.
		cmd++;
	}

	return data;
}
