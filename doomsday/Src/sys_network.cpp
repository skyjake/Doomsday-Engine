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

#define MASTER_HEARTBEAT	120 // seconds

#define MASTER_QUEUE_LEN	16
#define NETEVENT_QUEUE_LEN	32
#define SEND_TIMEOUT		15000	// 15 seconds
#define RELEASE(x)			if(x) ((x)->Release(), (x)=NULL)

#define MSG_MUTEX_NAME		"MsgQueueMutex"

// Net events.
typedef enum neteventtype_e {
	NE_CLIENT_ENTRY,
	NE_CLIENT_EXIT,
	NE_END_CONNECTION
} neteventtype_t;

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

// Flags for the sent message store (for to-be-confirmed messages):
#define SMSF_ORDERED	0x1	// Block other ordered messages until confirmed
#define SMSF_QUEUED		0x2	// Ordered message waiting to be sent
#define SMSF_CONFIRMED	0x4	// Delivery has been confirmed! (OK to remove)

// Length of the received message ID history.
#define STORE_HISTORY_SIZE	100

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

typedef struct netevent_s {
	neteventtype_t type;
	DPNID id;
} netevent_t;

typedef struct netmessage_s {
	struct netmessage_s *next;
	DPNID sender;
	int player;			// Set in N_GetMessage().
	unsigned int size;
	byte *data;
	DPNHANDLE handle;
} netmessage_t;

typedef struct sentmessage_s {
	struct sentmessage_s *next, *prev;
	struct store_s *store;
	msgid_t id;
	uint timeStamp;
	int flags;
	DPNID destination;
	unsigned int size;
	void *data;
} sentmessage_t;

typedef struct store_s {
	sentmessage_t *first, *last;
	msgid_t idCounter;
	msgid_t history[STORE_HISTORY_SIZE];
	int historyIdx;
} store_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

boolean	N_InitDPObject(boolean inServerMode);
void	N_StopLookingForHosts(void);
boolean	N_Disconnect(void);
int		N_IdentifyPlayer(DPNID id);
void	N_SendDataBuffer(void *data, uint size, DPNID destination);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		allowSending = true;

netbuffer_t	netbuffer;
int			maxQueuePackets = 0;

char		*serverName = "Doomsday";
char		*serverInfo = "Multiplayer game server";
char		*playerName = "Player";

// Some parameters passed to master server.
int			serverData[3];

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

// Master server info. Hardcoded defaults.
char		*masterAddress = "www.doomsdayhq.com"; 
int			masterPort = 0; // Uses 80 by default.
char		*masterPath = "/master.php";
boolean		masterAware = false;		

// Operating mode of the currently active service provider.
serviceprovider_t gCurrentProvider = NSP_NONE;
boolean		gServerMode = false;

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

// The message queue: list of incoming messages waiting for processing.
static netmessage_t *gMsgHead, *gMsgTail;

// A mutex is used to protect the addition and removal of messages from
// the message queue.
static int gMsgMutex;

// The master action queue.
static masteraction_t masterQueue[MASTER_QUEUE_LEN];
static int mqHead, mqTail;

// The net event queue (player arrive/leave).
static netevent_t netEventQueue[NETEVENT_QUEUE_LEN];
static int neqHead, neqTail;

static char *protocolNames[NUM_NSP] = { 
	"??", "TCP/IP", "IPX", "Modem", "Serial Link"
};

// The Sent Message Store: list of sent or queued messages waiting to be 
// confirmed.
static store_t stores[MAXPLAYERS];

#ifdef COUNT_BYTE_FREQS
static uint gByteCounts[256];
static uint gTotalByteCount;
#endif

// Number of bytes of outgoing data transmitted.
static uint gNumOutBytes;

// Number of bytes sent over the network (compressed).
static uint gNumSentBytes;

// CODE --------------------------------------------------------------------

#if 0
/*
 * Acquire or release ownership of the message queue mutex.
 * Returns true if successful.
 */
boolean N_LockQueue(boolean doAcquire)
{
	if(doAcquire)
	{
		// Five seconds is plenty.
		if(WaitForSingleObject(gMsgMutex, 5000) == WAIT_TIMEOUT) 
		{
			// Couldn't lock it.
			return false;
		}
	}
	else
	{
		// Release it, then.
		ReleaseMutex(gMsgMutex);
	}
	return true;
}
#endif

/*
 * Generate a new, non-zero message ID.
 */
ushort N_GetNewMsgID(int player)
{
	msgid_t *c = &stores[player].idCounter;

	while(!++*c);
	return *c;
}

/*
 * Register the ID number in the history of received IDs.
 */
void N_HistoryAdd(int player, msgid_t id)
{
	store_t *store = &stores[player];
	store->history[ store->historyIdx++ ] = id;
	store->historyIdx %= STORE_HISTORY_SIZE;
}

/*
 * Returns true if the ID is already in the history.
 */
boolean N_HistoryCheck(int player, msgid_t id)
{
	store_t *store = &stores[player];
	int i;

	for(i = 0; i < STORE_HISTORY_SIZE; i++)
	{
		if(store->history[i] == id) return true;
	}
	return false;
}

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
 * Add a net event to the queue, to wait for processing.
 */
