// Multiplayer Menu (for jDoom, jHeretic and jHexen)
// Contains an extension for edit fields.

// TODO: Remove unnecessary SC* declarations and other unused code.

#if defined __JDOOM__
#include "doomdef.h"
#include "doomstat.h"
#include "m_menu.h"
#include "d_config.h"
#include "hu_stuff.h"
#include "m_misc.h"
#include "m_random.h"
#include "s_sound.h"
#include "dstrings.h"
#include "p_local.h"
#include "Mn_def.h"
#elif defined __JHERETIC__
#include "Doomdef.h"
#include "settings.h"
#include "Soundst.h"
#include "P_local.h"
#include "Mn_def.h"
//#include "s_sound.h"
//#include "Dstrings.h"
#elif defined __JHEXEN__
#include "h2def.h"
#include "settings.h"
#include "soundst.h"
#include "p_local.h"
#include "mn_def.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>

// Macros -----------------------------------------------------------------

#if defined(__JHERETIC__) || defined(__JHEXEN__)
#define currentMenu CurrentMenu
#endif

#define MAX_EDIT_LEN		256
#define SLOT_WIDTH			200

#define IS_SERVER			Get(DD_SERVER)
#define IS_NETGAME			Get(DD_NETGAME)

// Types ------------------------------------------------------------------

typedef struct
{
	char	text[MAX_EDIT_LEN];
	char	oldtext[MAX_EDIT_LEN];	// If the current edit is canceled...
	int		firstVisible;
} EditField_t;

// Functions prototypes ---------------------------------------------------

void DrawMultiplayerMenu(void);
void DrawGameSetupMenu(void);
void DrawPlayerSetupMenu(void);

boolean SCEnterHostMenu(int option);
boolean SCEnterJoinMenu(int option);
boolean SCEnterGameSetup(int option);
boolean SCSetProtocol(int option);
boolean SCGameSetupFunc(int option);
boolean SCGameSetupEpisode(int option);
boolean SCGameSetupMission(int option);
boolean SCGameSetupSkill(int option);
boolean SCGameSetupDeathmatch(int option);
boolean SCGameSetupDamageMod(int option);
boolean SCGameSetupHealthMod(int option);
boolean SCOpenServer(int option);
boolean SCCloseServer(int option);
boolean SCChooseHost(int option);
boolean SCStartStopDisconnect(int option);
boolean SCEnterPlayerSetupMenu(int option);
void SCPlayerClass(int option);
boolean SCPlayerColor(int option);
boolean SCAcceptPlayer(int option);

void ResetJoinMenuItems();

// Edit fields.
void DrawEditField(Menu_t *menu, int index, EditField_t *ef);
boolean SCEditField(int efptr);

// Private data -----------------------------------------------------------

static EditField_t *ActiveEdit = NULL;	// No active edit field by default.
static char shiftTable[59] =	// Contains characters 32 to 90.
{
/* 32 */	0, 0, 0, 0, 0, 0, 0, '"',
/* 40 */	0, 0, 0, 0, '<', '_', '>', '?', ')', '!',
/* 50 */	'@', '#', '$', '%', '^', '&', '*', '(', 0, ':',
/* 60 */	0, '+', 0, 0, 0, 0, 0, 0, 0, 0,
/* 70 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 80 */	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 90 */	0
};

static EditField_t plrNameEd;
static int plrColor;

#ifdef __JHEXEN__
static int plrClass;
#endif

// Menus ------------------------------------------------------------------

#ifdef __JDOOM__
	#define EXTRADEF
#else
	#define EXTRADEF	,MENU_NONE
#endif

MenuItem_t MultiplayerItems[] =
{
	{ ITT_EFUNC, "player setup", SCEnterPlayerSetupMenu, 0 EXTRADEF },
	{ ITT_EFUNC, "join game", SCEnterJoinMenu, 0 EXTRADEF },
	{ ITT_EFUNC, "host game", SCEnterHostMenu, 0 EXTRADEF },
};

MenuItem_t MultiplayerServerItems[] =
{
	{ ITT_EFUNC, "player setup", SCEnterPlayerSetupMenu, 0 EXTRADEF },
	{ ITT_EFUNC, "game setup",	SCEnterHostMenu, 0 EXTRADEF },
	{ ITT_EFUNC, "close server", SCCloseServer, 0 EXTRADEF }
};

