// Multiplayer Menu (for JHexen)
// Contains an extension for edit fields.

#include "H2def.h"
#include "P_local.h"
#include "H2_Net.h"
#include "Soundst.h"
#include "Mn_def.h"
#include "Settings.h"

#include <ctype.h>
#include <stdarg.h>

// Macros -----------------------------------------------------------------

#define MAX_EDIT_LEN		256
#define SLOT_WIDTH			180
#define MAX_JOINITEMS		128

#define IS_SERVER			gi.Get(DD_SERVER)
#define IS_LIMBO			gi.Get(DD_LIMBO)
#define IS_NETGAME			gi.Get(DD_NETGAME)
#define IS_CONNECTED		(IS_LIMBO || IS_NETGAME)

#define NETGAME_CENTER_Y	171

// Types ------------------------------------------------------------------

typedef struct
{
	char	text[MAX_EDIT_LEN];
	char	oldtext[MAX_EDIT_LEN];	// If the current edit is canceled...
	int		firstVisible;
} EditField_t;

typedef struct
{
	boolean	present;
	char	name[80];
	int		color, class;
} PlayerInfo_t;

// Functions prototypes ---------------------------------------------------

void DrawMultiplayerMenu(void);
void DrawProtocolMenu(void);
void DrawHostMenu(void);
void DrawJoinMenu(void);
void DrawGameSetupMenu(void);
void DrawNetGameMenu(void);
void DrawPlayerSetupMenu(void);
void DrawTCPIPMenu(void);
void DrawSerialMenu(void);
void DrawModemMenu(void);

void DrawPlayerList(int y);
void DrawGameSetupInfo(int y);

boolean SCEnterHostMenu(int option);
boolean SCEnterJoinMenu(int option);
boolean SCEnterGameSetup(int option);
boolean SCSetProtocol(int option);
boolean SCGameSetupFunc(int option);
boolean SCGameSetupMission(int option);
boolean SCGameSetupSkill(int option);
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
boolean SCComPort(int option);
boolean SCBaudRate(int option);
boolean SCStopBits(int option);
boolean SCParity(int option);
boolean SCFlowControl(int option);
boolean SCModemSelector(int option);

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

static EditField_t hostNameEd, hostDescEd;
static EditField_t plrNameEd;
static int plrColor, plrClass;
static EditField_t ipAddrEd, ipPortEd, phoneEd;
static serverinfo_t	svInfo[MAX_JOINITEMS/2];
static PlayerInfo_t plrInfo[MAXPLAYERS];

static int baudRates[15] = 
{ 
	110, 300, 600, 1200, 2400, 
	4800, 9600, 14400, 19200, 38400, 
	56000, 57600, 115200, 128000, 256000
};


// Menus ------------------------------------------------------------------

MenuItem_t MultiplayerItems[] =
{
	{ ITT_SETMENU, "PROTOCOL", NULL, 0, MENU_PROTOCOL },
	{ ITT_EFUNC, "HOST GAME", SCEnterHostMenu, 0, MENU_NONE },
	{ ITT_EFUNC, "JOIN GAME", SCEnterJoinMenu, 0, MENU_NONE },
	{ ITT_EFUNC, "PLAYER SETUP", SCEnterPlayerSetupMenu, 0, MENU_NONE },
};

Menu_t MultiplayerMenu =
{
	110, 40,
	DrawMultiplayerMenu,
	4, MultiplayerItems,
	0,
	MENU_MAIN,
	MN_DrTextB_CS, ITEM_HEIGHT, 
	0, 4
};

MenuItem_t ProtocolItems[] = 
{
	{ ITT_EFUNC, "IPX", SCSetProtocol, 1, MENU_NONE },
	{ ITT_EFUNC, "TCP/IP", SCSetProtocol, 2, MENU_NONE },
	{ ITT_EFUNC, "SERIAL LINK", SCSetProtocol, 3, MENU_NONE },
	{ ITT_EFUNC, "MODEM", SCSetProtocol, 4, MENU_NONE },
	{ ITT_EFUNC, "NONE", SCSetProtocol, 0, MENU_NONE }
};

Menu_t ProtocolMenu =
{
	110, 40,
	DrawProtocolMenu,
	5, ProtocolItems,
	0,
	MENU_MULTIPLAYER,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 5
};

