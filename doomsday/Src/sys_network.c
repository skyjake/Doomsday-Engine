
//**************************************************************************
//**
//** SYS_NETWORK.C
//**
//** Must rewrite to use TCP/UDP.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <winsock.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_ui.h"

#ifdef _DEBUG
#include <assert.h>
#endif

// MACROS ------------------------------------------------------------------

#define LFID_NONE			-1
#define DCN_MISSING_NODE	-10

#define MASTER_QUEUE_LEN	16

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		netAvailable = false;
boolean		allowSending = true;

netbuffer_t	netbuffer;
int			maxQueuePackets = 0; //5;

char		*serverName = "Doomsday";
char		*serverInfo = "Multiplayer game server";
char		*playerName = "Player";

// Some parameters passed to master server.
int			serverData[3];

// Settings for the various network protocols.
int			nptActive = 0;
// TCP/IP.
char		*nptIPAddress = "";
int 		nptIPPort = 0;	// Automatical.
// Modem.
int			nptModem = 0;	// Selected modem index.
char		*nptPhoneNum = "";
// Serial.
int			nptSerialPort = 1;
int			nptSerialBaud = 57600;
int			nptSerialStopBits = 0;
int			nptSerialParity = 0;
int			nptSerialFlowCtrl = 4;

// Master server info. Hardcoded defaults.
char		*masterAddress = "www.doomsdayhq.com"; 
int			masterPort = 0; // Uses 80 by default.
char		*masterPath = "/master.php";
boolean		masterAware = false;		

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int	lastFrameId[MAXPLAYERS];
static int	localtime;
static char *protocolNames[5] = { NULL, "IPX", "TCP/IP", "Serial Link", "Modem" };

// The master action queue. Perhaps not the best way to do this,
// but works nonetheless.
static masteraction_t masterQueue[MASTER_QUEUE_LEN];
static int	mqHead, mqTail;

// CODE --------------------------------------------------------------------

// Add a master action command to the queue.
void N_MAPost(masteraction_t act)
{
	masterQueue[mqHead] = act;
	mqHead = (++mqHead) % MASTER_QUEUE_LEN;
	//if(mqHead == mqTail) I_Error("N_MAPost: master action queue overflow!\n");
}

boolean N_MAGet(masteraction_t *act)
{
	// Empty queue?
	if(mqHead == mqTail) return false;	
	*act = masterQueue[mqTail];
	return true;
}

void N_MARemove(void)
{
	if(mqHead != mqTail) mqTail = (++mqTail) % MASTER_QUEUE_LEN;
}

void N_MAClear(void)
{
	mqHead = mqTail = 0;
}

boolean N_MADone(void)
{
	return (mqHead == mqTail);
}

int N_UpdateNodes()
{
	unsigned int idlist[MAXPLAYERS];
	int		i, k, numnodes = jtNetGetPlayerIDs(idlist);

	if(!isClient)
	{
		clients[0].nodeID = jtNetGetInteger(JTNET_MY_PLAYER_ID);
		// Find the correct jtNet nodes for each player.
		for(i=0; i<MAXPLAYERS; i++)
			if(clients[i].connected)
			{
				clients[i].jtNetNode = DCN_MISSING_NODE;
				for(k=0; k<MAXPLAYERS; k++)			
					if(idlist[k] == clients[i].nodeID)
					{
						clients[i].jtNetNode = k;
						break;
					}
			}
	}
	else
	{
		// Clients only know about the server.
		clients[0].jtNetNode = JTNET_SERVER_NODE;
	}
	return numnodes;
}

// Returns the name of the specified player.
char *N_GetPlayerName(int player)
{
	return clients[player].name;
}

id_t N_GetPlayerID(int player)
{
	if(!clients[player].connected) return 0;
	return clients[player].id;
}

// Returns true if the operation is successful.
int N_SetPlayerData(void *data, int size)
{
	return jtNetSetData(JTNET_PLAYER_DATA, data, size);
}

// Returns true if the operation is successful.
int N_GetPlayerData(int playerNum, void *data, int size)
{
	int		node=-1;

	if(playerNum >= 0 && playerNum < MAXPLAYERS)
		node = clients[playerNum].jtNetNode;
	return jtNetGetData(JTNET_PLAYER_DATA, node, data, size);
}

