#ifndef __NETCL_H__
#define __NETCL_H__

#ifdef __JHERETIC__
#include "../JHeretic/DoomDef.h"
#endif

#include "d_Net.h"

void *NetCl_WriteCommands(ticcmd_t *cmd, int count);
void NetCl_UpdateGameState(byte *data);
void NetCl_SpawnMissile(int type, fixed_t x, fixed_t y, fixed_t z, 
						fixed_t momx, fixed_t momy, fixed_t momz);
void NetCl_UpdatePlayerState(byte *data, int plrNum);
void NetCl_UpdatePlayerState2(byte *data, int plrNum);
void NetCl_UpdatePSpriteState(byte *data);
void NetCl_Intermission(byte *data);
void NetCl_Finale(int packetType, byte *data);
void NetCl_UpdatePlayerInfo(byte *data);
void NetCl_SendPlayerInfo();
void NetCl_SaveGame(void *data);
void NetCl_LoadGame(void *data);
void NetCl_Paused(boolean setPause);

#endif