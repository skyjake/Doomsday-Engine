/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Ker�en <jaakko.keranen@iki.fi>
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
 * sys_network.cpp: Low-Level Network Services
 *
 * This file is way too long.
 *
 * Low-level network routines, using DirectPlay 8. A replacement for 
 * jtNet2, which is no longer needed. Written in C++ to reduce the PITA 
 * factor of using COM. 
 *
 * Confirmed/Ordered messages are stored in the Sent Message Store (SMS)
 * when sending. Confirmations are received and sent when packets are 
 * requested in N_GetNextMessage(). Each player has his own SMS. Message 
 * ID history is maintained and checked to detect spurious duplicates 
 * (result of delayed/lost confirmation). Duplicates are confirmed, but 
 * ignored. Confirmation messages only contain the message ID (2 bytes 
 * long). N_Update() handles the removing of old confirmed messages and 
 * the resending of timed out messages. When an Ordered message is 
 * confirmed, the next queued Ordered message is sent. Messages in the 
 * SMS are in FIFO order.
 */

/*
 * $Log$
 * Revision 1.9  2004/01/08 12:22:05  skyjake
 * Merged from branch-nix
 *
 * Revision 1.8  2003/09/09 21:38:24  skyjake
 * Renamed mutex routines to Sys_Lock and Sys_Unlock
 *
 * Revision 1.7  2003/09/08 22:19:40  skyjake
 * Float timing loop, use timespan_t in tickers, style cleanup
 *
 * Revision 1.6  2003/09/03 20:53:49  skyjake
 * Added a proper GPL banner
 *
 * Revision 1.5  2003/06/30 00:02:52  skyjake
 * RANDOM_PACKET_LOSS, proper N_SMSReset, Huffman stats, -huffavg
 *
 * Revision 1.4  2003/06/28 16:00:14  skyjake
 * Use Huffman encoding when sending data
 *
 * Revision 1.3  2003/06/27 20:20:28  skyjake
 * Protect incoming message queue using a mutex
 *
 * Revision 1.2  2003/06/23 08:19:24  skyjake
 * Confirmed and Ordered packets done manually
 *
 * Revision 1.1  2003/05/23 22:00:34  skyjake
 * sys_network.c rewritten to C++, .cpp
 *
 */

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <dplay8.h>

// The 'no return value' warning is wrong and useless in the case of
// FixedMul and FixedDiv2.
#pragma warning (disable: 4035)

extern "C" 
{
#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_misc.h"

#include "ui_mpi.h"
}

// MACROS ------------------------------------------------------------------

// Uncomment to enable the byte frequency counter.
//#define COUNT_BYTE_FREQS

// Uncomment to test random packet loss.
//#define RANDOM_PACKET_LOSS

#define SEND_TIMEOUT		15000	// 15 seconds
#define RELEASE(x)			if(x) ((x)->Release(), (x)=NULL)

// The player context value is used to identify the host player 
// on serverside.
enum {
	PCTX_CLIENT,
	PCTX_SERVER
};

// Service provider listings.
enum splisttype_t {
	SPL_MODEM,
	SPL_SERIAL,
	NUM_SP_LISTS
};

// TYPES -------------------------------------------------------------------

typedef struct providerlist_s {
	DPN_SERVICE_PROVIDER_INFO *info;
	DWORD count;
} providerlist_t;

typedef struct hostnode_s {
	struct hostnode_s *next;
	int index;
	GUID instance;
	IDirectPlay8Address *address;
	IDirectPlay8Address *device;
	serverinfo_t info;
} hostnode_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

boolean	N_InitDPObject(boolean inServerMode);
void	N_StopLookingForHosts(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// Settings for the various network protocols.
// Most recently used provider: 0=TCP/IP, 1=IPX, 2=Modem, 3=Serial.
int			nptActive;
// TCP/IP:
char		*nptIPAddress = "";
int 		nptIPPort = 0;	// This is the port *we* use to communicate.
// Modem:
int			nptModem = 0;	// Selected modem device index.
char		*nptPhoneNum = "";
// Serial:
int			nptSerialPort = 0;
int			nptSerialBaud = 57600;
int			nptSerialStopBits = 0;
int			nptSerialParity = 0;
int			nptSerialFlowCtrl = 4;

// Operating mode of the currently active service provider.
serviceprovider_t netCurrentProvider = NSP_NONE;
boolean		netServerMode = false;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// {7DDFA9A0-84EA-11d3-B689-E29406BD95EC}
static const GUID gDoomsdayGUID = { 0x7ddfa9a0, 0x84ea, 0x11d3, { 0xb6, 0x89, 0xe2, 0x94, 0x6, 0xbd, 0x95, 0xec } };

// As a matter of convenience, the return codes are always stored in 
// this global variable.
static HRESULT hr;

static IDirectPlay8Server *gServer;
static IDirectPlay8Client *gClient;
static IDirectPlay8Address *gDevice, *gHostAddress;
static providerlist_t gProviders[NUM_SP_LISTS];
static DPN_APPLICATION_DESC gAppInfo;
static serverinfo_t gSessionData;

static hostnode_t *gHostRoot;
static int gHostCount;
static DPNHANDLE gEnumHandle;

static char *protocolNames[NUM_NSP] = { 
	"??", "TCP/IP", "IPX", "Modem", "Serial Link"
};

#ifdef COUNT_BYTE_FREQS
static uint gByteCounts[256];
static uint gTotalByteCount;
#endif

// CODE --------------------------------------------------------------------

#if 0
/*
 * Converts the normal string to a wchar string. MaxLen is the length of
 * the destination string.
 */
void N_StrWide(WCHAR *wstr, const char *str, int maxLen)
{
	MultiByteToWideChar(CP_ACP, 0, str, -1, wstr, maxLen); 
}

/*
 * Converts the wchar string to a normal string. MaxLen is the length of
 * the destination string.
 */
void N_StrNarrow(char *str, const WCHAR *wstr, int maxLen)
{
	WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, maxLen, 0, 0);
}