int N_SetServerData(void *data, int size)
{
	return jtNetSetData(JTNET_SERVER_DATA, data, size);
}

int N_GetServerData(void *data, int size)
{
	return jtNetGetData(JTNET_SERVER_DATA, 0, data, size);
}

//===========================================================================
// N_InitService
//	Initialize the driver. Returns true if successful.
//	'sp' must be one of the jtNet server provider constants.
//===========================================================================
int N_InitService(int sp)
{
	int		i;

	// Clear the last frame message IDs.
	for(i = 0; i < MAXPLAYERS; i++) lastFrameId[i] = LFID_NONE;

	i = jtNetInit(sp);
	if(i == JTNET_ERROR_ALREADY_INITIALIZED) 
	{
		Con_Message("N_InitService: Network driver already initialized.\n");
		return true;
	}
	if(i != JTNET_ERROR_OK)
	{
		Con_Printf("N_InitService: Failed, error code %i.\n", i);
		return false;
	}

	Con_Printf("N_InitService: %s initialized.\n", N_GetProtocolName());

	netAvailable = true;

	// Set some callback functions.
	jtNetSetCallback(JTNET_PLAYER_CREATED_CALLBACK, Sv_PlayerArrives);
	jtNetSetCallback(JTNET_PLAYER_DESTROYED_CALLBACK, Sv_PlayerLeaves);

	localtime = 0;

	// The game dll specifies a game id for us.
	jtNetSetString(JTNET_APPLICATION_NAME, gx.Get(DD_GAME_ID));

	// Clear the master action queue.
	mqHead = mqTail = 0;

	// Setup the protocol in use.
	switch(sp)
	{
	case JTNET_SERVICE_TCPIP:
		jtNetSetString(JTNET_TCPIP_ADDRESS, nptIPAddress);
		jtNetSetInteger(JTNET_TCPIP_PORT, nptIPPort);
		Con_Printf("TCP/IP: %s, port: %i\n", nptIPAddress, nptIPPort); 
		break;

	case JTNET_SERVICE_MODEM:
		jtNetSetString(JTNET_PHONE_NUMBER, nptPhoneNum);
		jtNetSetInteger(JTNET_MODEM, nptModem);
		Con_Printf("Modem: #%i, dial to: %s\n", nptModem, nptPhoneNum);
		break;

	case JTNET_SERVICE_SERIAL:
		jtNetSetInteger(JTNET_COMPORT, nptSerialPort);
		jtNetSetInteger(JTNET_BAUDRATE, nptSerialBaud);
		jtNetSetInteger(JTNET_STOPBITS, nptSerialStopBits);
		jtNetSetInteger(JTNET_PARITY, nptSerialParity);
		jtNetSetInteger(JTNET_FLOWCONTROL, nptSerialFlowCtrl);
		Con_Printf("Serial: COM%i, rate: %i bps (stpb:%i,par:%i,flwct:%i)\n",
			nptSerialPort, nptSerialBaud, nptSerialStopBits,
			nptSerialParity, nptSerialFlowCtrl);
		break;

	default:
		// Otherwise no settings needed.
		break;
	}
	return true;
}

//===========================================================================
// N_ShutdownService
//===========================================================================
void N_ShutdownService(void)
{
	if(!netAvailable) return;
	if(netgame)
	{
		Con_Execute(isServer? "net server close" : "net disconnect", true);
	}
	netgame = false;
	netAvailable = false;
	jtNetShutdown();
}

//===========================================================================
// N_Init
//	Initialize the low-level network subsystem.
//===========================================================================
void N_Init(void)
{
	N_SockInit();
	N_MasterInit();
}

//===========================================================================
// N_Shutdown
//	Shut down the low-level network interface.
//===========================================================================
void N_Shutdown()
{
	N_ShutdownService();
	N_MasterShutdown();
	N_SockShutdown();
}

