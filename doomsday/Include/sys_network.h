//===========================================================================
// SYS_NETWORK.H
//===========================================================================
#ifndef __DOOMSDAY_SYSTEM_NETWORK_H__
#define __DOOMSDAY_SYSTEM_NETWORK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "con_decl.h"

typedef enum serviceprovider_e {
	NSP_NONE,
	NSP_TCPIP,
	NSP_IPX,
	NSP_MODEM,
	NSP_SERIAL,
	NUM_NSP
} serviceprovider_t;

// Send Packet flags:
#define SPF_REBOUND		0x00020000	// Write only to local loopback
#define SPF_DONT_SEND	0x00040000	// Don't really send out anything
#define SPF_CONFIRM		0x40000000	// Make sure it's received
#define SPF_ORDERED		0x80000000	// Send in order & confirm

// If a master action fails, the action queue is emptied.
typedef enum {
	MAC_REQUEST,	// Retrieve the list of servers from the master.
	MAC_WAIT,		// Wait for the server list to arrive.
	MAC_LIST		// Print the server list in the console.
} masteraction_t;

extern boolean	allowSending;
extern int		maxQueuePackets;

extern serviceprovider_t gCurrentProvider;

extern int		nptActive;
extern char		*nptIPAddress;
extern int		nptIPPort;
extern int		nptModem;	
extern char		*nptPhoneNum;
extern int		nptSerialPort;
extern int		nptSerialBaud;
extern int		nptSerialStopBits;
extern int		nptSerialParity;
extern int		nptSerialFlowCtrl;

extern char		*serverName, *serverInfo, *playerName;
extern int		serverData[];

extern char		*masterAddress;
extern int		masterPort;
extern char		*masterPath;

void	N_Init(void);
void	N_Shutdown(void);
boolean	N_InitService(serviceprovider_t provider, boolean inServerMode);
void	N_ShutdownService(void);
boolean	N_IsAvailable(void);
boolean N_LookForHosts(void);

void	N_SendPacket(int flags);
boolean	N_GetPacket();
//boolean	N_CheckSendQueue(int player);
uint	N_GetSendQueueCount(int player);
uint	N_GetSendQueueSize(int player);
void	N_TerminateClient(int console);

void	N_Update();
void	N_Ticker(void);
const char* N_GetProtocolName(void);

int		N_GetHostCount(void);
boolean	N_GetHostInfo(int index, struct serverinfo_s *info);
void	N_PrintServerInfo(int index, struct serverinfo_s *info);
unsigned int N_GetServiceProviderCount(serviceprovider_t type);
boolean	N_GetServiceProviderName(serviceprovider_t type, unsigned int index, char *name, int maxLength);

/*
int		N_SetPlayerData(void *data, int size);
int		N_GetPlayerData(int playerNum, void *data, int size);
int		N_SetServerData(void *data, int size);
int		N_GetServerData(void *data, int size);
*/

void	N_MAPost(masteraction_t act);
boolean	N_MADone(void);

// The 'net' console command.
D_CMD( Net );

#ifdef __cplusplus
}
#endif

#endif 
