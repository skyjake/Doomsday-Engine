
//**************************************************************************
//**
//** H2_NET.C ** JHexen networking
//** -Jaakko Keränen
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "H2def.h"
#include "P_local.h"
#include "Settings.h"
#include "H2_net.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// servdata_t can be *max* 12 bytes long (3 ints).
typedef struct
{
	unsigned char	nomonsters : 1;
	unsigned char	respawn : 1;
	unsigned char	randclass : 1;
	unsigned char	skill : 4;
	unsigned char	map : 7;
	unsigned char	deathmatch : 1;
	unsigned char	slot : 3;
    /*float           nmdamage; // netMobDamageModifier
    float           nmhealth; // netMobHealthModifier*/
	unsigned char	nmdamage, nmhealth; // netMobDamage/HealthModifier
} servdata_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void G_CheckDemoStatus();

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern boolean advancedemo;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void UpdateServerData(int set)
{
	servdata_t svd;

	if(set)
	{
		// Pack the data into the server data word.
		svd.deathmatch = netDeathmatch;
		svd.map = netMap;
		svd.nomonsters = netNomonsters;
		svd.randclass = netRandomclass;
		svd.respawn = netRespawn;
		svd.skill = netSkill;
		svd.slot = netSlot;
        svd.nmdamage = netMobDamageModifier;
        svd.nmhealth = netMobHealthModifier;
		gi.NetSetServerData(&svd, sizeof(svd));
	}
	else
	{
		gi.NetGetServerData(&svd, sizeof(svd));
		// Unpack the data.
		netDeathmatch = svd.deathmatch;
		netMap = svd.map;
		netNomonsters = svd.nomonsters;
		netRandomclass = svd.randclass;
		netRespawn = svd.respawn;
		netSkill = svd.skill;
		netSlot = svd.slot;
        netMobDamageModifier = svd.nmdamage; 
        netMobHealthModifier = svd.nmhealth; 
	}
}

int H2_NetServerOpen(int before)
{
	if(before) 
		UpdateServerData(true);
	else
	{
		plrdata_t pd = { netClass, netColor };
		gi.NetSetPlayerData(&pd, sizeof(pd));
	}
	return true;
}

int H2_NetServerClose(int before)
{
	if(!before)
	{
		// Restore normal game state.
		deathmatch = false;
		nomonsters = false;

		P_SetMessage(&players[consoleplayer], "NETGAME ENDS", true);
		S_StartSound(NULL, SFX_DOOR_LIGHT_CLOSE);
	}
	return true;
}

int H2_NetServerStarted(int before)
{
	int			i, server = gi.Get(DD_SERVER);
	plrdata_t	pd;

	if(before)
	{
		// If we are the server, we must update the game data.
		if(server) UpdateServerData(true);
		
		// Update our player data, for the last time.
		pd.class = netClass;
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
			PlayerClass[i] = pd.class;
			PlayerColor[i] = pd.color;
		}
	}

	// Set the game parameters.
	deathmatch = netDeathmatch;
	nomonsters = netNomonsters;
	randomclass = netRandomclass;
	respawnparm = netRespawn;

	G_CheckDemoStatus();
	advancedemo = false;

	// Start the game.	
	if(!netSlot)
	{
		G_StartNewInit();
		G_InitNew(netSkill, 1, P_TranslateMap(netMap));
	}
	else
	{
		// Load the game from the slot.
		G_LoadGame(netSlot-1);
	}
	// Close the menu, the game begins!!
	MN_DeactivateMenu();
	return true;
}

int H2_NetConnect(int before)
{
	plrdata_t	pd = { netClass, netColor };

	// We do nothing before the actual connection is made.
	if(before) return true;
	
	// After connecting we get the game data from the server.
	UpdateServerData(false);

	// Set the player data.
	gi.NetSetPlayerData(&pd, sizeof(pd));

	// Some diagnostics.
	gi.Message("Netgame: map%i, skill%i, dm%i, rs%i, rnd%i, !mnst%i, slot%i,\nnmDamage%i, nmHealth%i\n",
		netMap, netSkill, netDeathmatch, netRespawn, netRandomclass, netNomonsters, 
		netSlot, netMobDamageModifier, netMobHealthModifier);
	return true;
}

int H2_NetDisconnect(int before)
{
	if(before) return true;

	// Restore normal game state.
	deathmatch = false;
	nomonsters = false;
	return true;
}

int H2_NetPlayerEvent(int plrNumber, int peType, void *data)
{
	char	msgbuff[256];

	// If this isn't a netgame, we won't react.
	if(!gi.Get(DD_NETGAME)) return true;

	if(peType == DDPE_EXIT)
	{
		// A player has exited a netgame?
		strcpy(msgbuff, "PLAYER 1 LEFT THE GAME");
		msgbuff[7] += plrNumber;
		P_SetMessage(&players[consoleplayer], msgbuff, true);
		S_StartSound(NULL, SFX_CHAT);
	}
	// Don't show local chat messages.
	else if(peType == DDPE_CHAT_MESSAGE && plrNumber != consoleplayer)
	{
		int i, num, oldecho = echoMsg;
		// Count the number of players.
		for(i=num=0; i<MAXPLAYERS; i++)
			if(players[i].plr->ingame) num++;
		// If there are more than two players, include the name of
		// the player who sent this.
		if(num > 2)
			sprintf(msgbuff, "%s: %s", gi.GetPlayerName(plrNumber), data);
		else
			strcpy(msgbuff, data);
		MN_TextFilter(msgbuff);
		// The chat message is already echoed by the console.
		echoMsg = false;
		P_SetMessage(&players[consoleplayer], msgbuff, true);
		echoMsg = oldecho;
		S_StartSound(NULL, SFX_CHAT);
	}
	return true;
}