// Send the data in the doomcom databuffer.
// destination is a player number.
void N_SendPacket(int flags)
{
	int msgId;
	int priority = flags & SPF_PRIORITY_MASK;
	int	dest = netbuffer.player;

	// Is the network available?
	if(!allowSending || !netAvailable) return;

	if(dest >= 0 && dest < MAXPLAYERS)
	{
		dest = clients[dest].jtNetNode;
		// Do not send anything to local players.
		if(players[dest].flags & DDPF_LOCAL) return;
		if(dest == DCN_MISSING_NODE)
		{
			//Con_Message("SendPacket: destnode missing!\n");
			return;
		}
	}
	if(!priority)
	{
		// Non-reliable messages go first by default.
		if(!(flags & SPF_RELIABLE)) priority = 10;
	}
	if(flags & SPF_FRAME)
	{
		// Remove the existing frame packet from the send queue.
		if(lastFrameId[dest] != LFID_NONE) jtNetCancel(lastFrameId[dest]);
	}
	// Send the packet.
	jtNetSend(dest, &netbuffer.msg, netbuffer.headerLength + netbuffer.length,
		(flags & SPF_RELIABLE) != 0, priority, 0, &msgId);
	if(flags & SPF_FRAME)
	{
		// Update the last frame ID.
		lastFrameId[dest] = msgId;
	}
}

boolean N_GetPacket()
{
	int from, bytes, i;

	if(!netAvailable) return false;

	netbuffer.player = -1;
	netbuffer.length = 0;
	bytes = jtNetGet(&from, &netbuffer.msg, sizeof(netbuffer.msg));
	if(!bytes || from == -1) return false;	// No packet.

	// There was a packet!
	netbuffer.length = bytes - netbuffer.headerLength;	
	
	// What is the corresponding player number?
	for(i=0; i<MAXPLAYERS; i++)
		if(clients[i].jtNetNode == from)
		{
			netbuffer.player = i;
			break;
		}

#ifdef _DEBUG
	assert(netbuffer.player != -1);
#endif

	return true;
}

char *N_GetProtocolName(void)
{
	return protocolNames[jtNetGetInteger(JTNET_SERVICE)];
}

// Returns true if the send queue for the player is good for additional
// data. If not, new data shouldn't be sent until the queue has cleared up.
boolean N_CheckSendQueue(int player)
{
	int	bytes;

	// Servers send packets to themselves only when they're recording a demo.
	if(isServer && players[player].flags & DDPF_LOCAL) 
		return clients[player].recording;
	if(!netAvailable || playback) return false;
	if(!clients[player].connected || player == consoleplayer) return false;
	return jtNetCheckQueue(clients[player].jtNetNode, &bytes) 
		<= maxQueuePackets;
}


//-----------------------------------------------------------------
//
// Network Events
//
void N_ServerStart(boolean before)
{
	if(before)
	{
		Demo_StopPlayback();

		// The game DLL may have something that needs doing
		// before we actually begin.
		if(gx.NetServerStart) gx.NetServerStart(true);

		// The info strings.
		jtNetSetString(JTNET_SERVER_INFO, serverInfo);
		jtNetSetString(JTNET_NAME, playerName);
		jtNetSetInteger(JTNET_SERVER_DATA1, W_CRCNumber());

		// Update the server data parameters.
		serverData[0] = W_CRCNumber();
	}
	else
	{
		Sv_StartNetGame();

		// The game DLL might want to do something now that the
		// server is started.
		if(gx.NetServerStart) gx.NetServerStart(false);

		if(masterAware && jtNetGetInteger(JTNET_SERVICE) == 
			JTNET_SERVICE_TCPIP)
		{
			N_MasterAnnounceServer(true);
		}
	}
}

void N_ServerClose(boolean before)
{
	if(before)
	{
		if(masterAware && jtNetGetInteger(JTNET_SERVICE) == 
			JTNET_SERVICE_TCPIP)
		{
			N_MAClear();
			// Bye-bye, master server.
			N_MasterAnnounceServer(false);
		}
		if(gx.NetServerStop) gx.NetServerStop(true);
		Net_StopGame();
	}
	else
	{
		//isServer = false;
	}
	// Call game DLL's NetServerStop.
	if(gx.NetServerStop) gx.NetServerStop(before);
}

void N_Connecting(boolean before)
{
	if(before)
	{
		Demo_StopPlayback();
		// Call game DLL's NetConnect.
		gx.NetConnect(before);
		return;
	}

	handshake_received = false;
	netgame = true; // Allow sending/receiving of packets.
	isServer = false;
	isClient = true;
	N_UpdateNodes();
	// Call game DLL's NetConnect.
	gx.NetConnect(before);
	// G'day mate!
	Msg_Begin(pcl_hello);
	Msg_WriteLong(clientID);
	Net_SendBuffer(0, SPF_RELIABLE);
}

