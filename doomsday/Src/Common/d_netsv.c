#include <ctype.h>

#ifdef __JDOOM__
#  include "doomdef.h"
#  include "doomstat.h"
#  include "p_local.h"
#  include "d_config.h"
#  include "s_sound.h"
#  include "st_stuff.h"
#  include "m_random.h"
#  include "dstrings.h"
#  include "m_cheat.h"
#  include "g_common.h"
#elif __JHERETIC__
#  include "../jHeretic/Doomdef.h"
#  include "../jHeretic/Soundst.h"
#  include "../jHeretic/P_local.h"
#  include "../jHeretic/h_config.h"
#elif __JHEXEN__
#  include "../jHexen/h2def.h"
#  include "../jHexen/p_local.h"
#  include "../jHexen/x_config.h"
#elif __JSTRIFE__
#  include "../jStrife/h2def.h"
#  include "../jStrife/p_local.h"
#  include "../jStrife/d_config.h"
#endif

#include "d_net.h"
#include "p_svtexarc.h"

// MACROS -----------------------------------------------------------------

#if __JHEXEN__ || __JSTRIFE__
#  define SOUND_COUNTDOWN		SFX_PICKUP_KEY
#elif __JDOOM__
#  define SOUND_COUNTDOWN		sfx_getpow
#elif __JHERETIC__
#  define SOUND_COUNTDOWN		sfx_keyup
#endif

#define SOUND_VICTORY		SOUND_COUNTDOWN

#define UPD_BUFFER_LEN	500

// How long is the largest possible sector update?
#define MAX_SECTORUPD	20
#define MAX_SIDEUPD		9

#define WRITE_SHORT(byteptr, val)	{(*(short*)(byteptr) = SHORT(val)); byteptr += 2;}
#define WRITE_LONG(byteptr, val)	{(*(int*)(byteptr) = LONG(val)); byteptr += 4;}

// TYPES ------------------------------------------------------------------

typedef struct {
	boolean usetime, usefrags;
	int     time;				// Minutes.
	int     frags;				// Maximum frags for one player.
} maprule_t;

enum {
	CYCLE_IDLE,
	CYCLE_TELL_RULES,
	CYCLE_COUNTDOWN
};

// EXTERNAL FUNCTIONS -----------------------------------------------------

void    R_SetAllDoomsdayFlags();

#if __JHEXEN__ || __JSTRIFE__
void    SB_ChangePlayerClass(player_t *player, int newclass);
#endif

// PUBLIC DATA ------------------------------------------------------------

boolean cyclingMaps = false;
char   *mapCycle = "";
boolean mapCycleNoExit = true;
int     netSvAllowSendMsg = true;
int     netSvAllowCheats = false;

// This is returned in *_Get(DD_GAME_CONFIG). It contains a combination
// of space-separated keywords.
char    gameConfigString[128];

// PRIVATE DATA -----------------------------------------------------------

static int cycleIndex;
static int cycleCounter = -1, cycleMode = CYCLE_IDLE;
static int oldPals[MAXPLAYERS];

#if __JHEXEN__ || __JSTRIFE__
static int oldClasses[MAXPLAYERS];
#endif

// CODE -------------------------------------------------------------------

//===========================================================================
// NetSv_UpdateGameConfig
//  Update the game config string with keywords that describe the game.
//  The string is sent out in netgames (also to the master).
//  Keywords: dm, coop, jump, nomonst, respawn, skillN
//===========================================================================
void NetSv_UpdateGameConfig(void)
{
	if(IS_CLIENT)
		return;

	memset(gameConfigString, 0, sizeof(gameConfigString));

	sprintf(gameConfigString, "skill%i", gameskill + 1);

	if(deathmatch > 1)
		sprintf(gameConfigString, " dm%i", deathmatch);
	else if(deathmatch)
		strcat(gameConfigString, " dm");
	else
		strcat(gameConfigString, " coop");

	if(nomonsters)
		strcat(gameConfigString, " nomonst");
	if(respawnparm)
		strcat(gameConfigString, " respawn");
#if !__JHEXEN__
	if(cfg.jumpEnabled)
		strcat(gameConfigString, " jump");
#endif
}