/*
 * Returns a pointer to the host with the specified index.
 */
hostnode_t *N_GetHost(int index)
{
	for(hostnode_t *it = gHostRoot; it; it = it->next)
		if(it->index == index) 
			return it;
	return NULL;
}

/*
 * Resets all data associated with the given host node.
 */
void N_ResetHost(hostnode_t *host)
{
	// Free the address objects. They were created during enumeration.
	RELEASE( host->address );
	RELEASE( host->device );

	memset(&host->info, 0, sizeof(host->info));
}

/*
 * Allocate a new node from the list of enumerated hosts.
 */
hostnode_t* N_NewHost(GUID newInstance)
{
	// First we must check that the instance really is new.
	for(hostnode_t *it = gHostRoot; it; it = it->next)
		if(it->instance == newInstance) 
		{
			N_ResetHost(it);
			return it;
		}

	hostnode_t *newNode = (hostnode_t*) calloc(sizeof(hostnode_t), 1);
	newNode->index = gHostCount;
	newNode->next = gHostRoot;
	newNode->instance = newInstance;
	++gHostCount;
	return gHostRoot = newNode;
}

/*
 * Empties the list of enumerated hosts.
 */
void N_ClearHosts(void)
{
	while(gHostRoot)
	{
		hostnode_t *removed = gHostRoot;
		gHostRoot = gHostRoot->next;
		--gHostCount;
		N_ResetHost(removed);
		free(removed);
	}
}

/*
 * Free the DP buffer.
 */
void N_ReturnBuffer(void *handle)
{
	if(netServerMode)
	{
		gServer->ReturnBuffer(handle, 0);
	}
	else
	{
		gClient->ReturnBuffer(handle, 0);
	}
}

/*
 * Message receiver. Used by both the server and client message handlers.
 * The messages are placed in the message queue.
 */
void N_ReceiveMessage(PDPNMSG_RECEIVE received)
{
	netmessage_t *msg = (netmessage_t*) calloc(sizeof(netmessage_t), 1);

	msg->sender = received->dpnidSender;
	msg->data   = received->pReceiveData;
	msg->size   = received->dwReceiveDataSize;
	msg->handle = received->hBufferHandle;

	// The message queue will handle the message from now on.
	N_PostMessage(msg);
}

/*
 * Server message handler callback for DirectPlay. 
 * NOTE: This will get called at arbitrary times, so be careful.
 */
HRESULT WINAPI N_ServerMessageHandler
	(PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage)
{
	DPNMSG_ENUM_HOSTS_QUERY *msgEnum;
	DPNMSG_CREATE_PLAYER *msgNewPlr;
	DPNMSG_DESTROY_PLAYER *msgDelPlr;
	netevent_t event;

	switch(dwMessageType)
	{
	case DPN_MSGID_ENUM_HOSTS_QUERY:
		// Note that locking doesn't affect queries.

		// A description of the server: name, info, game, etc.
		Sv_GetInfo(&gSessionData);
		
		msgEnum = (PDPNMSG_ENUM_HOSTS_QUERY) pMessage;
		msgEnum->pvResponseData = &gSessionData;
		msgEnum->dwResponseDataSize = sizeof(gSessionData);
		break;

	case DPN_MSGID_INDICATE_CONNECT:
		// If the server is locked, attempts to connect are canceled.
		if(Sv_GetNumConnected() >= sv_maxPlayers) return DPNERR_GENERIC;
		break;

	case DPN_MSGID_CREATE_PLAYER:
		msgNewPlr = (PDPNMSG_CREATE_PLAYER) pMessage;
		// Is this the server's player? If so, nothing needs to be done.
		if(msgNewPlr->pvPlayerContext != (PVOID) PCTX_SERVER)
		{
			// Post a net event. It will be processed later in N_Update().
			event.type = NE_CLIENT_ENTRY;
			event.id = msgNewPlr->dpnidPlayer;
			N_NEPost(&event);
		}
		break;

	case DPN_MSGID_DESTROY_PLAYER:
		msgDelPlr = (PDPNMSG_DESTROY_PLAYER) pMessage;
		// Post a net event. It will be processed later in N_Update().
		event.type = NE_CLIENT_EXIT;
		event.id = msgDelPlr->dpnidPlayer;
		N_NEPost(&event);
		break;

	case DPN_MSGID_RECEIVE:
		N_ReceiveMessage( (PDPNMSG_RECEIVE) pMessage );
		return DPNSUCCESS_PENDING;
	}

	// The default return value.
	return DPN_OK;
}

