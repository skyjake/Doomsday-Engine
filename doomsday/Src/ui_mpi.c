/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * ui_mpi.c: Multiplayer Setup Interface
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_system.h"
#include "de_network.h"
#include "de_console.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

#define MAX_MODEMS			10	// Nobody has that many modems!
#define MAX_SERIAL_PORTS	10
#define MAX_FOUND			32
#define NUMITEMS(x)			(sizeof(x)/sizeof(uidata_listitem_t))

#define UIF_SERVER_LIST		UIF_ID0

// TYPES -------------------------------------------------------------------

typedef struct serverstrings_s {
	char desc[90];
	char version[20];
	char ping[20];
	char game[80];
	char names[256];
	char pwads[256];
	char warning[128];
} serverstrings_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

void MPIGotoPage(ui_object_t *ob);
void MPIGoBack(ui_object_t *ob);
void MPIToggleMasterItems(ui_object_t *ob);
void MPIStartServer(ui_object_t *ob);
void MPIShowProtocolSettings(ui_object_t *ob);
void MPISetupProtocol(ui_object_t *ob);
void MPISearch(ui_object_t *ob);
void MPIRetrieve(ui_object_t *ob);
void MPIUpdateFound(ui_object_t *ob);
void MPIConnect(ui_object_t *ob);
void MPIHelpDrawer(ui_object_t *ob);
void MPIUpdateServerInfo(ui_object_t *ob);
void MPIServerInfoDrawer(ui_object_t *ob);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char str_server[101];
char str_desc[201];
char str_masterip[128];
char str_ipport[11];
char str_ipaddr[128];
char str_phone[31];
serverstrings_t str_sinfo;

uidata_slider_t sld_player_limit = { 0, 16, 0, 1, false, "server-player-limit" };

uidata_listitem_t lstit_protocols[] =
{
	{ "TCP/IP", NSP_TCPIP },		// Group 1
	{ "IPX", NSP_IPX },				// Group 2
	{ "Modem", NSP_MODEM },			// Group 3
	{ "Serial Link", NSP_SERIAL }	// Group 4
};
uidata_listitem_t lstit_modems[MAX_MODEMS];
uidata_listitem_t lstit_ports[MAX_SERIAL_PORTS];
uidata_listitem_t lstit_rates[] =
{
	{ "110", 110 },
	{ "300", 300 },
	{ "600", 600 },
	{ "1200", 1200 },
	{ "2400", 2400 },
	{ "4800", 4800 },
	{ "9600", 9600 },
	{ "14400", 14400 },
	{ "19200", 19200 },
	{ "38400", 38400 },
	{ "56000", 56000 },
	{ "57600", 57600 },
	{ "115200", 115200 },
	{ "128000", 128000 },
	{ "256000", 256000 }
};
uidata_listitem_t lstit_parities[] =
{
	{ "None" },
	{ "Odd" },
	{ "Even" },
	{ "Mark" }
};
uidata_listitem_t lstit_stop[] =
{
	{ "1" },
	{ "1.5" },
	{ "2" }
};
uidata_listitem_t lstit_flow[] =
{
	{ "No" },
	{ "XON/XOFF" },
	{ "RTS" },
	{ "DTR" },
	{ "RTS/DTR" }
};
uidata_listitem_t lstit_found[MAX_FOUND];

uidata_edit_t ed_server = { str_server, 100 };
uidata_edit_t ed_desc = { str_desc, 200 };
uidata_edit_t ed_masterip = { str_masterip, 127 };
uidata_list_t lst_protocol = { lstit_protocols, NUMITEMS(lstit_protocols) };
uidata_edit_t ed_ipport = { str_ipport, 10 };
uidata_list_t lst_modem = { lstit_modems };
uidata_list_t lst_ports = { lstit_ports };
uidata_list_t lst_baud = { lstit_rates, NUMITEMS(lstit_rates) };
uidata_list_t lst_parity = { lstit_parities, 4 };
uidata_list_t lst_stopbit = { lstit_stop, 3 };
uidata_list_t lst_flow = { lstit_flow, 5 };
uidata_list_t lst_found = { lstit_found, 1 };
uidata_edit_t ed_ipsearch = { str_ipaddr, 127 };
uidata_edit_t ed_phone = { str_phone, 30 };

ui_page_t page_server, page_client, page_protocol;

