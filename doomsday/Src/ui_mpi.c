
//**************************************************************************
//**
//** UI_MPI.C
//**
//** Multiplayer Setup Interface.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "de_base.h"
#include "de_system.h"
#include "de_network.h"
#include "de_console.h"
#include "de_ui.h"

// MACROS ------------------------------------------------------------------

#define MAX_MODEMS		10	// Nobody has that many modems!
#define MAX_FOUND		32
#define NUMITEMS(x)		(sizeof(x)/sizeof(uidata_listitem_t))

// TYPES -------------------------------------------------------------------

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

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

char str_server[101];
char str_desc[201];
char str_masterip[128];
char str_masterport[21];
char str_ipport[11];
char str_comport[4];
char str_ipaddr[128];
char str_phone[31];

uidata_listitem_t lstit_protocols[] =
{
	{ "TCP/IP", JTNET_SERVICE_TCPIP },			// Group 1
	{ "IPX", JTNET_SERVICE_IPX },
	{ "Modem", JTNET_SERVICE_MODEM },			// Group 3
	{ "Serial Link", JTNET_SERVICE_SERIAL }		// Group 4
};
uidata_listitem_t lstit_modems[MAX_MODEMS];
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
	{ "No" },
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
uidata_edit_t ed_masterport = { str_masterport, 20 };
uidata_list_t lst_protocol = { lstit_protocols, NUMITEMS(lstit_protocols) };
uidata_edit_t ed_ipport = { str_ipport, 10 };
uidata_list_t lst_modem = { lstit_modems };
uidata_edit_t ed_comport = { str_comport, 3 };
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
	{ UI_TEXT,		0,	0,	50, 150,	0, 80,		"Server Name:",		UIText_Drawer },
	{ UI_EDIT,		0,	0,	300, 150,	500, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_server },
	{ UI_TEXT,		0,	0,	50, 300,	0, 80,		"Description:",		UIText_Drawer },
	{ UI_EDIT,		0,	0,	300, 300,	650, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_desc },
	{ UI_TEXT,		1,	0,	50, 450,	0, 80,		"Master IP Address:", UIText_Drawer },
	{ UI_EDIT,		1,	0,	300, 450,	400, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_masterip },
	{ UI_TEXT,		1,	0,	50, 600,	0, 80,		"Master IP Port:",	UIText_Drawer },
	{ UI_EDIT,		1,	0,	300, 600,	200, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_masterport },
	{ UI_BUTTON,	0,	0,	50, 900,	200, 100,	"Protocol Setup",	UIButton_Drawer, UIButton_Responder, 0, MPIGotoPage, &page_protocol },
	{ UI_BUTTON2,	0,	0,	300, 900,	200, 100,	"Inform Master",	UIButton_Drawer, UIButton_Responder, 0, MPIToggleMasterItems, &masterAware },
	{ UI_BUTTON,	0,	UIF_DEFAULT,	750, 900,	200, 100, "Start",	UIButton_Drawer, UIButton_Responder, 0, MPIStartServer },
	{ UI_NONE }
};

ui_object_t ob_client[] =
{
	{ UI_LIST,		0,	0,	25, 50,		950, 450,	"Located Servers:",	UIList_Drawer, UIList_Responder, MPIUpdateFound, 0, &lst_found },
	{ UI_TEXT,		1,	0,	50, 550,	0, 80,		"TCP/IP Address:",	UIText_Drawer },
	{ UI_EDIT,		1,	0,	300, 550,	400, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_ipsearch },
	{ UI_TEXT,		2,	0,	50, 550,	0, 80,		"Phone Number:",	UIText_Drawer },
	{ UI_EDIT,		2,	0,	300, 550,	400, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_phone },
	{ UI_TEXT,		3,	0,	50, 650,	0, 80,		"Master IP Address:", UIText_Drawer },
	{ UI_EDIT,		3,	0,	300, 650,	400, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_masterip },
	{ UI_TEXT,		3,	0,	50, 750,	0, 80,		"Master IP Port:",	UIText_Drawer },
	{ UI_EDIT,		3,	0,	300, 750,	200, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_masterport },
	{ UI_BUTTON,	0,	0,	25, 900,	200, 100,	"Protocol Setup",	UIButton_Drawer, UIButton_Responder, 0, MPIGotoPage, &page_protocol },
	{ UI_BUTTON,	0,	UIF_DEFAULT,	250, 900,	200, 100, "Search",	UIButton_Drawer, UIButton_Responder, 0, MPISearch },
	{ UI_BUTTON,	3,	0,	475, 900,	200, 100,	"Get From Master",	UIButton_Drawer, UIButton_Responder, 0, MPIRetrieve },
	{ UI_BUTTON,	4,	0,	775, 900,	200, 100,	"Connect",			UIButton_Drawer, UIButton_Responder, 0, MPIConnect },
	{ UI_TEXT,		5,	0,	25, 505,	950, 40,	"WARNING: This server seems to be using a different IWAD than you.", UIText_Drawer },
	{ UI_NONE }
};