/*
 * Client message handler callback for DirectPlay.
 * NOTE: This will get called at arbitrary times, so be careful.
 */
HRESULT WINAPI N_ClientMessageHandler
	(PVOID pvUserContext, DWORD dwMessageType, PVOID pMessage)
{
	DPNMSG_ENUM_HOSTS_RESPONSE *msgEnum;
	hostnode_t *host;
	netevent_t event;

	switch(dwMessageType)
	{
	case DPN_MSGID_ENUM_HOSTS_RESPONSE:
		msgEnum = (PDPNMSG_ENUM_HOSTS_RESPONSE) pMessage;

		// Add the information to the list of found hosts.
		host = N_NewHost(msgEnum->pApplicationDescription->guidInstance);

		// These are released when the list of hosts is cleared.
		msgEnum->pAddressSender->Duplicate( &host->address );
		msgEnum->pAddressDevice->Duplicate( &host->device );

		// Does the response data size match our expectations?
		if(msgEnum->dwResponseDataSize == sizeof(serverinfo_t))
		{
			memcpy(&host->info, msgEnum->pvResponseData, 
				sizeof(serverinfo_t));
		}
		
		// Some extra information that DP is kind enough to provide.
		host->info.ping = msgEnum->dwRoundTripLatencyMS;
		return DPN_OK;

	case DPN_MSGID_RECEIVE:
		N_ReceiveMessage( (PDPNMSG_RECEIVE) pMessage );
		return DPNSUCCESS_PENDING;

	case DPN_MSGID_TERMINATE_SESSION:
		event.type = NE_END_CONNECTION;
		event.id = 0;
		N_NEPost(&event);
		break;
	}

	// The default return value.
	return S_OK;
}

/*
 * Prints the URL version of the DP address to the console.
 */
void N_PrintAddress(char *title, IDirectPlay8Address *address)
{
	char buf[256];
	DWORD size = sizeof(buf);

	memset(buf, 0, sizeof(buf));
	address->GetURLA(buf, &size);
	Con_Printf("%s: %s\n", title, buf);
}

/*
 * Enumerates service providers. Returns the number of providers found.
 */
int N_EnumProviders(const GUID* providerGuid, providerlist_t *list)
{
	DWORD infoSize = 0;

	// Enumerate modem service providers.
	gClient->EnumServiceProviders(providerGuid, NULL, list->info,
		&infoSize, &list->count, 0);

	// Allocate memory for the info. This is freed when DP is shut down.
	list->info = (DPN_SERVICE_PROVIDER_INFO*) calloc(infoSize, 1);

	// Get the data.
	gClient->EnumServiceProviders(providerGuid, NULL, list->info,
		&infoSize, &list->count, 0);

	return list->count;
}

/*
 * Returns the number of enumerated service providers of the given type.
 */
unsigned int N_GetServiceProviderCount(serviceprovider_t type)
{
	if(type == NSP_MODEM) return gProviders[SPL_MODEM].count;
	if(type == NSP_SERIAL) return gProviders[SPL_SERIAL].count;
	return 0;
}

/*
 * Returns the name of the requested service provider, or false.
 * Indices are sequential, starting from zero. The first index that returns
 * false is the last one.
 */
boolean N_GetServiceProviderName
	(serviceprovider_t type, unsigned int index, char *name, int maxLength)
{
	providerlist_t *list = 
		( type == NSP_MODEM?  gProviders + SPL_MODEM 
		: type == NSP_SERIAL? gProviders + SPL_SERIAL 
		: NULL );

	// Bag args?
	if(!list || index >= list->count) 
		return false;

	N_StrNarrow(name, list->info[index].pwszName, maxLength);
	return true;
}

/*
 * Returns a pointer to GUID of a service provider.
 */