MenuItem_t HostItems[] =
{
	{ ITT_EFUNC, "HOST NAME:", SCEditField, (int) &hostNameEd, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "DESCRIPTION:", SCEditField, (int) &hostDescEd, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "PROCEED...", SCEnterGameSetup, 0, MENU_NONE }
};

Menu_t HostMenu =
{
	70, 40,
	DrawHostMenu,
	5, HostItems,
	0,
	MENU_MULTIPLAYER,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 5
};

MenuItem_t JoinMenuItems[MAX_JOINITEMS] =
{
	{ ITT_EMPTY, "(SEARCHING...)", 0, MENU_NONE }
};

Menu_t JoinMenu = 
{
	32, 30,
	DrawJoinMenu,
	1, JoinMenuItems,
	0,
	MENU_MULTIPLAYER,
	MN_DrTextA_CS, 9,
	0, 16
};

MenuItem_t GameSetupItems[] = 
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
	9, GameSetupItems,
	0,
	MENU_HOSTGAME,
	MN_DrTextA_CS, 9,
	0, 9
};

MenuItem_t PlayerSetupItems[] =
{
	{ ITT_EFUNC, "", SCEditField, (int) &plrNameEd, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "CLASS:", SCPlayerClass, 0, MENU_NONE },
	{ ITT_LRFUNC, "COLOR:", SCPlayerColor, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "ACCEPT CHANGES", SCAcceptPlayer, 0, MENU_NONE }
};

Menu_t PlayerSetupMenu =
{
	70, 42,
	DrawPlayerSetupMenu,
	6, PlayerSetupItems,
	0,
	MENU_MULTIPLAYER,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 6
};

MenuItem_t NetGameHostLimboItems[] =
{
	{ ITT_EFUNC, "START GAME", SCStartStopDisconnect, 0, MENU_NONE },
	{ ITT_EFUNC, "GAME SETUP", SCEnterGameSetup, 0, MENU_NONE },
	{ ITT_EFUNC, "CLOSE SERVER", SCCloseServer, 0, MENU_NONE }
};

MenuItem_t NetGameHostInGameItems[] =
{
	{ ITT_EFUNC, "STOP GAME", SCStartStopDisconnect, 0, MENU_NONE },
	{ ITT_EFUNC, "CLOSE SERVER", SCCloseServer, 0, MENU_NONE }
};

MenuItem_t NetGameClientItems[] =
{
	{ ITT_EFUNC, "DISCONNECT", SCStartStopDisconnect, 0, MENU_NONE }
};

Menu_t NetGameMenu =
{
	104, 155,
	DrawNetGameMenu,
	3, NetGameHostLimboItems,
	0,
	MENU_MAIN,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 3
};

MenuItem_t TCPIPItems[] =
{
	{ ITT_EFUNC, "IP ADDRESS:", SCEditField, (int) &ipAddrEd, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "PORT:", SCEditField, (int) &ipPortEd, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "PROCEED...", SCEnterJoinMenu, 0, MENU_NONE }
};

Menu_t TCPIPMenu =
{
	70, 40,
	DrawTCPIPMenu,
	5, TCPIPItems,
	0,
	MENU_MULTIPLAYER,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 5
};

MenuItem_t SerialItems[] =
{
	{ ITT_LRFUNC, "COM PORT:", SCComPort, 0, MENU_NONE },
	{ ITT_LRFUNC, "BAUD RATE:", SCBaudRate, 0, MENU_NONE },
	{ ITT_LRFUNC, "STOP BITS:", SCStopBits, 0, MENU_NONE },
	{ ITT_LRFUNC, "PARITY:", SCParity, 0, MENU_NONE },
	{ ITT_LRFUNC, "FLOW CONTROL:", SCFlowControl, 0, MENU_NONE },
	{ ITT_EFUNC, "PROCEED...", SCEnterJoinMenu, 0, MENU_NONE }
};

Menu_t SerialMenu =
{	
	70, 40,
	DrawSerialMenu,
	6, SerialItems,
	0,
	MENU_MULTIPLAYER,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 6
};

MenuItem_t ModemItems[] = 
{
	{ ITT_EFUNC, "PROCEED...", SCEnterJoinMenu, 0, MENU_NONE },
	{ ITT_LRFUNC, "MODEM:", SCModemSelector, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "PHONE NUMBER:", SCEditField, (int) &phoneEd, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE }
};