void N_NEPost(netevent_t *nev)
{
	netEventQueue[neqHead] = *nev;
	++neqHead %= NETEVENT_QUEUE_LEN;
}

/*
 * Returns true if there are net events waiting to be processed.
 * N_GetPacket() will not return a packet until all net events have
 * processed.
 */
boolean N_NEPending(void)
{
	return neqHead != neqTail;
}

/*
 * Get a net event from the queue. Returns true if an event was 
 * returned.
 */
boolean N_NEGet(netevent_t *nev)
{
	// Empty queue?
	if(!N_NEPending()) return false;	
	*nev = netEventQueue[neqTail];
	++neqTail %= NETEVENT_QUEUE_LEN;
	return true;
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
 * Add a new message to the Sent Message Store.
 */
sentmessage_t *N_SMSCreate
	(int player, msgid_t id, DPNID destID, void *data, uint length)
{
	store_t *store = &stores[player];
	sentmessage_t *msg = (sentmessage_t*) calloc(sizeof(sentmessage_t), 1);

	msg->store = store;
	msg->id = id;
	msg->destination = destID;
	msg->timeStamp = Sys_GetRealTime();
	msg->data = malloc(length);
	memcpy(msg->data, data, length);
	msg->size = length;

	// Link it to the end of the Send Message Store.
	if(store->last)
	{
		msg->prev = store->last;
		store->last->next = msg;
	}
	store->last = msg;
	if(!store->first)
	{
		store->first = msg;
	}

	return msg;
}

/*
 * Destroy the message from the Sent Message Store.
 * This is done during N_Update(), so it never conflicts with the creation
 * of SMS nodes.
 */
void N_SMSDestroy(sentmessage_t *msg)
{
	store_t *store = msg->store;

	// First unlink.
	if(store->first == msg)
	{
		store->first = msg->next;
	}
	if(msg->prev)
	{
		msg->prev->next = msg->next;
	}
	if(msg->next)
	{
		msg->next->prev = msg->prev;
	}
	if(store->last == msg)
	{
		store->last = msg->prev;
	}
	msg->prev = msg->next = NULL;

	free(msg->data);
	free(msg);
}

/*
 * Returns true if the Send Message Store contains any ordered messages.
 * Ordered messages are sent in order, one at a time.
 */
boolean N_SMSContainsOrdered(int player)
{
	store_t *store = &stores[player];

	for(sentmessage_t *msg = store->first; msg; msg = msg->next)
	{
		if(msg->flags & SMSF_CONFIRMED) 
		{
			continue;
		}
		if(msg->flags & SMSF_ORDERED) return true;
	}
	return false;
}

/*
 * Resends a message from the Sent Message Store.
 */
void N_SMSResend(sentmessage_t *msg)
{
	// It's now no longer queued.
	msg->flags &= ~SMSF_QUEUED;

	// Update the timestamp on the message.
	msg->timeStamp = Sys_GetRealTime();

	N_SendDataBuffer(msg->data, msg->size, msg->destination);

/*#ifdef _DEBUG
	Con_Printf("N_SMSResend: T%i:%i.\n", msg->store - stores, msg->id);
#endif*/
}

/*
 * Finds the next queued message and sends it.
 */
void N_SMSUnqueueNext(sentmessage_t *msg)
{
	for(; msg; msg = msg->next)
	{
		if(msg->flags & SMSF_CONFIRMED) continue;
		if(msg->flags & SMSF_QUEUED)
		{
/*#ifdef _DEBUG
			Con_Printf("N_SMSUnqueueNext: Unqueueing T%i:%i.\n", 
				msg->store - stores, msg->id);
#endif*/
			N_SMSResend(msg);
			return;
		}
	}
}

/*
 * Marks the specified message confirmed. It will be removed in N_Update().
 */
void N_SMSConfirm(int player, msgid_t id)
{
	store_t *store = &stores[player];

	for(sentmessage_t *msg = store->first; msg; msg = msg->next)
	{
		if(msg->flags & SMSF_CONFIRMED) continue;
		if(msg->id == id)
		{
			msg->flags |= SMSF_CONFIRMED;
			
			// Note how long it took to confirm the message.
			Net_SetAckTime(player, Sys_GetRealTime() - msg->timeStamp);

/*#ifdef _DEBUG
			Con_Printf("N_SMSConfirm: T%i:%i has been delivered in %ims!\n", 
				msg->store - stores, msg->id, 
				Sys_GetRealTime() - msg->timeStamp);
#endif*/

			if(msg->flags & SMSF_ORDERED)
			{
				// The confirmation of an ordered message allows the
				// next queued message to be sent.
				N_SMSUnqueueNext(msg);
			}
			return;
		}
	}
}

/*
 * Remove the confirmed messages from the Sent Message Store.
 * Called from N_Update().
 */
void N_SMSDestroyConfirmed(void)
{
	sentmessage_t *msg, *next = NULL;
	int i;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		for(msg = stores[i].first; msg; msg = next)
		{
			next = msg->next;
			if(msg->flags & SMSF_CONFIRMED)
			{
				N_SMSDestroy(msg);
			}
		}
	}
}

/*
 * Resend all unconfirmed messages that are older than the client's
 * ack threshold.
 */
