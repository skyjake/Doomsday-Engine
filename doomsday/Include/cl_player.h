//===========================================================================
// CL_PLAYER.H
//===========================================================================
#ifndef __DOOMSDAY_CLIENT_PLAYER_H__
#define __DOOMSDAY_CLIENT_PLAYER_H__

extern int psp_move_speed;
extern int cplr_thrust_mul;
extern playerstate_t playerstate[MAXPLAYERS];

void Cl_InitPlayers(void);
void Cl_LocalCommand(void);
void Cl_MovePlayer(ddplayer_t *pl);
void Cl_MoveLocalPlayer(int dx, int dy, int dz, boolean onground);
void Cl_UpdatePlayerPos(ddplayer_t *pl);
void Cl_MovePsprites(void);
void Cl_CoordsReceived(void);
int Cl_ReadPlayerDelta(void);
void Cl_ReadPlayerDelta2(boolean skip);

#endif 