Menu_t ModemMenu =
{
	70, 40, 
	DrawModemMenu,
	5, ModemItems,
	0,
	MENU_MULTIPLAYER,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 5
};


// Code -------------------------------------------------------------------

int NetQuery(int id)
{
	gi.Set(DD_NET_QUERY, id);
	return gi.Get(DD_QUERY_RESULT);
}

int Executef(int silent, char *format, ...)
{
	va_list argptr;
	char buffer[512];

	va_start(argptr, format);
	vsprintf(buffer, format, argptr);
	va_end(argptr);
	return gi.Execute(buffer, silent);
}

char *GetProtocolName()
{
	gi.Set(DD_NET_QUERY, DD_PROTOCOL);
	return (char*) gi.Get(DD_QUERY_RESULT);
}

void Notify(char *msg)
{
	if(msg) P_SetMessage(&players[consoleplayer], msg, true);
	S_StartSound(NULL, SFX_CHAT);
}

void DrANumber(int number, int x, int y)
{
	char buff[16];

	itoa(number, buff, 10);
	MN_DrTextAYellow_CS(buff, x, y);
}

void MN_DrCenterTextA_CS(char *text, int center_x, int y)
{
	MN_DrTextA_CS(text, center_x - MN_TextAWidth(text)/2, y);
}

void MN_DrCenterTextB_CS(char *text, int center_x, int y)
{
	MN_DrTextB_CS(text, center_x - MN_TextBWidth(text)/2, y);
}

//*****

void DrawMultiplayerMenu(void)
{
	DrawProtocolMenu(); // Show the active protocol.
}

void DrawProtocolMenu(void)
{
	char *pname = GetProtocolName();
	char buf[80];

	MN_DrTextA_CS("ACTIVE PROTOCOL:", 70, 150);
	if(!pname) 
	{
		strcpy(buf, "NONE"); 
	}
	else 
	{
		strcpy(buf, pname);
		strupr(buf);
	}
	MN_DrTextB_CS(buf, 190, 143);
}

void DrawJoinMenu(void)
{
	Menu_t *menu = &JoinMenu;

	MN_DrTextB_CS("CHOOSE A HOST", 92, 8);
}

void DrawHostMenu(void)
{
	Menu_t *menu = &HostMenu;

	DrawEditField(menu, 1, &hostNameEd);
	DrawEditField(menu, 3, &hostDescEd);
}

void DrawGameSetupMenu(void)
{
	char	*mapName = P_GetMapName(P_TranslateMap(cfg.netMap));
	char	*boolText[2] = { "NO", "YES" };
	char	*skillText[5] = { "BABY", "EASY", "MEDIUM", "HARD", "NIGHTMARE" };
	Menu_t	*menu = &GameSetupMenu;
	int		x = menu->x + 112, y = menu->y, h = menu->itemHeight;

	MN_DrCenterTextB_CS("GAME SETUP", 160, 40);

	DrANumber(cfg.netMap, x, y);
	MN_DrTextAYellow_CS(mapName, 160-MN_TextAWidth(mapName)/2, y+=h);
	//sprintf(buf, "%i - %s", netMap, P_GetMapName(P_TranslateMap(netMap)));
	//MN_DrTextAYellow_CS(buf, menu->x + 30, y);
	MN_DrTextAYellow_CS(skillText[cfg.netSkill], x, y+=h);
	MN_DrTextAYellow_CS(boolText[cfg.netDeathmatch], x, y+=h);
	MN_DrTextAYellow_CS(boolText[!cfg.netNomonsters], x, y+=h);
	MN_DrTextAYellow_CS(boolText[cfg.netRandomclass], x, y+=h);
	DrANumber(cfg.netMobDamageModifier, x, y+=h);
	DrANumber(cfg.netMobHealthModifier, x, y+=h);
}

void DrawNetGameMenu(void)
{
/*	if(!IS_CONNECTED)
	{
		MN_DrCenterTextB_CS("DISCONNECTED", 160, 2);
		return;
	}
	MN_DrCenterTextB_CS(IS_SERVER? "HOSTING" : "CONNECTED", 160, 2);			
	MN_DrCenterTextA_CS(IS_LIMBO? "LIMBO MODE" : "GAME IN PROGRESS", 160, 23);
	DrawPlayerList(34);
	DrawGameSetupInfo(115);*/
}

