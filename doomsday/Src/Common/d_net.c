#if __JDOOM__
#include "../jDoom/doomdef.h"
#include "../jDoom/doomstat.h"
#include "../jDoom/d_main.h"
#include "../jDoom/d_config.h"
#include "../jDoom/p_local.h"
#include "../jDoom/s_sound.h"
#include "../jDoom/g_game.h"
#include "../jDoom/m_menu.h"
#include "../jDoom/hu_stuff.h"
#include "../jDoom/st_stuff.h"
#endif

#if __JHERETIC__
#include "../jHeretic/Doomdef.h"
#include "../jHeretic/P_local.h"
#include "../jHeretic/Soundst.h"
#include "../jHeretic/settings.h"
#endif

#if __JHEXEN__
#include "../jHexen/h2def.h"
#include "../jHexen/p_local.h"
#include "../jHexen/soundst.h"
#include "../jHexen/settings.h"
#endif

#include "g_common.h"
#include "d_net.h"

// TYPES --------------------------------------------------------------------

// EXTERNAL FUNCTIONS -------------------------------------------------------

#if __JHEXEN__
void SB_ChangePlayerClass(player_t *player, int newclass);
#endif

extern int netSvAllowSendMsg;

// PUBLIC DATA --------------------------------------------------------------

char	msgBuff[256];
float	netJumpPower = 9;

// PRIVATE DATA -------------------------------------------------------------

// CODE ---------------------------------------------------------------------

int CCmdSetColor(int argc, char **argv)
{
#if __JHEXEN__
	int numColors = 8;
#else
	int numColors = 4;
#endif

	if(argc != 2)
	{
		Con_Printf("Usage: %s (color)\n", argv[0]);
		Con_Printf("Color #%i uses the player number as color.\n",
			numColors);
		return true;
	}
	cfg.netColor = atoi(argv[1]);
	if(IS_SERVER) // Player zero?
	{
		if(IS_DEDICATED) return false;

		// The server player, plr#0, must be treated as a special case
		// because this is a local mobj we're dealing with. We'll change 
		// the color translation bits directly.

		cfg.PlayerColor[0] = PLR_COLOR(0, cfg.netColor);
#ifdef __JDOOM__
		ST_updateGraphics();	
#endif
		// Change the color of the mobj (translation flags).
		players[0].plr->mo->flags &= ~MF_TRANSLATION;
#if __JHEXEN__
		// Additional difficulty is caused by the fact that the Fighter's
		// colors 0 (blue) and 2 (yellow) must be swapped.
		players[0].plr->mo->flags |= (cfg.PlayerClass[0] == PCLASS_FIGHTER?
			(cfg.PlayerColor[0]==0? 2 : cfg.PlayerColor[0]==2? 0 
			: cfg.PlayerColor[0]) : cfg.PlayerColor[0]) << MF_TRANSSHIFT;
		players[0].colormap = cfg.PlayerColor[0];
#else
		players[0].plr->mo->flags |= cfg.PlayerColor[0] << MF_TRANSSHIFT;
#endif

		// Tell the clients about the change.
		NetSv_SendPlayerInfo(0, DDSP_ALL_PLAYERS);
	}
	else
	{
		// Tell the server about the change.
		NetCl_SendPlayerInfo();
	}
	return true;
}

#if __JHEXEN__
int CCmdSetClass(int argc, char **argv)
{
	if(argc != 2)
	{
		Con_Printf( "Usage: %s (0-2)\n", argv[0]);
		return true;
	}
	cfg.netClass = atoi(argv[1]);
	if(cfg.netClass < 0) cfg.netClass = 0;
	if(cfg.netClass > 2) cfg.netClass = 2;
	if(IS_CLIENT)
	{
		// Tell the server that we've changed our class.
		NetCl_SendPlayerInfo();
	}
	else if(IS_DEDICATED) 
	{
		return false;
	}
	else
	{
		SB_ChangePlayerClass(players + consoleplayer, cfg.netClass);
	}
	return true;
}
#endif

