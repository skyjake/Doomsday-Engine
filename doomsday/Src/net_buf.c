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
 * net_buf.c: Network Message Handling and Buffering
 */

/*
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

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_system.h"
#include "de_network.h"
#include "de_console.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MSG_MUTEX_NAME	"MsgQueueMutex"

// Flags for the sent message store (for to-be-confirmed messages):
#define SMSF_ORDERED	0x1	// Block other ordered messages until confirmed
#define SMSF_QUEUED		0x2	// Ordered message waiting to be sent
#define SMSF_CONFIRMED	0x4	// Delivery has been confirmed! (OK to remove)

// Length of the received message ID history.
#define STORE_HISTORY_SIZE 100

// TYPES -------------------------------------------------------------------

typedef struct sentmessage_s {
	struct sentmessage_s *next, *prev;
	struct store_s *store;
	msgid_t id;
	uint timeStamp;
	int flags;
	nodeid_t destination;
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

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		allowSending = true;

netbuffer_t	netBuffer;

// The Sent Message Store: list of sent or queued messages waiting to be 
// confirmed.
static store_t stores[MAXPLAYERS];

// The message queue: list of incoming messages waiting for processing.
static netmessage_t *msgHead, *msgTail;

// A mutex is used to protect the addition and removal of messages from
// the message queue.
static int msgMutex;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// Number of bytes of outgoing data transmitted.
static uint numOutBytes;

// Number of bytes sent over the network (compressed).
static uint numSentBytes;

// CODE --------------------------------------------------------------------

/*
 * Initialize the low-level network subsystem. This is called always 
 * during startup (via Sys_Init()).
 */
void N_Init(void)
{
	// Create a mutex for the message queue.
	msgMutex = Sys_CreateMutex(MSG_MUTEX_NAME);
	
	N_SockInit();
	N_MasterInit();
	N_SystemInit(); // Platform dependent stuff.
}

/*
 * Shut down the low-level network interface. Called during engine 
 * shutdown (not before).
 */
void N_Shutdown(void)
{
	N_SystemShutdown();
	N_MasterShutdown();
	N_SockShutdown();

	// Close the handle of the message queue mutex.
	Sys_DestroyMutex(msgMutex);
	msgMutex = 0;
	
	if(ArgExists("-huffavg"))
	{
		Con_Execute("huffman", false);
	}
}

/*
 * Acquire or release ownership of the message queue mutex.
 * Returns true if successful.
 */
boolean N_LockQueue(boolean doAcquire)
{
	if(doAcquire)
		Sys_Lock(msgMutex);
	else
		Sys_Unlock(msgMutex);
	return true;
}

/*
 * Adds the given netmessage_s to the queue of received messages.
 * Before calling this, allocate the message using malloc().  We use a
 * mutex to synchronize access to the message queue.  This is called
 * in the network receiver thread.
 */
void N_PostMessage(netmessage_t *msg)
{
	N_LockQueue(true);

	// This will be the latest message.
	msg->next = NULL;

	if(msgTail)
	{
		// There are previous messages.
		msgTail->next = msg;
	}

	// The tail pointer points to the last message.
	msgTail = msg;

	// If there is no head, this'll be the first message.
	if(msgHead == NULL) msgHead = msg;

	N_LockQueue(false);
}

/*
 * Extracts the next message from the queue of received messages.
 * Returns NULL if no message is found. The caller must release the
 * message when it's no longer needed, using N_ReleaseMessage().  We
 * use a mutex to synchronize access to the message queue.  This is
 * called in the Doomsday thread.
 */
netmessage_t* N_GetMessage(void)
{
	// This is the message we'll return.
	netmessage_t *msg = NULL;
	
	N_LockQueue(true);
	if(msgHead != NULL)
	{
		msg = msgHead;

		// If there are no more messages, the tail pointer must be
		// cleared, too.
		if(!msgHead->next) msgTail = NULL;

		// Advance the head pointer.
		msgHead = msgHead->next;
	}
	N_LockQueue(false);

	// Identify the sender.
	if(msg)
	{
		msg->player = N_IdentifyPlayer(msg->sender);
	}
	return msg;
}

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
 * Add a new message to the Sent Message Store.
 */