const GUID* N_GetServiceProviderGUID(splisttype_t listType, int index)
{
	providerlist_t *list = gProviders + listType;

	if(index < 0 || (unsigned) index >= list->count) 
		return NULL;

	return &list->info[index].guid;
}

/*
 * Prints a list of service provider names into the console.
 */
void N_PrintProviders(const char *title, providerlist_t *list)
{
	Con_Printf("%s\n", title);
	for(unsigned i = 0; i < list->count; i++)
	{
		char temp[256];
		N_StrNarrow(temp, list->info[i].pwszName, sizeof(temp));
		Con_Printf("  %i: %s\n", i, temp);
	}
}

/*
 * Initializes DirectPlay by creating both the server and client objects.
 * They are not initialized yet, though.
 */
void N_InitDirectPlay(void)
{
	// Create the server object.
    if(FAILED(hr = CoCreateInstance(CLSID_DirectPlay8Server, NULL,
		CLSCTX_INPROC_SERVER, IID_IDirectPlay8Server, 
		(LPVOID*) &gServer)))
	{
		Con_Message("N_InitDirectPlay: Failed to create "
			"DP8Server [%x].\n", hr);
		gServer = NULL;
		return;
	}
    
	// Also create the client object. Only one will be initialized at 
	// a time, though.
	if(FAILED(hr = CoCreateInstance(CLSID_DirectPlay8Client, NULL,
		CLSCTX_INPROC_SERVER, IID_IDirectPlay8Client, 
		(LPVOID*) &gClient)))
	{
		Con_Message("N_InitDirectPlay: Failed to create "
			"DP8Client [%x].\n", hr);
		gClient = NULL;
		return;
	}

	VERBOSE( Con_Message("N_InitDirectPlay: Server=%p, Client=%p\n",
		gServer, gClient) );

	gDevice = NULL;

	// Enumeration of service providers requires us to init a DP object.
	// We'll use the client object.
	N_InitDPObject(false);

	N_EnumProviders(&CLSID_DP8SP_MODEM, &gProviders[SPL_MODEM]);
	N_EnumProviders(&CLSID_DP8SP_SERIAL, &gProviders[SPL_SERIAL]);

	if(verbose)
	{
		if(gProviders[SPL_MODEM].count > 0)
		{
			N_PrintProviders("N_InitDirectPlay: Modem service providers:",
				&gProviders[SPL_MODEM]);
		}

		if(gProviders[SPL_SERIAL].count > 0)
		{
			N_PrintProviders("N_InitDirectPlay: Serial link service "
				"providers:", &gProviders[SPL_SERIAL]);
		}
	}	

	gClient->Close(0);
}

/*
 * Shuts down DirectPlay by disposing of the server and client objects.
 */
void N_ShutdownDirectPlay(void)
{
	RELEASE( gDevice );
	RELEASE( gServer );
	RELEASE( gClient );

	// Clear the modem service providers list.
	for(int i = 0; i < NUM_SP_LISTS; i++)
		free(gProviders[i].info);

	memset(gProviders, 0, sizeof(gProviders));
}

/*
 * Returns true if DirectPlay is available.
 */
boolean N_CheckDirectPlay(void)
{
	return gServer && gClient;
}

/*
 * Initialize the low-level network subsystem. This is called always 
 * during startup (via Sys_Init()).
 */
void N_SystemInit(void)
{
	N_InitDirectPlay();
}

/*
 * Shut down the low-level network interface. Called during engine 
 * shutdown (not before).
 */
void N_SystemShutdown(void)
{
	N_ShutdownService();
	N_ShutdownDirectPlay();

#ifdef COUNT_BYTE_FREQS
	Con_Printf("Total number of bytes: %i\n", gTotalByteCount);
	for(int i = 0; i < 256; i++)
	{
		Con_Printf("%.12lf, ", gByteCounts[i] / (double) gTotalByteCount);
		if(i % 4 == 3) Con_Printf("\n");
	}
	Con_Printf("\n");
#endif
}

/*
 * Creates a DirectPlay8Address of the specified type.
 */
IDirectPlay8Address* N_NewAddress(serviceprovider_t provider)
{
	IDirectPlay8Address *newAddress = NULL;

	// Create the object.
	if(FAILED(hr = CoCreateInstance(CLSID_DirectPlay8Address, NULL,	
		CLSCTX_INPROC_SERVER, IID_IDirectPlay8Address, 
		(LPVOID*) &newAddress)))
	{
		return NULL;
	}

	hr = newAddress->SetSP( 
		  provider == NSP_TCPIP? &CLSID_DP8SP_TCPIP
		: provider == NSP_IPX?   &CLSID_DP8SP_IPX
		: provider == NSP_MODEM? &CLSID_DP8SP_MODEM
		: &CLSID_DP8SP_SERIAL );
	
	if(FAILED(hr))
	{
		RELEASE( newAddress );
		return NULL;
	}

	return newAddress;
}