MenuItem_t MultiplayerClientItems[] =
{
	{ ITT_EFUNC, "player setup", SCEnterPlayerSetupMenu, 0 EXTRADEF },
	{ ITT_EFUNC, "disconnect",	SCEnterJoinMenu, 0 EXTRADEF }
};

Menu_t MultiplayerMenu =
{
	116, 70,
	DrawMultiplayerMenu,
	3, MultiplayerItems,
	0, MENU_MAIN,
#ifdef __JDOOM__
	hu_font_a,
	LINEHEIGHT_A,
#else 
	MN_DrTextA_CS, 10, 
#endif
	0, 3
};

#ifdef __JHEXEN__

#define NUM_GAMESETUP_ITEMS		9

MenuItem_t GameSetupItems1[] = 
{
	{ ITT_LRFUNC, "MAP:", SCGameSetupMission, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "SKILL:", SCGameSetupSkill, 0, MENU_NONE },
	{ ITT_EFUNC, "DEATHMATCH:", SCGameSetupFunc, (int) &cfg.netDeathmatch, MENU_NONE },
	{ ITT_EFUNC, "MONSTERS:", SCGameSetupFunc, (int) &cfg.netNomonsters, MENU_NONE },
	{ ITT_EFUNC, "RANDOM CLASSES:", SCGameSetupFunc, (int) &cfg.netRandomclass, MENU_NONE },
	{ ITT_LRFUNC, "DAMAGE MOD:", SCGameSetupDamageMod, 0, MENU_NONE },
	{ ITT_LRFUNC, "HEALTH MOD:", SCGameSetupHealthMod, 0, MENU_NONE },
	{ ITT_EFUNC, "PROCEED...", SCOpenServer, 0, MENU_NONE }
};

Menu_t GameSetupMenu =
{
	90, 64,
	DrawGameSetupMenu,
	NUM_GAMESETUP_ITEMS, GameSetupItems1,
	0,
	MENU_MULTIPLAYER,
	MN_DrTextA_CS, 9,
	0, NUM_GAMESETUP_ITEMS
};

#else

#if __JHERETIC__

#define NUM_GAMESETUP_ITEMS		8

MenuItem_t GameSetupItems1[] = // for Heretic
{
	{ ITT_LRFUNC, "EPISODE :", SCGameSetupEpisode, 0 },
	{ ITT_LRFUNC, "MISSION :", SCGameSetupMission, 0 },
	{ ITT_LRFUNC, "SKILL :", SCGameSetupSkill, 0 },
	{ ITT_LRFUNC, "DEATHMATCH :", SCGameSetupDeathmatch, 0 },
	{ ITT_EFUNC, "MONSTERS :", SCGameSetupFunc, (int) &cfg.netNomonsters EXTRADEF },
	{ ITT_EFUNC, "RESPAWN MONSTERS :", SCGameSetupFunc, (int) &cfg.netRespawn EXTRADEF },
	{ ITT_EFUNC, "ALLOW JUMPING :", SCGameSetupFunc, (int) &cfg.netJumping EXTRADEF },
	{ ITT_EFUNC, "PROCEED...", SCOpenServer, 0 EXTRADEF }
};

#elif __JDOOM__

#define NUM_GAMESETUP_ITEMS		13

MenuItem_t GameSetupItems1[] = // for Doom 1
{
	{ ITT_LRFUNC, "EPISODE :", SCGameSetupEpisode, 0 },
	{ ITT_LRFUNC, "MISSION :", SCGameSetupMission, 0 },
	{ ITT_LRFUNC, "SKILL :", SCGameSetupSkill, 0 },
	{ ITT_LRFUNC, "MODE :", SCGameSetupDeathmatch, 0 },
	{ ITT_EFUNC, "MONSTERS :", SCGameSetupFunc, (int) &cfg.netNomonsters EXTRADEF },
	{ ITT_EFUNC, "RESPAWN MONSTERS :", SCGameSetupFunc, (int) &cfg.netRespawn EXTRADEF },
	{ ITT_EFUNC, "ALLOW JUMPING :", SCGameSetupFunc, (int) &cfg.netJumping EXTRADEF },
	{ ITT_EFUNC, "NO COOP DAMAGE :", SCGameSetupFunc, (int) &cfg.noCoopDamage EXTRADEF },
	{ ITT_EFUNC, "NO COOP WEAPONS :", SCGameSetupFunc, (int) &cfg.noCoopWeapons EXTRADEF },
	{ ITT_EFUNC, "NO COOP OBJECTS :", SCGameSetupFunc, (int) &cfg.noCoopAnything EXTRADEF },
	{ ITT_EFUNC, "NO BFG 9000 :", SCGameSetupFunc, (int) &cfg.noNetBFG EXTRADEF },
	{ ITT_EFUNC, "NO TEAM DAMAGE :", SCGameSetupFunc, (int) &cfg.noTeamDamage EXTRADEF },
	{ ITT_EFUNC, "PROCEED...", SCOpenServer, 0 EXTRADEF }
};