// before = true if the connection hasn't yet been closed.
void N_Disconnect(boolean before)
{
	if(before)
	{
		if(gx.NetDisconnect) gx.NetDisconnect(true);
		Net_StopGame();
	}
	else
	{
		// Call game DLL's NetDisconnected.
		if(gx.NetDisconnect) gx.NetDisconnect(false);
	}
}

//===========================================================================
// N_Ticker
//	Handle low-level net tick stuff: communication with the master server.
//===========================================================================
void N_Ticker(void)
{
	masteraction_t act;
	int	i, num;

//	if(!netAvailable) return;

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
			Con_Printf("    %-20s P/M  L Ver:  Game:            Location:\n", "Name:");
			num = i = N_MasterGet(0, 0);
			while(--i >= 0)
			{
				serverinfo_t info;
				N_MasterGet(i, &info);
				Con_Printf("%-2i: %-20s %i/%-2i %c %-5i %-16s %s:%i\n", 
					i, info.name,
					info.players, info.maxPlayers,
					info.canJoin? ' ':'*', info.version, info.game,
					info.address, info.port);
				Con_Printf("    %s (%x) %s\n", info.map, info.data[0], 
					info.description);
			}
			Con_Printf("%i server%s found.\n", num, num!=1? "s were" : " was");
			N_MARemove();			
			break;
		}
	}
}