/*
 * Create a DirectPlay8Address for the device that will be used.
 * The selection is done based on the npt* variables.
 * Returns true if successful.
 */
boolean N_SelectDevice(serviceprovider_t provider)
{
	// If there already is a device selected, release it first.
	RELEASE( gDevice );

	if((gDevice = N_NewAddress(provider)) == NULL)
		return false;

	DWORD data;
	const GUID *guid;
	WCHAR *wideStr;

	// Set extra values according to our npt* config.
	switch(provider)
	{
	case NSP_TCPIP:
		if(nptIPPort)
		{
			// Our TCP/IP port, used on this computer.
			data = nptIPPort;
			gDevice->AddComponent(DPNA_KEY_PORT, &data, sizeof(data), 
				DPNA_DATATYPE_DWORD);
		}
		break;

	case NSP_MODEM:
		if((guid = N_GetServiceProviderGUID(SPL_MODEM, nptModem)) != NULL)
		{
			gDevice->AddComponent(DPNA_KEY_DEVICE, guid, sizeof(GUID), 
				DPNA_DATATYPE_GUID);
		}
		break;

	case NSP_SERIAL:
		// Device to use.
		if((guid = N_GetServiceProviderGUID(SPL_SERIAL, nptSerialPort)) 
			!= NULL)
		{
			gDevice->AddComponent(DPNA_KEY_DEVICE, guid, sizeof(GUID),
				DPNA_DATATYPE_GUID);
		}

		// Baud rate.
		data = nptSerialBaud;
		gDevice->AddComponent(DPNA_KEY_BAUD, &data, sizeof(data), 
			DPNA_DATATYPE_DWORD);
		
		// Stop bits.
		wideStr = (nptSerialStopBits == 2? DPNA_STOP_BITS_TWO
			: nptSerialStopBits == 1? DPNA_STOP_BITS_ONE_FIVE
			: DPNA_STOP_BITS_ONE);
		gDevice->AddComponent(DPNA_KEY_STOPBITS, wideStr, 
			2*(wcslen(wideStr) + 1), DPNA_DATATYPE_STRING);
		
		// Parity.
		wideStr = (nptSerialParity == 3? DPNA_PARITY_MARK
			: nptSerialParity == 2? DPNA_PARITY_EVEN
			: nptSerialParity == 1? DPNA_PARITY_ODD
			: DPNA_PARITY_NONE);
		gDevice->AddComponent(DPNA_KEY_PARITY, wideStr, 
			2*(wcslen(wideStr) + 1), DPNA_DATATYPE_STRING);

		// Flow control.
		wideStr = (nptSerialFlowCtrl == 4? DPNA_FLOW_CONTROL_RTSDTR
			: nptSerialFlowCtrl == 3? DPNA_FLOW_CONTROL_DTR
			: nptSerialFlowCtrl == 2? DPNA_FLOW_CONTROL_RTS
			: nptSerialFlowCtrl == 1? DPNA_FLOW_CONTROL_XONXOFF
			: DPNA_FLOW_CONTROL_NONE);
		gDevice->AddComponent(DPNA_KEY_FLOWCONTROL, wideStr, 
			2*(wcslen(wideStr) + 1), DPNA_DATATYPE_STRING);
		break;
	}
	
	VERBOSE2( N_PrintAddress("Device address", gDevice) );
	return true;
}

/*
 * Initialize the DP object (either gServer or gClient). This is done at 
 * normal service init and after leaving a session.
 */
boolean N_InitDPObject(boolean inServerMode)
{
	return SUCCEEDED( hr = (inServerMode? 
		  gServer->Initialize(NULL, N_ServerMessageHandler, 0)
		: gClient->Initialize(NULL, N_ClientMessageHandler, 0)) );
}

/*
 * Initialize the chosen service provider each in server or client mode.
 * If a service provider has already been initialized, it will be shut 
 * down first. Returns true if successful.
 */
boolean N_InitService(serviceprovider_t provider, boolean inServerMode)
{
	if(!N_CheckDirectPlay()) return false;
	if(netCurrentProvider == provider && netServerMode == inServerMode)
	{
		// Nothing to change.
		return true;
	}

	// Get rid of the currently active service provider.
	N_ShutdownService();

	if(provider == NSP_NONE)
	{
		// This one's easy.
		return true;
	}

	if(!N_InitDPObject(inServerMode))
	{
		Con_Message("N_InitService: Failed to initialize DP [%x].\n", hr);
		return false;
	}

	if(!N_SelectDevice(provider))
	{
		Con_Message("N_InitService: Unable to select device for %s.\n", 
			protocolNames[provider]);
		
		// DP was already initialized, so close it.
		if(inServerMode)
			gServer->Close(0);
		else
			gClient->Close(0);
		return false;
	}

	// A smashing success.
	nptActive        = provider - 1; // -1 to match old values: 0=TCP/IP...
	netCurrentProvider = provider;
	netServerMode      = inServerMode;

	Con_Message("N_InitService: %s in %s mode.\n", N_GetProtocolName(),
		netServerMode? "server" : "client");
	return true;
}