int CCmdSetMap(int argc, char **argv)
{
	int ep, map;

	// Only the server can change the map.
	if(!IS_SERVER) return false;
#if !__JHEXEN__
	if(argc != 3)
	{
		Con_Printf("Usage: %s (episode) (map)\n", argv[0]);
		return true;
	}
#else
	if(argc != 2)
	{
		Con_Printf("Usage: %s (map)\n", argv[0]);
		return true;
	}
#endif

	// Update game mode.	
	deathmatch = cfg.netDeathmatch;
	nomonsters = cfg.netNomonsters;

#if !__JHEXEN__
	respawnparm = cfg.netRespawn;
	cfg.jumpEnabled = cfg.netJumping;
	ep = atoi(argv[1]);
	map = atoi(argv[2]);
#else
	randomclass = cfg.netRandomclass;
	ep = 1;
	map = P_TranslateMap(atoi(argv[1]));
#endif

	G_DeferedInitNew(gameskill, ep, map);	
	return true;
}

//===========================================================================
// D_ChatSound
//===========================================================================
void D_ChatSound(void)
{
#if __JHEXEN__
	S_LocalSound(SFX_CHAT, NULL);
#else
	S_LocalSound(sfx_chat, NULL);
#endif
}

//===========================================================================
// D_NetMessageEx
//	Show a message on screen, accompanied with a Chat sound effect.
//===========================================================================
void D_NetMessageEx(char *msg, boolean play_sound)
{
	strcpy(msgBuff, msg);
	// This is intended to be a local message. Let's make sure P_SetMessage
	// doesn't forward it anywhere.
	netSvAllowSendMsg = false;
#if __JDOOM__
	P_SetMessage(players + consoleplayer, msgBuff);
#else
	MN_TextFilter(msgBuff);
	P_SetMessage(players + consoleplayer, msgBuff, true);
#endif
	if(play_sound) D_ChatSound();
	netSvAllowSendMsg = true;
}

//===========================================================================
// D_NetMessage
//	Show message on screen, play chat sound.
//===========================================================================
void D_NetMessage(char *msg)
{
	D_NetMessageEx(msg, true);
}

//===========================================================================
// D_NetMessageNoSound
//	Show message on screen, play chat sound.
//===========================================================================
void D_NetMessageNoSound(char *msg)
{
	D_NetMessageEx(msg, false);
}

int D_NetServerClose(int before)
{
	if(!before)
	{
		// Restore normal game state.
		deathmatch = false;
		nomonsters = false;
#if __JHEXEN__
		randomclass = false;
#endif

#if __JDOOM__
		D_NetMessage("NETGAME ENDS");
#endif

#if __JHERETIC__
		P_SetMessage(&players[consoleplayer], "NETGAME ENDS", true);
		S_StartSound(sfx_dorcls, NULL);
#endif

#if __JHEXEN__
		P_SetMessage(&players[consoleplayer], "NETGAME ENDS", true);
		S_StartSound(SFX_DOOR_METAL_CLOSE, NULL);
#endif
	}
	return true;
}

int D_NetServerStarted(int before)
{
	int netMap;

	if(before) return true;

	G_StopDemo();

	// We're the server, so...
	cfg.PlayerColor[0] = PLR_COLOR(0, cfg.netColor);
#if __JHEXEN__
	cfg.PlayerClass[0] = cfg.netClass;
#endif
	
	// Set the game parameters.
	deathmatch = cfg.netDeathmatch;
	nomonsters = cfg.netNomonsters;
#if !__JHEXEN__
	respawnparm = cfg.netRespawn;
	cfg.jumpEnabled = cfg.netJumping;
#else
	randomclass = cfg.netRandomclass;
#endif

#ifdef __JDOOM__
	ST_updateGraphics();
#endif

	// Hexen has translated map numbers.
#ifdef __JHEXEN__
	netMap = P_TranslateMap(cfg.netMap);
#else
	netMap = cfg.netMap;
#endif
	G_InitNew(cfg.netSkill, cfg.netEpisode, netMap);

	// Close the menu, the game begins!!
#ifdef __JDOOM__
	M_ClearMenus();
#else
	MN_DeactivateMenu();
#endif
	return true;
}