//-----------------------------------------------------------------
// 
// The NET Console Command
//
int CCmdNet(int argc, char **argv)
{
	int		i;
	boolean success = true;

	if(argc == 1)	// No args?
	{
		Con_Printf( "Usage: %s (cmd/args)\n", argv[0]);
		return true;
	}
	if(argc == 2)	// One argument?
	{
		if(!stricmp(argv[1], "shutdown"))
		{
			N_Shutdown();
			Con_Printf( "Network shut down.\n");
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
		}
		else if(!stricmp(argv[1], "search"))
		{
			int num = jtNetGetServerInfo(NULL, 0);
			if(num < 0)
			{
				Con_Printf( "Server list is being retrieved. Try again in a moment.\n");
			}
			else if(num == 0)
			{
				Con_Printf( "No server list available.\n");
			}
			else 
			{
				jtnetserver_t *buffer = malloc(sizeof(jtnetserver_t) * num);
				jtNetGetServerInfo(buffer, num);
				Con_Printf( "%d server%s found.\n", num, num!=1? "s were" : " was");
				if(num)
				{
					Con_Printf("   %-20s P/M  L Description:\n", "Name:");
					for(i=0; i<num; i++)
					{
						Con_Printf("%i: %-20s %d/%-2d %c %s\n", i,
							buffer[i].name,
							buffer[i].players, buffer[i].maxPlayers,
							buffer[i].canJoin? ' ':'*',
							buffer[i].description);
					}
				}
				free(buffer);
			}			
		}
		else if(!stricmp(argv[1], "servers")) 
		{
			N_MAPost(MAC_REQUEST);
			N_MAPost(MAC_WAIT);
			N_MAPost(MAC_LIST);
		}
		else if(!stricmp(argv[1], "players"))
		{
			int num = 0;
			char **nameList = jtNetGetStringList(JTNET_PLAYER_NAME_LIST, &num);
			if(!num)
			{
				Con_Printf( "No player list available.\n");
			}
			else 
			{
				Con_Printf( "There %s %d player%s.\n", num==1? "is" : "are", num, num!=1? "s" : "");
				for(i=0; i<num; i++)
					Con_Printf( "%d: %s\n", i, nameList[i]);
			}
		}			
		else if(!stricmp(argv[1], "info"))
		{
			if(isServer)
			{
				Con_Printf( "Clients:\n");
				for(i=0; i<MAXPLAYERS; i++)
				{
					if(!clients[i].connected) continue;
					Con_Printf( "%i: node %i, node ID %x, entered at %i (ingame:%i)\n", 
						i, clients[i].jtNetNode,
						clients[i].nodeID, 
						clients[i].enterTime,
						players[i].ingame);
				}
			}
			
			Con_Printf("Network game: %s\n", netgame? "yes" : "no");
			Con_Printf("Server: %s\n", isServer? "yes" : "no");
			Con_Printf("Client: %s\n", isClient? "yes" : "no");
			Con_Printf("Console number: %i\n", consoleplayer);
			Con_Printf("TCP/IP address: %s\n", jtNetGetString(JTNET_TCPIP_ADDRESS));
			Con_Printf("TCP/IP port: %d (0x%x)\n", jtNetGetInteger(JTNET_TCPIP_PORT),
				jtNetGetInteger(JTNET_TCPIP_PORT));
			Con_Printf("Modem: %d (%s)\n", jtNetGetInteger(JTNET_MODEM),
				jtNetGetString(JTNET_MODEM));
			Con_Printf("Phone number: %s\n", jtNetGetString(JTNET_PHONE_NUMBER));
			Con_Printf("Serial: COM %d, baud %d, stop %d, parity %d, flow %d\n",
				jtNetGetInteger(JTNET_COMPORT),
				jtNetGetInteger(JTNET_BAUDRATE),
				jtNetGetInteger(JTNET_STOPBITS),
				jtNetGetInteger(JTNET_PARITY),
				jtNetGetInteger(JTNET_FLOWCONTROL));
			if((i = jtNetGetInteger(JTNET_BANDWIDTH)) != 0)
				Con_Printf("Bandwidth: %i bps\n", i);
			Con_Printf("Est. latency: %i ms\n", jtNetGetInteger(JTNET_EST_LATENCY));
			Con_Printf("Packet header: %i bytes\n", jtNetGetInteger(JTNET_PACKET_HEADER_SIZE));
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

			// Things that have to be done before closing the connection.
			N_Disconnect(true);

			success = !jtNetDisconnect();
			if(success)
			{
				Con_Printf( "Disconnected.\n");
				// Things that are done afterwards.
				N_Disconnect(false);
			}
			else
				Con_Printf( "Error disconnecting.\n");
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
			int sp = (!stricmp(argv[2], "tcp/ip") || !stricmp(argv[2], "tcpip"))? JTNET_SERVICE_TCPIP 
				: !stricmp(argv[2], "ipx")? JTNET_SERVICE_IPX 
				: !stricmp(argv[2], "serial")? JTNET_SERVICE_SERIAL
				: !stricmp(argv[2], "modem")? JTNET_SERVICE_MODEM
				: JTNET_SERVICE_UNKNOWN;

			if(sp == JTNET_SERVICE_UNKNOWN)
			{
				Con_Message( "%s is not a supported service provider.\n", argv[2]);
				return false;
			}

			// Initialization.
			success = N_InitService(sp);
			if(success)
				Con_Message( "Network initialization OK.\n");
			else
				Con_Message( "Network initialization failed!\n");

			// Let everybody know of this.
			CmdReturnValue = success;
		}
		else if(!stricmp(argv[1], "server"))
		{
			if(!stricmp(argv[2], "go") || !stricmp(argv[2], "start"))
			{
				if(netgame)
				{
					Con_Printf( "Already in a netgame.\n");
					return false;
				}
				N_ServerStart(true);

				// Now we can try to open the server.
				success = !jtNetOpenServer(serverName);
				if(success)
				{
					Con_Printf( "Server \"%s\" started.\n", serverName);
					N_ServerStart(false);
				}
				else
					Con_Printf( "Server start failed.\n");
				CmdReturnValue = success;
			}
			else if(!stricmp(argv[2], "close"))
			{
				if(!isServer)
				{
					Con_Printf( "This is not a server!\n");
					return false;
				}
				// Close the server and kick everybody out.
				N_ServerClose(true);

				success = !jtNetCloseServer();
				if(success)
				{
					Con_Printf( "Server closed.\n");
					N_ServerClose(false);
				}
				else
					Con_Printf( "Server closing failed.\n");
			}
			else
			{
				Con_Printf( "Bad arguments.\n");
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

			// Set the name of the player.
			jtNetSetString(JTNET_NAME, playerName);

			N_Connecting(true);

			idx = strtol(argv[2], NULL, 10);
			success = !jtNetConnect2(idx);

			if(success)
			{
				Con_Printf("Connected.\n");
				N_Connecting(false);				
			}
			else
			{
				Con_Printf("Connection failed.\n");
			}
			CmdReturnValue = success;
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
	if(argc >= 3)
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
	}
	return success;
}