/*
 * If a service provider has been initialized, it is shut down.
 */
void N_ShutdownService(void)
{
	if(!N_IsAvailable()) return; // Nothing to do.
	if(netgame)
	{
		// We seem to be shutting down while a netgame is running.
		Con_Execute(isServer? "net server close" : "net disconnect", true);
	}

	N_StopLookingForHosts();

	// The list of found hosts can be deleted.
	N_ClearHosts();

	if(netServerMode)
		gServer->Close(0);
	else
		gClient->Close(0);

	// Any queued messages will be destroyed.
	N_ClearMessages();

	RELEASE( gDevice );
	RELEASE( gHostAddress );

	// Reset the current provider's info.
	netCurrentProvider = NSP_NONE;
	netServerMode      = false;
}

/*
 * Returns true if the low-level network routines have been initialized
 * and are expected to be working.
 */
boolean	N_IsAvailable(void)
{
	return netCurrentProvider != NSP_NONE;
}

/*
 * Returns true if the internet is available.
 */
boolean N_UsingInternet(void)
{
	return netCurrentProvider == NSP_TCPIP;
}

/*
 * Sets the DP name of the local player.
 */
void N_SetPlayerInfo(void)
{
	if(!N_IsAvailable()) return;

	DPN_PLAYER_INFO info;
	memset(&info, 0, sizeof(info));
	info.dwSize = sizeof(info);
	info.dwInfoFlags = DPNINFO_DATA;
	info.pvData = playerName;
	info.dwDataSize = strlen(playerName) + 1;

	// SetClientInfo fails without a handle...
	DPNHANDLE handle = 0; 

	if(netServerMode)
	{
		// This is not used anywhere?
		gServer->SetServerInfo(&info, NULL, &handle, 0);
	}
	else
	{
		if(FAILED(hr = gClient->SetClientInfo(&info, NULL, &handle, 0)))
		{
			Con_Message("N_SetPlayerInfo: Failed to SetClientInfo "
				"[%x].\n", hr);
		}
	}
}

/*
 * Send the buffer to the destination. For clients, the server is the only
 * possible destination (doesn't depend on the value of 'destination').
 */
void N_SendDataBuffer(void *data, uint size, nodeid_t destination)
{
	DPNHANDLE asyncHandle = 0;
	DPN_BUFFER_DESC buffer;

#ifdef RANDOM_PACKET_LOSS
	if(M_Random() < 64) return; // 25%
#endif

/*
#ifdef COUNT_BYTE_FREQS
	// This is in the wrong place; the data is already encoded.
	for(uint i = 0; i < size; i++)
	{
		gByteCounts[ ((byte*)data)[i] ]++;
	}
	gTotalByteCount += size;
#endif
*/
	
	buffer.dwBufferSize = size;
	buffer.pBufferData = (byte*) data;

	// DirectPlay flags.
	DWORD sendFlags = DPNSEND_NOLOOPBACK | DPNSEND_NONSEQUENTIAL
		| DPNSEND_NOCOMPLETE;

	if(netServerMode)
	{
		hr = gServer->SendTo(destination, &buffer, 1, SEND_TIMEOUT, 0, 
			&asyncHandle, sendFlags);
	}
	else
	{
		hr = gClient->Send(&buffer, 1, SEND_TIMEOUT, 0, 
			&asyncHandle, sendFlags);
	}
}

/*
 * Returns the number of messages waiting in the player's send queue.
 */
uint N_GetSendQueueCount(int player)
{
	DWORD numMessages = 0;

	if(netServerMode)
	{
		gServer->GetSendQueueInfo(clients[player].nodeID, 
			&numMessages, NULL, 0);
	}
	else
	{
		gClient->GetSendQueueInfo(&numMessages, NULL, 0);
	}

	return numMessages;
}

/*
 * Returns the number of bytes waiting in the player's send queue.
 */
uint N_GetSendQueueSize(int player)
{
	DWORD numBytes = 0;

	if(netServerMode)
	{
		gServer->GetSendQueueInfo(clients[player].nodeID, 
			NULL, &numBytes, 0);
	}
	else
	{
		gClient->GetSendQueueInfo(NULL, &numBytes, 0);
	}

	return numBytes;
}

const char* N_GetProtocolName(void)
{
	return protocolNames[ netCurrentProvider ];
}

/*
 * Returns the player name associated with the given network node.
 */