int D_NetConnect(int before)
{
	// We do nothing before the actual connection is made.
	if(before) return true;
	
	// After connecting we tell the server a bit about ourselves.
	NetCl_SendPlayerInfo();

	// Close the menu, the game begins!!
//	advancedemo = false;
#ifdef __JDOOM__
	M_ClearMenus();
#else
	MN_DeactivateMenu();
#endif
	return true;
}

int D_NetDisconnect(int before)
{
	if(before) return true;

	// Restore normal game state.
	deathmatch = false;
	nomonsters = false;
#if __JHEXEN__
	randomclass = false;
#endif

	// Start demo.
	G_StartTitle();
	return true;
}

int D_NetPlayerEvent(int plrNumber, int peType, void *data)
{
	// Kludge: To preserve the ABI, these are done through player events.
	// (They are, sort of.)
	if(peType == DDPE_WRITE_COMMANDS)
	{
		// It's time to send ticcmds to the server.
		// 'plrNumber' contains the number of commands.
		return (int) NetCl_WriteCommands(data, plrNumber);
	}
	else if(peType == DDPE_READ_COMMANDS)
	{
		// Read ticcmds sent by a client.
		// 'plrNumber' is the length of the packet.
		return (int) NetSv_ReadCommands(data, plrNumber);
	}
	
	// If this isn't a netgame, we won't react.
	if(!IS_NETGAME) return true;
	
	if(peType == DDPE_ARRIVAL)
	{
		boolean showmsg = true;
		if(IS_SERVER)
		{
			NetSv_NewPlayerEnters(plrNumber);
		}
		else if(plrNumber == consoleplayer)
		{
			// We have arrived, the game should be begun.
			Con_Message("PE: (client) arrived in netgame.\n");
			gamestate = GS_WAITING;
			showmsg = false;
		}
		else 
		{
			// Client responds to new player?			
			Con_Message("PE: (client) player %i has arrived.\n", plrNumber);
			G_DoReborn(plrNumber);
			//players[plrNumber].playerstate = PST_REBORN;
		}
		if(showmsg)
		{
			// Print a notification.
			sprintf(msgBuff, "%s joined the game", 
				Net_GetPlayerName(plrNumber));
			D_NetMessage(msgBuff);
		}
	}
	else if(peType == DDPE_EXIT)
	{
		Con_Message("PE: player %i has left.\n", plrNumber);
		// Spawn a teleport fog.
		/*P_SpawnTeleFog(players[plrNumber].plr->mo->x,
			players[plrNumber].plr->mo->y);
		// Let's get rid of the mobj.
		P_RemoveMobj(players[plrNumber].plr->mo);
		players[plrNumber].plr->mo = NULL;
		players[plrNumber].playerstate = PST_REBORN;*/

		players[plrNumber].playerstate = PST_GONE;

		// Print a notification.
		sprintf(msgBuff, "%s left the game", Net_GetPlayerName(plrNumber));
		D_NetMessage(msgBuff);

		if(IS_SERVER) P_DealPlayerStarts();
	}
	// DDPE_CHAT_MESSAGE occurs when a pkt_chat is received. 
	// Here we will only display the message (if not a local message).
	else if(peType == DDPE_CHAT_MESSAGE 
		&& plrNumber != consoleplayer)
	{
		int i, num, oldecho = cfg.echoMsg;
		// Count the number of players.
		for(i = num = 0; i < MAXPLAYERS; i++)
			if(players[i].plr->ingame) num++;
		// If there are more than two players, include the name of
		// the player who sent this.
		if(num > 2)
			sprintf(msgBuff, "%s: %s", Net_GetPlayerName(plrNumber),
					(const char*)data);
		else
			strcpy(msgBuff, data);

		// The chat message is already echoed by the console.
		cfg.echoMsg = false;
		D_NetMessage(msgBuff);
		cfg.echoMsg = oldecho;
	}
	return true;
}

