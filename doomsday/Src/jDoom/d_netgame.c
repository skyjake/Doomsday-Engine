#include "doomdef.h"
#include "doomstat.h"
#include "p_local.h"
#include "d_netgame.h"
#include "d_config.h"
#include "s_sound.h"
#include "sounds.h"
#include "g_game.h"
#include "dstrings.h"
#include "st_stuff.h"
#include "hu_stuff.h"

// TYPES --------------------------------------------------------------------

// servdata_t can be *max* 12 bytes long (3 ints).
typedef struct
{
	unsigned char	nomonsters : 1;
	unsigned char	respawn : 1;
	unsigned char	skill : 4;
	unsigned char	map : 5;
	unsigned char	episode : 3;
	unsigned char	deathmatch : 2;
	unsigned char	slot : 3;
	unsigned char	jumping : 1;
} servdata_t;

// EXTERNAL FUNCTIONS -------------------------------------------------------

void M_ClearMenus (void);

// EXTERNAL DATA ------------------------------------------------------------

extern boolean advancedemo;

// PUBLIC DATA --------------------------------------------------------------

/*boolean		netDeathmatch = false, netNomonsters = false, netRespawn = false;
boolean		netJumping = true;
byte		netEpisode = 1, netMap = 1, netSkill = sk_medium, netSlot = 0;
byte		netColor = 0;

int			PlayerColor[MAXPLAYERS];*/

// CODE ---------------------------------------------------------------------

/*void UpdateServerData(int set)
{
	servdata_t svd;

	if(set)
	{
		// Pack the data into the server data word.
		svd.deathmatch = netDeathmatch;
		svd.map = netMap;
		svd.episode = netEpisode;
		svd.nomonsters = netNomonsters;
		svd.respawn = netRespawn;
		svd.skill = netSkill;
		svd.slot = netSlot;
		svd.jumping = netJumping;
		gi.NetSetServerData(&svd, sizeof(svd));
	}
	else
	{
		gi.NetGetServerData(&svd, sizeof(svd));
		// Unpack the data.
		netDeathmatch = svd.deathmatch;
		netMap = svd.map;
		netEpisode = svd.episode;
		netNomonsters = svd.nomonsters;
		netRespawn = svd.respawn;
		netSkill = svd.skill;
		netSlot = svd.slot;
		netJumping = svd.jumping;
	}
}
*/
int D_NetServerOpen(int before)
{
/*	if(before) 
		UpdateServerData(true);
	else
	{
		plrdata_t pd = { netColor };
		gi.NetSetPlayerData(&pd, sizeof(pd));
	}*/
	return true;
}

int D_NetServerClose(int before)
{
/*	if(!before)
	{
		// Restore normal game state.
		deathmatch = false;
		nomonsters = false;

		//P_SetMessage(&players[consoleplayer], "NETGAME ENDS", true);
		players[consoleplayer].message = "NETGAME ENDS";	
		S_StartSound(NULL, sfx_dorcls);
	}*/
	return true;
}

int D_NetServerStarted(int before)
{
	/*int			i, server = gi.Get(DD_SERVER);
	plrdata_t	pd;

	if(before)
	{
		// If we are the server, we must update the game data.
		if(server) UpdateServerData(true);
		
		// Update our player data, for the last time.
		pd.color = netColor;
		gi.NetSetPlayerData(&pd, sizeof(pd));
		return true;
	}
	// For clients: read the final server data.
	if(!server)	UpdateServerData(false);

	// Let's read the data of each player.
	for(i=0; i<MAXPLAYERS; i++)
	{
		if(players[i].plr->ingame)
		{
			gi.NetGetPlayerData(i, &pd, sizeof(pd));
			PlayerColor[i] = pd.color;
		}
	}

	G_CheckDemoStatus();
	advancedemo = false;

	// Set the game parameters.
	deathmatch = netDeathmatch;
	nomonsters = netNomonsters;
	respawnparm = netRespawn;
	cfg.jumpEnabled = netJumping;

	// Start the game.	
	if(!netSlot)
	{
		// A defered init would be cleaner... but we need to get this 
		// started *NOW*.
		G_InitNew(netSkill, netEpisode, netMap);
	}
	else
	{
		char filename[16];
		sprintf(filename, SAVEGAMENAME"%c.dsg", netSlot-1);
		// Load the game from the slot.
		G_LoadGame(filename);
	}
	ST_updateGraphics(); // For status bar face background.
	// Close the menu, the game begins!!
	M_ClearMenus();*/
	return true;
}

int D_NetConnect(int before)
{
/*	plrdata_t	pd = { netColor };

	// We do nothing before the actual connection is made.
	if(before) return true;
	
	// After connecting we get the game data from the server.
	UpdateServerData(false);

	// Set the player data.
	gi.NetSetPlayerData(&pd, sizeof(pd));

	// Some diagnostics.
	gi.Message("Netgame: map%i, episode%i, skill%i, dm%i, rs%i, !mnst%i, slot%i, jump%i\n",
		netMap, netEpisode, netSkill, netDeathmatch, netRespawn, netNomonsters, netSlot, netJumping);
*/
	return true;
}

int D_NetDisconnect(int before)
{
/*	if(before) return true;

	// Restore normal game state.
	deathmatch = false;
	nomonsters = false;*/
	return true;
}

char	msgbuff[256];

int D_NetPlayerEvent(int plrNumber, int peType, void *data)
{
/*	// If this isn't a netgame, we won't react.
	if(!gi.Get(DD_NETGAME)) return true;

	if(peType == DDPE_EXIT)
	{
		// A player has exited a netgame?
		strcpy(msgbuff, "PLAYER 1 LEFT THE GAME");
		msgbuff[7] += plrNumber;
		//P_SetMessage(&players[consoleplayer], msgbuff, true);
		players[consoleplayer].message = msgbuff;
		S_StartSound(NULL, sfx_itemup);
	}
	// Don't show local chat messages.
	else if(peType == DDPE_CHAT_MESSAGE)// && plrNumber != consoleplayer)
	{
		int i, num;
		// Count the number of players.
		for(i=num=0; i<MAXPLAYERS; i++)
			if(players[i].plr->ingame) num++;
		// If there are more than two players, include the name of
		// the player who sent this.
		if(num > 2)
			sprintf(msgbuff, "%s: %s", gi.GetPlayerName(plrNumber), data);
		else
			strcpy(msgbuff, data);
		strupr(msgbuff);
		// The chat message is already echoed by the console.
		message_noecho = true;
		players[consoleplayer].message = msgbuff;
		S_StartSound(0, gamemode == commercial? sfx_radio : sfx_tink);
	}*/
	return true;
}
