#ifndef __COMMON_GAME_H__
#define __COMMON_GAME_H__

enum
{ 
	JOYAXIS_NONE,
	JOYAXIS_MOVE,
	JOYAXIS_TURN,
	JOYAXIS_STRAFE,
	JOYAXIS_LOOK
};

void G_StartTitle(void);

// Spawn player at a dummy place.
void G_DummySpawnPlayer(int playernum);

boolean P_IsCamera(mobj_t *mo);
void P_CameraThink(player_t *player);
int P_CameraXYMovement(mobj_t *mo);
int P_CameraZMovement(mobj_t *mo);
void P_Thrust3D(player_t *player, angle_t angle, float lookdir, 
				int forwardmove, int sidemove);

int CCmdMakeLocal(int argc, char **argv);
int CCmdSetCamera(int argc, char **argv);
int CCmdSetViewLock(int argc, char **argv);

#endif