int D_NetWorldEvent(int type, int parm, void *data)
{
	//short *ptr;
	int i;
	//int mtype, flags;
	//fixed_t x, y, z, momx, momy, momz;

	switch(type)
	{
	//
	// Server events:
	//
	case DDWE_HANDSHAKE: 
		// A new player is entering the game. We as a server should send him
		// the handshake packet(s) to update his world.
		// If 'data' is zero, this is a re-handshake that's used to 
		// begin demos.
		Con_Message("D_NetWorldEvent: Sending a %shandshake to player %i.\n", 
			data? "" : "(re)", parm);
				
		// Mark new player for update.
		players[parm].update |= PSF_REBORN;

		// First, the game state.
		NetSv_SendGameState(GSF_CHANGE_MAP | GSF_CAMERA_INIT
			| (data? 0 : GSF_DEMO), parm);
	
		// Send info about all players to the new one.
		for(i = 0; i < MAXPLAYERS; i++)
			if(players[i].plr->ingame && i != parm)
				NetSv_SendPlayerInfo(i, parm);

		// Send info about our jump power.
		NetSv_SendJumpPower(parm, cfg.jumpEnabled? cfg.jumpPower : 0);
		break;

	//
	// Client events:
	//
#if 0
	case DDWE_PROJECTILE:
#ifdef _DEBUG
		if(parm > 32) // Too many?
			gi.Error("D_NetWorldEvent: Too many missiles (%i).\n", parm);
#endif
		// Projectile data consists of shorts.
		for(ptr=*(short**)data, i=0; i<parm; i++)
		{
			flags = *(unsigned short*) ptr & DDMS_FLAG_MASK;
			mtype = *(unsigned short*) ptr & ~DDMS_FLAG_MASK;
			ptr++;
			x = *ptr++ << 16;
			y = *ptr++ << 16;
			z = *ptr++ << 16;
			momx = momy = momz = 0;
			if(flags & DDMS_MOVEMENT_XY)
			{
				momx = *ptr++ << 8;
				momy = *ptr++ << 8;
			}
			if(flags & DDMS_MOVEMENT_Z)
			{
				momz = *ptr++ << 8;
			}
			NetCl_SpawnMissile(mtype, x, y, z, momx, momy, momz);
		}
		// Update pointer.
		*(short**)data = ptr;
		break;
#endif

	case DDWE_SECTOR_SOUND:
		// High word: sector number, low word: sound id.
		if(parm & 0xffff)
			S_StartSound(parm & 0xffff, (mobj_t*) &sectors[parm >> 16].soundorg);
		else
			S_StopSound(0, (mobj_t*) &sectors[parm >> 16].soundorg);
		break;

	case DDWE_DEMO_END:
		// Demo playback has ended. Advance demo sequence.
		if(parm) // Playback was aborted.
			G_DemoAborted();
		else // Playback ended normally.
			G_DemoEnds();

		// Restore normal game state.
		deathmatch = false;
		nomonsters = false;
#if !__JHEXEN__
		respawnparm = false;
#else
		randomclass = false;
#endif
		break;

	default:
		return false;
	}
	return true;		
}