boolean N_GetNodeName(nodeid_t id, char *name)
{
	DPN_PLAYER_INFO *info = NULL;
	DWORD size = 0;
	char name[256];

	// First determine how much memory is needed. (Don't you love
	// the ease of use of this API?)
	if((hr = gServer->GetClientInfo(event.id, info, &size, 0)) ==
	   DPNERR_BUFFERTOOSMALL)
	{
		// Allocate enough memory and get the data.
		info = (DPN_PLAYER_INFO*) calloc(size, 1);
		info->dwSize = sizeof(*info);
		gServer->GetClientInfo(event.id, info, &size, 0);
		strncpy(name, (const char*) info->pvData, sizeof(name) - 1);
		free(info);
		return true;
	}
	else
	{
		// If this happens, DP has fouled up.
		strcpy(name, "-nobody-");
		return false;
	}
}

/*
 * Things to do when starting a server.
 */
boolean N_ServerOpen(void)
{
	if(!N_IsAvailable()) return false;

	// Let's make sure the correct service provider is initialized
	// in server mode.
	N_InitService(netCurrentProvider, true);

	Demo_StopPlayback();

	// The game DLL may have something that needs doing
	// before we actually begin.
	if(gx.NetServerStart) gx.NetServerStart(true);

	// The info strings.
	/* -- DISABLED --
	jtNetSetString(JTNET_SERVER_INFO, serverInfo);
	jtNetSetString(JTNET_NAME, playerName);
	jtNetSetInteger(JTNET_SERVER_DATA1, W_CRCNumber());
	*/

	/*// Update the server data parameters.
	serverData[0] = W_CRCNumber();*/

	// Setup the DP application description.
	memset(&gAppInfo, 0, sizeof(gAppInfo));
	gAppInfo.dwSize = sizeof(gAppInfo);
	gAppInfo.dwFlags = DPNSESSION_CLIENT_SERVER;
	gAppInfo.guidApplication = gDoomsdayGUID;
	gAppInfo.dwMaxPlayers = MAXPLAYERS - (isDedicated? 1 : 0);

	// Try to begin hosting using the gServer object.
	if(FAILED(hr = gServer->Host(&gAppInfo, &gDevice, 1, NULL, NULL,
		(PVOID) PCTX_SERVER /* Player Context */, 0)))
	{
		Con_Message("N_ServerOpen: Failed to Host [%x].\n", hr);
		return false;
	}

	Sv_StartNetGame();

	// The game DLL might want to do something now that the
	// server is started.
	if(gx.NetServerStart) gx.NetServerStart(false);

	if(masterAware && N_UsingInternet())
		N_MasterAnnounceServer(true);

	return true;
}

/*
 * Things to do when closing the server.
 */
boolean N_ServerClose(void)
{
	if(!N_IsAvailable()) return false;

	if(masterAware && N_UsingInternet())
	{
		N_MAClear();

		// Bye-bye, master server.
		N_MasterAnnounceServer(false);
	}
	if(gx.NetServerStop) gx.NetServerStop(true);
	Net_StopGame();

	// Close the gServer object.
	gServer->Close(0);	
	N_InitDPObject(true);	// But also reinit it.

	if(gx.NetServerStop) gx.NetServerStop(false);
	return true;
}

/*
 * The client is removed from the game without delay. This is used
 * when the server needs to terminate a client's connection
 * abnormally.
 */
void N_TerminateNode(nodeid_t id)
{
	gServer->DestroyClient(id, NULL, 0, 0);
}

/*
 * Creates an appropriate DP address for the target host, based on npt*.
 * The host address is stored to gHostAddress.
 */
void N_SetTargetHostAddress(void)
{
	WCHAR buf[128];

	// Release a previously created host address.
	RELEASE( gHostAddress );

	if((gHostAddress = N_NewAddress(netCurrentProvider)) == NULL)
	{
		Con_Message("N_SetTargetHostAddress: Failed! [%x]\n", hr);
		return;
	}
	
	switch(netCurrentProvider)
	{
	case NSP_TCPIP:
		if(nptIPAddress[0])
		{
			VERBOSE( Con_Message("N_SetTargetHostAddress: Using %s.\n", 
				nptIPAddress) );

			// Convert to unicode.
			N_StrWide(buf, nptIPAddress, 127);

			// Set the host name.
			gHostAddress->AddComponent(DPNA_KEY_HOSTNAME, 
				buf, 2*(strlen(nptIPAddress) + 1), 
				DPNA_DATATYPE_STRING);
		}
		break;

	case NSP_MODEM:
		if(nptPhoneNum[0])
		{
			VERBOSE( Con_Message("N_SetTargetHostAddress: Using phone number %s.\n", 
				nptPhoneNum) );
			
			// Convert to unicode.
			N_StrWide(buf, nptPhoneNum, 127);

			gHostAddress->AddComponent(DPNA_KEY_PHONENUMBER,
				buf, 2*(strlen(nptPhoneNum) + 1),
				DPNA_DATATYPE_STRING);
		}
		break;
	}

	VERBOSE2( N_PrintAddress("Host address", gHostAddress) );
}