ui_object_t ob_server[] =	// Observer?
{
	{ UI_TEXT,		0,	0,		50, 200,	0, 80,		"Server name",		UIText_Drawer },
	{ UI_EDIT,		0,	0,		320, 200,	500, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_server },
	{ UI_TEXT,		0,	0,		50, 350,	0, 80,		"Description",		UIText_Drawer },
	{ UI_EDIT,		0,	0,		320, 350,	630, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_desc },
	{ UI_TEXT,		0,	0,		50, 500,	0, 80,		"Max. players",		UIText_Drawer },
	{ UI_SLIDER,	0,	0,		320, 510,	350, 60,	"",					UISlider_Drawer, UISlider_Responder, UISlider_Ticker, CP_CvarSlider, &sld_player_limit },
	{ UI_TEXT,		1,	0,		50, 650,	0, 80,		"Master TCP/IP address", UIText_Drawer },
	{ UI_EDIT,		1,	0,		320, 650,	350, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_masterip },
	{ UI_BUTTON,	0,	0,		50, 920,	200, 80,	"Network Setup",	UIButton_Drawer, UIButton_Responder, 0, MPIGotoPage, &page_protocol },
	{ UI_BUTTON2,	2,	0,		300, 920,	200, 80,	"Public Server",	UIButton_Drawer, UIButton_Responder, 0, MPIToggleMasterItems, &masterAware },
	{ UI_BUTTON,	0,	UIF_DEFAULT, 750, 920, 200, 80,	"Start",			UIButton_Drawer, UIButton_Responder, 0, MPIStartServer },
	{ UI_NONE }
};

ui_object_t ob_client[] =
{
	{ UI_BUTTON,	0,	UIF_DEFAULT, 0, 0,	200, 80,	"Search",			UIButton_Drawer, UIButton_Responder, 0, MPISearch },
	{ UI_TEXT,		1,	0,		220, 0,		0, 80,		"Search address",	UIText_Drawer },
	{ UI_EDIT,		1,	0,		400, 0,		380, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_ipsearch },
	{ UI_TEXT,		2,	0,		220, 0,		0, 80,		"Phone number",		UIText_Drawer },
	{ UI_EDIT,		2,	0,		400, 0,		380, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_phone },

	/* The List of Servers */
	{ UI_BOX,		0,	0,		0, 100,		780, 900,	"",					UIFrame_Drawer },
	{ UI_LIST,		0,	UIF_SERVER_LIST, 20, 130,	740, 300, "",			UIList_Drawer, UIList_Responder, MPIUpdateFound, MPIUpdateServerInfo, &lst_found },
	{ UI_TEXT,		0,	0,		20,	450,	0, 70,		"Description",		UIText_Drawer },
	{ UI_TEXT,		0,	0,		190, 450,	570, 70,	"",					MPIServerInfoDrawer, 0, 0, 0, str_sinfo.desc },
	{ UI_TEXT,		0,	0,		20,	530,	0, 70,		"Game",				UIText_Drawer },
	{ UI_TEXT,		0,	0,		190, 530,	250, 70,	"",					MPIServerInfoDrawer, 0, 0, 0, str_sinfo.game },
	{ UI_TEXT,		0,	0,		460, 530,	0, 70,		"Version",			UIText_Drawer },
	{ UI_TEXT,		0,	0,		560, 530,	200, 70,	"",					MPIServerInfoDrawer, 0, 0, 0, str_sinfo.version },
	{ UI_TEXT,		0,	0,		20,	610,	0, 70,		"Setup",			UIText_Drawer },
	{ UI_TEXT,		0,	0,		190, 610,	570, 120,	"",					MPIServerInfoDrawer, 0, 0, 0, str_sinfo.pwads },
	{ UI_TEXT,		0,	0,		20,	740,	0, 70,		"Players",			UIText_Drawer },
	{ UI_TEXT,		0,	0,		190, 740,	570, 120,	"",					MPIServerInfoDrawer, 0, 0, 0, str_sinfo.names },
	{ UI_TEXT,		0,	0,		20,	870,	0, 70,		"Ping",				UIText_Drawer },
	{ UI_TEXT,		0,	0,		190, 870,	200, 70,	"",					MPIServerInfoDrawer, 0, 0, 0, str_sinfo.ping },

	{ UI_BUTTON,	3,	0,		800, 0,		200, 80,	"Get From Master",	UIButton_Drawer, UIButton_Responder, 0, MPIRetrieve },
	{ UI_BUTTON,	0,	0,		800, 100,	200, 80,	"Network Setup",	UIButton_Drawer, UIButton_Responder, 0, MPIGotoPage, &page_protocol },
	{ UI_BUTTON,	0,	0,		800, 200,	200, 80,	"Exit (Esc)",		UIButton_Drawer, UIButton_Responder, 0, MPIGoBack },
	{ UI_BUTTON,	4,	0,		800, 900,	200, 80,	"Connect",			UIButton_Drawer, UIButton_Responder, 0, MPIConnect },
	{ UI_TEXT,		5,	0,		800, 330,	200, 450,	"",					MPIServerInfoDrawer, 0, 0, 0, str_sinfo.warning },
	{ UI_NONE }
};

