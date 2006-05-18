#ifndef __NETSV_H__
#define __NETSV_H__

#ifdef __JDOOM__
#include "../jDoom/p_local.h"
#endif

#ifdef __JHERETIC__
#include "../jHeretic/P_local.h"
#endif

#ifdef __JHEXEN__
#include "../jHexen/p_local.h"
#endif

#ifdef __JSTRIFE__
#include "../jStrife/p_local.h"
#endif

extern boolean  cyclingMaps, mapCycleNoExit;
extern int      netSvAllowCheats;
extern char    *mapCycle;
extern char     gameConfigString[];

void            P_Telefrag(mobj_t *thing);

void            NetSv_NewPlayerEnters(int plrnumber);
void           *NetSv_ReadCommands(byte *msg, uint size);
void            NetSv_SendGameState(int flags, int to);
void            NetSv_SendMessage(int plrNum, char *msg);
void            NetSv_SendYellowMessage(int plrNum, char *msg);
void            NetSv_SendPlayerState(int srcPlrNum, int destPlrNum, int flags,
                                      boolean reliable);
void            NetSv_SendPlayerState2(int srcPlrNum, int destPlrNum,
                                       int flags, boolean reliable);
void            NetSv_PSpriteChange(int plrNum, int state);
void            NetSv_Sound(mobj_t *origin, int sound_id, int toPlr);   // toPlr=0: broadcast.
void            NetSv_SoundAtVolume(mobj_t *origin, int sound_id, int volume,
                                    int toPlr);
void            NetSv_Intermission(int flags, int state, int time);
void            NetSv_Finale(int flags, char *script, boolean *conds,
                             int numConds);
void            NetSv_SendPlayerInfo(int whose, int to_whom);
void            NetSv_ChangePlayerInfo(int from, byte *data);
void            NetSv_Ticker(void);
void            NetSv_SaveGame(unsigned int game_id);
void            NetSv_LoadGame(unsigned int game_id);
void            NetSv_LoadReply(int plnum, int console);
void            NetSv_FragsForAll(player_t *player);
void            NetSv_KillMessage(player_t *killer, player_t *fragged,
                                  boolean stomping);
void            NetSv_UpdateGameConfig(void);
void            NetSv_Paused(boolean isPaused);
void            NetSv_DoCheat(int player, const char *data);
void            NetSv_SendJumpPower(int target, float power);

DEFCC(CCmdMapCycle);

#endif