MenuItem_t GameSetupItems2[] = // for Doom 2
{
	{ ITT_LRFUNC, "LEVEL :", SCGameSetupMission, 0 },
	{ ITT_LRFUNC, "SKILL :", SCGameSetupSkill, 0 },
	{ ITT_LRFUNC, "MODE :", SCGameSetupDeathmatch, 0 },
	{ ITT_EFUNC, "MONSTERS :", SCGameSetupFunc, (int) &cfg.netNomonsters EXTRADEF },
	{ ITT_EFUNC, "RESPAWN MONSTERS :", SCGameSetupFunc, (int) &cfg.netRespawn EXTRADEF },
	{ ITT_EFUNC, "ALLOW JUMPING :", SCGameSetupFunc, (int) &cfg.netJumping EXTRADEF },
	{ ITT_EFUNC, "NO COOP DAMAGE :", SCGameSetupFunc, (int) &cfg.noCoopDamage EXTRADEF },
	{ ITT_EFUNC, "NO COOP WEAPONS :", SCGameSetupFunc, (int) &cfg.noCoopWeapons EXTRADEF },
	{ ITT_EFUNC, "NO COOP OBJECTS :", SCGameSetupFunc, (int) &cfg.noCoopAnything EXTRADEF },	
	{ ITT_EFUNC, "NO BFG 9000 :", SCGameSetupFunc, (int) &cfg.noNetBFG EXTRADEF },
	{ ITT_EFUNC, "NO TEAM DAMAGE :", SCGameSetupFunc, (int) &cfg.noTeamDamage EXTRADEF },
	{ ITT_EFUNC, "PROCEED...", SCOpenServer, 0 EXTRADEF }
};

#endif

Menu_t GameSetupMenu =
{
#ifdef __JDOOM__
	90, 54,
#elif defined __JHERETIC__
	74, 64,
#endif
	DrawGameSetupMenu,
	NUM_GAMESETUP_ITEMS, GameSetupItems1,
	0, MENU_MULTIPLAYER,
#ifdef __JDOOM__
	hu_font_a, //1, 0, 0, 
	LINEHEIGHT_A,
#elif defined __JHERETIC__
	MN_DrTextA_CS, 10,
#endif
	0, NUM_GAMESETUP_ITEMS
};

#endif // __JHEXEN__

#if __JHEXEN__
#define NUM_PLAYERSETUP_ITEMS	6
#else
#define NUM_PLAYERSETUP_ITEMS	6
#endif

MenuItem_t PlayerSetupItems[] =
{
	{ ITT_EFUNC, "", SCEditField, (int) &plrNameEd EXTRADEF },
	{ ITT_EMPTY, NULL, NULL, 0 },
#if __JHEXEN__
	{ ITT_LRFUNC, "Class:", SCPlayerClass, 0, MENU_NONE },
#else
	{ ITT_EMPTY, NULL, NULL, 0 },
#endif
	{ ITT_LRFUNC, "Color:", SCPlayerColor, 0 },
	{ ITT_EMPTY, NULL, NULL, 0 },
	{ ITT_EFUNC, "Accept Changes", SCAcceptPlayer, 0 EXTRADEF }
};