ui_object_t ob_protocol[] =
{
	{ UI_LIST,		0,	0,	0, 250,		400, 400,	"Active Protocol:",	UIList_Drawer, UIList_Responder, UIList_Ticker, MPIShowProtocolSettings, &lst_protocol },
	{ UI_TEXT,		1,	0,	450, 400,	0, 80,		"TCP/IP Port:",		UIText_Drawer },
	{ UI_EDIT,		1,	0,	600, 400,	200, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_ipport },
	{ UI_LIST,		3,	0,	450, 250,	550, 400,	"Modems:",			UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_modem },
	{ UI_TEXT,		4,	0,	450, 50,	0, 80,		"COM Port:",		UIText_Drawer },
	{ UI_EDIT,		4,	0,	600, 50,	100, 80,	"",					UIEdit_Drawer, UIEdit_Responder, 0, 0, &ed_comport },
	{ UI_LIST,		4,	0,	450, 200,	250, 300,	"Baud Rate:",		UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_baud },
	{ UI_LIST,		4,	0,	750, 200,	250, 200,	"Parity:",			UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_parity },
	{ UI_LIST,		4,	0,	450, 600,	250, 200,	"Stop Bits:",		UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_stopbit },
	{ UI_LIST,		4,	0,	750, 550,	250, 250,	"Flow Control:",	UIList_Drawer, UIList_Responder, UIList_Ticker, 0, &lst_flow },
	{ UI_BUTTON,	0,	0,	50, 900,	200, 100,	"Cancel",			UIButton_Drawer, UIButton_Responder, 0, MPIGoBack },
	{ UI_BUTTON,	0,	0,	750, 900,	200, 100,	"OK",				UIButton_Drawer, UIButton_Responder, 0, MPISetupProtocol },
	{ UI_NONE }
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean searching = false;
static boolean retrieving = false;
static boolean masterlist = false;
static unsigned int my_crc = 0;

// CODE --------------------------------------------------------------------

void MPIToggleMasterItems(ui_object_t *ob)
{
	UI_FlagGroup(page_server.objects, 1, UIF_DISABLED, UIFG_XOR);
}

void MPIShowProtocolSettings(ui_object_t *ob)
{
	int i;

	for(i=1; i<=4; i++)
	{
		UI_FlagGroup(page_protocol.objects, i, UIF_HIDDEN, 
			i != lst_protocol.selection+1);
	}
}

void MPIGotoPage(ui_object_t *ob)
{
	UI_SetPage( (ui_page_t*) ob->data);
}

void MPIGoBack(ui_object_t *ob)
{
	UI_SetPage(ui_page->previous);
}

void MPISetupProtocol(ui_object_t *ob)
{
	// Update the variables.
	nptActive = lst_protocol.selection;
	nptIPPort = atoi(str_ipport);
	nptModem = lst_modem.selection;
	nptSerialPort = atoi(str_comport);
	nptSerialBaud = lstit_rates[lst_baud.selection].data;
	nptSerialParity = lst_parity.selection;
	nptSerialStopBits = lst_stopbit.selection;
	nptSerialFlowCtrl = lst_flow.selection;
	
	// Shut down the previous active service provider.
	N_ShutdownService();

	// Init with new provider, return to previous page if successful.
	if(N_InitService(lstit_protocols[nptActive].data))
	{
		// Show and hide the appropriate objects on the Client Setup page.
		UI_FlagGroup(ob_client, 1, UIF_HIDDEN, nptActive != 0);
		UI_FlagGroup(ob_client, 2, UIF_HIDDEN, nptActive != 2);
		UI_FlagGroup(ob_client, 3, UIF_DISABLED, nptActive != 0);
		searching = (nptActive != 0 && nptActive != 2);
		// Go back to Server or Client Setup.
		MPIGoBack(ob);
	}
}

void MPIStartServer(ui_object_t *ob)
{
	// Update the variables.
	Con_SetString("server-name", str_server);
	Con_SetString("server-info", str_desc);
	Con_SetString("net-master-address", str_masterip);
	masterPort = atoi(str_masterport);
	// Start the server.
	Con_Execute("net server start", false);
	UI_End();
}

void MPISearch(ui_object_t *ob)
{
	if(retrieving) return;
	searching = true;
	// Make sure the search address is right.
	if(nptActive == 0 || nptActive == 2)
	{
		N_ShutdownService();
		// Update the proper variable.
		if(nptActive == 0) 
			Con_SetString("net-ip-address", str_ipaddr);
		if(nptActive == 2)
			Con_SetString("net-modem-phone", str_phone);
		N_InitService(lstit_protocols[nptActive].data);
	}
}

// 'Found Servers' list ticker.
void MPIUpdateFound(ui_object_t *ob)
{
	static int counter = 0;
	jtnetserver_t *buffer;
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
		num = jtNetGetServerInfo(NULL, 0);
		if(!num)
		{
			lst_found.count = 1;
			strcpy(lstit_found[0].text, "(Searching...)");
			lstit_found[0].data = -1;
			UI_FlagGroup(ob_client, 4, UIF_DISABLED, true);
		}
		else
		{
			buffer = malloc(sizeof(jtnetserver_t) * num);
			jtNetGetServerInfo(buffer, num);
			if(num > MAX_FOUND) num = MAX_FOUND;
			for(i = 0; i < num; i++)
			{
				sprintf(lstit_found[i].text, "%s\t%s\t%s\t%i/%i Players\t%s",
					buffer[i].app, buffer[i].name, buffer[i].description,
					buffer[i].players, buffer[i].maxPlayers,
					buffer[i].canJoin? "Open" : "Closed");
				lstit_found[i].data = buffer[i].data[0];
			}
			free(buffer);
			lst_found.count = num;
			UI_FlagGroup(ob_client, 4, UIF_DISABLED, false);
		}
		UI_InitColumns(ob);		
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

			sprintf(lstit_found[k].text, "%s\t%s\t%s\t%i/%i Players\tv%i",
				info.name, info.description, info.map,
				info.players, info.maxPlayers, info.version);
			lstit_found[k].data = info.data[0];
			// Connection will be formed using this index.
			lstit_found[k].data2 = i;
			k++;
		}
		lst_found.count = k;
		UI_FlagGroup(ob_client, 4, UIF_DISABLED, !k);
		UI_InitColumns(ob);
	}
}

