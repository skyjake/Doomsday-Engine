// jtNet.h : main header file for the JTNET DLL
//

#ifndef __JTNETDLL_H__
#define __JTNETDLL_H__

#include <dplay.h>
#include <dplobby.h>
#include "jtNetEx.h"

// Version information.
#define JTNET_VERSION_NUM			241
#define JTNET_VERSION_STR			"2.4.1"

#ifndef JTNET_DX3
#define JTNET_VER_ID				"DX6"
#else
#define JTNET_VER_ID				"DX3 BETA"
#endif

#define JTNET_VERSION_FULL			"jtNet Version "JTNET_VERSION_STR" "__DATE__" ("JTNET_VER_ID")"

// {7DDFA9A0-84EA-11d3-B689-E29406BD95EC}
static const GUID GUID_jtNet = 
{ 0x7ddfa9a0, 0x84ea, 0x11d3, { 0xb6, 0x89, 0xe2, 0x94, 0x6, 0xbd, 0x95, 0xec } };

// {42A23743-4B6E-11d4-9FA8-F0D471C10801}
static const GUID GUID_jtNetMaster = 
{ 0x42a23743, 0x4b6e, 0x11d4, { 0x9f, 0xa8, 0xf0, 0xd4, 0x71, 0xc1, 0x8, 0x1 } };

// TYPES ------------------------------------------------------------------

struct jtnetcon_t
{
	GUID	guid;
	int		type;			// JTNET_SERVICE_*
	void	*connection;	// The connection data.
	int		size;			// Size of the connection data.
	char	name[100];		
};

struct jtnetsession_t
{
	DPSESSIONDESC2		desc;
	char				name[64];
	char				info[128];
	char				app[100];
};

// GLOBAL VARIABLES -------------------------------------------------------

#ifndef JTNET_DX3
extern LPDIRECTPLAY4A	dPlay;
#else
extern LPDIRECTPLAY2A	dPlay;
#endif

extern LPDIRECTPLAYLOBBY3A dPLobby;

extern DPID				thisPlrId;	// Id of the local player.
extern bool				serverMode;

extern char				appName[100];
extern char				serverNameStr[100];
extern char				serverInfoStr[100];
extern DPSESSIONDESC2	serverSession;


extern jtnetcon_t		*connections;
extern int				numConnections;
extern jtnetsession_t	*sessions;
extern int				numSessions;
extern jtnetplayer_t	*players;
extern int				numPlayers;

extern bool				connectionInitOk; 

extern char				masterAddress[128];
extern int				masterPort;

extern int				masterConnection;
extern int				listReceived;

// FUNCTIONS --------------------------------------------------------------

// jtNet.cpp
int jtEnumerateSessions(bool wait=false);
int jtEnumeratePlayers();
int jtValidateConnectionEx(void *DPlayAddr);
int jtConnect(DPSESSIONDESC2 *sd);
BOOL FAR PASCAL sessionEnumerator(LPCDPSESSIONDESC2 lpThisSD, LPDWORD lpdwTimeOut, 
								  DWORD dwFlags, LPVOID lpContext);

#endif __JTNETDLL_H__