/*
 * Stops the host enumeration operation.
 */
void N_StopLookingForHosts(void)
{
	if(!N_IsAvailable() || netServerMode || !gEnumHandle) return;
	gClient->CancelAsyncOperation(gEnumHandle, 0);
	gEnumHandle = 0;
}

/*
 * Looks for hosts and stores them in a serverinfo_t list.
 */
boolean N_LookForHosts(void)
{
	// We must be a client.
	if(!N_IsAvailable() || netServerMode) return false;

	// Is the enumeration already in progress?
	if(gEnumHandle) 
	{
		Con_Message("N_LookForHosts: Still looking...\n");

		if(gHostCount)
		{
			Con_Printf("%i server%s been found.\n", gHostCount, 
				gHostCount != 1? "s have" : " has");
			N_PrintServerInfo(0, NULL);
			for(hostnode_t *it = gHostRoot; it; it = it->next)
				N_PrintServerInfo(it->index, &it->info);
		}
		return true;
	}

	// Get rid of previous findings.
	N_ClearHosts();

	// Let's determine the address we will be looking into.
	N_SetTargetHostAddress();

	// Search parameters.
	DPN_APPLICATION_DESC enumedApp;
	memset(&enumedApp, 0, sizeof(enumedApp));
	enumedApp.dwSize = sizeof(enumedApp);
	enumedApp.guidApplication = gDoomsdayGUID;

	// Start the enumeration.
	if(FAILED(hr = gClient->EnumHosts( 
		&enumedApp /* application description */,
		gHostAddress /* where to look? */,
		gDevice	/* the device to search with */,
		NULL /* custom enum data */,
		0 /* size of custom enum data */,
		INFINITE /* how many times to do the enum */,
		0 /* enum interval (ms), zero is the default */,
		INFINITE /* timeout: until canceled */,
		NULL /* user context */,
		&gEnumHandle /* handle to this enum operation */,
		0 /* flags */)))
	{
		Con_Message("N_LookForHosts: Failed to EnumHosts [%x].\n", hr);
		return false;
	}

	Con_Message("N_LookForHosts: Looking for servers...\n");
	return true;
}

/*
 * Returns the number of hosts found so far.
 */
int N_GetHostCount(void)
{
	return gHostCount;
}

/*
 * Returns information about the specified host, if it exists.
 */
boolean N_GetHostInfo(int index, serverinfo_t *info)
{
	hostnode_t *host = N_GetHost(index);
	if(!host) return false; // Failure.
	memcpy(info, &host->info, sizeof(*info));	
	return true;
}

/*
 * Things to do when connecting.
 */
boolean N_Connect(int index)
{
	if(!N_IsAvailable() || netServerMode) return false;

	hostnode_t *host = N_GetHost(index);
	if(!host)
	{
		Con_Message("N_Connect: Invalid host %i.\n", index);
		return false;
	}

	N_SetPlayerInfo();

	Demo_StopPlayback();

	// Call game DLL's NetConnect.
	gx.NetConnect(true);

	DPN_APPLICATION_DESC destApp;
	memset(&destApp, 0, sizeof(destApp));
	destApp.dwSize = sizeof(destApp);
	destApp.guidApplication = gDoomsdayGUID;

	// Try to connect to 'index'.
	if(FAILED(hr = gClient->Connect(&destApp /* application desc */,
		host->address, host->device, 
		NULL, NULL /* security, credentials */,
		NULL, 0 /* user connect data */,
		NULL, NULL /* async context */,
		DPNCONNECT_SYNC)))
	{
		Con_Message("N_Connect: Failed to Connect [%x].\n", hr);
		return false;
	}

	// Connection has been established, stop any enumerations.
	N_StopLookingForHosts();

	handshakeReceived = false;
	netgame = true; // Allow sending/receiving of packets.
	isServer = false;
	isClient = true;
	
	// Call game's NetConnect.
	gx.NetConnect(false);
	
	// G'day mate!
	Cl_SendHello();
	return true;
}

/*
 * Disconnect from the server.
 */
boolean N_Disconnect(void)
{
	if(!N_IsAvailable()) return false;
	
	// Tell the Game that a disconnecting is about to happen.
	if(gx.NetDisconnect) gx.NetDisconnect(true);
	
	Net_StopGame();
	N_ClearMessages();

	// Exit the session, but reinit the client interface.
	gClient->Close(0);
	N_InitDPObject(false);

	// Tell the Game that disconnecting is now complete.
	if(gx.NetDisconnect) gx.NetDisconnect(false);

	return true;
}
