#ifndef __DOOMSDAY_CLIENT_H__
#define __DOOMSDAY_CLIENT_H__

#define	SHORTP(x)		(*(short*) (x))
#define	USHORTP(x)		(*(unsigned short*) (x))

extern id_t clientID;
extern int server_time;
extern int predict_tics;

typedef struct clmobj_s
{
	struct clmobj_s *next, *prev;
	mobj_t mo;
} clmobj_t;

typedef struct playerstate_s
{
	clmobj_t	*cmo;
	thid_t		mobjid;
	int			forwardmove;
	int			sidemove;
	int			angle;
	angle_t		turndelta;
	int			friction;
} playerstate_t;

// 
// cl_main.c
//
extern boolean handshake_received;
extern int game_ready;
extern boolean net_loggedin;
extern clmobj_t cmRoot;

void Cl_InitID(void);
void Cl_CleanUp();
void Cl_GetPackets(void);
void Cl_Ticker(void);
int Cl_GameReady();

// 
// cl_frame.c
//
void Cl_FrameReceived(void);

// 
// cl_mobj.c
//
void Cl_InitClientMobjs();
void Cl_CleanClientMobjs();
void Cl_DestroyClientMobjs();
void Cl_SwapBuffers();
void Cl_HandleFrame();
void Cl_PredictMovement(boolean forward, boolean age_buffer);
void Cl_UnpackMobj(mobj_t *mobj);
void Cl_UnpackMobj2(int unpack_flags, mobj_t *mobj);
void Cl_UnsetThingPosition(mobj_t *thing);
void Cl_SetThingPosition(mobj_t *thing);
int Cl_ReadMobjDelta(void);
clmobj_t *Cl_FindMobj(thid_t id);
void Cl_CheckMobj(mobj_t *mo);
void Cl_UpdateRealPlayerMobj(mobj_t *mo, mobj_t *clmo, int flags);

//
// cl_player.c
//
extern int psp_move_speed;
extern int cplr_thrust_mul;
extern playerstate_t playerstate[MAXPLAYERS];

void Cl_InitPlayers(void);
void Cl_LocalCommand(void);
void Cl_MovePlayer(ddplayer_t *pl);
void Cl_MoveLocalPlayer(int dx, int dy, int dz, boolean onground);
int Cl_ReadPlayerDelta(void);
void Cl_UpdatePlayerPos(ddplayer_t *pl);
void Cl_MovePsprites(void);
void Cl_CoordsReceived(void);

//
// cl_world.c
//
void Cl_InitTranslations(void);
int Cl_ReadSectorDelta(void);
int Cl_ReadLumpDelta(void);
int Cl_ReadSideDelta(void);
int Cl_ReadPolyDelta(void);
void Cl_InitMovers();
void Cl_RemoveMovers();
void Cl_HandleSectorUpdate();
void Cl_HandleWallUpdate();
void Cl_HandlePlaneSound();

//
// cl_sound.c
//
void Cl_Sound(void);

#endif