// Multiplayer Menu (for JHeretic)
// Contains an extension for edit fields.

#include "DoomDef.h"
#include "P_local.h"
#include "H_Net.h"
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

#define NETGAME_CENTER_Y	168

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
	int		color;
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
boolean SCGameSetupEpisode(int option);
boolean SCGameSetupMission(int option);
boolean SCGameSetupSkill(int option);
boolean SCOpenServer(int option);
boolean SCCloseServer(int option);
boolean SCChooseHost(int option);
boolean SCStartStopDisconnect(int option);
boolean SCEnterPlayerSetupMenu(int option);
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
static int plrColor;
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
	{ ITT_EFUNC, "PLAYER SETUP", SCEnterPlayerSetupMenu, 0, MENU_PLAYERSETUP },
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
	{ ITT_EFUNC, "DEATHMATCH:", SCGameSetupFunc, (int) &netDeathmatch, MENU_NONE },
	{ ITT_EFUNC, "MONSTERS:", SCGameSetupFunc, (int) &netNomonsters, MENU_NONE },
	{ ITT_EFUNC, "RESPAWN:", SCGameSetupFunc, (int) &netRespawn, MENU_NONE },
	{ ITT_EFUNC, "ALLOW JUMPING:", SCGameSetupFunc, (int) &netJumping, MENU_NONE },
	{ ITT_LRFUNC, "EPISODE:", SCGameSetupEpisode, 0, MENU_NONE },
	{ ITT_LRFUNC, "MISSION:", SCGameSetupMission, 0, MENU_NONE },
	{ ITT_LRFUNC, "SKILL:", SCGameSetupSkill, 0, MENU_NONE },
	{ ITT_EFUNC, "PROCEED...", SCOpenServer, 0, MENU_NONE }
};

Menu_t GameSetupMenu =
{
	98, 64,
	DrawGameSetupMenu,
	8, GameSetupItems,
	0,
	MENU_HOSTGAME,
	MN_DrTextA_CS, 9,
	0, 8
};

MenuItem_t PlayerSetupItems[] =
{
	{ ITT_EFUNC, "", SCEditField, (int) &plrNameEd, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_LRFUNC, "COLOR:", SCPlayerColor, 0, MENU_NONE },
	{ ITT_EMPTY, NULL, NULL, 0, MENU_NONE },
	{ ITT_EFUNC, "ACCEPT CHANGES", SCAcceptPlayer, 0, MENU_NONE }
};

Menu_t PlayerSetupMenu =
{
	70, 52,
	DrawPlayerSetupMenu,
	5, PlayerSetupItems,
	0,
	MENU_MULTIPLAYER,
	MN_DrTextB_CS, ITEM_HEIGHT,
	0, 5
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
	S_LocalSound(sfx_chat, NULL);
}

void DrANumber(int number, int x, int y)
{
	char buff[16];

	itoa(number, buff, 10);
	MN_DrTextA_CS(buff, x, y);
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
	char	*boolText[2] = { "NO", "YES" };
	char	*skillText[5] = { "BABY", "EASY", "MEDIUM", "HARD", "NIGHTMARE" };
	Menu_t	*menu = &GameSetupMenu;
	int		x = menu->x + 100, y = menu->y, h = menu->itemHeight;

	MN_DrTextB_CS("GAME SETUP", 108, 40);

	MN_DrTextA_CS(boolText[netDeathmatch], x, y);
	MN_DrTextA_CS(boolText[!netNomonsters], x, y+=h);
	MN_DrTextA_CS(boolText[netRespawn], x, y+=h);
	MN_DrTextA_CS(boolText[netJumping], x, y+=h);
	DrANumber(netEpisode, x, y+=h);
	DrANumber(netMap, x, y+=h);
	MN_DrTextA_CS(skillText[netSkill], x, y+=h);
}

void DrawNetGameMenu(void)
{
	if(!IS_CONNECTED)
	{
		MN_DrCenterTextB_CS("DISCONNECTED", 160, 2);
		return;
	}
	MN_DrCenterTextB_CS(IS_SERVER? "HOSTING" : "CONNECTED", 160, 2);			
	MN_DrCenterTextA_CS(IS_LIMBO? "LIMBO MODE" : "GAME IN PROGRESS", 160, 23);
	DrawPlayerList(37);
	DrawGameSetupInfo(88);
}

int CurrentPlrFrame = 0;