Menu_t PlayerSetupMenu =
{
	60, 52,
	DrawPlayerSetupMenu,
	NUM_PLAYERSETUP_ITEMS, PlayerSetupItems,
	0, MENU_MULTIPLAYER,
#if __JDOOM__
	hu_font_b, LINEHEIGHT_B,
#elif __JHERETIC__
	MN_DrTextB_CS, ITEM_HEIGHT,
#else // __JHEXEN__
	MN_DrTextB_CS, ITEM_HEIGHT,
#endif
	0, NUM_PLAYERSETUP_ITEMS
};

// Code -------------------------------------------------------------------

int Executef(int silent, char *format, ...)
{
	va_list argptr;
	char buffer[512];

	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);
	return Con_Execute(buffer, silent);
}

void Notify(char *msg)
{
#ifdef __JDOOM__
	if(msg) P_SetMessage(players + consoleplayer, msg);
	S_LocalSound(sfx_dorcls, NULL);
#endif

#ifdef __JHERETIC__
	if(msg) P_SetMessage(&players[consoleplayer], msg, true);
	S_LocalSound(sfx_chat, NULL);
#endif

#ifdef __JHEXEN__
	if(msg) P_SetMessage(&players[consoleplayer], msg, true);
	S_LocalSound(SFX_CHAT, NULL);
#endif
}

void DrANumber(int number, int x, int y)
{
	char buff[40];

	sprintf(buff, "%i", number);
#ifdef __JDOOM__
	M_WriteText2(x, y, buff, hu_font_a, 1, 1, 1);
#else
	MN_DrTextA_CS(buff, x, y);
#endif
}

void MN_DrCenterTextA_CS(char *text, int center_x, int y)
{
#ifdef __JDOOM__
	M_WriteText2(center_x - M_StringWidth(text, hu_font_a)/2, y,
		text, hu_font_a, 1, 0, 0);
#else
	MN_DrTextA_CS(text, center_x - MN_TextAWidth(text)/2, y);
#endif
}

void MN_DrCenterTextB_CS(char *text, int center_x, int y)
{
#ifdef __JDOOM__
	M_WriteText2(center_x - M_StringWidth(text, hu_font_b)/2, y,
		text, hu_font_b, 1, 0, 0);
#else
	MN_DrTextB_CS(text, center_x - MN_TextBWidth(text)/2, y);
#endif
}

#ifdef __JDOOM__
static void MN_DrTextA_CS(char *text, int x, int y)
{
	M_WriteText2(x, y, text, hu_font_a, 1, 0, 0);
}

static void MN_DrTextB_CS(char *text, int x, int y)
{
	M_WriteText2(x, y, text, hu_font_b, 1, 0, 0);
}

#else

void M_WriteMenuText(Menu_t *menu, int index, char *text)
{
	MN_DrawMenuText(menu, index, text);
}

void M_DrawTitle(char *text, int y)
{
	// For now we're ignoring the color.
	MN_DrawTitle(text, y);
}
#endif

//*****

void DrawMultiplayerMenu(void)
{
	M_DrawTitle("MULTIPLAYER", MultiplayerMenu.y-20);
}