int CurrentPlrFrame = 0;

void DrawPlayerSetupMenu(void)
{
	spriteinfo_t	sprInfo;
	int				/*w, h,*/ alpha, c;
	Menu_t *menu = &PlayerSetupMenu;

	MN_DrCenterTextB_CS("PLAYER SETUP", 160, menu->y - 22);

	DrawEditField(menu, 0, &plrNameEd);	
	
	gl.GetIntegerv(DGL_A, &alpha);

	// Draw the color selection as a random player frame.
	gi.GetSpriteInfo(!plrClass? SPR_PLAY : plrClass==1? SPR_CLER : SPR_MAGE, 
		CurrentPlrFrame, &sprInfo);
	c = plrColor;
	if(plrClass == PCLASS_FIGHTER)
	{
		// Fighter's colors are a bit different.
		if(c == 0) c = 2; else if(c == 2) c = 0;
	}
	/*gi.Set(DD_TRANSLATED_SPRITE_TEXTURE, DD_TSPR_PARM(sprInfo.lump, plrClass, c));
	gi.Set(DD_SPRITE_SIZE_QUERY, sprInfo.lump);
	w = gi.Get(DD_QUERY_RESULT) >> 16;		// High word.
	h = gi.Get(DD_QUERY_RESULT) & 0xffff;	// Low word.
	gi.GL_DrawRect(162-(sprInfo.offset>>16), menu->y+90 - (sprInfo.topOffset>>16), w, h, 1, 1, 1, alpha/255.0f);
	*/
	// Restore original alpha.
	gl.Color4ub(255, 255, 255, alpha);
}

void DrawTCPIPMenu(void)
{
	Menu_t *menu = &TCPIPMenu;

	DrawEditField(menu, 1, &ipAddrEd);
	DrawEditField(menu, 3, &ipPortEd);
}

void DrawSerialMenu(void)
{
	char	buf[10];	
	char	*stopbitText[3] = { "1", "1.5", "2" };
	char	*parityText[4] = { "NO", "ODD", "EVEN", "MARK" };
	char	*flowText[5] = { "NO", "XON/XOFF", "RTS", "DTR", "RTS/DTR" };
	Menu_t	*menu = &SerialMenu;
	int		x = menu->x + 130, y = menu->y, h = menu->itemHeight;

	itoa(NetQuery(DD_COM_PORT), buf, 10);
	MN_DrTextB_CS(buf, x, y);
	itoa(NetQuery(DD_BAUD_RATE), buf, 10);
	MN_DrTextB_CS(buf, x, y+=h);
	MN_DrTextB_CS(stopbitText[NetQuery(DD_STOP_BITS)], x, y+=h);
	MN_DrTextB_CS(parityText[NetQuery(DD_PARITY)], x, y+=h);
	MN_DrTextB_CS(flowText[NetQuery(DD_FLOW_CONTROL)], x, y+=h);
}

void DrawModemMenu(void)
{
	int					modemsel = NetQuery(DD_MODEM);
	modemdataquery_t	mdq;
	char				buf[200];
	Menu_t				*menu = &ModemMenu;
	int					h = menu->itemHeight;

	if(menu->itemCount > 3)
		DrawEditField(menu, 4, &phoneEd); 

	gi.Set(DD_MODEM_DATA_QUERY, (int) &mdq);
	if(mdq.list)
	{
		strcpy(buf, mdq.list[modemsel]);
		MN_TextFilter(buf);
		MN_DrCenterTextA_CS(buf, 160, menu->y + h*2 + 5);
	}
}

void DrawPlayerList(int y)
{
	int	i, col[3] = { 30, 180, 245 };
	char *classText[3] = { "FIGHTER", "CLERIC", "MAGE" };
	char *colorText[8] = { "BLUE", "RED", "YELLOW", "GREEN", "JADE", "WHITE", "HAZEL", "PURPLE" };

	MN_DrTextAYellow_CS("NAME", col[0], y);
	MN_DrTextAYellow_CS("CLASS", col[1], y);
	MN_DrTextAYellow_CS("COLOR", col[2], y);
	for(i=0, y+=9; i<MAXPLAYERS; i++)
	{
		if(!plrInfo[i].present) continue;
		MN_DrTextA_CS(plrInfo[i].name, col[0], y);
		MN_DrTextA_CS(classText[plrInfo[i].class], col[1], y);
		MN_DrTextA_CS(colorText[plrInfo[i].color], col[2], y);
		y += 9;
	}
}