void D_HandlePacket(int fromplayer, int type, void *data, int length)
{
	byte *bData = data;

	//
	// Server events.
	//
	if(IS_SERVER)
	{
		switch(type)
		{
		case GPT_PLAYER_INFO:
			// A player has changed color or other settings.
			NetSv_ChangePlayerInfo(fromplayer, data);
			break;

		case GPT_CHEAT_REQUEST:
			NetSv_DoCheat(fromplayer, data);
			break;
		}
		return;
	}
	//
	// Client events.
	//
	switch(type) 
	{
	case GPT_GAME_STATE:
		NetCl_UpdateGameState(data);

		// Tell the engine we're ready to proceed. It'll start handling
		// the world updates after this variable is set.
		Set(DD_GAME_READY, true);
		break;

	case GPT_MESSAGE:
#ifdef __JDOOM__
		strcpy(msgBuff, data);
		P_SetMessage(&players[consoleplayer], msgBuff);
#else
		P_SetMessage(&players[consoleplayer], data, true);
#endif
		break;

#ifdef __JHEXEN__
	case GPT_YELLOW_MESSAGE:
		P_SetYellowMessage(&players[consoleplayer], data, true);
		break;
#endif

	case GPT_CONSOLEPLAYER_STATE:
		NetCl_UpdatePlayerState(data, consoleplayer);
		break;

	case GPT_CONSOLEPLAYER_STATE2:
		NetCl_UpdatePlayerState2(data, consoleplayer);
		break;

	case GPT_PLAYER_STATE:
		NetCl_UpdatePlayerState(bData+1, bData[0]);
		break;

	case GPT_PLAYER_STATE2:
		NetCl_UpdatePlayerState2(bData+1, bData[0]);
		break;

	case GPT_PSPRITE_STATE:
		NetCl_UpdatePSpriteState(data);
		break;

	case GPT_INTERMISSION:
		NetCl_Intermission(data);
		break;

	case GPT_FINALE:
	case GPT_FINALE2:
		NetCl_Finale(type, data);
		break;

	case GPT_PLAYER_INFO:
		NetCl_UpdatePlayerInfo(data);
		break;

#if __JHEXEN__
	case GPT_CLASS:
		players[consoleplayer].class = bData[0];
		break;
#endif

	case GPT_SAVE:
		NetCl_SaveGame(data);
		break;

	case GPT_LOAD:
		NetCl_LoadGame(data);
		break;

	case GPT_PAUSE:
		NetCl_Paused(bData[0]);
		break;

	case GPT_JUMP_POWER:
		NetCl_UpdateJumpPower(data);
		break;

	default:
		Con_Message("H_HandlePacket: Received unknown packet, "
			"type=%i.\n", type);
	}
}

//===========================================================================

ccmd_t netCCmds[] =
{
	{ "setcolor",	CCmdSetColor,	"Set player color." },
	{ "setmap",		CCmdSetMap,		"Set map." },
#if __JHEXEN__
	{ "setclass",	CCmdSetClass,	"Set player class." },
#endif
	{ "startcycle",	CCmdMapCycle,	"Begin map rotation." },
	{ "endcycle",	CCmdMapCycle,	"End map rotation." },
	{ NULL }
};

cvar_t netCVars[] =
{
	"MapCycle",	CVF_HIDE|CVF_NO_ARCHIVE, CVT_CHARPTR, &mapCycle, 0, 0, "Map rotation sequence.",
	
	"server-game-mapcycle",	0, CVT_CHARPTR, &mapCycle, 0, 0, "Map rotation sequence.",
	"server-game-mapcycle-noexit", 0, CVT_BYTE, &mapCycleNoExit, 0, 1, "1=Disable exit buttons during map rotation.",
	"server-game-cheat", 0, CVT_INT, &netSvAllowCheats, 0, 1, "1=Allow cheating in multiplayer games (god, noclip, give).",
	NULL
};

//===========================================================================
// D_NetConsoleRegistration
//	Register the console commands and variables of the common netcode.
//===========================================================================
void D_NetConsoleRegistration(void)
{
	int i;

	for(i = 0; netCCmds[i].name; i++) Con_AddCommand(netCCmds + i);
	for(i = 0; netCVars[i].name; i++) Con_AddVariable(netCVars + i);
}