void DrawGameSetupMenu(void)
{
	char	*boolText[2] = { "NO", "YES" }, buf[50];
	char	*skillText[5] = { "BABY", "EASY", "MEDIUM", "HARD", "NIGHTMARE" };
#if __JDOOM__
	char	*dmText[3] = { "COOPERATIVE", "DEATHMATCH 1", "DEATHMATCH 2" };
#else
	char	*dmText[3] = { "NO", "YES", "YES" };
#endif
	Menu_t	*menu = &GameSetupMenu;
	int		idx;
#if __JHEXEN__
	char	*mapName = P_GetMapName(P_TranslateMap(cfg.netMap));
#endif

	M_DrawTitle("GAME SETUP", menu->y-20);

	idx = 0;

#ifndef __JHEXEN__

#ifdef __JDOOM__
	if(gamemode != commercial)
#endif
	{
		sprintf(buf, "%i", cfg.netEpisode);
		M_WriteMenuText(menu, idx++, buf);
	}
	sprintf(buf, "%i", cfg.netMap);
	M_WriteMenuText(menu, idx++, buf);
	M_WriteMenuText(menu, idx++, skillText[cfg.netSkill]);
	M_WriteMenuText(menu, idx++, dmText[cfg.netDeathmatch]);
	M_WriteMenuText(menu, idx++, boolText[!cfg.netNomonsters]);
	M_WriteMenuText(menu, idx++, boolText[cfg.netRespawn]);
	M_WriteMenuText(menu, idx++, boolText[cfg.netJumping]);

#if __JDOOM__

	M_WriteMenuText(menu, idx++, boolText[cfg.noCoopDamage]);
	M_WriteMenuText(menu, idx++, boolText[cfg.noCoopWeapons]);
	M_WriteMenuText(menu, idx++, boolText[cfg.noCoopAnything]);
	M_WriteMenuText(menu, idx++, boolText[cfg.noNetBFG]);
	M_WriteMenuText(menu, idx++, boolText[cfg.noTeamDamage]);

#endif // __JDOOM__

#else // __JHEXEN__

	sprintf(buf, "%i", cfg.netMap);
	M_WriteMenuText(menu, 0, buf);
	MN_DrTextAYellow_CS(mapName, 160-MN_TextAWidth(mapName)/2, 
		menu->y + menu->itemHeight);
	M_WriteMenuText(menu, 2, skillText[cfg.netSkill]);
	M_WriteMenuText(menu, 3, dmText[cfg.netDeathmatch]);
	M_WriteMenuText(menu, 4, boolText[!cfg.netNomonsters]);
	M_WriteMenuText(menu, 5, boolText[cfg.netRandomclass]);
	sprintf(buf, "%i", cfg.netMobDamageModifier);
	M_WriteMenuText(menu, 6, buf);
	sprintf(buf, "%i", cfg.netMobHealthModifier);
	M_WriteMenuText(menu, 7, buf);

#endif // __JHEXEN__
}

int CurrentPlrFrame = 0;

// Finds the power of 2 that is equal to or greater than 
// the specified number.
static int CeilPow2(int num)
{
	int cumul;
	for(cumul=1; num > cumul; cumul <<= 1);
	return cumul;
}

void DrawPlayerSetupMenu(void)
{
	spriteinfo_t sprInfo;
	Menu_t *menu = &PlayerSetupMenu;
	int useColor = plrColor;
#ifdef __JHEXEN__
	int numColors = 8;
	int sprites[3] = { SPR_PLAY, SPR_CLER, SPR_MAGE };
#else
	int plrClass = 0;
	int numColors = 4;
	int sprites[3] = { SPR_PLAY, SPR_PLAY, SPR_PLAY };
#endif
	int alpha;

	M_DrawTitle("PLAYER SETUP", menu->y-20);

	// Get current alpha.
	gl.GetIntegerv(DGL_A, &alpha);

	DrawEditField(menu, 0, &plrNameEd);	

	if(useColor == numColors) useColor = MenuTime/5 % numColors;

	// Draw the color selection as a random player frame.
//#ifndef __JHEXEN__
//	gi.GetSpriteInfo(SPR_PLAY, CurrentPlrFrame, &sprInfo);
//#else
	
	R_GetSpriteInfo(sprites[plrClass], CurrentPlrFrame, &sprInfo);

#if __JHEXEN__
	if(plrClass == PCLASS_FIGHTER)
	{
		// Fighter's colors are a bit different.
		if(useColor == 0) useColor = 2; else if(useColor == 2) useColor = 0;
	}
#endif
	Set(DD_TRANSLATED_SPRITE_TEXTURE, DD_TSPR_PARM
		(sprInfo.lump, plrClass, useColor));
	
	GL_DrawRect(162-sprInfo.offset, 
#ifdef __JDOOM__
		menu->y+70 - sprInfo.topOffset, 
#elif defined __JHERETIC__
		menu->y+80 - sprInfo.topOffset,
#else
		menu->y+90 - sprInfo.topOffset,
#endif
		CeilPow2(sprInfo.width), 
		CeilPow2(sprInfo.height), 
		1, 1, 1, alpha/255.0f);

	if(plrColor == numColors)
	{
		gl.Color4f(1, 1, 1, alpha/255.0f);
		MN_DrTextA_CS("AUTOMATIC", 184, 
#ifdef __JDOOM__
			menu->y + 49);
#elif defined __JHERETIC__
			menu->y + 65);
#else
			menu->y + 64);
#endif
	}
}

