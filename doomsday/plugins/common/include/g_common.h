#ifndef __COMMON_GAME_H__
#define __COMMON_GAME_H__

#include "dd_share.h"

#if __JDOOM__
# include "jDoom/p_mobj.h"
#elif __JHERETIC__
# include "jHeretic/p_mobj.h"
#elif __JHEXEN__
# include "jHexen/h2def.h"
#elif __JSTRIFE__
# include "jStrife/h2def.h"
#endif

#define OBSOLETE        CVF_HIDE|CVF_NO_ARCHIVE

enum {
    JOYAXIS_NONE,
    JOYAXIS_MOVE,
    JOYAXIS_TURN,
    JOYAXIS_STRAFE,
    JOYAXIS_LOOK
};

void            G_Register(void);
void            G_PreInit(void);
void            G_PostInit(void);
void            G_BuildTiccmd(ticcmd_t *cmd, float elapsedTime);
void            G_MergeTiccmd(ticcmd_t *dest, ticcmd_t *src);
void            G_StartTitle(void);
void            G_PostInit(void);
void            G_PreInit(void);

// Spawn player at a dummy place.
void            G_DummySpawnPlayer(int playernum);
void            G_DeathMatchSpawnPlayer(int playernum);

boolean         P_IsCamera(mobj_t *mo);
void            P_CameraThink(struct player_s *player);
int             P_CameraXYMovement(mobj_t *mo);
int             P_CameraZMovement(mobj_t *mo);
void            P_Thrust3D(struct player_s *player, angle_t angle,
                           float lookdir, int forwardmove, int sidemove);

DEFCC( CCmdMakeLocal );
DEFCC( CCmdSetCamera );
DEFCC( CCmdSetViewLock );
DEFCC( CCmdLocalMessage );
DEFCC( CCmdExitLevel );
#endif