void DrawGameSetupInfo(int y)
{
	int		y2 = y, off = 30;
	char	buf[80];
	char	*skillText[5] = { "BABY", "EASY", "MEDIUM", "HARD", "NIGHTMARE" };

	MN_DrTextAYellow_CS("GAME SETUP:", off, y);
	
	sprintf(buf, "MAP %i, %s%s", cfg.netMap, skillText[cfg.netSkill],
		cfg.netRandomclass? ", RANDOM CLASSES" : "");
	MN_DrTextA_CS(buf, off, y+=9);
	
	strcpy(buf, cfg.netDeathmatch? "DEATHMATCH" : "CO-OP");
	strcat(buf, cfg.netNomonsters? ", NO MONSTERS" : " WITH MONSTERS");
	MN_DrTextA_CS(buf, off, y+=9);

	if(cfg.netMobDamageModifier > 1)
	{
		sprintf(buf, "D:%i", cfg.netMobDamageModifier);
		MN_DrTextAYellow_CS(buf, 285, y2+=9);
	}
	if(cfg.netMobHealthModifier > 1)
	{
		sprintf(buf, "H:%i", cfg.netMobHealthModifier);
		MN_DrTextAYellow_CS(buf, 285, y2+=9);
	}
}

boolean SCEnterMultiplayerMenu(int option)
{
	// Show the appropriate menu: mplayer menu when not connected or hosting,
	// limbo menu for limbo mode and net menu for games in progress.
//	SetMenu(IS_CONNECTED? MENU_NETGAME : MENU_MULTIPLAYER);
	return true;
}

boolean SCEnterHostMenu(int option)
{
	if(GetProtocolName() == NULL) 
	{
		Notify("NO PROTOCOL CHOSEN!");
		return false;	// Can't enter!!
	}
/*	if(IS_CONNECTED)
	{
		Notify(IS_SERVER? "ALREADY HOSTING" : "CONNECTED TO A HOST");
		return false;
	}*/

	if(!strcmp(GetProtocolName(), "Serial Link") && CurrentMenu != &SerialMenu)
	{
		SerialMenu.items[5].func = SCEnterHostMenu;
		SetMenu(MENU_SERIAL);
		return false;
	}
	if(!strcmp(GetProtocolName(), "Modem") && CurrentMenu != &ModemMenu)
	{
		ModemMenu.items[0].func = SCEnterHostMenu;
		ModemMenu.y = 70;
		ModemMenu.itemCount = ModemMenu.numVisItems = 3;
		ModemMenu.oldItPos = 0;
		SetMenu(MENU_MODEM);
		return false;
	}
	
	// Set the edit fields.
	strncpy(hostNameEd.text, *(char**) gi.GetCVar("n_servername")->ptr, 255);
	strncpy(hostDescEd.text, *(char**) gi.GetCVar("n_serverinfo")->ptr, 255);
	SetMenu(MENU_HOSTGAME);
	return true;
}

boolean SCEnterJoinMenu(int option)
{
	char *prot = GetProtocolName(), buf[200];

	if(CurrentMenu == &TCPIPMenu)
	{
		// Apply the settings and go to the Join Game menu.
		Executef(false, buf, "net tcpip address %s", ipAddrEd.text);
		Executef(false, buf, "net tcpip port %s", ipPortEd.text);
		SetMenu(MENU_JOINGAME);
		return true;
	}
	if(CurrentMenu == &SerialMenu)
	{
		SetMenu(MENU_JOINGAME);
		return true;
	}
	if(CurrentMenu == &ModemMenu)
	{
		Executef(false, "net modem phone %s", phoneEd.text);
		SetMenu(MENU_JOINGAME);
		return true;
	}

	if(prot == NULL) 
	{
		Notify("NO PROTOCOL CHOSEN!");
		return false;	// Can't enter!!
	}

	ResetJoinMenuItems();
	if(!strcmp(prot, "TCP/IP"))
	{
		// Query the current TCP/IP address and port.
		strcpy(ipAddrEd.text, (char*) NetQuery(DD_TCPIP_ADDRESS));
		itoa(NetQuery(DD_TCPIP_PORT), ipPortEd.text, 10);
		// Go to the TCP/IP settings menu. 
		JoinMenu.prevMenu = MENU_TCPIP;
		SetMenu(MENU_TCPIP);
	}	
	else if(!strcmp(prot, "Serial Link"))
	{
		JoinMenu.prevMenu = MENU_SERIAL;
		SerialMenu.items[5].func = SCEnterJoinMenu;
		SetMenu(MENU_SERIAL);
	}
	else if(!strcmp(prot, "Modem"))
	{
		strcpy(phoneEd.text, (char*) NetQuery(DD_PHONE_NUMBER));
		JoinMenu.prevMenu = MENU_MODEM;
		ModemMenu.y = 50;
		ModemMenu.itemCount = ModemMenu.numVisItems = 5;
		ModemMenu.items[0].func = SCEnterJoinMenu;
		SetMenu(MENU_MODEM);
	}
	else
	{
		// IPX needs no setup.
		JoinMenu.prevMenu = MENU_MULTIPLAYER;
		SetMenu(MENU_JOINGAME);
	}
	return true;
}