boolean SCEnterMultiplayerMenu(int option)
{
	int count;

	// Choose the correct items for the Game Setup menu.
#ifdef __JDOOM__
	if(gamemode == commercial)
	{
		GameSetupMenu.items = GameSetupItems2;
		GameSetupMenu.itemCount = GameSetupMenu.numVisItems = 
			NUM_GAMESETUP_ITEMS - 1;
	}
	else
#endif
	{
		GameSetupMenu.items = GameSetupItems1;
		GameSetupMenu.itemCount 
			= GameSetupMenu.numVisItems 
			= NUM_GAMESETUP_ITEMS;
	}

	// Show the appropriate menu.
	if(IS_NETGAME)
	{
		MultiplayerMenu.items = IS_SERVER? MultiplayerServerItems 
			: MultiplayerClientItems;
		count = IS_SERVER? 3 : 2;
	}
	else
	{
		MultiplayerMenu.items = MultiplayerItems;
		count = 3;
	}
	MultiplayerMenu.itemCount = MultiplayerMenu.numVisItems = count;
#ifdef __JDOOM__
	MultiplayerMenu.lastOn = 0;
#else
	MultiplayerMenu.oldItPos = 0;
#endif

	SetMenu(MENU_MULTIPLAYER);
	return true;
}

boolean SCEnterHostMenu(int option)
{
	SCEnterGameSetup(0);
	return true;
}

boolean SCEnterJoinMenu(int option)
{
	if(IS_NETGAME)
	{
		Con_Execute("net disconnect", false);
#ifdef __JDOOM__
		M_ClearMenus();
#else
		MN_DeactivateMenu();
#endif
		return true;
	}
	Con_Execute("net setup client", false);
	return true;
}

boolean SCEnterGameSetup(int option)
{
	// See to it that the episode and mission numbers are correct.
#ifdef __JDOOM__
	if(gamemode == commercial)
	{
		cfg.netEpisode = 1;
	}
	else if(gamemode == retail)
	{
		if(cfg.netEpisode > 4) cfg.netEpisode = 4;
		if(cfg.netMap > 9) cfg.netMap = 9;
	}
	else if(gamemode == registered)
	{
		if(cfg.netEpisode > 3) cfg.netEpisode = 3;
		if(cfg.netMap > 9) cfg.netMap = 9;
	}
	else if(gamemode == shareware)
	{
		cfg.netEpisode = 1;
		if(cfg.netMap > 9) cfg.netMap = 9;
	}
#elif defined __JHERETIC__
	if(cfg.netMap > 9) cfg.netMap = 9;
	if(cfg.netEpisode > 6) cfg.netEpisode = 6;
	if(cfg.netEpisode == 6 && cfg.netMap > 3) cfg.netMap = 3;
#elif defined __JHEXEN__
	if(cfg.netMap < 1) cfg.netMap = 1;
	if(cfg.netMap > 31) cfg.netMap = 31;
#endif
	SetMenu(MENU_GAMESETUP);
	return true;
}

boolean SCGameSetupFunc(int option)
{
	byte *ptr = (byte*) option;

	*ptr ^= 1;
	return true;
}

boolean SCGameSetupDeathmatch(int option)
{
	if(option == RIGHT_DIR)
	{
#ifdef __JDOOM__
		if(cfg.netDeathmatch < 2) 
#else
		if(cfg.netDeathmatch < 1)
#endif
			cfg.netDeathmatch++;
	}	
	else if(cfg.netDeathmatch > 0)
	{
		cfg.netDeathmatch--;
	}
	return true;
}

boolean SCGameSetupEpisode(int option)
{
#ifdef __JDOOM__
	if(gamemode == shareware)
	{
		cfg.netEpisode = 1;
		return true;
	}
	if(option == RIGHT_DIR)
	{
		if(cfg.netEpisode < (gamemode==retail? 4 : 3)) cfg.netEpisode++;
	}	
	else if(cfg.netEpisode > 1)
	{
		cfg.netEpisode--;
	}
#elif defined(__JHERETIC__)
	if(shareware)
	{
		cfg.netEpisode = 1;
		return true;
	}
	if(option == RIGHT_DIR)
	{
		if(cfg.netEpisode < 6) cfg.netEpisode++;
	}
	else if(cfg.netEpisode > 1)
	{
		cfg.netEpisode--;
	}
#endif
	return true;
}

