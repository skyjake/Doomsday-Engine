#ifndef __DOOMSDAY_SERVER_H__
#define __DOOMSDAY_SERVER_H__

#include "dd_def.h"

#define SV_WELCOME_STRING "Doomsday "DOOMSDAY_VERSION_TEXT" Server (R6)"

// Anything closer than this is always taken into consideration when 
// deltas are being generated.
#define CLOSE_MOBJ_DIST		512

// Anything farther than this will never be taken into consideration.
#define FAR_MOBJ_DIST		1500

extern int sv_maxPlayers;
extern int allow_frames;	// Allow sending of frames.
extern int send_all_players;
extern int frame_interval;	// In tics.
extern int net_remoteuser;	// The client who is currently logged in.
extern char *net_password;	// Remote login password.

// sv_main.c
void Sv_Shutdown(void);
void Sv_StartNetGame();
boolean Sv_PlayerArrives(unsigned int nodeID, char *name);
void Sv_PlayerLeaves(unsigned int nodeID);
void Sv_Handshake(int playernum, boolean newplayer);
void Sv_GetPackets(void);
void Sv_SendText(int to, int con_flags, char *text);
void Sv_FixLocalAngles();
void Sv_Ticker(void);
int Sv_Latency(byte cmdtime);
void Sv_Kick(int who);
void Sv_GetInfo(serverinfo_t *info);
int Sv_GetNumPlayers(void);
int Sv_GetNumConnected(void);

// sv_pool.c
void Sv_InitPools(void);
void Sv_DoFrameDelta(int playerNum);
void Sv_WriteFrameDelta(int playerNum);
void Sv_AckDeltaSet(int playerNum, byte set);
void Sv_AckDeltaSetLocal(int playerNum);
void Sv_InitPoolForClient(int clnum);
void Sv_ClientCoords(int playerNum);
void Sv_DrainPool(int playerNum);
void Sv_PoolTicker(void);

// sv_frame.c
void Sv_TransmitFrame(void);

// sv_missile.c
void Sv_ScanForMissiles();
void Sv_PackMissiles(int plrNum);

// sv_track.c
/*void Sv_InitTrackers(void);
void Sv_TextureChanges(int sidenum, int flags);
void Sv_PlaneSound(sector_t *sector, boolean isCeiling, int sound_id, int pace);
void Sv_SectorReport(sector_t *sector, boolean isCeiling, fixed_t dest,
						fixed_t speed, int floorpic, int ceilingpic);
void Sv_LightReport(sector_t *sector);
void Sv_SendWorldUpdate(int playernum);
*/

#endif