//===========================================================================
// NetSv_ScanCycle
//  Reads through the MapCycle cvar and finds the map with the given index.
//  Rules that apply to the map are returned in 'rules'.
//===========================================================================
int NetSv_ScanCycle(int index, maprule_t * rules)
{
	char   *ptr = mapCycle, *end;
	int     i, pos = -1, episode, mission;

#if __JHEXEN__ || __JSTRIFE__
	int     m;
#endif
	char    tmp[3], lump[10];
	boolean clear = false, has_random = false;
	maprule_t dummy;

	if(!rules)
		rules = &dummy;

	// By default no rules apply.
	rules->usetime = rules->usefrags = false;

	for(; *ptr; ptr++)
	{
		if(isspace(*ptr))
			continue;
		if(*ptr == ',' || *ptr == '+' || *ptr == ';' || *ptr == '/' ||
		   *ptr == '\\')
		{
			// These symbols are allowed to combine "time" and "frags".
			// E.g. "Time:10/Frags:5" or "t:30, f:10"
			clear = false;
		}
		else if(!strnicmp("time", ptr, 1))
		{
			// Find the colon.
			while(*ptr && *ptr != ':')
				ptr++;
			if(!*ptr)
				return -1;
			if(clear)
				rules->usefrags = false;
			clear = true;
			rules->usetime = true;
			rules->time = strtol(ptr + 1, &end, 0);
			ptr = end - 1;
		}
		else if(!strnicmp("frags", ptr, 1))
		{
			// Find the colon.
			while(*ptr && *ptr != ':')
				ptr++;
			if(!*ptr)
				return -1;
			if(clear)
				rules->usetime = false;
			clear = true;
			rules->usefrags = true;
			rules->frags = strtol(ptr + 1, &end, 0);
			ptr = end - 1;
		}
		else if(*ptr == '*' || (*ptr >= '0' && *ptr <= '9'))
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
				if(tmp[0] == '*' || tmp[1] == '*')
					has_random = true;
				// This is the map we're looking for. Return it.
				// But first randomize the asterisks.
				for(i = 0; i < 100; i++)	// Try many times to find a good map.
				{
					// The differences in map numbering make this harder
					// than it should be.
#if __JDOOM__
					if(gamemode == commercial)
					{
						sprintf(lump, "MAP%i%i", episode =
								tmp[0] == '*' ? M_Random() % 4 : tmp[0] - '0',
								mission =
								tmp[1] ==
								'*' ? M_Random() % 10 : tmp[1] - '0');
					}
					else
					{
						sprintf(lump, "E%iM%i", episode =
								tmp[0] ==
								'*' ? 1 + M_Random() % 4 : tmp[0] - '0',
								mission =
								tmp[1] ==
								'*' ? 1 + M_Random() % 9 : tmp[1] - '0');
					}
#elif __JSTRIFE__
					sprintf(lump, "MAP%i%i", episode =
							tmp[0] == '*' ? M_Random() % 4 : tmp[0] - '0',
							mission =
							tmp[1] ==
							'*' ? M_Random() % 10 : tmp[1] - '0');
#elif __JHERETIC__
					sprintf(lump, "E%iM%i", episode =
							tmp[0] == '*' ? 1 + M_Random() % 6 : tmp[0] - '0',
							mission =
							tmp[1] == '*' ? 1 + M_Random() % 9 : tmp[1] - '0');
#elif __JHEXEN__
					sprintf(lump, "%i%i", episode =
							tmp[0] == '*' ? M_Random() % 4 : tmp[0] - '0',
							mission =
							tmp[1] == '*' ? M_Random() % 10 : tmp[1] - '0');
					m = P_TranslateMap(atoi(lump));
					if(m < 0)
						continue;
					sprintf(lump, "MAP%02i", m);
#endif
					if(W_CheckNumForName(lump) >= 0)
					{
						tmp[0] = episode + '0';
						tmp[1] = mission + '0';
						break;
					}
					else if(!has_random)
						return -1;
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
	char    tmp[3], cmd[80];

	sprintf(tmp, "%02i", map);
#if __JDOOM__
	if(gamemode == commercial)
		sprintf(cmd, "setmap 1 %i", map);
	else
		sprintf(cmd, "setmap %c %c", tmp[0], tmp[1]);
#elif __JHERETIC__
	sprintf(cmd, "setmap %c %c", tmp[0], tmp[1]);
#elif __JHEXEN__ || __JSTRIFE__
	sprintf(cmd, "setmap %i", map);
#endif
	Con_Execute(cmd, false);

	// In a couple of seconds, send everyone the rules of this map.
	cycleMode = CYCLE_TELL_RULES;
	cycleCounter = 3 * TICSPERSEC;
}

//===========================================================================
// NetSv_GetFrags
//  Calculates the frags of player 'pl'.
//===========================================================================
int NetSv_GetFrags(int pl)
{
	int     i, frags = 0;

	for(i = 0; i < MAXPLAYERS; i++)
	{
#if __JDOOM__
		frags += players[pl].frags[i] * (i == pl ? -1 : 1);
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
	int     map, i, f;
	maprule_t rules;
	char    msg[100], tmp[50];

	if(!cyclingMaps)
		return;
	cycleCounter--;
	switch (cycleMode)
	{
	case CYCLE_IDLE:
		// Check if the current map should end.
		if(cycleCounter <= 0)
		{
			// Test every ten seconds.
			cycleCounter = 10 * TICSPERSEC;

			map = NetSv_ScanCycle(cycleIndex, &rules);
			if(map < 0)
			{
				map = NetSv_ScanCycle(cycleIndex = 0, &rules);
				if(map < 0)
				{
					// Hmm?! Abort cycling.
					Con_Message
						("NetSv_CheckCycling: All of a sudden MapCycle is invalid!\n");
					Con_Execute("endcycle", false);
					return;
				}
			}
			if(rules.usetime &&
			   leveltime > (rules.time * 60 - 29) * TICSPERSEC)
			{
				// Time runs out!
				cycleMode = CYCLE_COUNTDOWN;
				cycleCounter = 31 * TICSPERSEC;
			}
			if(rules.usefrags)
			{
				for(i = 0; i < MAXPLAYERS; i++)
				{
					if(!players[i].plr->ingame)
						continue;
					if((f = NetSv_GetFrags(i)) >= rules.frags)
					{
						sprintf(msg, "--- %s REACHES %i FRAGS ---",
								Net_GetPlayerName(i), f);
						NetSv_SendMessage(DDSP_ALL_PLAYERS, msg);
						S_StartSound(SOUND_VICTORY, NULL);
						cycleMode = CYCLE_COUNTDOWN;
						cycleCounter = 15 * TICSPERSEC;	// No msg for 15 secs.
						break;
					}
				}
			}
		}
		break;

	case CYCLE_TELL_RULES:
		if(cycleCounter <= 0)
		{
			// Get the rules of the current map.
			NetSv_ScanCycle(cycleIndex, &rules);
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
					sprintf(tmp, "%s%i FRAGS", rules.usetime ? " OR " : "",
							rules.frags);
					strcat(msg, tmp);
				}
			}
			// Send it to all players.
			NetSv_SendMessage(DDSP_ALL_PLAYERS, msg);
			// Start checking.
			cycleMode = CYCLE_IDLE;
		}
		break;

	case CYCLE_COUNTDOWN:
		if(cycleCounter == 30 * TICSPERSEC || cycleCounter == 15 * TICSPERSEC
		   || cycleCounter == 10 * TICSPERSEC ||
		   cycleCounter == 5 * TICSPERSEC)
		{
			sprintf(msg, "--- WARPING IN %i SECONDS ---",
					cycleCounter / TICSPERSEC);
			NetSv_SendMessage(DDSP_ALL_PLAYERS, msg);
			// Also, a warning sound.
			S_StartSound(SOUND_COUNTDOWN, NULL);
		}
		else if(cycleCounter <= 0)
		{
			// Next map, please!
			map = NetSv_ScanCycle(++cycleIndex, NULL);
			if(map < 0)
			{
				// Must be past the end?
				map = NetSv_ScanCycle(cycleIndex = 0, NULL);
				if(map < 0)
				{
					// Hmm?! Abort cycling.
					Con_Message
						("NetSv_CheckCycling: All of a sudden MapCycle is invalid!\n");
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
//  Handles the console commands "startcycle" and "endcycle".
//===========================================================================
int CCmdMapCycle(int argc, char **argv)
{
	int     map;

	if(!IS_SERVER)
	{
		Con_Printf("Only allowed for a server.\n");
		return false;
	}
	if(!stricmp(argv[0], "startcycle"))	// (Re)start rotation?
	{
		// Find the first map in the sequence.
		map = NetSv_ScanCycle(cycleIndex = 0, 0);
		if(map < 0)
		{
			Con_Printf("MapCycle \"%s\" is invalid.\n", mapCycle);
			return false;
		}
		// Warp there.
		NetSv_CycleToMapNum(map);
		cyclingMaps = true;
	}
	else						// OK, then we need to end it.
	{
		if(cyclingMaps)
		{
			cyclingMaps = false;
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

	if(deathmatch)
	{
		G_DeathMatchSpawnPlayer(plrnumber);
	}
	else
	{
		// Spawn the player into the world.
		// FIXME: Spawn a telefog in front of the player.
		P_SpawnPlayer(&playerstarts[plr->startspot], plrnumber);
	}
	
	// Get rid of anybody at the starting spot.
	P_Telefrag(plr->plr->mo);
}

//===========================================================================
// NetSv_SendMessageEx
//===========================================================================
void NetSv_SendMessageEx(int plrNum, char *msg, boolean yellow)
{
	if(IS_CLIENT || !netSvAllowSendMsg)
		return;
	if(plrNum >= 0 && plrNum < MAXPLAYERS)
		if(!players[plrNum].plr->ingame)
			return;
	if(plrNum == DDSP_ALL_PLAYERS)
	{
		// Also show locally. No sound is played!
		D_NetMessageNoSound(msg);
	}
	Net_SendPacket(plrNum | DDSP_ORDERED,
				   yellow ? GPT_YELLOW_MESSAGE : GPT_MESSAGE, msg,
				   strlen(msg) + 1);
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
//  More player state information. Had to be separate because of backwards
//  compatibility.
//===========================================================================
void NetSv_SendPlayerState2(int srcPlrNum, int destPlrNum, int flags,
							boolean reliable)
{
	int     pType =
		(srcPlrNum ==
		 destPlrNum ? GPT_CONSOLEPLAYER_STATE2 : GPT_PLAYER_STATE2);
	player_t *pl = &players[srcPlrNum];
	byte    buffer[UPD_BUFFER_LEN], *ptr = buffer;
	int     i, fl;

	// Check that this is a valid call.
	if(IS_CLIENT || !pl->plr->ingame ||
	   (destPlrNum >= 0 && destPlrNum < MAXPLAYERS &&
		!players[destPlrNum].plr->ingame))
		return;

	// Include the player number if necessary.
	if(pType == GPT_PLAYER_STATE2)
		*ptr++ = srcPlrNum;
	WRITE_LONG(ptr, flags);

	if(flags & PSF2_OWNED_WEAPONS)
	{
		// This supports up to 16 weapons.
		for(fl = 0, i = 0; i < NUMWEAPONS; i++)
			if(pl->weaponowned[i])
				fl |= 1 << i;
		WRITE_SHORT(ptr, fl);
	}

	if(flags & PSF2_STATE)
	{
		*ptr++ = pl->playerstate |
#if __JDOOM__ || __JHERETIC__				// Hexen doesn't have armortype.
			(pl->armortype << 4);
#else
			0;
#endif
		*ptr++ = pl->cheats;
	}

	// Finally, send the packet.
	Net_SendPacket(destPlrNum | (reliable ? DDSP_ORDERED : 0), pType, buffer,
				   ptr - buffer);
}

//===========================================================================
// NetSv_SendPlayerState
//===========================================================================
void NetSv_SendPlayerState(int srcPlrNum, int destPlrNum, int flags,
						   boolean reliable)
{
	int     pType =
		srcPlrNum == destPlrNum ? GPT_CONSOLEPLAYER_STATE : GPT_PLAYER_STATE;
	player_t *pl = &players[srcPlrNum];
	byte    buffer[UPD_BUFFER_LEN], *ptr = buffer, fl;
	int     i, k;

	if(IS_CLIENT || !pl->plr->ingame ||
	   (destPlrNum >= 0 && destPlrNum < MAXPLAYERS &&
		!players[destPlrNum].plr->ingame))
		return;

	// Include the player number if necessary.
	if(pType == GPT_PLAYER_STATE)
		*ptr++ = srcPlrNum;

	// The first bytes contain the flags.
	WRITE_SHORT(ptr, flags);
	if(flags & PSF_STATE)
	{
		*ptr++ = pl->playerstate |
#if __JDOOM__ || __JHERETIC__					// Hexen doesn't have armortype.
			(pl->armortype << 4);
#else
			0;
#endif

	}
	if(flags & PSF_HEALTH)
		*ptr++ = pl->health;
	if(flags & PSF_ARMOR_POINTS)
	{
#if __JHEXEN__
		// Hexen has many types of armor points, send them all.
		for(i = 0; i < NUMARMOR; i++)
			*ptr++ = pl->armorpoints[i];
#else
		*ptr++ = pl->armorpoints;
#endif
	}

#if __JHERETIC__ || __JHEXEN__ || __JSTRIFE__
	if(flags & PSF_INVENTORY)
	{
		*ptr++ = pl->inventorySlotNum;
		for(i = 0; i < pl->inventorySlotNum; i++)
			WRITE_SHORT(ptr,
						(pl->inventory[i].
						 type & 0xff) | ((pl->inventory[i].
										  count & 0xff) << 8));
	}
#endif

	if(flags & PSF_POWERS)		// Austin P.?
	{
		// First see which powers should be sent.
#if __JHEXEN__ || __JSTRIFE__
		for(i = 1, *ptr = 0; i < NUMPOWERS; i++)
			if(pl->powers[i])
				*ptr |= 1 << (i - 1);
#else
		for(i = 0, *ptr = 0; i < NUMPOWERS; i++)
		{
#  ifdef __JDOOM__
			if(i == pw_ironfeet || i == pw_strength)
				continue;
#  endif
			if(pl->powers[i])
				*ptr |= 1 << i;
		}
#endif
		ptr++;

		// Send the non-zero powers.
#if __JHEXEN__ || __JSTRIFE__
		for(i = 1; i < NUMPOWERS; i++)
			if(pl->powers[i])
				*ptr++ = (pl->powers[i] + 34) / 35;
#else
		for(i = 0; i < NUMPOWERS; i++)
		{
#  if __JDOOM__
			if(i == pw_ironfeet || i == pw_strength)
				continue;
#  endif
			if(pl->powers[i])
				*ptr++ = (pl->powers[i] + 34) / 35;	// Send as seconds.
		}
#endif
	}
	if(flags & PSF_KEYS)
	{
		*ptr = 0;

#if __JDOOM__ | __JHERETIC__
		for(i = 0; i < NUMKEYS; i++)
			if(pl->keys[i])
				*ptr |= 1 << i;
#endif

		ptr++;
	}
	if(flags & PSF_FRAGS)
	{
		byte   *count = ptr++;

		// We'll send all non-zero frags. The topmost four bits of
		// the word define the player number.
		for(i = 0, *count = 0; i < MAXPLAYERS; i++)
			if(pl->frags[i])
			{
				WRITE_SHORT(ptr, (i << 12) | pl->frags[i]);
				(*count)++;
			}
	}
	if(flags & PSF_OWNED_WEAPONS)
	{
		for(k = 0, i = 0; i < NUMWEAPONS; i++)
			if(pl->weaponowned[i])
				k |= 1 << i;
		*ptr++ = k;
	}
	if(flags & PSF_AMMO)
	{
#if __JHEXEN__ || __JSTRIFE__
		// 200 is the mana limit for Hexen.
		for(i = 0; i < NUMMANA; i++)
			*ptr++ = pl->mana[i];
#else
		for(i = 0; i < NUMAMMO; i++)
			WRITE_SHORT(ptr, pl->ammo[i]);
#endif
	}
	if(flags & PSF_MAX_AMMO)
	{
#if __JDOOM__ || __JHERETIC__				// Hexen has no use for max ammo.
		for(i = 0; i < NUMAMMO; i++)
			WRITE_SHORT(ptr, pl->maxammo[i]);
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
		if(flags & PSF_PENDING_WEAPON)
			fl |= pl->pendingweapon & 0xf;
		if(flags & PSF_READY_WEAPON)
			fl |= (pl->readyweapon & 0xf) << 4;
		*ptr++ = fl;
	}
	if(flags & PSF_VIEW_HEIGHT)
	{
		*ptr++ = (pl->plr->viewheight >> 16);
	}

#if __JHERETIC__
	if(flags & PSF_CHICKEN_TIME)
	{
		*ptr++ = (pl->chickenTics + 34) / 35;
	}
#endif

#if __JHEXEN__ || __JSTRIFE__
	if(flags & PSF_MORPH_TIME)
	{
		// Send as seconds.
		*ptr++ = (pl->morphTics + 34) / 35;
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
	   Con_Printf( "\n"); */

	// Finally, send the packet.
	Net_SendPacket(destPlrNum | (reliable ? DDSP_ORDERED : 0), pType, buffer,
				   ptr - buffer);
}

void NetSv_PSpriteChange(int plrNum, int state)
{
	/*  byte buffer[20], *ptr = buffer;

	   if(IS_CLIENT) return;

	   WRITE_SHORT(ptr, state);
	   Net_SendPacket(plrNum, GPT_PSPRITE_STATE, buffer, ptr-buffer); */
}

void NetSv_SendGameState(int flags, int to)
{
	byte    buffer[256], *ptr;
	//packet_gamestate_t *gs = (packet_gamestate_t *) buffer;
	int     i;

	if(IS_CLIENT)
		return;
	if(gamestate != GS_LEVEL)
		return;

	// Print a short message that describes the game state.
	if(ArgExists("-verbose") || IS_DEDICATED)
	{
		Con_Printf("Game setup: ep%i map%i %s\n", gameepisode, gamemap,
				   gameConfigString);
	}

    // Send an update to all the players in the game.
	for(i = 0; i < MAXPLAYERS; i++)
	{
#if __JHEXEN__ || __JSTRIFE__
        int gameStateSize = 16;
#else
        int gameStateSize = 8;
#endif
        //int k;

        if(!players[i].plr->ingame || (to != DDSP_ALL_PLAYERS && to != i))
			continue;

		ptr = buffer;

        // The contents of the game state package are a bit messy
        // due to compatibility with older versions.
        
        /*byte setup[16];
        Con_Message("Sizeof(gs)=%i\n", sizeof(packet_gamestate_t));
        Con_Message("Sent game state packet (len=%i):\n", ptr - buffer);
        for(k = 0; k < ptr - buffer; ++k)
            Con_Message("%02x ", buffer[k]);
        Con_Message("\n");*/

#ifdef __JDOOM__
        ptr[0] = gamemode;
#else
        ptr[0] = 0;
#endif
        ptr[1] = flags;
        ptr[2] = gameepisode;
        ptr[3] = gamemap;
        ptr[4] = (deathmatch & 0x3)
            | (!nomonsters? 0x4 : 0)
            | (respawnparm? 0x8 : 0)
#if __JDOOM__ || __JHERETIC__              
            | (cfg.jumpEnabled? 0x10 : 0)
            | (gameskill << 5);
#else
        ;
#endif
#if __JDOOM__ || __JHERETIC__            
        ptr[5] = 0;

#else
        ptr[5] = gameskill & 0x7;
#endif
        ptr[6] = (Get(DD_GRAVITY) >> 8) & 0xff; // low byte
        ptr[7] = (Get(DD_GRAVITY) >> 16) & 0xff; // high byte
        memset(ptr + 8, 0, 8);
        /*
        Con_Message("Alternative state packet:\n");
        for(k = 0; k < gameStateSize; ++k)
            Con_Message("%02x ", setup[k]);
        Con_Message("\n");
        */

        ptr += gameStateSize;
        
#if 0
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
#if __JDOOM__ || __JHERETIC__
		gs->jumping = cfg.jumpEnabled;
#endif
		gs->gravity = SHORT((Get(DD_GRAVITY) >> 8) & 0xffff);
		ptr += sizeof(packet_gamestate_t);
#endif // #if 0

		if(flags & GSF_CAMERA_INIT)
		{
			mobj_t *mo = players[i].plr->mo;

			WRITE_SHORT(ptr, mo->x >> 16);
			WRITE_SHORT(ptr, mo->y >> 16);
			WRITE_SHORT(ptr, mo->z >> 16);
			WRITE_SHORT(ptr, mo->angle >> 16);
		}

		// Send the packet.
		Net_SendPacket(i | DDSP_ORDERED, GPT_GAME_STATE, buffer, ptr - buffer);
	}
}

void NetSv_Intermission(int flags, int state, int time)
{
	byte    buffer[32], *ptr = buffer;

	if(IS_CLIENT)
		return;

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

#if __JHEXEN__ || __JSTRIFE__
	if(flags & IMF_BEGIN)
	{
		*ptr++ = state;			// LeaveMap
		*ptr++ = time;			// LeavePosition
	}
#endif

	if(flags & IMF_STATE)
		*ptr++ = state;
	if(flags & IMF_TIME)
		WRITE_SHORT(ptr, time);
	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_ORDERED, GPT_INTERMISSION, buffer,
				   ptr - buffer);
}

//===========================================================================
// NetSv_Finale
//  The actual script is sent to the clients. 'script' can be NULL.
//  
//===========================================================================
void NetSv_Finale(int flags, char *script, boolean *conds, int numConds)
{
	byte   *buffer, *ptr;
	int     i, len;

	if(IS_CLIENT)
		return;

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
		for(i = 0; i < numConds; i++)
			*ptr++ = conds[i];

		// Then the script itself.
		strcpy((char*)ptr, script);
	}

	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_ORDERED, GPT_FINALE2, buffer, len);
	Z_Free(buffer);
}

void NetSv_SendPlayerInfo(int whose, int to_whom)
{
	byte    buffer[10], *ptr = buffer;

	if(IS_CLIENT)
		return;

	*ptr++ = whose;
	*ptr++ = cfg.PlayerColor[whose];
#if __JHEXEN__
	*ptr++ = cfg.PlayerClass[whose];
#endif
	Net_SendPacket(to_whom | DDSP_ORDERED, GPT_PLAYER_INFO, buffer,
				   ptr - buffer);
}

void NetSv_ChangePlayerInfo(int from, byte *data)
{
	player_t *pl = players + from;
	int     col;

	// Color is first.
	col = *data++;
	cfg.PlayerColor[from] = PLR_COLOR(from, col);
#if __JHEXEN__
	cfg.PlayerClass[from] = *data++;
	Con_Printf("NetSv_ChangePlayerInfo: pl%i, col=%i, class=%i\n", from,
			   cfg.PlayerColor[from], cfg.PlayerClass[from]);
	// The 'colormap' variable controls the setting of the color 
	// translation flags when the player is (re)spawned (which will 
	// be done in SB_ChangePlayerClass).
	pl->colormap = cfg.PlayerColor[from];
	SB_ChangePlayerClass(pl, cfg.PlayerClass[from]);
#else
	Con_Printf("NetSv_ChangePlayerInfo: pl%i, col=%i\n", from,
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
	if(!IS_SERVER || !IS_NETGAME)
		return;
	// This will make the clients save their games.
	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_CONFIRM, GPT_SAVE, &game_id, 4);
}

void NetSv_LoadGame(unsigned int game_id)
{
	if(!IS_SERVER || !IS_NETGAME)
		return;
	// The clients must tell their old console numbers.
	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_CONFIRM, GPT_LOAD, &game_id, 4);
}

//===========================================================================
// NetSv_FragsForAll
//  Sends the frags of player 'whose' to all other players.
//===========================================================================
void NetSv_FragsForAll(player_t *player)
{
	NetSv_SendPlayerState(player - players, DDSP_ALL_PLAYERS, PSF_FRAGS, true);
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
//  Send one of the kill messages, depending on the weapon of the killer.
//===========================================================================
void NetSv_KillMessage(player_t *killer, player_t *fragged, boolean stomping)
{
#if __JDOOM__
	char    buf[160], *in, tmp[2];

	if(!cfg.killMessages || !deathmatch)
		return;

	buf[0] = 0;
	tmp[1] = 0;

	// Choose the right kill message template.
	in = GET_TXT(stomping ? TXT_KILLMSG_STOMP : killer ==
				 fragged ? TXT_KILLMSG_SUICIDE : TXT_KILLMSG_WEAPON0 +
				 killer->readyweapon);
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
			if(in[1] == '%')
				in++;
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
	char    setPause = (isPaused != false);

	if(!IS_SERVER || !IS_NETGAME)
		return;

	// This will make the clients save their games.
	Net_SendPacket(DDSP_ALL_PLAYERS | DDSP_CONFIRM, GPT_PAUSE, &setPause, 1);
}

/*
 * The default jump power is 9.
 */
void NetSv_SendJumpPower(int target, float power)
{
	char    msg[50];

	if(!IS_SERVER)
		return;

    power = FLOAT(power);
	memcpy((void *) msg, &power, 4);
	Net_SendPacket(target | DDSP_CONFIRM, GPT_JUMP_POWER, msg, 4);
}

//===========================================================================
// NetSv_Ticker
//===========================================================================
void NetSv_Ticker(void)
{
	player_t *plr;
	int     i, red, palette;
	float   power;

#if __JDOOM__
	int     bz;
#endif

	// Map rotation checker.
	NetSv_CheckCycling();

	// This is done here for servers.
	R_SetAllDoomsdayFlags();

	// Set the camera filters for players.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!players[i].plr->ingame)
			continue;

		plr = &players[i];

		red = plr->damagecount;
#ifdef __JDOOM__
		if(plr->powers[pw_strength])
		{
			// Slowly fade the berzerk out.
			bz = 12 - (plr->powers[pw_strength] >> 6);
			if(bz > red)
				red = bz;
		}
#endif
		if(red)
		{
			palette = (red + 7) >> 3;
			if(palette >= NUMREDPALS)
			{
				palette = NUMREDPALS - 1;
			}
			palette += STARTREDPALS;
		}
		else if(plr->bonuscount)
		{
			palette = (plr->bonuscount + 7) >> 3;
			if(palette >= NUMBONUSPALS)
			{
				palette = NUMBONUSPALS - 1;
			}
			palette += STARTBONUSPALS;
		}
#ifdef __JDOOM__
		else if(plr->powers[pw_ironfeet] > 4 * 32 ||
				plr->powers[pw_ironfeet] & 8)
			palette = 13;		//RADIATIONPAL;
#elif __JHEXEN__
		else if(plr->poisoncount)
		{
			palette = (plr->poisoncount + 7) >> 3;
			if(palette >= NUMPOISONPALS)
			{
				palette = NUMPOISONPALS - 1;
			}
			palette += STARTPOISONPALS;
		}
		else if(plr->plr->mo && plr->plr->mo->flags2 & MF2_ICEDAMAGE)
		{
			palette = STARTICEPAL;
		}
#endif
		else
		{
			palette = 0;
		}
		if(oldPals[i] != palette)
		{
			// The filter changes, send it to the client.
			plr->plr->flags |= DDPF_FILTER;
			oldPals[i] = palette;
		}
#if __JDOOM__
		plr->plr->filter = D_GetFilterColor(palette);
#elif __JHERETIC__
		plr->plr->filter = H_GetFilterColor(palette);

#else /*__JHEXEN__*/
		plr->plr->filter = H2_GetFilterColor(palette);
#endif
	}

#if __JHEXEN__ || __JSTRIFE__
	// Keep track of player class changes (fighter, cleric, mage, pig).
	// Notify clients accordingly. This is mostly just FYI (it'll update
	// pl->class on the clientside).
	for(i = 0; i < MAXPLAYERS; i++)
	{
		if(!players[i].plr->ingame)
			continue;
		if(oldClasses[i] != players[i].class)
		{
			oldClasses[i] = players[i].class;
			NetSv_SendPlayerClass(i, players[i].class);
		}
	}
#endif

	// Inform clients about jumping?
	power = (cfg.jumpEnabled ? cfg.jumpPower : 0);
	if(power != netJumpPower)
	{
		netJumpPower = power;
		for(i = 0; i < MAXPLAYERS; i++)
			if(players[i].plr->ingame)
				NetSv_SendJumpPower(i, power);
	}

	// Send the player state updates.
	for(i = 0, plr = players; i < MAXPLAYERS; i++, plr++)
	{
		// Don't send on every tic. Also, don't send to all 
		// players at the same time.
		if((gametic + i) % 10)
			continue;
		if(!plr->plr->ingame || !plr->update)
			continue;

		// Owned weapons and player state will be sent in a new kind of 
		// packet.
		if(plr->update & (PSF_OWNED_WEAPONS | PSF_STATE))
		{
			NetSv_SendPlayerState2(i, i,
								   (plr->
									update & PSF_OWNED_WEAPONS ?
									PSF2_OWNED_WEAPONS : 0) | (plr->
															   update &
															   PSF_STATE ?
															   PSF2_STATE : 0),
								   true);
			plr->update &= ~(PSF_OWNED_WEAPONS | PSF_STATE);
			// That was all?
			if(!plr->update)
				continue;
		}

		// The delivery of the state packet will be confirmed.
		NetSv_SendPlayerState(i, i, plr->update, true);
		plr->update = 0;
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
	byte   *end = msg + size, flags;
	ushort *count = (ushort *) data;

	memset(data, 0, sizeof(data));

	// The first two bytes of the data contain the number of commands.
	*count = 0;

	// The first command.
	cmd = (void *) (data + 2);

	while(msg < end)
	{
		// One more command.
		*count += 1;

		// First the flags.
		flags = *msg++;

		if(flags & CMDF_FORWARDMOVE)
			cmd->forwardMove = *msg++;
		if(flags & CMDF_SIDEMOVE)
			cmd->sideMove = *msg++;
		if(flags & CMDF_ANGLE)
        {
			cmd->angle = SHORT( *(short *) msg );
            msg += 2;
        }
		if(flags & CMDF_LOOKDIR)
        {
			cmd->pitch = SHORT( *(short *) msg );
            msg += 2;
        }
		if(flags & CMDF_BUTTONS)
		{
			byte buttons = *msg++;
 			cmd->attack = ((buttons & CMDF_BTN_ATTACK) != 0);
			cmd->use = ((buttons & CMDF_BTN_USE) != 0);
			cmd->jump = ((buttons & CMDF_BTN_JUMP) != 0);
			cmd->pause = ((buttons & CMDF_BTN_PAUSE) != 0);
			cmd->suicide = ((buttons & CMDF_BTN_SUICIDE) != 0);
		}
		else
		{
			cmd->attack = cmd->use = cmd->jump = cmd->pause = 
				cmd->suicide = false;
		}
		if(flags & CMDF_LOOKFLY)
			cmd->fly = *msg++;
		if(flags & CMDF_ARTI)
			cmd->arti = *msg++;
		if(flags & CMDF_CHANGE_WEAPON)
        {
			cmd->changeWeapon = SHORT( *(short *) msg );
            msg += 2;
        }

		// Copy to next command (only differences have been written).
		memcpy(cmd + 1, cmd, sizeof(ticcmd_t));

		// Move to next command.
		cmd++;
	}

	return data;
}

/*
 * Process the requested cheat command, if possible.
 */
void NetSv_DoCheat(int player, const char *data)
{
	char    command[40];

	memset(command, 0, sizeof(command));
	strncpy(command, data, sizeof(command) - 1);

	// If cheating is not allowed, we ain't doing nuthin'.
	if(!netSvAllowCheats)
		return;

	if(!strnicmp(command, "god", 3))
	{
		cht_GodFunc(players + player);
	}
	else if(!strnicmp(command, "noclip", 6))
	{
		cht_NoClipFunc(players + player);
	}
	else if(!strnicmp(command, "give", 4))
	{
		Con_Executef(false, "%s %i", command, player);
	}
}