boolean SCGameSetupMission(int option)
{
	if(option == RIGHT_DIR)
	{
#ifdef __JDOOM__
		if(cfg.netMap < (gamemode==commercial? 32 : 9)) cfg.netMap++;
#elif defined __JHERETIC__
		if(cfg.netMap < 9) cfg.netMap++;
#elif defined __JHEXEN__
		if(cfg.netMap < 31) cfg.netMap++;
#endif 
	}	
	else if(cfg.netMap > 1)
	{
		cfg.netMap--;
	}
	return true;
}

boolean SCGameSetupSkill(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.netSkill < 4) cfg.netSkill++;
	}
	else if(cfg.netSkill > 0)
	{
		cfg.netSkill--;
	}
	return true;
}

boolean SCOpenServer(int option)
{
	if(IS_NETGAME)
	{
		// Game already running, just change level.
#ifdef __JHEXEN__
		Executef(false, "setmap %i", cfg.netMap);		
#else
		Executef(false, "setmap %i %i", cfg.netEpisode, cfg.netMap);
#endif

#ifdef __JDOOM__
		M_ClearMenus();
#else
		MN_DeactivateMenu();
#endif
		return true;
	}

	// Go to DMI to setup server.
	Con_Execute("net setup server", false);
	return true;
}

boolean SCCloseServer(int option)
{
	Con_Execute("net server close", false);
#ifdef __JDOOM__
	M_ClearMenus();
#else
	MN_DeactivateMenu();
#endif
	return true;
}

boolean SCEnterPlayerSetupMenu(int option)
{
	strncpy(plrNameEd.text, *(char**) Con_GetVariable("n_plrname")->ptr, 255);
	plrColor = cfg.netColor;
#ifdef __JHEXEN__
	plrClass = cfg.netClass;
#endif
	SetMenu(MENU_PLAYERSETUP);
	return true;
}

#ifdef __JHEXEN__
void SCPlayerClass(int option)
{
	if(option == RIGHT_DIR)
	{
		if(plrClass < 2) plrClass++;
	}
	else if(plrClass > 0) plrClass--;
}
#endif

boolean SCPlayerColor(int option)
{
	if(option == RIGHT_DIR)
	{
#ifdef __JHEXEN__
		if(plrColor < 8) plrColor++;
#else
		if(plrColor < 4) plrColor++;
#endif
	}
	else if(plrColor > 0) plrColor--;
	return true;
}

boolean SCAcceptPlayer(int option)
{
	char buf[300];
	
	cfg.netColor = plrColor;
#ifdef __JHEXEN__
	cfg.netClass = plrClass;
#endif

	strcpy(buf, "n_plrname ");
	strcatQuoted(buf, plrNameEd.text);
	Con_Execute(buf, false);

	if(IS_NETGAME)
	{
		sprintf(buf, "setname ");
		strcatQuoted(buf, plrNameEd.text);
		Con_Execute(buf, false);
#ifdef __JHEXEN__
		// Must do 'setclass' first; the real class and color do not change
		// until the server sends us a notification -- this means if we do
		// 'setcolor' first, the 'setclass' after it will override the color
		// change (or such would appear to be the case).
		Executef(false, "setclass %i", plrClass);
#endif
		Executef(false, "setcolor %i", plrColor);
	}

	SetMenu(MENU_MULTIPLAYER);
	return true;
}

#if __JHEXEN__
boolean SCGameSetupDamageMod(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.netMobDamageModifier < 100) cfg.netMobDamageModifier++;
	}
	else if(cfg.netMobDamageModifier > 1) cfg.netMobDamageModifier--;
	return true;
}

boolean SCGameSetupHealthMod(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.netMobHealthModifier < 20) cfg.netMobHealthModifier++;
	}
	else if(cfg.netMobHealthModifier > 1) cfg.netMobHealthModifier--;
	return true;
}
#endif

// Menu routines ------------------------------------------------------------

void MN_TickerEx(void)	// The extended ticker.
{
	static int FrameTimer = 0;

	if(currentMenu == &PlayerSetupMenu)
	{
		if(FrameTimer++ >= 14)
		{
			FrameTimer = 0;
			CurrentPlrFrame = M_Random() % 8;
		}
	}
}