ui_object_t ob_protocol[] =
{
	{ UI_LIST,		0,	0,		0, 55,		260, 400,	"Network Type",		UIList_Drawer, UIList_Responder, UIList_Ticker, MPIShowProtocolSettings, &lst_protocol },
	{ UI_TEXT,		0,	0,		280, 0,		0, 50,		"Network Settings", UIText_Drawer },
	{ UI_BUTTON,	0,	UIF_DEFAULT, 20, 475, 220, 80,	"OK",				UIButton_Drawer, UIButton_Responder, 0, MPISetupProtocol },
	{ UI_BUTTON,	0,	0,		20, 940,	220, 60,	"Cancel (Esc)",		UIButton_Drawer, UIButton_Responder, 0, MPIGoBack },
	{ UI_BOX,		0,	0,		280, 55,	720, 945,	"",					UIFrame_Drawer },
	{ UI_TEXT,		1,	0,		300, 80,	0, 60,		"Local TCP/IP port", UIText_Drawer },
	{ UI_EDIT,		1,	0,		680, 80,	160, 60,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_ipport },
	{ UI_TEXT,		1,	0,		680, 140,	0, 60,		"0: Autoselect",	UIText_Drawer },
	{ UI_TEXT,		3,	0,		300, 80,	0, 60,		"Modem device",		UIText_Drawer },
	{ UI_LIST,		3,	0,		500, 80,	480, 400,	"",					UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_modem },
	{ UI_TEXT,		4,	0,		300, 80,	0, 60,		"Serial port",		UIText_Drawer },
	{ UI_LIST,		4,	0,		680, 80,	300, 150,	"",					UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_ports },
	{ UI_TEXT,		4,	0,		300, 235,	0, 60,		"Baud rate",		UIText_Drawer },
	{ UI_LIST,		4,	0,		680, 235,	300, 150,	"",					UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_baud },
	{ UI_TEXT,		4,	0,		300, 390,	0, 60,		"Parity",			UIText_Drawer },
	{ UI_LIST,		4,	0,		680, 390,	300, 150,	"",					UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_parity },
	{ UI_TEXT,		4,	0,		300, 545,	0, 60,		"Stop bits",		UIText_Drawer },
	{ UI_LIST,		4,	0,		680, 545,	300, 150,	"",					UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_stopbit },
	{ UI_TEXT,		4,	0,		300, 700,	0, 60,		"Flow control",		UIText_Drawer },
	{ UI_LIST,		4,	0,		680, 700,	300, 150,	"",					UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_flow },
	{ UI_BOX,		0,	0,		300, 0,		680, 0,		"",					MPIHelpDrawer },
	{ UI_NONE }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean searching = false;
static boolean retrieving = false;
static boolean masterlist = false;
static unsigned int my_crc = 0;

// CODE --------------------------------------------------------------------

/*
 * Clear all the strings that display information about currently 
 * selected server.
 */
void MPIClearServerInfo(void)
{
	memset(&str_sinfo, 0, sizeof(str_sinfo));
}

/*
 * Copies string from src to dest, replacing 'match' chars with 'rep'.
 */
void MPITranslateString(char *dest, const char *src, char match, 
						const char *rep)
{
	char *dp = dest;
	const char *sp = src;
	
	for(; *sp; sp++)
		if(*sp == match)
		{
			strcpy(dp, rep);
			dp += strlen(rep);
		}
		else
		{
			*dp++ = *sp;
		}
	*dp = 0;
}

/*
 * Update the strings that display information about currently selected
 * server. This is called when the server selection changes in the list.
 */
void MPIUpdateServerInfo(ui_object_t *ob)
{
	char str[256];
	serverinfo_t info;
	int success = (masterlist?
		  N_MasterGet(lstit_found[lst_found.selection].data2, &info)
		: N_GetHostInfo(lstit_found[lst_found.selection].data2, &info));

	if(!success) 
	{
		MPIClearServerInfo();
		return;
	}

	strcpy(str_sinfo.desc, info.description);
	sprintf(str_sinfo.version, "%i", info.version);
	if(info.ping)
		sprintf(str_sinfo.ping, "%i ms", info.ping);
	else
		strcpy(str_sinfo.ping, "?");
	strcpy(str_sinfo.game, info.game);
	MPITranslateString(str_sinfo.names, info.clientNames, ';', ", ");
	MPITranslateString(str, info.pwads, ';', ", ");
	strcpy(str_sinfo.pwads, info.gameMode);
	if(info.gameConfig[0]) 
	{
		strcat(str_sinfo.pwads, " ");
		strcat(str_sinfo.pwads, info.gameConfig);
	}
	if(info.pwads[0])
	{
		strcat(str_sinfo.pwads, " (");
		strcat(str_sinfo.pwads, str);
		strcat(str_sinfo.pwads, ")");
	}

	W_GetIWADFileName(str, sizeof(str));
	sprintf(str_sinfo.warning, "WARNING:\bThis server is using %s (%x), "
		"but you have %s (%x). Errors may occur during game play.", 
		info.iwad, info.wadNumber, str, my_crc);
}

/*
 * Draw a framed text box.
 */
void MPIServerInfoDrawer(ui_object_t *ob)
{
	UI_DrawHelpBox(ob->x, ob->y, ob->w, ob->h, 1, ob->data);
}

/*
 * The "Public Server" and "Master TCP/IP Address" objects are hidden
 * when the TCP/IP protocol is not active.
 */
void MPIEnablePublic(void)
{
	UI_FlagGroup(ob_server, 1, UIF_HIDDEN, !N_UsingInternet());
	UI_FlagGroup(ob_server, 2, UIF_HIDDEN, !N_UsingInternet());
}

void MPIToggleMasterItems(ui_object_t *ob)
{
	UI_FlagGroup(page_server.objects, 1, UIF_DISABLED, UIFG_XOR);
}

void MPIShowProtocolSettings(ui_object_t *ob)
{
	int i;

	for(i = 1; i <= 4; i++)
	{
		UI_FlagGroup(page_protocol.objects, i, UIF_HIDDEN, 
			i != lst_protocol.selection + 1);
	}
}

void MPIGotoPage(ui_object_t *ob)
{
	UI_SetPage( (ui_page_t*) ob->data);
}

void MPIGoBack(ui_object_t *ob)
{
	if(!ui_page->previous)
		UI_End();
	else
		UI_SetPage(ui_page->previous);
}

void MPISetupProtocol(ui_object_t *ob)
{
	// Update the variables.
	nptActive = lst_protocol.selection;
	nptIPPort = atoi(str_ipport);
	nptModem = lst_modem.selection;
	nptSerialPort = lst_ports.selection;
	nptSerialBaud = lstit_rates[lst_baud.selection].data;
	nptSerialParity = lst_parity.selection;
	nptSerialStopBits = lst_stopbit.selection;
	nptSerialFlowCtrl = lst_flow.selection;
	
	// Shut down the previous active service provider.
	N_ShutdownService();

	// Init with new provider, return to previous page if successful.
	if(N_InitService(lstit_protocols[nptActive].data,
		page_protocol.previous == &page_server))
	{
		// Show and hide the appropriate objects on the Client Setup page.
		UI_FlagGroup(ob_client, 1, UIF_HIDDEN, nptActive != 0);
		UI_FlagGroup(ob_client, 2, UIF_HIDDEN, nptActive != 2);
		UI_FlagGroup(ob_client, 3, UIF_DISABLED, nptActive != 0);
		searching = false; /*(nptActive != 0 && nptActive != 2);*/
		lst_found.count = 0;
		MPIClearServerInfo();
		// Go back to Server or Client Setup.
		MPIGoBack(ob);
	}

	MPIEnablePublic();
}

void MPIStartServer(ui_object_t *ob)
{
	// Update the variables.
	Con_SetString("server-name", str_server);
	Con_SetString("server-info", str_desc);
	Con_SetString("net-master-address", str_masterip);
	//masterPort = atoi(str_masterport);
	// Start the server.
	Con_Execute("net server start", false);
	UI_End();
}

void MPISearch(ui_object_t *ob)
{
	if(retrieving) return;
	searching = true;
	N_ShutdownService();
	// Make sure the search address is right.
	// Update the proper variable.
	if(nptActive == 0) 
		Con_SetString("net-ip-address", str_ipaddr);
	if(nptActive == 2)
		Con_SetString("net-modem-phone", str_phone);
	N_InitService(lstit_protocols[nptActive].data, false);
	N_LookForHosts();
}

/*
 * Formats the given serverinfo_t into a list-viewable tabbed string.
 */
void MPIFormatServerInfo(char *dest, serverinfo_t *info)
{
	sprintf(dest, "%s\t%i / %i players\t%s\t%s", info->name,
			info->numPlayers, info->maxPlayers, info->map, info->iwad);
}

/*
 * "Located Servers" list ticker.
 */
void MPIUpdateFound(ui_object_t *ob)
{
	static int counter = 0;
	int num, i, k;

	// Call list ticker.
	UIList_Ticker(ob);
	
	// Show IWAD warning?
	UI_FlagGroup(ob_client, 5, UIF_HIDDEN, !(lst_found.count >= 1 
		&& lst_found.selection >= 0
		&& lstit_found[lst_found.selection].data != -1
		&& lstit_found[lst_found.selection].data != (int) my_crc));
	
	if(searching)
	{
		// Update at one second intervals.
		if(counter-- > 0) return;
		counter = 35;			
		
		// How many found?
		masterlist = false;
		num = N_GetHostCount();
		if(!num)
		{
			lst_found.count = 1;
			strcpy(lstit_found[0].text, "(Searching...)");
			lstit_found[0].data = -1;
			UI_FlagGroup(ob_client, 4, UIF_DISABLED, true);
		}
		else
		{
			serverinfo_t info;
			if(num > MAX_FOUND) num = MAX_FOUND;
			for(i = 0; i < num; i++)
			{
				memset(&info, 0, sizeof(info));
				N_GetHostInfo(i, &info);
				MPIFormatServerInfo(lstit_found[i].text, &info);
				lstit_found[i].data = info.wadNumber;
				lstit_found[i].data2 = i;
			}
			lst_found.count = num;
			UI_FlagGroup(ob_client, 4, UIF_DISABLED, false);
		}
		UI_InitColumns(ob);		
		MPIUpdateServerInfo(NULL);
	}

	if(retrieving && N_MADone())
	{
		serverinfo_t info;

		// The list has been retrieved.
		retrieving = false;
		masterlist = true;
		num = N_MasterGet(0, 0);
		for(i = 0, k = 0; i < num && k < MAX_FOUND; i++)
		{
			N_MasterGet(i, &info);
			
			// Is this suitable?
			if(info.version != DOOMSDAY_VERSION
				|| stricmp(info.game, gx.Get(DD_GAME_ID))
				|| !info.canJoin) continue;

			MPIFormatServerInfo(lstit_found[k].text, &info);
			lstit_found[k].data = info.wadNumber;
			// Connection will be formed using this index.
			lstit_found[k].data2 = i;
			k++;
		}
		lst_found.count = k;
		UI_FlagGroup(ob_client, 4, UIF_DISABLED, !k);
		UI_InitColumns(ob);
		MPIUpdateServerInfo(NULL);
	}
}

void MPIRetrieve(ui_object_t *ob)
{
	ui_object_t *list = UI_FindObject(ob_client, 0, UIF_SERVER_LIST);

	if(retrieving) return;	// Already retrieving!
	searching = false;
	retrieving = true;
	// Disable Connect button.
	UI_FlagGroup(ob_client, 4, UIF_DISABLED, true);
	MPIClearServerInfo();
	lst_found.count = 1;
	strcpy(lstit_found[0].text, "(Retrieving servers from master...)");
	lstit_found[0].data = -1;
	UI_InitColumns(list);
	// Update master settings.
	Con_SetString("net-master-address", str_masterip);
	//masterPort = atoi(str_masterport);
	// Get the list.
	N_MAPost(MAC_REQUEST);
	N_MAPost(MAC_WAIT);
}

void MPIConnect(ui_object_t *ob)
{
	char buf[80];

	sprintf(buf, "net %sconnect %i", masterlist? "m" : "", 
		lstit_found[lst_found.selection].data2);
	if(Con_Execute(buf, false))
	{
		// Success.
		UI_End();
	}
}

/*
 * Draw the per-page information about the currently displayed protocol.
 * The text is retrieved from the Help strings.
 */
void MPIHelpDrawer(ui_object_t *ob)
{
	int yPos[4] = { 250, 80, 530, 900 };
	int selection = lst_protocol.selection;
	static int lastSelection = -1;
	static void *help = NULL;
	char *text;

	// Has the page changed?
	if(lastSelection != selection)
	{
		// Retrieve new help text.
		lastSelection = selection;
		help = DH_Find( lstit_protocols[selection].text );
	}

	if((text = DH_GetString(help, HST_DESCRIPTION)) != NULL)
	{
		UI_TextOutWrap(text, ob->x, UI_ScreenY(yPos[selection]), ob->w,
			UI_ScreenH(980 - yPos[selection]));
	}
}

/*
 * The GUI interface for setting up a server/client.
 */
void DD_NetSetup(int server_mode)
{
	int i;

	if(server_mode)
	{
		// Prepare Server Setup.
		UI_InitPage(&page_server, ob_server);
		sprintf(page_server.title, "Doomsday %s Server Setup",
				DOOMSDAY_VERSION_TEXT);
		strcpy(str_server, serverName);
		strcpy(str_desc, serverInfo);
		// Disable the Master Address and Port edit boxes.
		UI_FlagGroup(ob_server, 1, UIF_DISABLED, !masterAware);
	}
	else
	{
		// Prepare Client Setup.
		UI_InitPage(&page_client, ob_client);
		sprintf(page_client.title, "Doomsday %s Client Setup",
				DOOMSDAY_VERSION_TEXT);
		strcpy(str_ipaddr, nptIPAddress);
		strcpy(str_phone, nptPhoneNum);
		UI_FlagGroup(ob_client, 1, UIF_HIDDEN, nptActive != 0);
		UI_FlagGroup(ob_client, 2, UIF_HIDDEN, nptActive != 2);
		UI_FlagGroup(ob_client, 3, UIF_DISABLED, nptActive != 0);
		UI_FlagGroup(ob_client, 4, UIF_DISABLED, true);
		lst_found.count = 0;
		searching = (nptActive != 0 && nptActive != 2);
		my_crc = W_CRCNumber();
		UI_FlagGroup(ob_client, 5, UIF_HIDDEN, true);
		MPIClearServerInfo();
	}
	strcpy(str_masterip, masterAddress);
	// Prepare Protocol Setup.
	UI_InitPage(&page_protocol, ob_protocol);
	strcpy(page_protocol.title, "Network Setup");
	page_protocol.previous = server_mode? &page_server : &page_client;
	sprintf(str_ipport, "%.10i", nptIPPort);
	lst_protocol.selection = nptActive;
	// Hide the 'wrong' settings.
	for(i = 1; i <= 4; i++)
	{
		UI_FlagGroup(ob_protocol, i, UIF_HIDDEN, i != nptActive+1);
	}
	// List of modems.
	memset(lstit_modems, 0, sizeof(lstit_modems));
	if((lst_modem.count = N_GetServiceProviderCount(NSP_MODEM)) > 0)
	{
		for(i = 0; i < lst_modem.count; i++)
		{
			N_GetServiceProviderName(NSP_MODEM, i, lstit_modems[i].text, 
				sizeof(lstit_modems[i].text));
		}
		lst_modem.selection = nptModem;
	}
	else
	{
		lst_modem.count = 1;
		lst_modem.selection = 0;
		strcpy(lstit_modems[0].text, "(No modems detected)");
	}
	// List of serial ports.
	memset(lstit_ports, 0, sizeof(lstit_ports));
	if((lst_ports.count = N_GetServiceProviderCount(NSP_SERIAL)) > 0)
	{
		for(i = 0; i < lst_ports.count; i++)
		{
			N_GetServiceProviderName(NSP_SERIAL, i, lstit_ports[i].text,
				sizeof(lstit_ports[i].text));
		}
		lst_ports.selection = nptSerialPort;
	}
	else
	{
		lst_ports.count = 1;
		lst_ports.selection = 0;
		strcpy(lstit_ports[0].text, "(No ports detected)");
	}
	for(i = 0; i < lst_baud.count; i++)
		if(nptSerialBaud >= ((uidata_listitem_t*)lst_baud.items)[i].data)
			lst_baud.selection = i;
	lst_parity.selection = nptSerialParity;
	lst_stopbit.selection = nptSerialStopBits;
	lst_flow.selection = nptSerialFlowCtrl;

	UI_Init();
	UI_SetPage(N_IsAvailable()? (server_mode? &page_server : &page_client) 
		: &page_protocol);

	MPIEnablePublic();
	CP_InitCvarSliders(ob_server);
}