void N_SMSResendTimedOut(void)
{
	sentmessage_t *msg;
	uint nowTime = Sys_GetRealTime(), threshold;
	int i;

	for(i = 0; i < MAXPLAYERS; i++)
	{
		threshold = Net_GetAckThreshold(i);

		for(msg = stores[i].first; msg; msg = msg->next)
		{
			if(msg->flags & (SMSF_CONFIRMED | SMSF_QUEUED))
			{
				// Confirmed messages will soon be removed and queued
				// haven't been sent yet.
				continue;
			}

			if(nowTime - msg->timeStamp > threshold)			
			{
				// This will now be resent.
				N_SMSResend(msg);
			}
		}
	}
}

/*
 * Reset the Sent Message Store back to defaults.
 */
void N_SMSReset(store_t *store)
{
	// Destroy all the messages in the store.
	while(store->first)
	{
		N_SMSDestroy(store->first);
	}

	// Reset everything back to zero.
	memset(store, 0, sizeof(*store));
}

/*
 * Adds the given netmessage_s to the queue of received messages.
 * Before calling this, allocate the message using malloc().
 * We use a mutex to synchronize access to the message queue.
 * This is called in the DirectPlay thread.
 */
void N_PostMessage(netmessage_t *msg)
{
	// Lock the message queue.
	Sys_AcquireMutex(gMsgMutex);

	// This will be the latest message.
	msg->next = NULL;

	if(gMsgTail)
	{
		// There are previous messages.
		gMsgTail->next = msg;
	}

	// The tail pointer points to the last message.
	gMsgTail = msg;

	// If there is no head, this'll be the first message.
	if(gMsgHead == NULL) gMsgHead = msg;

	Sys_ReleaseMutex(gMsgMutex);
}

/*
 * Extracts the next message from the queue of received messages.
 * Returns NULL if no message is found. The caller must release the
 * message when it's no longer needed, using N_ReleaseMessage().
 * We use a mutex to synchronize access to the message queue.
 * This is called in the Doomsday thread.
 */
netmessage_t* N_GetMessage(void)
{
	if(gMsgHead == NULL) return NULL;

	Sys_AcquireMutex(gMsgMutex);

	// This is the message we'll return.
	netmessage_t *msg = gMsgHead;

	// If there are no more messages, the tail pointer must be cleared, too.
	if(!gMsgHead->next) gMsgTail = NULL;

	// Advance the head pointer.
	gMsgHead = gMsgHead->next;

	Sys_ReleaseMutex(gMsgMutex);

	// Identify the sender.
	msg->player = N_IdentifyPlayer(msg->sender);

	return msg;
}

/*
 * Free the DP buffer.
 */
void N_ReturnBuffer(DPNHANDLE handle)
{
	if(gServerMode)
	{
		gServer->ReturnBuffer(handle, 0);
	}
	else
	{
		gClient->ReturnBuffer(handle, 0);
	}
}

/*
 * Frees the message.
 */
void N_ReleaseMessage(netmessage_t *msg)
{
	if(msg->handle) 
	{
		N_ReturnBuffer(msg->handle);
	}
	free(msg);
}

/*
 * Empties the message buffers.
 */
