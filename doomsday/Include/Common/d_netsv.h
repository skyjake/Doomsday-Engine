#ifndef __NETSV_H__
#define __NETSV_H__

#ifdef __JDOOM__
#include "../JDoom/p_local.h"
#endif

#ifdef __JHERETIC__
#include "../JHeretic/P_local.h"
#endif

#ifdef __JHEXEN__
#include "../JHexen/P_local.h"
#endif

extern boolean	cycling_maps, map_cycle_noexit;
extern char		*map_cycle;
extern char		gameConfigString[128];

void P_Telefrag(mobj_t *thing);

void NetSv_NewPlayerEnters(int plrnumber);
void NetSv_SendGameState(int flags, int to);
void NetSv_SendMessage(int plrNum, char *msg);
void NetSv_SendYellowMessage(int plrNum, char *msg);
void NetSv_SendPlayerState(int srcPlrNum, int destPlrNum, int flags, boolean reliable);
void NetSv_SendPlayerState2(int srcPlrNum, int destPlrNum, int flags, boolean reliable);
void NetSv_PSpriteChange(int plrNum, int state);
void NetSv_Sound(mobj_t *origin, int sound_id, int toPlr); // toPlr=0: broadcast.
void NetSv_SoundAtVolume(mobj_t *origin, int sound_id, int volume, int toPlr);
void NetSv_Intermission(int flags, int state, int time);
void NetSv_Finale(int flags, char *script, boolean *conds, int numConds);
void NetSv_SendPlayerInfo(int whose, int to_whom);
void NetSv_ChangePlayerInfo(int from, byte *data);
void NetSv_Ticker(void);
void NetSv_SaveGame(unsigned int game_id);
void NetSv_LoadGame(unsigned int game_id);
void NetSv_LoadReply(int plnum, int console);
void NetSv_FragsForAll(player_t *player);
void NetSv_KillMessage(player_t *killer, player_t *fragged);
void NetSv_UpdateGameConfig(void);
void NetSv_Paused(boolean isPaused);

int CCmdMapCycle(int argc, char **argv);

#endif