sentmessage_t *N_SMSCreate(int player, msgid_t id, nodeid_t destID,
						   void *data, uint length)
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
	sentmessage_t *msg;

	for(msg = store->first; msg; msg = msg->next)
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
	sentmessage_t *msg;

	for(msg = store->first; msg; msg = msg->next)
	{
		if(msg->flags & SMSF_CONFIRMED) continue;
		if(msg->id == id)
		{
			msg->flags |= SMSF_CONFIRMED;
			
			// Note how long it took to confirm the message.
			Net_SetAckTime(player, Sys_GetRealTime() - msg->timeStamp);

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
void N_SMSReset(int player)
{
	store_t *store = &stores[player];
	
	// Destroy all the messages in the store.
	while(store->first)
	{
		N_SMSDestroy(store->first);
	}

	// Reset everything back to zero.
	memset(store, 0, sizeof(*store));
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
	int i;

	while((msg = N_GetMessage()) != NULL)
		N_ReleaseMessage(msg);

	// The queue is now empty.
	msgHead = msgTail = NULL;

	// Also clear the sent message store.
	for(i = 0; i < MAXPLAYERS; i++)
	{
		while(stores[i].first)
			N_SMSDestroy(stores[i].first);
	}
}

/*
 * Send a Confirmation of Delivery message. 
 */
void N_SendConfirmation(msgid_t id, nodeid_t where)
{
	uint size;

	// All data is sent using Huffman codes.
	void *data = Huff_Encode((byte*)&id, 2, &size);
	N_SendDataBuffer(data, size, where);

	// Increase the counters.
	numOutBytes += 2;
	numSentBytes += size;
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
	boolean isQueued = false;

	// Is the network available?
	if(!allowSending || !N_IsAvailable()) return;

	// Figure out the destination DPNID.
	if(netServerMode)
	{
		if(netBuffer.player >= 0 && netBuffer.player < MAXPLAYERS)
		{
			if(players[ netBuffer.player ].flags & DDPF_LOCAL
				|| !clients[ netBuffer.player ].connected) 
			{
				// Do not send anything to local or disconnected players.
				return;
			}

			dest = clients[ netBuffer.player ].nodeID;
		}
		else
		{
			// Broadcast to all non-local players, using recursive calls.
			for(i = 0; i < MAXPLAYERS; i++)
			{
				netBuffer.player = i;
				N_SendPacket(flags);
			}

			// Reset back to -1 to notify of the broadcast.
			netBuffer.player = NSP_BROADCAST;
			return;
		}
	}

	if(flags & SPF_ORDERED)
	{
		// If the Store already contains an ordered message for this player, 
		// this new one will be queued. The queue-status is lifted (and the 
		// message sent) when the previous ordered message is acknowledged.
		if(N_SMSContainsOrdered(netBuffer.player))
		{
			// Queued messages will not be sent yet.
			isQueued = true;
		}
	}

	// Do we need to generate an ID for the message?
	if(flags & SPF_CONFIRM || flags & SPF_ORDERED)
	{
		netBuffer.msg.id = N_GetNewMsgID(netBuffer.player);
	}
	else
	{
		// Normal, unconfirmed messages do not use IDs.
		netBuffer.msg.id = 0;
	}

	// This is what will be sent.
	numOutBytes += netBuffer.headerLength + netBuffer.length;

	// Compress using Huffman codes.
	data = Huff_Encode( (byte*) &netBuffer.msg, 
		netBuffer.headerLength + netBuffer.length, &size);

	// This many bytes are actually sent.
	numSentBytes += size;

	// Ordered and confirmed messages are placed in the Store until they
	// have been acknowledged.
	if(flags & SPF_CONFIRM || flags & SPF_ORDERED)
	{
		sentMsg = N_SMSCreate(netBuffer.player, netBuffer.msg.id, 
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
int N_IdentifyPlayer(nodeid_t id)
{
	int i;
	
	if(netServerMode)
	{
		// What is the corresponding player number? Only the server keeps
		// a list of all the IDs.
		for(i = 0; i < MAXPLAYERS; i++)
			if(clients[i].nodeID == id)
				return i;

		// Bogus?
		return -1;
	}

	// Clients receive messages only from the server.
	return 0;
}


/*
 * Returns the next message waiting in the incoming message queue.
 * Confirmations are handled here.
 *
 * NOTE: Skips all messages from unknown nodeids!
 */
netmessage_t *N_GetNextMessage(void)
{
	netmessage_t *msg;
	msgid_t id;

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

		// Decode the Huffman codes. The returned buffer is static, so
		// it doesn't need to be freed (not thread-safe, though).
		msg->data = Huff_Decode(msg->data, msg->size, &msg->size);

		// The DP buffer can be freed.
		N_ReturnBuffer(msg->handle);
		msg->handle = NULL;

		// First check the message ID (in the first two bytes).
		id = *(msgid_t*) msg->data;
		if(id)
		{
			// Confirmations of delivery are not time-critical, so
			// they can be done here.
			if(msg->size == 2)
			{
				// All the message contains is a short? This is a
				// confirmation from the receiver. The message will be
				// removed from the SMS in N_Update().
				N_SMSConfirm(msg->player, id);
				N_ReleaseMessage(msg);
				continue;
			}
			else
			{
				// The arrival of this message must be confirmed. Send
				// a reply immediately. Writes to the Huffman encoding
				// buffer.
				N_SendConfirmation(id, msg->sender);

/*#ifdef _DEBUG
				Con_Printf("N_GetNextMessage: Acknowledged arrival of "
					"F%i:%i.\n", msg->player, id);
#endif*/
			}

			// It's possible that a message times out just before the
			// confirmation is received. It's also possible that the
			// message was received, but the confirmation was lost. In
			// these cases, the recipient will get a second copy of
			// the message. We keep track of the ID numbers in order
			// to detect this.
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
boolean	N_GetPacket(void)
{
	netmessage_t *msg;

	// If there are net events pending, let's not return any packets
	// yet. The net events may need to be processed before the
	// packets.
	if(!N_IsAvailable() || N_NEPending()) return false;

	netBuffer.player = -1;
	netBuffer.length = 0;

	if((msg = N_GetNextMessage()) == NULL)
	{
		// No messages at this time.
		return false;
	}

	/*
	Con_Message("N_GetPacket: from=%x, len=%i\n", msg->sender, msg->size);
	*/

	// There was a packet!
	netBuffer.player = msg->player;
	netBuffer.length = msg->size - netBuffer.headerLength;	
	memcpy(&netBuffer.msg, msg->data, 
		MIN_OF(sizeof(netBuffer.msg), msg->size));

	// The message can now be freed.
	N_ReleaseMessage(msg);

	// We have no idea who sent this (on serverside).
	if(netBuffer.player == -1) return false;

	return true;
}

/*
 * Console command for printing the Huffman efficiency.
 */
int CCmdHuffmanStats(int argc, char **argv)
{
	if(!numOutBytes)
	{
		Con_Printf("Nothing has been sent yet.\n");
	}
	else
	{
		Con_Printf("Huffman efficiency: %.3f%% (data: %i bytes, sent: %i "
			"bytes)\n", 100 - (100.0f * numSentBytes) / numOutBytes,
			numOutBytes, numSentBytes);
	}
	return true;
}
