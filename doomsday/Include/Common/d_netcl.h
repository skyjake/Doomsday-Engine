#ifndef __NETCL_H__
#define __NETCL_H__

#ifdef __JHERETIC__
#include "../jHeretic/Doomdef.h"
#endif

#include "d_net.h"

void *NetCl_WriteCommands(ticcmd_t *cmd, int count);
void NetCl_UpdateGameState(byte *data);
void NetCl_CheatRequest(const char *command);
void NetCl_UpdatePlayerState(byte *data, int plrNum);
void NetCl_UpdatePlayerState2(byte *data, int plrNum);
void NetCl_UpdatePSpriteState(byte *data);
void NetCl_UpdateJumpPower(void *data);
void NetCl_Intermission(byte *data);
void NetCl_Finale(int packetType, byte *data);
void NetCl_UpdatePlayerInfo(byte *data);
void NetCl_SendPlayerInfo();
void NetCl_SaveGame(void *data);
void NetCl_LoadGame(void *data);
void NetCl_Paused(boolean setPause);

#endif