// Edit fields --------------------------------------------------------------

#ifdef __JDOOM__
int Ed_VisibleSlotChars(char *text, int (*widthFunc)(char *text, dpatch_t *font))
#else
int Ed_VisibleSlotChars(char *text, int (*widthFunc)(char *text))
#endif
{
	char cbuf[2] = { 0, 0 };
	int	i, w;
	
	for(i=0, w=0; text[i]; i++)
	{
		cbuf[0] = text[i];
#ifdef __JDOOM__
		w += widthFunc(cbuf, hu_font);
#else
		w += widthFunc(cbuf);
#endif
		if(w > SLOT_WIDTH) break;
	}
	return i;
}

void Ed_MakeCursorVisible()
{
	char buf[MAX_EDIT_LEN+1];
	int i, len, vis;
	
	strcpy(buf, ActiveEdit->text);
	strupr(buf);
	strcat(buf, "_"); // The cursor.
	len = strlen(buf);
	for(i=0; i<len; i++)
	{
#ifdef __JDOOM__
		vis = Ed_VisibleSlotChars(buf + i, M_StringWidth);
#else
		vis = Ed_VisibleSlotChars(buf + i, MN_TextAWidth);
#endif
		if(i+vis >= len) 
		{
			ActiveEdit->firstVisible = i;
			break;
		}
	}
}

boolean Ed_Responder(event_t *event)
{
	int	c;
	char *ptr;

	// Is there an active edit field?
	if(!ActiveEdit) return false;
	// Check the type of the event.
	if(event->type != ev_keydown && event->type != ev_keyrepeat)
		return false;	// Not interested in anything else.
	
	switch(event->data1)
	{
	case DDKEY_ENTER:
		ActiveEdit->firstVisible = 0;
		ActiveEdit = NULL;
		Notify(NULL);
		break;

	case DDKEY_ESCAPE:
		ActiveEdit->firstVisible = 0;
		strcpy(ActiveEdit->text, ActiveEdit->oldtext);
		ActiveEdit = NULL;
		break;

	case DDKEY_BACKSPACE:
		c = strlen(ActiveEdit->text);
		if(c > 0) ActiveEdit->text[c-1] = 0;
		Ed_MakeCursorVisible();
		break;

	default:
		c = toupper(event->data1);
		if(c >= ' ' && c <= 'Z')
		{
			if(shiftdown && shiftTable[c-32]) 
				c = shiftTable[c-32];
			else
				c = event->data1;
			if(strlen(ActiveEdit->text) < MAX_EDIT_LEN-2)
			{
				ptr = ActiveEdit->text + strlen(ActiveEdit->text);
				ptr[0] = c;
				ptr[1] = 0;				
				Ed_MakeCursorVisible();
			}
		}
	}
	// All keydown events eaten when an edit field is active.
	return true;
}

void DrawEditField(Menu_t *menu, int index, EditField_t *ef)
{
	int x = menu->x, y = menu->y + menu->itemHeight * index, vis;
	char buf[MAX_EDIT_LEN+1], *text;

#ifdef __JDOOM__
	M_DrawSaveLoadBorder(x+11, y+5);
	strcpy(buf, ef->text);
	strupr(buf);
	if(ActiveEdit == ef && MenuTime & 0x8) strcat(buf, "_");
	text = buf + ef->firstVisible;
	vis = Ed_VisibleSlotChars(text, M_StringWidth);
	text[vis] = 0;
	M_WriteText2(x+5, y+5, text, hu_font_a, 1, 1, 1);

#else
	GL_DrawPatch_CS(x, y, W_GetNumForName("M_FSLOT"));
	strcpy(buf, ef->text);
	strupr(buf);
	if(ActiveEdit == ef && MenuTime & 0x8) strcat(buf, "_"); 
	text = buf + ef->firstVisible;
	vis = Ed_VisibleSlotChars(text, MN_TextAWidth);
	text[vis] = 0;
	MN_DrTextA_CS(text, x+5, y+5);	
	
#endif
}

boolean SCEditField(int efptr)
{
	EditField_t *ef = (EditField_t*) efptr;

	// Activate this edit field.
	ActiveEdit = ef;
	strcpy(ef->oldtext, ef->text);
	Ed_MakeCursorVisible();
	return true;
}