void MPIRetrieve(ui_object_t *ob)
{
	if(retrieving) return;	// Already retrieving!
	searching = false;
	retrieving = true;
	// Disable Connect button.
	UI_FlagGroup(ob_client, 4, UIF_DISABLED, true);
	lst_found.count = 1;
	strcpy(lstit_found[0].text, "(Retrieving servers from master...)");
	lstit_found[0].data = -1;
	UI_InitColumns(ob_client);
	// Update master settings.
	Con_SetString("net-master-address", str_masterip);
	masterPort = atoi(str_masterport);
	// Get the list.
	N_MAPost(MAC_REQUEST);
	N_MAPost(MAC_WAIT);
}

void MPIConnect(ui_object_t *ob)
{
	char buf[80];

	sprintf(buf, "net %sconnect %i", masterlist? "m" : "", 
		masterlist? lstit_found[lst_found.selection].data2 
		: lst_found.selection);
	if(Con_Execute(buf, false))
	{
		// Success.
		UI_End();
	}
}

void DD_NetSetup(int server_mode)
{
	int i, num;
	char **list;

	if(server_mode)
	{
		// Prepare Server Setup.
		UI_InitPage(&page_server, ob_server);
		sprintf(page_server.title, "Doomsday %s Server Setup", DOOMSDAY_VERSION_TEXT);
		strcpy(str_server, serverName);
		strcpy(str_desc, serverInfo);
		// Disable the Master Address and Port edit boxes.
		UI_FlagGroup(ob_server, 1, UIF_DISABLED, !masterAware);
	}
	else
	{
		// Prepare Client Setup.
		UI_InitPage(&page_client, ob_client);
		sprintf(page_client.title, "Doomsday %s Client Setup", DOOMSDAY_VERSION_TEXT);
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
	}
	strcpy(str_masterip, masterAddress);
	itoa(masterPort, str_masterport, 10);
	// Prepare Protocol Setup.
	UI_InitPage(&page_protocol, ob_protocol);
	strcpy(page_protocol.title, "Network Protocol Setup");
	page_protocol.previous = server_mode? &page_server : &page_client;
	itoa(nptIPPort, str_ipport, 10);
	lst_protocol.selection = nptActive;
	// Hide the 'wrong' settings.
	for(i=1; i<=4; i++)
	{
		UI_FlagGroup(ob_protocol, i, UIF_HIDDEN, i != nptActive+1);
	}
	// List of modems.
	memset(lstit_modems, 0, sizeof(lstit_modems));
	list = jtNetGetStringList(JTNET_MODEM_LIST, &num);
	if(list)
	{
		if(num > MAX_MODEMS) num = MAX_MODEMS;
		for(i=0; i<num; i++)
			strcpy(lstit_modems[i].text, list[i]);
	}
	lst_modem.count = num;
	lst_modem.selection = nptModem;
	itoa(nptSerialPort, str_comport, 10);
	for(i=0; i<lst_baud.count; i++)
		if(nptSerialBaud >= ((uidata_listitem_t*)lst_baud.items)[i].data)
			lst_baud.selection = i;
	lst_parity.selection = nptSerialParity;
	lst_stopbit.selection = nptSerialStopBits;
	lst_flow.selection = nptSerialFlowCtrl;

	UI_Init();
	UI_SetPage(netAvailable? (server_mode? &page_server : &page_client) 
		: &page_protocol);
}