void N_ClearMessages(void)
{
	netmessage_t *msg;

	while((msg = N_GetMessage()) != NULL)
		N_ReleaseMessage(msg);

	// The queue is now empty.
	gMsgHead = gMsgTail = NULL;

	// Also clear the sent message store.
	for(int i = 0; i < MAXPLAYERS; i++)
	{
		while(stores[i].first)
			N_SMSDestroy(stores[i].first);
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
void N_Init(void)
{
	// Create a mutex for the message queue.
	gMsgMutex = Sys_CreateMutex(MSG_MUTEX_NAME);

	N_SockInit();
	N_MasterInit();
	N_InitDirectPlay();
}

/*
 * Shut down the low-level network interface. Called during engine 
 * shutdown (not before).
 */
void N_Shutdown(void)
{
	N_ShutdownService();
	N_ShutdownDirectPlay();
	N_MasterShutdown();
	N_SockShutdown();

	// Close the handle of the message queue mutex.
	Sys_DestroyMutex(gMsgMutex);
	gMsgMutex = 0;

#ifdef COUNT_BYTE_FREQS
	Con_Printf("Total number of bytes: %i\n", gTotalByteCount);
	for(int i = 0; i < 256; i++)
	{
		Con_Printf("%.12lf, ", gByteCounts[i] / (double) gTotalByteCount);
		if(i % 4 == 3) Con_Printf("\n");
	}
	Con_Printf("\n");
#endif

	if(ArgExists("-huffavg"))
	{
		Con_Execute("huffman", false);
	}
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
	if(gCurrentProvider == provider && gServerMode == inServerMode)
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
	gCurrentProvider = provider;
	gServerMode      = inServerMode;

	Con_Message("N_InitService: %s in %s mode.\n", N_GetProtocolName(),
		gServerMode? "server" : "client");
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

	if(gServerMode)
		gServer->Close(0);
	else
		gClient->Close(0);

	// Any queued messages will be destroyed.
	N_ClearMessages();

	RELEASE( gDevice );
	RELEASE( gHostAddress );

	// Reset the current provider's info.
	gCurrentProvider = NSP_NONE;
	gServerMode      = false;
}

/*
 * Returns true if the low-level network routines have been initialized
 * and are expected to be working.
 */
boolean	N_IsAvailable(void)
{
	return gCurrentProvider != NSP_NONE;
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

	if(gServerMode)
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
void N_SendDataBuffer(void *data, uint size, DPNID destination)
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

	if(gServerMode)
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
 * Send the data in the netbuffer. The message is sent using an 
 * unreliable, nonsequential (i.e. fast) method.
 * 
 * Handles broadcasts using recursion.
 * Clients can only send stuff to the server.
 */
void N_SendPacket(int flags)
{
	int i, dest = 0;
	sentmessage_t *sentMsg;
	void *data;
	uint size;

	// Is the network available?
	if(!allowSending || !N_IsAvailable()) return;

	// Figure out the destination DPNID.
	if(gServerMode)
	{
		if(netbuffer.player >= 0 && netbuffer.player < MAXPLAYERS)
		{
			if(players[ netbuffer.player ].flags & DDPF_LOCAL
				|| !clients[ netbuffer.player ].connected) 
			{
				// Do not send anything to local or disconnected players.
				return;
			}

			dest = clients[ netbuffer.player ].nodeID;
		}
		else
		{
			// Broadcast to all non-local players, using recursive calls.
			for(i = 0; i < MAXPLAYERS; i++)
			{
				netbuffer.player = i;
				N_SendPacket(flags);
			}

			// Reset back to -1 to notify of the broadcast.
			netbuffer.player = NSP_BROADCAST;
			return;
		}
	}

	boolean isQueued = false;

	if(flags & SPF_ORDERED)
	{
		// If the Store already contains an ordered message for this player, 
		// this new one will be queued. The queue-status is lifted (and the 
		// message sent) when the previous ordered message is acknowledged.
		if(N_SMSContainsOrdered(netbuffer.player))
		{
			// Queued messages will not be sent yet.
			isQueued = true;
		}
	}

	// Do we need to generate an ID for the message?
	if(flags & SPF_CONFIRM || flags & SPF_ORDERED)
	{
		netbuffer.msg.id = N_GetNewMsgID(netbuffer.player);
	}
	else
	{
		// Normal, unconfirmed messages do not use IDs.
		netbuffer.msg.id = 0;
	}

	// This is what will be sent.
	gNumOutBytes += netbuffer.headerLength + netbuffer.length;

	// Compress using Huffman codes.
	data = Huff_Encode( (byte*) &netbuffer.msg, 
		netbuffer.headerLength + netbuffer.length, &size);

	// This many bytes are actually sent.
	gNumSentBytes += size;

	// Ordered and confirmed messages are placed in the Store until they
	// have been acknowledged.
	if(flags & SPF_CONFIRM || flags & SPF_ORDERED)
	{
		sentMsg = N_SMSCreate(netbuffer.player, netbuffer.msg.id, 
			dest, data, size);
		
		if(flags & SPF_ORDERED)
		{
			// This message will block other ordered messages to 
			// this player.
			sentMsg->flags |= SMSF_ORDERED;
		}
			
		if(isQueued)
		{
			// The message will not be sent at this time.
			sentMsg->flags |= SMSF_QUEUED;
			return;
		}
	}

	N_SendDataBuffer(data, size, dest);
}

/*
 * Returns the player number that corresponds the DPNID.
 */
int N_IdentifyPlayer(DPNID id)
{
	if(gServerMode)
	{
		// What is the corresponding player number? Only the server keeps
		// a list of all the IDs.
		for(int i = 0; i < MAXPLAYERS; i++)
			if(clients[i].nodeID == id)
				return i;

		// Bogus?
		return -1;
	}

	// Clients receive messages only from the server.
	return 0;
}

/*
 * Send a Confirmation of Delivery message. 
 */
void N_SendConfirmation(msgid_t id, DPNID where)
{
	uint size;

	// All data is sent using Huffman codes.
	void *data = Huff_Encode((byte*)&id, 2, &size);
	N_SendDataBuffer(data, size, where);

	// Increase the counters.
	gNumOutBytes += 2;
	gNumSentBytes += size;
}

/*
 * Returns the next message waiting in the incoming message queue.
 * Confirmations are handled here.
 *
 * NOTE: Skips all messages from unknown DPNIDs!
 */
netmessage_t *N_GetNextMessage(void)
{
	netmessage_t *msg;

	while((msg = N_GetMessage()) != NULL)
	{
		if(msg->player < 0)
		{
			// From an unknown ID?
			N_ReleaseMessage(msg);

/*#ifdef _DEBUG
			Con_Printf("N_GetNextMessage: Unknown sender, skipped...\n");
#endif*/
			continue;
		}

		// Decode the Huffman codes. The returned buffer is static, so it 
		// doesn't need to be freed (not thread-safe, though).
		msg->data = Huff_Decode(msg->data, msg->size, &msg->size);

		// The DP buffer can be freed.
		N_ReturnBuffer(msg->handle);
		msg->handle = NULL;

		// First check the message ID (in the first two bytes).
		msgid_t id = *(msgid_t*) msg->data;
		if(id)
		{
			// Confirmations of delivery are not time-critical, so they can 
			// be done here.
			if(msg->size == 2)
			{
				// All the message contains is a short? This is a confirmation
				// from the receiver. The message will be removed from the SMS
				// in N_Update().
				N_SMSConfirm(msg->player, id);
				N_ReleaseMessage(msg);
				continue;
			}
			else
			{
				// The arrival of this message must be confirmed. Send a reply
				// immediately. Writes to the Huffman encoding buffer.
				N_SendConfirmation(id, msg->sender);

/*#ifdef _DEBUG
				Con_Printf("N_GetNextMessage: Acknowledged arrival of "
					"F%i:%i.\n", msg->player, id);
#endif*/
			}

			// It's possible that a message times out just before the 
			// confirmation is received. It's also possible that the message
			// was received, but the confirmation was lost. In these cases, 
			// the recipient will get a second copy of the message. We keep 
			// track of the ID numbers in order to detect this.
			if(N_HistoryCheck(msg->player, id))
			{
/*#ifdef _DEBUG
				Con_Printf("N_GetNextMessage: DUPE of F%i:%i.\n", 
					msg->player, id);
#endif*/
				// This is a duplicate!
				N_ReleaseMessage(msg);
				continue;
			}

			// Record this ID in the history of received messages.
			N_HistoryAdd(msg->player, id);
		}
		return msg;
	}

	// There are no more messages.
	return NULL;
}

/*
 * A message is extracted from the message queue. Returns true if a message
 * is successfully extracted.
 */
boolean	N_GetPacket()
{
	netmessage_t *msg;

	// If there are net events pending, let's not return any 
	// packets yet. The net events may need to be processed before 
	// the packets.
	if(!N_IsAvailable() || N_NEPending()) return false;

	netbuffer.player = -1;
	netbuffer.length = 0;

	if((msg = N_GetNextMessage()) == NULL)
	{
		// No messages at this time.
		return false;
	}

	/*
	Con_Message("N_GetPacket: from=%x, len=%i\n", msg->sender, msg->size);
	*/

	// There was a packet!
	netbuffer.player = msg->player;
	netbuffer.length = msg->size - netbuffer.headerLength;	
	memcpy(&netbuffer.msg, msg->data, 
		MIN_OF(sizeof(netbuffer.msg), msg->size));

	// The message can now be freed.
	N_ReleaseMessage(msg);

	// We have no idea who sent this (on serverside).
	if(netbuffer.player == -1) return false;

	return true;
}

/*
 * Returns the number of messages waiting in the player's send queue.
 */
uint N_GetSendQueueCount(int player)
{
	DWORD numMessages = 0;

	if(gServerMode)
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

	if(gServerMode)
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
	return protocolNames[ gCurrentProvider ];
}

/*
 * The event list is checked for arrivals and exits, and the 'clients'
 * and 'players' arrays are updated accordingly.
 */
void N_Update(void)
{
	netevent_t event;
	DPN_PLAYER_INFO *info = NULL;
	DWORD size = 0;
	char name[256];

	// Remove all confirmed messages in the Send Message Store.
	N_SMSDestroyConfirmed();

	// Resend unconfirmed, timed-out messages.
	N_SMSResendTimedOut();

	// Are there any events to process?
	while(N_NEGet(&event))
	{
		switch(event.type)
		{
		case NE_CLIENT_ENTRY:
			// Find out the name of the new player.
			memset(name, 0, sizeof(name));

			// First determine how much memory is needed. (Don't you love
			// the ease of use of this API?)
			if((hr = gServer->GetClientInfo(event.id, info, &size, 0)) 
				== DPNERR_BUFFERTOOSMALL)
			{
				// Allocate enough memory and get the data.
				info = (DPN_PLAYER_INFO*) calloc(size, 1);
				info->dwSize = sizeof(*info);
				gServer->GetClientInfo(event.id, info, &size, 0);
				strncpy(name, (const char*) info->pvData, sizeof(name) - 1);
				free(info);
			}
			else
			{
				// If this happens, DP has fouled up.
				strcpy(name, "-nobody-");
			}

			// Assign a console to the new player.
			Sv_PlayerArrives(event.id, name);
			break;

		case NE_CLIENT_EXIT:
			if(N_IdentifyPlayer(event.id) >= 0)
			{
				// Clear this client's Sent Message Store.
				N_SMSReset(&stores[ N_IdentifyPlayer(event.id) ]);
			}
			Sv_PlayerLeaves(event.id);
			break;

		case NE_END_CONNECTION:
			// A client receives this event when the connection is 
			// terminated.
			if(netgame)
			{
				// We're still in a netgame, which means we didn't disconnect
				// voluntarily.
				Con_Message("N_Update: Connection was terminated.\n");
				N_Disconnect();
			}
			break;
		}
	}
}

/*
 * Add a master action command to the queue. The master action stuff 
 * really doesn't belong in this file...
 */
void N_MAPost(masteraction_t act)
{
	masterQueue[mqHead] = act;
	mqHead = (++mqHead) % MASTER_QUEUE_LEN;
}

/*
 * Get a master action command from the queue.
 */
boolean N_MAGet(masteraction_t *act)
{
	// Empty queue?
	if(mqHead == mqTail) return false;	
	*act = masterQueue[mqTail];
	return true;
}

/*
 * Remove a master action command from the queue.
 */
void N_MARemove(void)
{
	if(mqHead != mqTail) mqTail = (++mqTail) % MASTER_QUEUE_LEN;
}

/*
 * Clear the master action command queue.
 */
void N_MAClear(void)
{
	mqHead = mqTail = 0;
}

/*
 * Returns true if the master action command queue is empty.
 */
boolean N_MADone(void)
{
	return (mqHead == mqTail);
}

/*
 * Prints server/host information into the console. The header line is 
 * printed if 'info' is NULL.
 */
void N_PrintServerInfo(int index, serverinfo_t *info)
{
	if(!info)
	{
		Con_Printf("    %-20s P/M  L Ver:  Game:            Location:\n", 
			"Name:");
	}
	else
	{
		Con_Printf("%-2i: %-20s %i/%-2i %c %-5i %-16s %s:%i\n", 
			index, info->name,
			info->players, info->maxPlayers,
			info->canJoin? ' ':'*', info->version, info->game,
			info->address, info->port);
		Con_Printf("    %s (%s:%x) p:%ims %-40s\n", info->map, info->iwad, 
			info->wadNumber, info->ping, info->description);

		Con_Printf("    %s %s\n", info->gameMode, info->gameConfig);

		// Optional: PWADs in use.
		if(info->pwads[0])
			Con_Printf("    PWADs: %s\n", info->pwads);

		// Optional: names of players.
		if(info->clientNames[0])
			Con_Printf("    Players: %s\n", info->clientNames);

		// Optional: data values.
		if(info->data[0] || info->data[1] || info->data[2])
		{
			Con_Printf("    Data: (%08x, %08x, %08x)\n", 
				info->data[0], info->data[1], info->data[2]);
		}
	}
}

/*
 * Handles low-level net tick stuff: communication with the master server.
 */
void N_Ticker(timespan_t time)
{
	masteraction_t act;
	int	i, num;
	static trigger_t heartBeat = { MASTER_HEARTBEAT };

	if(netgame)
	{
		// Update master every 2 minutes.
		if(masterAware && gCurrentProvider == NSP_TCPIP 
			&& M_CheckTrigger(&heartBeat, time))
		{
			N_MasterAnnounceServer(true);
		}
	}

	// Is there a master action to worry about?
	if(N_MAGet(&act))
	{
		switch(act)
		{
		case MAC_REQUEST:
			// Send the request for servers.
			N_MasterRequestList();
			N_MARemove();					
			break;

		case MAC_WAIT:
			// Handle incoming messages.
			if(N_MasterGet(0, 0) >= 0)
			{
				// The list has arrived!
				N_MARemove();
			}
			break;

		case MAC_LIST:
			//Con_Printf("    %-20s P/M  L Ver:  Game:            Location:\n", "Name:");
			N_PrintServerInfo(0, NULL);
			num = i = N_MasterGet(0, 0);
			while(--i >= 0)
			{
				serverinfo_t info;
				N_MasterGet(i, &info);
				/*Con_Printf("%-2i: %-20s %i/%-2i %c %-5i %-16s %s:%i\n", 
					i, info.name,
					info.players, info.maxPlayers,
					info.canJoin? ' ':'*', info.version, info.game,
					info.address, info.port);
				Con_Printf("    %s (%x) %s\n", info.map, info.data[0], 
					info.description);*/
				N_PrintServerInfo(i, &info);
			}
			Con_Printf("%i server%s found.\n", num, num!=1? "s were" : " was");
			N_MARemove();			
			break;
		}
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
	N_InitService(gCurrentProvider, true);

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

	if(masterAware && gCurrentProvider == NSP_TCPIP)
		N_MasterAnnounceServer(true);

	return true;
}

/*
 * Things to do when closing the server.
 */
boolean N_ServerClose(void)
{
	if(!N_IsAvailable()) return false;

	if(masterAware && gCurrentProvider == NSP_TCPIP)
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
 * The client is removed from the game without delay. This is used when the
 * server needs to terminate a client's connection abnormally.
 */
void N_TerminateClient(int console)
{
	if(!N_IsAvailable() 
		|| !clients[console].connected 
		|| !gServerMode) return;

	Con_Message("N_TerminateClient: '%s' from console %i.\n", 
		clients[console].name, console);

	// Clear this client's Sent Message Store.
	N_SMSReset(&stores[console]);

	gServer->DestroyClient(clients[console].nodeID, NULL, 0, 0);
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

	if((gHostAddress = N_NewAddress(gCurrentProvider)) == NULL)
	{
		Con_Message("N_SetTargetHostAddress: Failed! [%x]\n", hr);
		return;
	}
	
	switch(gCurrentProvider)
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
	if(!N_IsAvailable() || gServerMode || !gEnumHandle) return;
	gClient->CancelAsyncOperation(gEnumHandle, 0);
	gEnumHandle = 0;
}

/*
 * Looks for hosts and stores them in a serverinfo_t list.
 */
boolean N_LookForHosts(void)
{
	// We must be a client.
	if(!N_IsAvailable() || gServerMode) return false;

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
	if(!N_IsAvailable() || gServerMode) return false;

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

/*
 * The 'net' console command.
 */
int CCmdNet(int argc, char **argv)
{
	int		i;
	boolean success = true;

	if(argc == 1)	// No args?
	{
		Con_Printf("Usage: %s (cmd/args)\n", argv[0]);
		Con_Printf("Commands:\n");
		Con_Printf("  init tcpip/ipx/modem/serial\n");
		Con_Printf("  shutdown\n");
		Con_Printf("  setup client\n");
		Con_Printf("  setup server\n");
		Con_Printf("  info\n");
		Con_Printf("  announce\n");
		Con_Printf("  request\n");
		Con_Printf("  search (local or targeted query)\n");
		Con_Printf("  servers (asks the master server)\n");
		Con_Printf("  connect (idx)\n");
		Con_Printf("  mconnect (m-idx)\n");
		Con_Printf("  disconnect\n");
		Con_Printf("  server go/start\n");
		Con_Printf("  server close/stop\n");
		return true;
	}
	if(argc == 2)	// One argument?
	{
		if(!stricmp(argv[1], "shutdown"))
		{
			if(N_IsAvailable())
			{
				Con_Printf("Shutting down %s.\n", N_GetProtocolName());
				N_ShutdownService();
			}
			else
			{
				success = false;
			}
		}
		else if(!stricmp(argv[1], "announce"))
		{
			N_MasterAnnounceServer(true);
		}
		else if(!stricmp(argv[1], "request"))
		{
			N_MasterRequestList();
		}			
		else if(!stricmp(argv[1], "modems"))
		{
			/* -- DISABLED --
			int num;
			char **modemList = jtNetGetStringList(JTNET_MODEM_LIST, &num);
			if(modemList == NULL)
			{
				Con_Printf( "No modem list available.\n");
			}
			else for(i=0; i<num; i++)
			{
				Con_Printf( "%d: %s\n", i, modemList[i]);
			}
			*/
		}
		else if(!stricmp(argv[1], "search"))
		{
			success = N_LookForHosts();
		}
		else if(!stricmp(argv[1], "servers")) 
		{
			N_MAPost(MAC_REQUEST);
			N_MAPost(MAC_WAIT);
			N_MAPost(MAC_LIST);
		}
		else if(!stricmp(argv[1], "info"))
		{
			if(isServer)
			{
				Con_Printf( "Clients:\n");
				for(i = 0; i < MAXPLAYERS; i++)
				{
					if(!clients[i].connected) continue;
					Con_Printf("%i: node %x, entered at %i (ingame:%i)\n", 
						i, clients[i].nodeID, clients[i].enterTime, 
						players[i].ingame);
				}
			}
			
			Con_Printf("Network game: %s\n", netgame? "yes" : "no");
			Con_Printf("Server: %s\n", isServer? "yes" : "no");
			Con_Printf("Client: %s\n", isClient? "yes" : "no");
			Con_Printf("Console number: %i\n", consoleplayer);
			Con_Printf("TCP/IP address: %s\n", nptIPAddress);
			Con_Printf("TCP/IP port: %d (0x%x)\n", nptIPPort, nptIPPort);
			Con_Printf("Modem: %d (%s)\n", 0, "?" /*jtNetGetInteger(JTNET_MODEM),
				jtNetGetString(JTNET_MODEM)*/);
			/*Con_Printf("Phone number: %s\n", jtNetGetString(JTNET_PHONE_NUMBER));*/
			Con_Printf("Serial: COM %d, baud %d, stop %d, parity %d, "
				"flow %d\n", nptSerialPort, nptSerialBaud, nptSerialStopBits,
				nptSerialParity, nptSerialFlowCtrl);
			/*if((i = jtNetGetInteger(JTNET_BANDWIDTH)) != 0)
				Con_Printf("Bandwidth: %i bps\n", i);
			Con_Printf("Est. latency: %i ms\n", jtNetGetInteger(JTNET_EST_LATENCY));
			Con_Printf("Packet header: %i bytes\n", jtNetGetInteger(JTNET_PACKET_HEADER_SIZE));
			*/
		}
		else if(!stricmp(argv[1], "disconnect"))
		{
			if(!netgame)
			{
				Con_Printf("This client is not connected to a server.\n");
				return false;
			}
			if(!isClient)
			{
				Con_Printf("This is not a client.\n");
				return false;
			}

			if((success = N_Disconnect()) != false)
			{
				Con_Message("Disconnected.\n");
			}
		}
		else 
		{
			Con_Printf( "Bad arguments.\n");
			return false; // Bad args.
		}
	}
	if(argc == 3)	// Two arguments?
	{
		if(!stricmp(argv[1], "init"))
		{
			serviceprovider_t sp = (!stricmp(argv[2], "tcp/ip") 
					|| !stricmp(argv[2], "tcpip"))? NSP_TCPIP
				: !stricmp(argv[2], "ipx")? NSP_IPX
				: !stricmp(argv[2], "serial")? NSP_SERIAL
				: !stricmp(argv[2], "modem")? NSP_MODEM
				: NSP_NONE;

			if(sp == NSP_NONE)
			{
				Con_Message("%s is not a supported service provider.\n", 
					argv[2]);
				return false;
			}

			// Init the service (assume client mode).
			if( (success = N_InitService(sp, false)) )
				Con_Message("Network initialization OK.\n");
			else
				Con_Message("Network initialization failed!\n");

			// Let everybody know of this.
			CmdReturnValue = success;
		}
		else if(!stricmp(argv[1], "server"))
		{
			if(!stricmp(argv[2], "go") || !stricmp(argv[2], "start"))
			{
				if(netgame)
				{
					Con_Printf("Already in a netgame.\n");
					return false;
				}

				CmdReturnValue = success = N_ServerOpen();

				if(success)
				{
					Con_Message("Server \"%s\" started.\n", serverName);
				}
			}
			else if(!stricmp(argv[2], "close") || !stricmp(argv[2], "stop"))
			{
				if(!isServer)
				{
					Con_Printf( "This is not a server!\n");
					return false;
				}

				// Close the server and kick everybody out.
				if((success = N_ServerClose()) != false)
				{
					Con_Message("Server \"%s\" closed.\n", serverName);
				}
			}
			else
			{
				Con_Printf("Bad arguments.\n");
				return false;
			}
		}
		else if(!stricmp(argv[1], "connect"))
		{	
			int idx;

			if(netgame)
			{
				Con_Printf("Already connected.\n");
				return false;
			}

			idx = strtol(argv[2], NULL, 10);		
			CmdReturnValue = success = N_Connect(idx);

			if(success)
			{
				Con_Message("Connected.\n");
			}
		}
		else if(!stricmp(argv[1], "mconnect"))
		{
			serverinfo_t info;
			if(N_MasterGet(strtol(argv[2], 0, 0), &info))
			{
				// Connect using TCP/IP.
				return Con_Executef(false, "connect %s %i", info.address, 
					info.port);
			}
			else return false;
		}
		else if(!stricmp(argv[1], "setup"))
		{
			// Start network setup.
			DD_NetSetup(!stricmp(argv[2], "server"));
			CmdReturnValue = true;
		}
	}
/*	if(argc >= 3)
	{
		// TCP/IP settings.
		if(!stricmp(argv[1], "tcpip") || !stricmp(argv[1], "tcp/ip"))
		{
			if(!stricmp(argv[2], "address") && argc > 3)
			{
				success = jtNetSetString(JTNET_TCPIP_ADDRESS, argv[3]);
				Con_Printf( "TCP/IP address: %s\n", argv[3]);
			}
			else if(!stricmp(argv[2], "port") && argc > 3)
			{
				int val = strtol(argv[3], NULL, 0);
				success = jtNetSetInteger(JTNET_TCPIP_PORT, val);
				Con_Printf( "TCP/IP port: %d\n", val);
			}
		}
		// Modem settings.
		if(!stricmp(argv[1], "modem"))
		{
			if(!stricmp(argv[2], "phone"))
			{
				success = jtNetSetString(JTNET_PHONE_NUMBER, argv[3]);
				Con_Printf( "Modem phone number: %s\n", argv[3]);
			}
			else 
			{
				int val = strtol(argv[2], NULL, 0);
				char **modemList = jtNetGetStringList(JTNET_MODEM_LIST, NULL);
				success = jtNetSetInteger(JTNET_MODEM, val);
				if(success)
				{
					Con_Printf( "Selected modem: %d, %s\n", val, modemList[val]);
				}
			}
		}
		// Serial settings.
		else if(!stricmp(argv[1], "serial"))
		{
			if(!stricmp(argv[2], "com"))
			{
				int val = strtol(argv[3], NULL, 0);
				success = jtNetSetInteger(JTNET_COMPORT, val);
				if(success) Con_Printf( "COM port: %d\n", val);
			}
			else if(!stricmp(argv[2], "baud"))
			{
				int val = strtol(argv[3], NULL, 0);
				success = jtNetSetInteger(JTNET_BAUDRATE, val);
				if(success) Con_Printf( "Baud rate: %d\n", val);
			}
			else if(!stricmp(argv[2], "stop"))
			{
				int val = strtol(argv[3], NULL, 0);
				success = jtNetSetInteger(JTNET_STOPBITS, val);
				if(success) Con_Printf( "Stopbits: %d\n", val);
			}
			else if(!stricmp(argv[2], "parity"))
			{
				int val = strtol(argv[3], NULL, 0);
				success = jtNetSetInteger(JTNET_PARITY, val);
				if(success) Con_Printf( "Parity: %d\n", val);
			}
			else if(!stricmp(argv[2], "flow"))
			{
				int val = strtol(argv[3], NULL, 0);
				success = jtNetSetInteger(JTNET_FLOWCONTROL, val);
				if(success) Con_Printf( "Flow control: %d\n", val);
			}
		}
	}*/
	return success;
}

/*
 * Console command for printing the Huffman efficiency.
 */
int CCmdHuffmanStats(int argc, char **argv)
{
	if(!gNumOutBytes)
	{
		Con_Printf("Nothing has been sent yet.\n");
	}
	else
	{
		Con_Printf("Huffman efficiency: %.3f%% (data: %i bytes, sent: %i "
			"bytes)\n", 100 - (100.0f * gNumSentBytes) / gNumOutBytes,
			gNumOutBytes, gNumSentBytes);
	}
	return true;
}