void DrawPlayerSetupMenu(void)
{
	spriteinfo_t	sprInfo;
	int				w, h, alpha;
	Menu_t *menu = &PlayerSetupMenu;

	MN_DrCenterTextB_CS("PLAYER SETUP", 160, menu->y - 22);

	DrawEditField(menu, 0, &plrNameEd);	
	
	gl.GetIntegerv(DGL_A, &alpha);

	// Draw the color selection as a random player frame.
	gi.GetSpriteInfo(SPR_PLAY, CurrentPlrFrame, &sprInfo);
	gi.Set(DD_TRANSLATED_SPRITE_TEXTURE, DD_TSPR_PARM(sprInfo.lump, 0, plrColor));
	gi.Set(DD_SPRITE_SIZE_QUERY, sprInfo.lump);
	w = gi.Get(DD_QUERY_RESULT) >> 16;		// High word.
	h = gi.Get(DD_QUERY_RESULT) & 0xffff;	// Low word.
	gi.GL_DrawRect(162-(sprInfo.offset>>16), menu->y+73 - (sprInfo.topOffset>>16), w, h, 1, 1, 1, alpha/255.0f);
	
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
	int	i, col[2] = { 56, 220 };
	char *colorText[4] = { "GREEN", "YELLOW", "RED", "BLUE" };

	MN_DrTextAGreen_CS("NAME", col[0], y);
	MN_DrTextAGreen_CS("COLOR", col[1], y);
	for(i=0, y+=9; i<MAXPLAYERS; i++)
	{
		if(!plrInfo[i].present) continue;
		MN_DrTextA_CS(plrInfo[i].name, col[0], y);
		MN_DrTextA_CS(colorText[plrInfo[i].color], col[1], y);
		y += 9;
	}
}

void DrawGameSetupInfo(int y)
{
	int		off = 56;
	char	buf[80];
	char	*skillText[5] = { "BABY", "EASY", "MEDIUM", "HARD", "NIGHTMARE" };

	MN_DrTextAGreen_CS("GAME SETUP:", off, y);
	
	sprintf(buf, "START IN E%cM%c, %s", '0'+netEpisode, '0'+netMap, skillText[netSkill]);
	MN_DrTextA_CS(buf, off, y+=9);
	
	strcpy(buf, netDeathmatch? "DEATHMATCH" : "CO-OP");
	if(netRespawn) strcat(buf, ", MONSTERS RESPAWN");
	MN_DrTextA_CS(buf, off, y+=9);

	strcpy(buf, netNomonsters? "NO MONSTERS" : "MONSTERS PRESENT");
	MN_DrTextA_CS(buf, off, y+=9);

	strcpy(buf, netJumping? "JUMPING ALLOWED" : "NO JUMPING");
	MN_DrTextA_CS(buf, off, y+=9);
}

boolean SCEnterMultiplayerMenu(int option)
{
	// Show the appropriate menu: mplayer menu when not connected or hosting,
	// limbo menu for limbo mode and net menu for games in progress.
	SetMenu(IS_CONNECTED? MENU_NETGAME : MENU_MULTIPLAYER);
	return true;
}

boolean SCEnterHostMenu(int option)
{
	if(GetProtocolName() == NULL) 
	{
		Notify("NO PROTOCOL CHOSEN!");
		return false;	// Can't enter!!
	}
	if(IS_CONNECTED)
	{
		Notify(IS_SERVER? "ALREADY HOSTING" : "CONNECTED TO A HOST");
		return false;
	}

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

boolean SCGameSetupEpisode(int option)
{
	if(option == RIGHT_DIR)
	{
		if(netEpisode < 6) netEpisode++;
	}
	else if(netEpisode > 1)
	{
		netEpisode--;
	}
	return true;
}

boolean SCGameSetupMission(int option)
{
	if(option == RIGHT_DIR)
	{
		if(netMap < 9) netMap++;
	}
	else if(netMap > 1)
	{
		netMap--;
	}
	return true;
}

boolean SCGameSetupSkill(int option)
{
	if(option == RIGHT_DIR)
	{
		if(netSkill < 4) netSkill++;
	}
	else if(netSkill > 0)
	{
		netSkill--;
	}
	return true;
}

void UpdateNetGameMenuItems(void)
{
	if(IS_SERVER)
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
}

boolean SCOpenServer(int option)
{
	if(!IS_CONNECTED)
	{
		gi.Execute("net server open", false);
		if(!gi.Get(DD_CCMD_RETURN))
		{
			Notify("FAILED TO OPEN SERVER");
			return false;
		}
	}
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
	if(IS_SERVER)
	{
		gi.Execute(IS_LIMBO? "net server go" : "net server stop", false);
		UpdateNetGameMenuItems();
	}
	else
	{
		gi.Execute("net disconnect", false);
		MN_DeactivateMenu();
	}
	return true;
}

boolean SCEnterPlayerSetupMenu(int option)
{
	if(IS_CONNECTED)
	{
		Notify("CAN'T CHANGE PLAYER SETUP WHEN CONNECTED!");
		return false;
	}
	strncpy(plrNameEd.text, *(char**) gi.GetCVar("n_plrname")->ptr, 255);
	plrColor = netColor;
	return true;
}

boolean SCPlayerColor(int option)
{
	if(option == RIGHT_DIR)
	{
		if(plrColor < 3) plrColor++;
	}
	else if(plrColor > 0) plrColor--;
	return true;
}

boolean SCAcceptPlayer(int option)
{
	char buf[300];

	netColor = plrColor;

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
	plrdata_t		pd;
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

	if(CurrentMenu == &NetGameMenu && UpdateCount++ >= 35)
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
	}
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

	gi.GL_DrawPatchCS(x, y, W_GetNumForName("M_FSLOT"));
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