boolean SCEnterGameSetup(int option)
{
	char buf[300];
	
	if(CurrentMenu == &HostMenu)
	{
		GameSetupMenu.prevMenu = MENU_HOSTGAME;

		strcpy(buf, "n_servername ");
		strcatQuoted(buf, hostNameEd.text);
		gi.Execute(buf, false);

		strcpy(buf, "n_serverinfo ");
		strcatQuoted(buf, hostDescEd.text);
		gi.Execute(buf, false);
	}
	else
	{
		GameSetupMenu.prevMenu = MENU_NETGAME;
	}
	SetMenu(MENU_GAMESETUP);
	return true;
}

boolean SCSetProtocol(int option)
{
	char *pros[4] = { "ipx", "tcpip", "serial", "modem" };

	// Close the current protocol.
	if(GetProtocolName()) gi.Execute("net shutdown", false);
	if(!option) return true;
	Executef(false, "net init %s", pros[option-1]);
	if(!gi.Get(DD_CCMD_RETURN)) Notify("FAILURE!");
	return true;
}

boolean SCGameSetupFunc(int option)
{
	byte *ptr = (byte*) option;

	*ptr ^= 1;
	return true;
}

boolean SCGameSetupMission(int option)
{
	if(option == RIGHT_DIR)
	{
		if(cfg.netMap < 31) cfg.netMap++;
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

void UpdateNetGameMenuItems(void)
{
/*	if(IS_SERVER)
	{
		NetGameMenu.items = IS_LIMBO? NetGameHostLimboItems : NetGameHostInGameItems;
		NetGameMenu.itemCount = NetGameMenu.numVisItems = IS_LIMBO? 3 : 2;
	}
	else
	{
		NetGameMenu.items = NetGameClientItems;
		NetGameMenu.itemCount = NetGameMenu.numVisItems = 1;
	}
	NetGameMenu.oldItPos = 0;
	NetGameMenu.y = NETGAME_CENTER_Y - NetGameMenu.itemHeight*NetGameMenu.itemCount/2;
	*/
}

boolean SCOpenServer(int option)
{
/*	if(!IS_CONNECTED)
	{
		gi.Execute("net server open", false);
		if(!gi.Get(DD_CCMD_RETURN))
		{
			Notify("FAILED TO OPEN SERVER");
			return false;
		}
	}*/
	// Now we're open, go to netgame menu.
	UpdateNetGameMenuItems();
	SetMenu(MENU_NETGAME);
	return true;
}

boolean SCCloseServer(int option)
{
	gi.Execute("net server close", false);
	MN_DeactivateMenu();
	return true;
}

boolean SCChooseHost(int option)
{
	// Try to connect to the chosen server.
	char cmd[256];

	if(JoinMenu.itemCount == 1) return false; // Just "(searching...)"

	strcpy(cmd, "net connect ");
	strcatQuoted(cmd, svInfo[option].name);
	gi.Execute(cmd, false);
	if(gi.Get(DD_CCMD_RETURN))
	{
		UpdateNetGameMenuItems();
		SetMenu(MENU_NETGAME);
		return true;		
	}
	// A connection could not be made.
	Notify("FAILURE!");
	return false;
}

boolean SCStartStopDisconnect(int option)
{
/*	if(IS_SERVER)
	{
		gi.Execute(IS_LIMBO? "net server go" : "net server stop", false);
		UpdateNetGameMenuItems();
	}
	else
	{
		gi.Execute("net disconnect", false);
		MN_DeactivateMenu();
	}*/
	return true;
}

boolean SCEnterPlayerSetupMenu(int option)
{
/*	if(IS_CONNECTED)
	{
		Notify("CAN'T CHANGE PLAYER SETUP WHEN CONNECTED!");
		return false;
	}
	strncpy(plrNameEd.text, *(char**) gi.GetCVar("n_plrname")->ptr, 255);
	plrClass = netClass;
	plrColor = netColor;
	SetMenu(MENU_PLAYERSETUP);*/
	return true;
}

void SCPlayerClass(int option)
{
	if(option == RIGHT_DIR)
	{
		if(plrClass < 2) plrClass++;
	}
	else if(plrClass > 0) plrClass--;
}

boolean SCPlayerColor(int option)
{
	if(option == RIGHT_DIR)
	{
		if(plrColor < 7) plrColor++;
	}
	else if(plrColor > 0) plrColor--;
	return true;
}

boolean SCAcceptPlayer(int option)
{
	char buf[300];
	
	cfg.netClass = plrClass;
	cfg.netColor = plrColor;

	strcpy(buf, "n_plrname ");
	strcatQuoted(buf, plrNameEd.text);
	gi.Execute(buf, false);

	SetMenu(MENU_MULTIPLAYER);
	return true;
}

boolean SCComPort(int option)
{
	int port = NetQuery(DD_COM_PORT);

	if(option == RIGHT_DIR)
	{
		if(port < 4) port++;
	}
	else if(port > 1) port--;

	Executef(false, "net serial com %d", port);
	return true;
}

boolean SCBaudRate(int option)
{
	int i, idx = -1, baud = NetQuery(DD_BAUD_RATE);

	// Find the index.
	for(i=0; i<15; i++)
		if(baud == baudRates[i])
		{
			idx = i;
			break;
		}
	if(idx == -1) idx = 12; // default to 115200
	
	if(option == RIGHT_DIR) 
	{
		if(idx < 14) idx++;
	}
	else if(idx > 0) idx--;

	Executef(false, "net serial baud %d", baudRates[idx]);
	return true;
}

boolean SCStopBits(int option)
{
	int	i = NetQuery(DD_STOP_BITS);

	if(option == RIGHT_DIR)
	{
		if(i < 2) i++;
	}
	else if(i > 0) i--;
	Executef(false, "net serial stop %d", i);
	return true;
}

boolean SCParity(int option)
{
	int	i = NetQuery(DD_PARITY);

	if(option == RIGHT_DIR)
	{
		if(i < 3) i++;
	}
	else if(i > 0) i--;
	Executef(false, "net serial parity %d", i);
	return true;
}

boolean SCFlowControl(int option)
{
	int	i = NetQuery(DD_FLOW_CONTROL);

	if(option == RIGHT_DIR)
	{
		if(i < 4) i++;
	}
	else if(i > 0) i--;
	Executef(false, "net serial flow %d", i);
	return true;
}

boolean SCModemSelector(int option)
{
	int					sel;
	modemdataquery_t	mdq;

	gi.Set(DD_MODEM_DATA_QUERY, (int) &mdq);
	if(!mdq.list) return false;
	sel = NetQuery(DD_MODEM);
	if(option == RIGHT_DIR)
	{
		if(sel < mdq.num-1) sel++;
	}
	else if(sel > 0) sel--;
	Executef(false, "net modem %d", sel);
	return true;
}

void ResetJoinMenuItems()
{
	JoinMenuItems[0].type = ITT_EMPTY;
	JoinMenuItems[0].text = "(SEARCHING...)";
	JoinMenuItems[0].func = NULL;
	JoinMenuItems[0].option = 0;
	JoinMenuItems[0].menu = MENU_NONE;
	JoinMenu.itemCount = 1;
	JoinMenu.oldItPos = 0;
	JoinMenu.firstItem = 0;
}


// Menu routines ------------------------------------------------------------

void MN_TickerEx(void)	// The extended ticker.
{
	int				i, num;
	serverdataquery_t svQuery;
//	plrdata_t		pd;
	static int		UpdateCount = 0, FrameTimer = 0;

	if(CurrentMenu == &JoinMenu && UpdateCount++ >= 35)
	{
		UpdateCount = 0;
		
		// Update the list of available servers.
		gi.Set(DD_NET_QUERY, DD_NUM_SERVERS);
		svQuery.num = gi.Get(DD_QUERY_RESULT);
		svQuery.found = 0;
		num = 0;
		if(svQuery.num)
		{
			svQuery.data = svInfo;
			gi.Set(DD_SERVER_DATA_QUERY, (int) &svQuery);
			if(svQuery.found)
			{
				MenuItem_t *mi;
				// Update the menu.
				for(i=0, num=0; i<svQuery.found; i++)
				{
					if(!svInfo[i].canJoin || svInfo[i].players == svInfo[i].maxPlayers
						|| svInfo[i].players >= MAXPLAYERS)
						continue;	// Locked or full.
					strupr(svInfo[i].name);
					strupr(svInfo[i].description);
					// Name.
					mi = JoinMenuItems + num++;
					mi->type = ITT_EFUNC;
					mi->text = svInfo[i].name;
					mi->func = SCChooseHost;
					mi->option = i;
					mi->menu = MENU_NONE; 
					// Description.
					mi = JoinMenuItems + num++;
					mi->type = ITT_EMPTY;
					mi->text = svInfo[i].description;
					mi->func = NULL;
					mi->option = 0;
					mi->menu = MENU_NONE;
				}
				JoinMenu.itemCount = num;
				if(CurrentItPos >= JoinMenu.itemCount) 
					CurrentItPos = JoinMenu.itemCount-2;
				if(CurrentItPos < 0) CurrentItPos = 0;
			}
		}
		if(!num)
		{
			ResetJoinMenuItems();
			CurrentItPos = 0;
		}
	}

/*	if(CurrentMenu == &NetGameMenu && UpdateCount++ >= 35)
	{
		UpdateCount = 0;
		
		// Update the list of connected players.
		for(i=0; i<MAXPLAYERS; i++)
		{
			memset(&plrInfo[i], 0, sizeof(PlayerInfo_t));
			if(!gi.NetGetPlayerData(i, &pd, sizeof(pd)))
			{
				plrInfo[i].present = false;
				continue;
			}
			plrInfo[i].present = true;
			plrInfo[i].color = pd.color;
			plrInfo[i].class = pd.class;
			strncpy(plrInfo[i].name, gi.GetPlayerName(i), 79);
			MN_TextFilter(plrInfo[i].name);
		}
	}

	if(CurrentMenu == &PlayerSetupMenu)
	{
		if(FrameTimer++ >= 14)
		{
			FrameTimer = 0;
			gi.Set(DD_NUM_SPRITE_FRAMES_QUERY, SPR_PLAY);
			CurrentPlrFrame = M_Random() % 8;//gi.Get(DD_QUERY_RESULT);
		}
	}*/
}


// Edit fields --------------------------------------------------------------

int Ed_VisibleSlotChars(char *text, int (*widthFunc)(char *text))
{
	char cbuf[2] = { 0, 0 };
	int	i, w;
	
	for(i=0, w=0; text[i]; i++)
	{
		cbuf[0] = text[i];
		w += widthFunc(cbuf);
		if(w > SLOT_WIDTH) break;
	}
	return i;
}

void Ed_MakeCursorVisible()
{
	char buf[MAX_EDIT_LEN+1];
	int i, len, vis;
	
	strcpy(buf, ActiveEdit->text);
	MN_TextFilter(buf);
	strcat(buf, "["); // The cursor.
	len = strlen(buf);
	for(i=0; i<len; i++)
	{
		vis = Ed_VisibleSlotChars(buf + i, MN_TextAWidth);
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

	gi.GL_DrawPatchCS(x, y, gi.W_GetNumForName("M_FSLOT"));
	strcpy(buf, ef->text);
	MN_TextFilter(buf);
	if(ActiveEdit == ef && MenuTime & 0x8) strcat(buf, "["); // The cursor (why '['?).
	text = buf + ef->firstVisible;
	vis = Ed_VisibleSlotChars(text, MN_TextAWidth);
	text[vis] = 0;
	MN_DrTextA_CS(text, x+5, y+5);		
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