
//**************************************************************************
//**
//** G_GAME.C
//**
//** Top-level game routines.
//** Compiles for jDoom, jHeretic and jHexen.
//** Could use some tidying up.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#if __JDOOM__
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "doomdef.h" 
#include "doomstat.h"
#include "D_Action.h"
#include "d_config.h"
#include "m_argv.h"
#include "m_misc.h"
#include "m_menu.h"
#include "m_random.h"
#include "p_local.h"
#include "p_setup.h"
#include "p_saveg.h"
#include "p_tick.h"
#include "d_main.h"
#include "d_netJD.h"
#include "wi_stuff.h"
#include "hu_stuff.h"
#include "st_stuff.h"
#include "am_map.h"
#include "p_local.h" 
#include "s_sound.h"
#include "dstrings.h"
#include "g_game.h"
#endif

#if __JHERETIC__
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "Doomdef.h"
#include "P_local.h"
#include "H_Action.h"
#include "Soundst.h"
#include "settings.h"
#include "p_saveg.h"
#endif

#if __JHEXEN__
#include <string.h>
#include <math.h>
#include <assert.h>
#include "h2def.h"
#include "p_local.h"
#include "soundst.h"
#include "settings.h"
//#include "g_demo.h"
#include "h2_actn.h"
#include "d_net.h"
#endif

#include "f_infine.h"

// MACROS ------------------------------------------------------------------

#define LOCKF_FULL		0x10000
#define LOCKF_MASK		0xff

#if __JDOOM__
#define MAXPLMOVE		(forwardmove[1]) 
#define TURBOTHRESHOLD	0x32
#endif

#if __JHERETIC__
#define MAXPLMOVE       0x32
#endif

#define SLOWTURNTICS		6 
#define NUMGKEYS			256 
#define	BODYQUESIZE			32
#define JOY(x)				(x /*-cfg.joydead*/) / (100 /*-cfg.joydead*/)
#define NUM_MOUSE_BUTTONS	6

// TYPES -------------------------------------------------------------------

// Joystick axes.
enum 
{ 
	JA_X, JA_Y, JA_Z, JA_RX, JA_RY, JA_RZ, JA_SLIDER0, JA_SLIDER1, 
	NUM_JOYSTICK_AXES 
};

#if __JHERETIC__

struct
{	
	int	action;
	int	artifact;
}
ArtifactHotkeys[] =
{
	{ A_INVULNERABILITY,	arti_invulnerability },
	{ A_INVISIBILITY,		arti_invisibility },
	{ A_HEALTH,				arti_health },
	{ A_SUPERHEALTH,		arti_superhealth },
	{ A_TORCH,				arti_torch },
	{ A_FIREBOMB,			arti_firebomb },
	{ A_EGG,				arti_egg },
	{ A_FLY,				arti_fly },
	{ A_TELEPORT,			arti_teleport },
	{ A_PANIC,				NUMARTIFACTS },
	{ 0,					arti_none }// Terminator.
};

struct
{
	mobjtype_t type;
	int speed[2];
} MonsterMissileInfo[] =
{
	{ MT_IMPBALL, 10, 20 },
	{ MT_MUMMYFX1, 9, 18 },
	{ MT_KNIGHTAXE, 9, 18 },
	{ MT_REDAXE, 9, 18 },
	{ MT_BEASTBALL, 12, 20 },
	{ MT_WIZFX1, 18, 24 },
	{ MT_SNAKEPRO_A, 14, 20 },
	{ MT_SNAKEPRO_B, 14, 20 },
	{ MT_HEADFX1, 13, 20 },
	{ MT_HEADFX3, 10, 18 },
	{ MT_MNTRFX1, 20, 26 },
	{ MT_MNTRFX2, 14, 20 },
	{ MT_SRCRFX1, 20, 28 },
	{ MT_SOR2FX1, 20, 28 },
	{ -1, -1, -1 } // Terminator
};

#endif

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void	P_InitPlayerValues(player_t *p);
void	P_RunPlayers(void);
boolean	P_IsPaused(void);
void	P_DoTick(void);

#if __JHEXEN__
void	P_InitSky(int map);
void	P_PlayerNextArtifact(player_t *player);
#endif

void	HU_UpdatePsprites(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void	G_PlayerReborn (int player); 
void	G_InitNew (skill_t skill, int episode, int map); 
void	G_DoInitNew(void);
void	G_DoReborn (int playernum); 
void	G_DoLoadLevel(void); 
void	G_DoNewGame (void); 
void	G_DoLoadGame (void); 
void	G_DoPlayDemo (void); 
void	G_DoCompleted (void); 
void	G_DoVictory (void); 
void	G_DoWorldDone (void); 
void	G_DoSaveGame (void); 
void	G_DoScreenShot (void);

#if __JHEXEN__
void	G_DoTeleportNewMap(void);
void	G_DoSingleReborn(void);
void	H2_PageTicker(void);
void	H2_AdvanceDemo(void);
#endif

void	G_StopDemo(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

#if __JDOOM__
extern char*	pagename; 
#endif

#if __JHERETIC__
extern boolean	inventory;
extern int		curpos;
extern int		inv_ptr;
extern boolean	noartiskip;
extern int		curpos;
extern int		inv_ptr;
extern int		playerkeys;
#endif

#if __JHEXEN__
extern boolean	mn_SuicideConsole;
extern boolean	inventory;
extern int		curpos;
extern int		inv_ptr;
extern boolean	artiskip;
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

gameaction_t    gameaction; 
gamestate_t     gamestate = GS_DEMOSCREEN; 
skill_t         gameskill; 
int             gameepisode; 
int             gamemap; 

#ifndef __JHEXEN__
boolean			respawnmonsters;
#endif

#ifndef __JDOOM__
int				prevmap;
#endif
 
boolean         paused; 
boolean         sendpause;             	// send a pause event next tic 
boolean         usergame;               // ok to save / end game 
 
boolean         timingdemo;             // if true, exit with report on completion 
boolean         nodrawers;				// for comparative timing purposes 
boolean         noblit;                 // for comparative timing purposes 
int             starttime;          	// for comparative timing purposes  	 
 
boolean         viewactive; 
 
boolean         deathmatch;           	// only if started as net death 
player_t        players[MAXPLAYERS]; 
 
int             levelstarttic;          // gametic at level start 
int             totalkills, totalitems, totalsecret;    // for intermission 
 
char            defdemoname[32]; 
boolean         singledemo;            	// quit after playing a demo from cmdline 
 
boolean         precache = true;        // if true, load all graphics at start 

#if __JDOOM__ 
wbstartstruct_t	wminfo;					// parms for world map / intermission 
jdoom_config_t cfg;

#elif __JHERETIC__
jheretic_config_t cfg;

#elif __JHEXEN__
jhexen_config_t cfg;
//#error "sbarscale and others need to be initialized."
#endif

// Looking around.
int				povangle = -1;			// -1 means centered (really 0 - 7).
float			targetLookOffset=0;
float			lookOffset = 0;

fixed_t			angleturn[3] = {640, 1280, 320};	// + slow turn 

#if __JDOOM__ || __JHERETIC__
fixed_t			forwardmove[2] = {0x19, 0x32}; 
fixed_t			sidemove[2] = {0x18, 0x28}; 

#else
fixed_t MaxPlayerMove[NUMCLASSES] = { 0x3C, 0x32, 0x2D, 0x31 };
fixed_t forwardmove[NUMCLASSES][2] = 
{
	{ 0x1D, 0x3C },
	{ 0x19, 0x32 },
	{ 0x16, 0x2E },
	{ 0x18, 0x31 }
};
fixed_t sidemove[NUMCLASSES][2] = 
{
	{ 0x1B, 0x3B },
	{ 0x18, 0x28 },
	{ 0x15, 0x25 },
	{ 0x17, 0x27 }
};
#endif

boolean         gamekeydown[NUMGKEYS]; 
int             turnheld;				// for accelerative turning 

//int			do_chimes;
 
boolean			mousearray[NUM_MOUSE_BUTTONS + 1]; 
boolean*		mousebuttons = &mousearray[1];		// allow [-1]

// mouse values are used once 
int				mousex;
int				mousey;         

int				dclicktime;
int				dclickstate;
int				dclicks; 
int				dclicktime2;
int				dclickstate2;
int				dclicks2;

// joystick values are repeated 
int				joymove[NUM_JOYSTICK_AXES];	// X, Y, Z, Rx, Ry, Rz, S1, S2
boolean			joyarray[33]; 
boolean*		joybuttons = &joyarray[1];		// allow [-1] 
 
int				savegameslot; 
char			savedescription[32]; 

#if __JDOOM__
mobj_t*			bodyque[BODYQUESIZE]; 
int				bodyqueslot; 
#endif

#if __JHERETIC__ || __JHEXEN__
int				lookheld;
int				inventoryTics;
#endif

#if __JHEXEN__
byte			demoDisabled = 0; // is demo playing disabled?
// Position indicator for cooperative net-play reborn
int				RebornPosition;
int				LeaveMap;
int				LeavePosition;
#endif

boolean			secretexit; 
char			savename[256];


// PRIVATE DATA DEFINITIONS ------------------------------------------------

boolean			usearti = true;

#if __JHEXEN__
static skill_t	TempSkill;
static int		TempEpisode;
static int		TempMap;
static int		GameLoadSlot;
#endif

// CODE --------------------------------------------------------------------

/*
 * Begin the titlescreen animation sequence.
 */
void G_StartTitle (void)
{
	char *name = "title";
	void *script;

	G_StopDemo();
	usergame = false;

	// The title script must always be defined.
	if(!Def_Get(DD_DEF_FINALE, name, &script))
	{
		Con_Error("G_StartTitle: Script \"%s\" not defined.\n", name);
	}

	FI_Start(script, FIMODE_LOCAL);
}

#if __JHERETIC__ || __JHEXEN__
static int findWeapon(player_t *plr, boolean forward)
{
	int	i, c;

	for(i = plr->readyweapon + (forward? 1 : -1), c = 0; 
#if __JHERETIC__
		c < NUMWEAPONS - 1; c++, forward? i++ : i--) 
	{
		if(i >= NUMWEAPONS - 1) i = 0;
		if(i < 0) i = NUMWEAPONS - 2;
#elif __JHEXEN__
		c < NUMWEAPONS; c++, forward? i++ : i--)
	{
		if(i > NUMWEAPONS - 1) i = 0;
		if(i < 0) i = NUMWEAPONS - 1;
#endif
		if(plr->weaponowned[i]) return i;
	}
	return plr->readyweapon;
}

static boolean inventoryMove(player_t *plr, int dir)
{
	inventoryTics = 5*35;
	if(!inventory)
	{
		inventory = true;
		return(false);
	}

	if(dir == 0)
	{
		inv_ptr--;
		if(inv_ptr < 0)
		{
			inv_ptr = 0;
		}
		else
		{
			curpos--;
			if(curpos < 0)
			{
				curpos = 0;
			}
		}
	}
	else
	{
		inv_ptr++;
		if(inv_ptr >= plr->inventorySlotNum)
		{
			inv_ptr--;
			if(inv_ptr < 0)
				inv_ptr = 0;
		}
		else
		{
			curpos++;
			if(curpos > 6)
			{
				curpos = 6;
			}
		}
	}
	return(true);
}

int CCmdInventory(int argc, char **argv)
{
	inventoryMove(players+consoleplayer, !stricmp(argv[0], "invright"));
	return true;
}
#endif
 
// Offset is in 'angles', where 110 corresponds 85 degrees.
// The delta has higher precision with small offsets.
char G_MakeLookDelta(float offset)
{
	boolean minus = offset < 0;

	offset = sqrt(fabs(offset)) * DELTAMUL;
	if(minus) offset = -offset;
	// It's only a char...
	if(offset > 127) offset = 127;
	if(offset < -128) offset = -128;
	return (signed char) offset;
}

void G_AdjustAngle(player_t *player, int turn)
{
	if(!player->plr->mo) return;	// Sorry, can't help you, pal.

	// Turn around.
	if(player->playerstate != PST_DEAD)
		player->plr->clAngle += (turn << FRACBITS);
}

void G_AdjustLookDir(player_t *player, int look)
{
	ddplayer_t *ddplr = player->plr;

	if(look)
	{
		if(look == TOCENTER)
		{
			player->centering = true;
		}
		else
		{
			int spd = cfg.lookSpeed;
			ddplr->clLookDir += spd*look;
		}
	}
	if(player->centering)
	{
		if(ddplr->clLookDir > 0)
		{
			ddplr->clLookDir -= 8;
		}
		else if(ddplr->clLookDir < 0)
		{
			ddplr->clLookDir += 8;
		}
		if(abs(ddplr->clLookDir) < 8)
		{
			ddplr->clLookDir = 0;
			player->centering = false;
		}
	}
}

void G_SetCmdViewAngles(ticcmd_t *cmd, player_t *pl)
{
	// These will be sent to the server (or P_MovePlayer).
	cmd->angle = pl->plr->clAngle >> 16;
	// 110 corresponds 85 degrees.
	if(pl->plr->clLookDir > 110) pl->plr->clLookDir = 110;
	if(pl->plr->clLookDir < -110) pl->plr->clLookDir = -110;
	cmd->pitch = pl->plr->clLookDir/110 * DDMAXSHORT;
}

/*
 * Builds a ticcmd from all of the available inputs. 
 */
void G_BuildTiccmd (ticcmd_t* cmd) 
{ 
	static boolean mlook_pressed = false;
#if __JDOOM__
	boolean pausestate = paused || (!IS_NETGAME && menuactive);
#endif
    int		i; 
    boolean	strafe;
    boolean	bstrafe; 
    int		speed;
    int		tspeed; 
    int		forward;
    int		side;
	int		turn = 0;
	player_t *cplr = &players[consoleplayer];
	int		joyturn = 0, joystrafe = 0, joyfwd = 0, joylook = 0;
	int		*axes[5] = { 0, &joyfwd, &joyturn, &joystrafe, &joylook };
#if __JHERETIC__ || __JHEXEN__
	int		flyheight, look, arti, lspeed;
#endif
#if __JHEXEN__
	int		pClass = players[consoleplayer].class;
#endif
   
	memset(cmd, 0, sizeof(*cmd));

	// During demo playback, all cmds will be blank.
	if(Get(DD_PLAYBACK)) return;

	// Check the joystick axes.
	for(i = 0; i < 8; i++) 
		if(axes[cfg.joyaxis[i]]) *axes[cfg.joyaxis[i]] += joymove[i];

    strafe = actions[A_STRAFE].on; 
    speed = actions[A_SPEED].on;

#if __JDOOM__
	forward = side = 0;
#else
	forward = side = look = arti = flyheight = 0;
#endif

	// Walk -> run, run -> walk.
	if(cfg.alwaysRun) speed = !speed; 
    
    // Use two stage accelerative turning on the keyboard and joystick.
    if(joyturn < -0 /*cfg.joydead*/ 
		|| joyturn > 0 /*cfg.joydead*/ 
		|| actions[A_TURNRIGHT].on 
		|| actions[A_TURNLEFT].on)
		turnheld++; 
    else 
		turnheld = 0; 

    if(turnheld < SLOWTURNTICS) 
		tspeed = 2;             // slow turn 
    else 
		tspeed = speed;
    
#if __JHERETIC__ || __JHEXEN__
	if(actions[A_LOOKDOWN].on || actions[A_LOOKUP].on)
	{
		lookheld++;
	}
	else
	{
		lookheld = 0;
	}
	if(lookheld < SLOWTURNTICS)
	{
		lspeed = 1;
	}
	else
	{
		lspeed = 2;
	}
#endif

    // let movement keys cancel each other out
    if(strafe) 
    { 
#if __JDOOM__ || __JHERETIC__
		if(actions[A_TURNRIGHT].on) side += sidemove[speed]; 
		if(actions[A_TURNLEFT].on) side -= sidemove[speed]; 
#else
		if(actions[A_TURNRIGHT].on) side += sidemove[pClass][speed];
		if(actions[A_TURNLEFT].on) side -= sidemove[pClass][speed];
#endif
		// Swap strafing and turning.
		i = joystrafe;
		joystrafe = joyturn;
		joyturn = i;
    } 
    else 
    { 
		if(actions[A_TURNRIGHT].on) turn -= angleturn[tspeed]; 
		if(actions[A_TURNLEFT].on) turn += angleturn[tspeed]; 
	} 

	// Joystick turn.
	if (joyturn > 0 /*cfg.joydead*/)
		turn -= angleturn[tspeed] * JOY(joyturn);
	if (joyturn < -0 /*cfg.joydead*/)
		turn += angleturn[tspeed] * JOY(-joyturn);

	// Joystick strafe.
#ifndef __JHEXEN__
	if(joystrafe < -0 /*cfg.joydead*/)
		side -= sidemove[speed] * JOY(-joystrafe);
	if(joystrafe > 0 /*cfg.joydead*/)
		side += sidemove[speed] * JOY(joystrafe);
#else
	if(joystrafe < -0 /*cfg.joydead*/)
		side -= sidemove[pClass][speed] * JOY(-joystrafe);
	if(joystrafe > 0 /*cfg.joydead*/)
		side += sidemove[pClass][speed] * JOY(joystrafe);
#endif
 
    if(actions[A_FORWARD].on) 
    {
#if __JHEXEN__
		forward += forwardmove[pClass][speed]; 
#else
		forward += forwardmove[speed]; 
#endif
    }
    if(actions[A_BACKWARD].on) 
    {
#if __JHEXEN__
		forward -= forwardmove[pClass][speed];
#else
		forward -= forwardmove[speed]; 
#endif
    }

#ifndef __JHEXEN__
	if(joyfwd < -0 /*cfg.joydead*/)
		forward += forwardmove[speed] * JOY(-joyfwd);
	if(joyfwd > 0 /*cfg.joydead*/)
		forward -= forwardmove[speed] * JOY(joyfwd);
#else
	if(joyfwd < -0 /*cfg.joydead*/)
		forward += forwardmove[pClass][speed] * JOY(-joyfwd);
	if(joyfwd > 0 /*cfg.joydead*/)
		forward -= forwardmove[pClass][speed] * JOY(joyfwd);
#endif

#ifndef __JHEXEN__
    if(actions[A_STRAFERIGHT].on) side += sidemove[speed]; 
    if(actions[A_STRAFELEFT].on) side -= sidemove[speed];
	
	if(joystrafe < -0 /*cfg.joydead*/)
		side -= sidemove[speed] * JOY(-joystrafe);
	if(joystrafe > 0 /*cfg.joydead*/)
		side += sidemove[speed] * JOY(joystrafe);
#else
	if(actions[A_STRAFERIGHT].on) side += sidemove[pClass][speed];
	if(actions[A_STRAFELEFT].on) side -= sidemove[pClass][speed];

	if(joystrafe < -0 /*cfg.joydead*/)
		side -= sidemove[pClass][speed] * JOY(-joystrafe);
	if(joystrafe > 0 /*cfg.joydead*/)
		side += sidemove[pClass][speed] * JOY(joystrafe);
#endif

#if __JHERETIC__
	// Look up/down/center keys
	if(!cfg.lookSpring || (cfg.lookSpring && !forward))
	{
		if(actions[A_LOOKUP].on)
		{
			look = lspeed;
		}
		if(actions[A_LOOKDOWN].on)
		{
			look = -lspeed;
		}
		if(actions[A_LOOKCENTER].on)
		{	
			look = TOCENTER;
		}
	}
	// Fly up/down/drop keys
	if(actions[A_FLYUP].on)
	{
		flyheight = 5; // note that the actual flyheight will be twice this
	}
	if(actions[A_FLYDOWN].on)
	{
		flyheight = -5;
	}
	if(actions[A_FLYCENTER].on)
	{
		flyheight = TOCENTER;
		if(!cfg.usemlook) look = TOCENTER;
	}
	// Use artifact key
	if(actions[A_USEARTIFACT].on)
	{
		if(actions[A_SPEED].on && !noartiskip)
		{
			if(players[consoleplayer].inventory[inv_ptr].type != arti_none)
			{
				actions[A_USEARTIFACT].on = false;
				cmd->arti = 0xff; // skip artifact code
			}
		}
		else
		{
			if(inventory)
			{
				players[consoleplayer].readyArtifact =
					players[consoleplayer].inventory[inv_ptr].type;
				inventory = false;
				cmd->arti = cfg.chooseAndUse? players[consoleplayer].inventory[inv_ptr].type : 0;
				usearti = false;
			}
			else if(usearti)
			{
				cmd->arti = players[consoleplayer].inventory[inv_ptr].type;
				usearti = false;
			}
		}
	}
	// Check Tome of Power and other artifact hotkeys.
	if(actions[A_TOMEOFPOWER].on && !cmd->arti
		&& !players[consoleplayer].powers[pw_weaponlevel2])
	{
		actions[A_TOMEOFPOWER].on = false;
		cmd->arti = arti_tomeofpower;
	}
	for(i=0; ArtifactHotkeys[i].artifact != arti_none && !cmd->arti; i++)
	{
		if(actions[ArtifactHotkeys[i].action].on)
		{
			actions[ArtifactHotkeys[i].action].on = false;
			cmd->arti = ArtifactHotkeys[i].artifact;
			break;
		}
	}	
#endif

#if __JHEXEN__
	// Look up/down/center keys
	if(!cfg.lookSpring || (cfg.lookSpring && !forward))
	{
		if(actions[A_LOOKUP].on)
		{
			look = lspeed;
		}
		if(actions[A_LOOKDOWN].on)
		{
			look = -lspeed;
		}
		if(actions[A_LOOKCENTER].on)
		{	
			look = TOCENTER;
		}
	}
	// Fly up/down/drop keys
	if(actions[A_FLYUP].on)
	{
		flyheight = 5; // note that the actual flyheight will be twice this
	}
	if(actions[A_FLYDOWN].on)
	{
		flyheight = -5;
	}
	if(actions[A_FLYCENTER].on)
	{
		flyheight = TOCENTER;
		look = TOCENTER;
	}
	// Use artifact key
	if(actions[A_USEARTIFACT].on)
	{
		if(speed && artiskip)
		{
			if(players[consoleplayer].inventory[inv_ptr].type != arti_none)
			{ // Skip an artifact
				actions[A_USEARTIFACT].on = false;
				P_PlayerNextArtifact(&players[consoleplayer]);			
			}
		}
		else
		{
			if(inventory)
			{
				players[consoleplayer].readyArtifact =
					players[consoleplayer].inventory[inv_ptr].type;
				inventory = false;
				if(cfg.chooseAndUse)
					cmd->arti |= players[consoleplayer].inventory[inv_ptr].type&AFLAG_MASK;
				else
					cmd->arti = 0;
			}
			else if(usearti)
			{
				cmd->arti |= 
					players[consoleplayer].inventory[inv_ptr].type&AFLAG_MASK;
			}
		}
		actions[A_USEARTIFACT].on = false;
	}
	if(actions[A_JUMP].on)
	{
		cmd->arti |= AFLAG_JUMP;
	}
	if(mn_SuicideConsole)
	{
		cmd->arti |= AFLAG_SUICIDE;
		mn_SuicideConsole = false;
	}

	// Artifact hot keys
	if(actions[A_PANIC].on && !cmd->arti)
	{
		actions[A_PANIC].on = false; 	// Use one of each artifact
		cmd->arti = NUMARTIFACTS;
	}
	else if(players[consoleplayer].plr->mo && actions[A_HEALTH].on && !cmd->arti 
	&& (players[consoleplayer].plr->mo->health < MAXHEALTH))
	{
		actions[A_HEALTH].on = false;
		cmd->arti = arti_health;						
	}
	else if(actions[A_POISONBAG].on && !cmd->arti)
	{
		actions[A_POISONBAG].on = false;
		cmd->arti = arti_poisonbag;						
	}
	else if(actions[A_BLASTRADIUS].on && !cmd->arti)
	{
		actions[A_BLASTRADIUS].on = false;
		cmd->arti = arti_blastradius;					
	}
	else if(actions[A_TELEPORT].on && !cmd->arti)
	{
		actions[A_TELEPORT].on = false;
		cmd->arti = arti_teleport;						
	}
	else if(actions[A_TELEPORTOTHER].on && !cmd->arti)
	{
		actions[A_TELEPORTOTHER].on = false;
		cmd->arti = arti_teleportother;						
	}
	else if(actions[A_EGG].on && !cmd->arti)
	{
		actions[A_EGG].on = false;
		cmd->arti = arti_egg;						
	}
	else if(actions[A_INVULNERABILITY].on && !cmd->arti
		&& !players[consoleplayer].powers[pw_invulnerability])
	{
		actions[A_INVULNERABILITY].on = false;
		cmd->arti = arti_invulnerability;				
	}
	else if(actions[A_MYSTICURN].on && !cmd->arti)
	{
		actions[A_MYSTICURN].on = false;
		cmd->arti = arti_superhealth;
	}
	else if(actions[A_TORCH].on && !cmd->arti)
	{
		actions[A_TORCH].on = false;
		cmd->arti = arti_torch;
	}
	else if(actions[A_KRATER].on && !cmd->arti)
	{
		actions[A_KRATER].on = false;
		cmd->arti = arti_boostmana;
	}
	else if(actions[A_SPEEDBOOTS].on & !cmd->arti)
	{
		actions[A_SPEEDBOOTS].on = false;
		cmd->arti = arti_speed;
	}
	else if(actions[A_DARKSERVANT].on && !cmd->arti)
	{
		actions[A_DARKSERVANT].on = false;
		cmd->arti = arti_summon;
	}
#endif

    // Buttons
 
    if(actions[A_FIRE].on) cmd->actions |= BT_ATTACK; 

    if(actions[A_USE].on) 
	{ 
		cmd->actions |= BT_USE;
		// clear double clicks if hit use button 
		dclicks = 0;                   
    } 

#if __JDOOM__
	if(actions[A_JUMP].on) cmd->actions |= BT_JUMP;
#elif __JHERETIC__
	if(actions[A_JUMP].on) cmd->arti |= AFLAG_JUMP;
#endif

#if __JDOOM__

#define GOTWPN(x)	(cplr->weaponowned[x])
#define ISWPN(x)	(cplr->readyweapon == x)

	// Determine whether a weapon change should be done.
	if(actions[A_WEAPONCYCLE1].on) // Fist/chainsaw.
	{
		if(ISWPN(wp_fist) && GOTWPN(wp_chainsaw))
			i = wp_chainsaw;
		else if(ISWPN(wp_chainsaw))
			i = wp_fist;
		else if(GOTWPN(wp_chainsaw))
			i = wp_chainsaw;
		else
			i = wp_fist;
	
		cmd->actions &= ~BT_JUMP;
		cmd->actions |= BT_CHANGE; 
		cmd->actions |= i << BT_WEAPONSHIFT; 
	}
	else if(actions[A_WEAPONCYCLE2].on) // Shotgun/super sg.
	{
		if(ISWPN(wp_shotgun) && GOTWPN(wp_supershotgun)
			&& gamemode == commercial)
			i = wp_supershotgun;
		else if(ISWPN(wp_supershotgun))
			i = wp_shotgun;
		else if(GOTWPN(wp_supershotgun) && gamemode == commercial)
			i = wp_supershotgun;
		else
			i = wp_shotgun;

		cmd->actions &= ~BT_JUMP;
		cmd->actions |= BT_CHANGE; 
		cmd->actions |= i << BT_WEAPONSHIFT; 
	}
	else
	{
		// Take the first weapon action.
	    for(i = 0; i < NUMWEAPONS; i++)        
			if(actions[A_WEAPON1+i].on) 
			{ 
				cmd->actions &= ~BT_JUMP;
				cmd->actions |= BT_CHANGE; 
				cmd->actions |= i << BT_WEAPONSHIFT; 
				break;
			}
	}
	if(actions[A_NEXTWEAPON].on || actions[A_PREVIOUSWEAPON].on)
	{
		cmd->actions = BT_SPECIAL 
			| (actions[A_NEXTWEAPON].on? BTS_NEXTWEAPON : BTS_PREVWEAPON);
	}
#else
	if(actions[A_PREVIOUSWEAPON].on)
	{
		cmd->actions |= BT_CHANGE | (findWeapon(players+consoleplayer, 
			false) << BT_WEAPONSHIFT);
	}
	else if(actions[A_NEXTWEAPON].on)
	{
		cmd->actions |= BT_CHANGE | (findWeapon(players+consoleplayer, 
			true) << BT_WEAPONSHIFT);
	}
#if __JHERETIC__
	else for(i = 0; i < NUMWEAPONS-2; i++)
#else
	else for(i = 0; i < NUMWEAPONS; i++)
#endif
	{
		if(actions[A_WEAPON1 + i].on)
		{
#ifdef __JHERETIC__
			// Staff and Gautlets are on the same key.
			if(i == wp_staff 
				&& players[consoleplayer].readyweapon != wp_gauntlets
				&& players[consoleplayer].weaponowned[wp_gauntlets])
			{
				i = wp_gauntlets;
			}
#endif
			cmd->actions |= BT_CHANGE;
			cmd->actions |= i << BT_WEAPONSHIFT;
			break;
		}
	}
#endif

	// forward double click
    if(actions[A_FORWARD].on != dclickstate 
		&& dclicktime > 1 
		&& cfg.dclickuse)  
    { 
		dclickstate = actions[A_FORWARD].on; 

		if (dclickstate) 
			dclicks++; 
		if (dclicks == 2) 
		{ 
			cmd->actions |= BT_USE; 
			dclicks = 0; 
		} 
		else 
			dclicktime = 0; 
	} 
	else 
	{ 
		dclicktime++; 
		if (dclicktime > 20) 
		{ 
			dclicks = 0; 
			dclickstate = 0; 
		} 
	}
    
    // strafe double click
    bstrafe = strafe; 
    if(bstrafe != dclickstate2 && dclicktime2 > 1 && cfg.dclickuse) 
    { 
		dclickstate2 = bstrafe; 
		if (dclickstate2) 
			dclicks2++; 
		if (dclicks2 == 2) 
		{ 
			cmd->actions |= BT_USE; 
			dclicks2 = 0; 
		} 
		else 
			dclicktime2 = 0; 
    } 
    else 
    { 
		dclicktime2++; 
		if (dclicktime2 > 20) 
		{ 
			dclicks2 = 0; 
			dclickstate2 = 0; 
		} 
    } 

	// Mouse strafe and turn (X axis).
    if(strafe) 
		side += mousex*2; 
    else 
		turn -= mousex*0x8; 

#if __JDOOM__
	if(!pausestate)
	{
#else
	if(!paused)
	{
		G_AdjustAngle(cplr, turn);
#endif
		if(strafe || (!cfg.usemlook && !actions[A_MLOOK].on)
			|| players[consoleplayer].playerstate == PST_DEAD) 
		{
			forward += mousey;
		}
		else
		{
			float adj = (((mousey*0x8)<<16)/(float)ANGLE_180 * 180 * 110.0/85.0);
			if(cfg.mlookInverseY) adj = -adj;
			cplr->plr->clLookDir += adj;
		}
		if(cfg.usejlook)
		{
			if(cfg.jlookDeltaMode)
				cplr->plr->clLookDir += joylook/20.0f*cfg.lookSpeed
					* (cfg.jlookInverseY? -1 : 1);
			else
				cplr->plr->clLookDir = joylook * 1.1f * 
					(cfg.jlookInverseY? -1 : 1);
		}
	}

    mousex = mousey = 0; 
	
#if __JHEXEN__
#define MAXPLMOVE MaxPlayerMove[pClass]
#endif

    if(forward > MAXPLMOVE) 
		forward = MAXPLMOVE; 
    else if (forward < -MAXPLMOVE) 
		forward = -MAXPLMOVE; 
    if(side > MAXPLMOVE) 
		side = MAXPLMOVE; 
    else if (side < -MAXPLMOVE) 
		side = -MAXPLMOVE; 

#if __JHEXEN__
	if(cplr->powers[pw_speed] && !cplr->morphTics)
	{ 
		// Adjust for a player with a speed artifact
		forward = (3*forward)>>1;
		side = (3*side)>>1;
	}
#endif
 
    cmd->forwardMove += forward; 
    cmd->sideMove += side;

	if(cfg.lookSpring && !actions[A_MLOOK].on 
		&& (cmd->forwardMove > MAXPLMOVE/3 
		|| cmd->forwardMove < -MAXPLMOVE/3
		|| cmd->sideMove > MAXPLMOVE/3
		|| cmd->sideMove < -MAXPLMOVE/3
		|| mlook_pressed))

/*		if(abs(forward) >= forwardmove[0]
			|| abs(side) >= sidemove[0]
			|| (!cfg.usemlook && mlook_pressed))
			look = TOCENTER;
*/
	{
		// Center view when mlook released w/lookspring, or when moving.
		cplr->centering = true;
#ifndef __JDOOM__
		look = TOCENTER;
#endif
	}

#if __JDOOM__
	if(cplr->playerstate == PST_LIVE && !pausestate)
	{
		i = 0;
		// Key look.
		if(actions[A_LOOKCENTER].on)
		{
			i = TOCENTER;
		}
		else 
		{
			if(actions[A_LOOKUP].on)
			{
				i += cfg.lookSpeed;
			}
			if(actions[A_LOOKDOWN].on)
			{
				i -= cfg.lookSpeed;
			}	
		}
		// Adjust the look direction.
		G_AdjustLookDir(cplr, i);
	}
#else 
	if(players[consoleplayer].playerstate == PST_LIVE)
	{
		G_AdjustLookDir(cplr, look);
		if(look < 0)
		{
			look += 16;
		}
		cmd->lookfly = look;
	}
#endif

#if __JDOOM__
	if(!pausestate) G_AdjustAngle(cplr, turn);
#else
	if(flyheight < 0) flyheight += 16;
	cmd->lookfly |= flyheight<<4;
#endif

	// Store the current mlook key state.
	mlook_pressed = actions[A_MLOOK].on;

	G_SetCmdViewAngles(cmd, cplr);

	// special buttons
    if(sendpause) 
    { 
		sendpause = false; 
		// Clients can't pause anything.
		if(!IS_CLIENT)
			cmd->actions = BT_SPECIAL | BTS_PAUSE; 
    } 

	if(IS_CLIENT)
	{
		// Clients mirror their local commands.
		memcpy(&players[consoleplayer].cmd, cmd, sizeof(*cmd));
	}
} 

//
// G_DoLoadLevel 
//
 
void G_DoLoadLevel (void) 
{ 
	action_t	*act;
    int         i; 
#if __JHEXEN__
    static int firstFragReset = 1;
#endif

    levelstarttic = gametic;        // for time calculation
    gamestate = GS_LEVEL; 

	// If we're the server, let clients know the map will change.
	NetSv_SendGameState(GSF_CHANGE_MAP, DDSP_ALL_PLAYERS);
	
    for (i=0 ; i<MAXPLAYERS ; i++) 
    { 
		if (players[i].plr->ingame && players[i].playerstate == PST_DEAD) 
			players[i].playerstate = PST_REBORN; 
#if __JHEXEN__
        if(!netgame || (netgame != 0 && deathmatch != 0) 
           || firstFragReset == 1) 
		{
    		memset (players[i].frags, 0, sizeof(players[i].frags));
            firstFragReset = 0;
        }
#else
		memset(players[i].frags, 0, sizeof(players[i].frags)); 
#endif
    } 
	
#if __JHEXEN__
	SN_StopAllSequences();	
#endif

	// Set all player mobjs to NULL.
	for(i = 0; i < MAXPLAYERS; i++)	players[i].plr->mo = NULL;

    P_SetupLevel (gameepisode, gamemap, 0, gameskill);    
    Set(DD_DISPLAYPLAYER, consoleplayer);	// view the guy you are playing    
    starttime = Sys_GetTime(); 
    gameaction = ga_nothing; 
    Z_CheckHeap ();
    
    // clear cmd building stuff
    memset(gamekeydown, 0, sizeof(gamekeydown)); 
	memset(joymove, 0, sizeof(joymove));
    mousex = mousey = 0; 
    sendpause = paused = false; 
    memset(mousebuttons, 0, sizeof(mousebuttons)); 
    memset(joybuttons, 0, sizeof(joybuttons)); 

	// Deactivate all action keys.
	for(act=actions; act->name[0]; act++) act->on = false;

	// Start a briefing, if there is one.
	FI_Briefing(gameepisode, gamemap);
} 
 

int CCmdCycleSpy(int argc, char **argv)
{
	// FIXME: The engine should do this.
	Con_Printf("Spying not allowed.\n");
#if 0
	if(gamestate == GS_LEVEL && !deathmatch)
	{ // Cycle the display player
		do
		{
			Set(DD_DISPLAYPLAYER, displayplayer+1);
			if(displayplayer == MAXPLAYERS)
			{
				Set(DD_DISPLAYPLAYER, 0);
			}
		} while(!players[displayplayer].plr->ingame
			&& displayplayer != consoleplayer);
	}
#endif
	return true;
}

//
// G_Responder  
// Get info needed to make ticcmd_ts for the players.
// Return false if the event should be checked for bindings.
// 
boolean G_Responder (event_t* ev) 
{ 
	int		i;
#if __JHERETIC__ || __JHEXEN__
	player_t *plr = &players[consoleplayer];
	//extern boolean MenuActive;

	if(!actions[A_USEARTIFACT].on)
	{ // flag to denote that it's okay to use an artifact
		if(!inventory)
		{
			plr->readyArtifact = plr->inventory[inv_ptr].type;
		}
		usearti = true;
	}

#else
    // any other key pops up menu if in demos
    if(gameaction == ga_nothing && !singledemo && 
		(Get(DD_PLAYBACK) || FI_IsMenuTrigger(ev))) 
    { 
		if(ev->type == ev_keydown 
			|| ev->type == ev_mousebdown 
			|| ev->type == ev_joybdown) 
		{ 
			M_StartControlPanel (); 
			return false; 
		} 
		return false; 
    } 
#endif

	if(FI_Responder(ev)) return true; 

#if __JHEXEN__
	if(CT_Responder(ev))
	{ // Chat ate the event
		return(true);
	}
#endif

	switch(gamestate)
	{
	case GS_LEVEL:
#if __JDOOM__
		if(HU_Responder(ev)) 
			return true;	// chat ate the event 
		if(ST_Responder(ev)) 
			return true;	// status window ate it 

#elif __JHERETIC__
		if(CT_Responder(ev)) // Chat ate the event
			return true;
		if(SB_Responder(ev)) // Status bar ate the event
			return true;

#elif __JHEXEN__
		if(SB_Responder(ev)) // Status bar ate the event
			return(false);
#endif
		if(AM_Responder(ev)) 
			return true;	// automap ate it 
		break;

	default:
		break;
	}

    switch (ev->type) 
    { 
    case ev_keydown:
		if(ev->data1 < NUMGKEYS) gamekeydown[ev->data1] = true; 
		return false;  
 
	case ev_keyup: 
		if(ev->data1 < NUMGKEYS) gamekeydown[ev->data1] = false; 
		return false;   // always let key up events filter down 

	case ev_keyrepeat:
		return false; 
		 
	case ev_mouse:
		mousex += (ev->data1 * (cfg.mouseSensiX*2+5)) / 6;
		mousey += (ev->data2 * (cfg.mouseSensiY*2+5)) / 6;
		return(true); // eat events

	case ev_mousebdown:
		for(i = 0; i < NUM_MOUSE_BUTTONS; i++)
			if(ev->data1 & (1<<i))
				mousebuttons[i] = true;
		return(false);

	case ev_mousebup:
		for(i = 0; i < NUM_MOUSE_BUTTONS; i++)
			if(ev->data1 & (1<<i))
				mousebuttons[i] = false;
		return(false);

	case ev_joystick: // Joystick movement
		joymove[JA_X] = ev->data1;
		joymove[JA_Y] = ev->data2;
		joymove[JA_Z] = ev->data3;
		joymove[JA_RX] = ev->data4;
		joymove[JA_RY] = ev->data5;
		joymove[JA_RZ] = ev->data6;
		return true; // eat events

	case ev_joyslider: // Joystick slider movement
		joymove[JA_SLIDER0] = ev->data1;
		joymove[JA_SLIDER1] = ev->data2;
		return true;

	case ev_joybdown: 
		for(i = 0; i < 32; i++) 
			if(ev->data1 & (1<<i)) 
				joybuttons[i] = true;
		return false; // eat events

	case ev_joybup: 
		for(i = 0; i < 32; i++) 
			if(ev->data1 & (1<<i)) 
				joybuttons[i] = false;
		return false; // eat events
 
	case ev_povup:
		povangle = -1;
		// If looking around with PoV, don't allow bindings.
		if(cfg.povLookAround) return true;
		break;

	case ev_povdown:
		povangle = ev->data1;
		if(cfg.povLookAround) return true;
		break;

	default: 
		break; 
    } 
	// The event wasn't used. 
    return false; 
} 

#if __JHERETIC__ || __JHEXEN__
void G_InventoryTicker (void)
{
	if(!players[consoleplayer].plr->ingame) return;

	// turn inventory off after a certain amount of time
	if(inventory && !(--inventoryTics))
	{
		players[consoleplayer].readyArtifact =
			players[consoleplayer].inventory[inv_ptr].type;
		inventory = false;
/*#if __JHEXEN__
		cmd->arti = 0;
#endif*/
	}
}
#endif

void G_SpecialButton(player_t *pl)
{
	if (pl->plr->ingame) 
	{ 
		if (pl->cmd.actions & BT_SPECIAL) 
		{ 
			switch (pl->cmd.actions & BT_SPECIALMASK) 
			{ 
			case BTS_PAUSE: 
				paused ^= 1; 
				if(paused) 
				{
					// This will stop all sounds from all origins.
					S_StopSound(0, 0);
				}

				// Servers are responsible for informing clients about
				// pauses in the game.
				NetSv_Paused(paused);

				pl->cmd.actions = 0;
				break; 
			} 
		} 
	}
}

//===========================================================================
// G_Ticker
//===========================================================================
void G_Ticker (void) 
{ 
    int		i;
    
	if(IS_CLIENT && !Get(DD_GAME_READY)) return;

#if _DEBUG
	Z_CheckHeap();
#endif

    // do player reborns if needed
    for (i = 0; i < MAXPLAYERS; i++) 
	{
		if(players[i].plr->ingame && players[i].playerstate == PST_REBORN) 
			G_DoReborn (i);
		
		// Player has left?
		if(players[i].playerstate == PST_GONE)
		{
			players[i].playerstate = PST_REBORN;
			if(!IS_CLIENT)
			{
				P_SpawnTeleFog(players[i].plr->mo->x, players[i].plr->mo->y);
			}
			// Let's get rid of the mobj.
#if _DEBUG
			Con_Message("G_Ticker: Removing player %i's mobj.\n", i);
#endif
			P_RemoveMobj(players[i].plr->mo);
			players[i].plr->mo = NULL;
		}
	}
		
	// do things to change the game state
	while (gameaction != ga_nothing) 
	{ 
		switch (gameaction) 
		{ 
#if __JHEXEN__
		case ga_initnew:
			G_DoInitNew();
			break;
		case ga_singlereborn:
			G_DoSingleReborn();
			break;
		case ga_leavemap:
			Draw_TeleportIcon();
			G_DoTeleportNewMap();
			break;
#endif
		case ga_loadlevel: 
			G_DoLoadLevel (); 
			break; 
		case ga_newgame: 
			G_DoNewGame (); 
			break; 
		case ga_loadgame: 
#if __JHEXEN__
			Draw_LoadIcon();
#endif
			G_DoLoadGame (); 
			break; 
		case ga_savegame: 
#if __JHEXEN__
			Draw_SaveIcon();
#endif
			G_DoSaveGame (); 
			break; 
#if __JHEXEN__
		case ga_playdemo:
			if(demoDisabled) {
				gameaction = ga_nothing;
			}
            else {
				G_DoPlayDemo();
			}
			break;
#else
		case ga_playdemo: 
			G_DoPlayDemo (); 
			break; 
#endif
		case ga_completed: 
			G_DoCompleted (); 
			break; 
		case ga_victory: 
//f __JHERETIC__ || __JHEXEN__
			gameaction = ga_nothing;
/*lse
			F_StartFinale (); 
#endif*/
			break; 
		case ga_worlddone: 
			G_DoWorldDone (); 
			break; 
		case ga_screenshot: 
			G_DoScreenShot (); 
			gameaction = ga_nothing; 
			break; 
		case ga_nothing: 
			break; 
		} 
	}
		
	// Look around.
	if(povangle != -1)
	{
		targetLookOffset = povangle/8.0f;
		if(targetLookOffset == .5f)
		{
			if(lookOffset < 0) targetLookOffset = -.5f;
		}
		else if(targetLookOffset > .5)
			targetLookOffset -= 1;
	}
	else targetLookOffset = 0;
	
	if(targetLookOffset != lookOffset && cfg.povLookAround)
	{
		float diff = (targetLookOffset - lookOffset)/2;
		if(diff > .075f) diff = .075f;
		if(diff < -.075f) diff = -.075f; 
		lookOffset += diff;
	}

#if __JHERETIC__ || __JHEXEN__
	G_InventoryTicker();
#endif

	// Enable/disable sending of frames (delta sets) to clients.
	Set(DD_ALLOW_FRAMES, gamestate == GS_LEVEL);
	if(!IS_CLIENT)
	{
		// Tell Doomsday when the game is paused (clients can't pause 
		// the game.)
		Set(DD_CLIENT_PAUSED, P_IsPaused());
	}

	// Must be called on every tick.
	P_RunPlayers();

	// Do main actions.
	switch (gamestate) 
	{ 
	case GS_LEVEL: 
		P_DoTick(); 
		HU_UpdatePsprites();

		// Active briefings once again (they were disabled when loading
		// a saved game).
		brief_disabled = false;

		if(IS_DEDICATED) break;

#if __JDOOM__
		ST_Ticker (); 
		AM_Ticker (); 
		HU_Ticker ();            
#else
		SB_Ticker ();
		AM_Ticker ();
		CT_Ticker ();
#endif
		break; 
		
	case GS_INTERMISSION: 
#if __JDOOM__
		WI_Ticker (); 
#else
		IN_Ticker ();
#endif
		break;

	default:
		break;
	} 
	
	// InFine ticks whenever it's active.
	FI_Ticker();

	// Servers will have to update player information and do such stuff.
	if(!IS_CLIENT) NetSv_Ticker();
} 
 
 
//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_InitPlayer 
// Called at the start.
// Called by the game initialization functions.
//
void G_InitPlayer (int player) 
{ 
    player_t*	p; 
 
    // set up the saved info         
    p = &players[player]; 
	 
    // clear everything else to defaults 
    G_PlayerReborn (player); 
} 
 

#if __JHEXEN__
//==========================================================================
//
// G_PlayerExitMap
//
// Called when the player leaves a map.
//
//==========================================================================

void G_PlayerExitMap(int playerNumber)
{
	int i;
	player_t *player;
	int flightPower;

	player = &players[playerNumber];

	// Strip all current powers (retain flight)
	flightPower = player->powers[pw_flight];
	memset(player->powers, 0, sizeof(player->powers));
	player->powers[pw_flight] = flightPower;
	player->update |= PSF_POWERS;

	if(deathmatch)
	{
		player->powers[pw_flight] = 0;
	}
	else
	{
		if(P_GetMapCluster(gamemap) != P_GetMapCluster(LeaveMap))
		{ // Entering new cluster
			// Strip all keys
			player->keys = 0;

			// Strip flight artifact
			for(i = 0; i < 25; i++)
			{
				player->powers[pw_flight] = 0;
				P_PlayerUseArtifact(player, arti_fly);
			}
			player->powers[pw_flight] = 0;
		}
	}

	player->update |= PSF_MORPH_TIME;
	if(player->morphTics)
	{
		player->readyweapon = player->plr->mo->special1; // Restore weapon
		player->morphTics = 0;
	}
	player->messageTics = 0;
	player->plr->lookdir = 0;
	player->plr->mo->flags &= ~MF_SHADOW; // Remove invisibility
	player->plr->extralight = 0; // Remove weapon flashes
	player->plr->fixedcolormap = 0; // Remove torch
	// Clear filter.
	player->plr->filter = 0;
	player->plr->flags |= DDPF_FILTER;
	player->damagecount = 0; // No palette changes
	player->bonuscount = 0;
	player->poisoncount = 0;
	if(player == &players[consoleplayer])
	{
		SB_state = -1; // refresh the status bar
	}
}

#else

//
// G_PlayerFinishLevel
// Can when a player completes a level.
//
void G_PlayerFinishLevel (int player) 
{ 
    player_t *p = &players[player]; 	 
#if __JHERETIC__
	int i;

	for(i=0; i<p->inventorySlotNum; i++)
	{
		p->inventory[i].count = 1;
	}
	p->artifactCount = p->inventorySlotNum;
	if(!deathmatch)
		for(i = 0; i < 16; i++)
		{
			P_PlayerUseArtifact(p, arti_fly);
		}
#endif

	p->update |= PSF_POWERS | PSF_KEYS;
	memset (p->powers, 0, sizeof (p->powers)); 
#if __JDOOM__
	memset (p->cards, 0, sizeof (p->cards)); 
#elif __JHERETIC__
	p->update |= PSF_CHICKEN_TIME;
	memset(p->keys, 0, sizeof(p->keys));
	playerkeys = 0;
	if(p->chickenTics)
	{
		p->readyweapon = p->plr->mo->special1; // Restore weapon
		p->chickenTics = 0;
	}
	p->messageTics = 0;
	p->rain1 = NULL;
	p->rain2 = NULL;
/*	if(p == &players[consoleplayer])
	{
		SB_state = -1; // refresh the status bar
	}*/
#endif
    
    p->plr->mo->flags &= ~MF_SHADOW;		// cancel invisibility 
    p->plr->extralight = 0;			// cancel gun flashes 
    p->plr->fixedcolormap = 0;		// cancel ir gogles 
    p->damagecount = 0;			// no palette changes 
    p->bonuscount = 0; 
	p->plr->lookdir = 0;
	// Clear filter.
	p->plr->filter = 0;
	p->plr->flags |= DDPF_FILTER;
} 

#endif 

//==========================================================================
//
// ClearPlayer
//
// Safely clears the player data structures.
//
//==========================================================================

void ClearPlayer(player_t *p)
{
	ddplayer_t	*ddplayer = p->plr;
	int			playeringame = ddplayer->ingame;
	int			flags = ddplayer->flags;
	int			start = p->startspot;

	memset(p, 0, sizeof(*p));
	// Restore the pointer to ddplayer.
	p->plr = ddplayer;
	// Also clear ddplayer.
	memset(ddplayer, 0, sizeof(*ddplayer));
	// Restore the pointer to this player.
	ddplayer->extradata = p;
	// Restore the playeringame data.
	ddplayer->ingame = playeringame;
	ddplayer->flags = flags;
	// Don't clear the start spot.
	p->startspot = start;
}


//
// G_PlayerReborn
// Called after a player dies 
// almost everything is cleared and initialized 
//
void G_PlayerReborn (int player) 
{ 
    player_t*	p; 
    int			frags[MAXPLAYERS]; 
    int			killcount;
    int			itemcount;
    int			secretcount; 
#if !__JHEXEN__
    int			i; 
#endif
#if __JHERETIC__
	boolean		secret = false;
	int			spot;
#elif __JHEXEN__
	uint		worldTimer;
#endif
	 
    memcpy(frags, players[player].frags, sizeof(frags)); 
    killcount = players[player].killcount; 
    itemcount = players[player].itemcount; 
    secretcount = players[player].secretcount; 
#if __JHEXEN__
	worldTimer = players[player].worldTimer;
#endif
	 
    p = &players[player]; 
#if __JHERETIC__
	if(p->didsecret) secret = true;
	spot = p->startspot;
#endif

	// Clears (almost) everything.
    ClearPlayer(p);

#if __JHERETIC__
	p->startspot = spot;
#endif
 
    memcpy(players[player].frags, frags, sizeof(players[player].frags)); 
    players[player].killcount = killcount; 
    players[player].itemcount = itemcount; 
    players[player].secretcount = secretcount; 
#if __JHEXEN__
	players[player].worldTimer = worldTimer;
	players[player].class = cfg.PlayerClass[player];
	players[player].colormap = cfg.PlayerColor[player];
#endif
 
    p->usedown = p->attackdown = true;	// don't do anything immediately 
    p->playerstate = PST_LIVE;       
    p->health = MAXHEALTH; 
#if __JDOOM__
    p->readyweapon = p->pendingweapon = wp_pistol; 
    p->weaponowned[wp_fist] = true; 
    p->weaponowned[wp_pistol] = true; 
	p->ammo[am_clip] = 50; 

	// See if the Values specify anything.
	P_InitPlayerValues(p);

#elif __JHERETIC__
	p->readyweapon = p->pendingweapon = wp_goldwand;
	p->weaponowned[wp_staff] = true;
	p->weaponowned[wp_goldwand] = true;
	p->ammo[am_goldwand] = 50;

	if(gamemap == 9 || secret)
	{
		p->didsecret = true;
	}

#else
	p->readyweapon = p->pendingweapon = WP_FIRST;
	p->weaponowned[WP_FIRST] = true;
	localQuakeHappening[player] = false;

#endif

#ifndef __JHEXEN__
	// Reset maxammo.
    for (i=0 ; i<NUMAMMO ; i++) p->maxammo[i] = maxammo[i]; 
#endif

#if __JDOOM__
	// We'll need to update almost everything.
	p->update |= PSF_REBORN;

#elif __JHERETIC__
	if(p == &players[consoleplayer])
	{
		inv_ptr = 0; // reset the inventory pointer
		curpos = 0;
	}
	// We'll need to update almost everything.
	p->update |= PSF_STATE | PSF_HEALTH | PSF_ARMOR_TYPE 
		| PSF_ARMOR_POINTS | PSF_INVENTORY | PSF_POWERS 
		| PSF_KEYS | PSF_OWNED_WEAPONS | PSF_AMMO 
		| PSF_MAX_AMMO | PSF_PENDING_WEAPON | PSF_READY_WEAPON;

#elif __JHEXEN__
	if(p == &players[consoleplayer])
	{
		SB_state = -1; // refresh the status bar
		inv_ptr = 0; // reset the inventory pointer
		curpos = 0;
	}
	// We'll need to update almost everything.
	p->update |= PSF_REBORN;
#endif

	p->plr->flags &= ~DDPF_DEAD;
}


//
// G_DeathMatchSpawnPlayer 
// Spawns a player at one of the random death match spots 
// called at level load and each death 
//
void G_DeathMatchSpawnPlayer (int playernum) 
{ 
    int             i,j; 
    int				selections; 
	mapthing_t		faraway;
	boolean			using_dummy = false;
	ddplayer_t		*pl = players[playernum].plr;

	// Spawn player initially at a distant location.
	if(!pl->mo)
	{
		faraway.x = faraway.y = DDMAXSHORT;
		faraway.angle = 0;
		P_SpawnPlayer(&faraway, playernum);
		using_dummy = true;
	}
	
	// Now let's find an available deathmatch start.
    selections = deathmatch_p - deathmatchstarts; 
    if (selections < 2) 
		Con_Error ("Only %i deathmatch spots, 2 required", selections); 

    for (j=0 ; j<20 ; j++) 
    { 
		i = P_Random() % selections; 
		if(P_CheckSpot(playernum, &deathmatchstarts[i], true)) 
		{
#if __JHERETIC__ || __JHEXEN__
			deathmatchstarts[i].type = playernum+1;
#endif
			break; 
		}
    } 
	if(using_dummy)
	{
		// Destroy the dummy.
		P_RemoveMobj(pl->mo);
		pl->mo = NULL;
	}
	P_SpawnPlayer(&deathmatchstarts[i], playernum);

#ifndef __JHEXEN__
	// Gib anything at the spot.
	P_Telefrag(players[playernum].plr->mo);
#endif
} 

//===========================================================================
// G_DummySpawnPlayer
//	Spawns the given player at a dummy place.
//===========================================================================
void G_DummySpawnPlayer(int playernum)
{
	mapthing_t faraway;

	faraway.x = faraway.y = DDMAXSHORT;
	faraway.angle = (short) 0;
	P_SpawnPlayer(&faraway, playernum);			
}

#ifdef __JDOOM__
//===========================================================================
// G_QueueBody
//===========================================================================
void G_QueueBody(mobj_t *body)
{
    // flush an old corpse if needed 
    if(bodyqueslot >= BODYQUESIZE) 
		P_RemoveMobj(bodyque[bodyqueslot % BODYQUESIZE]); 
    bodyque[bodyqueslot % BODYQUESIZE] = body; 
    bodyqueslot++;
}
#endif

//
// G_DoReborn 
// 
void G_DoReborn (int playernum) 
{
#if __JHEXEN__
	int i;
	boolean oldWeaponowned[NUMWEAPONS];
	int oldKeys;
	int oldPieces;
	boolean foundSpot;
	int bestWeapon;
#else
    mapthing_t *assigned;
#endif

	// Clear the currently playing script, if any.
	FI_Reset();

    if (!IS_NETGAME)
    {
		// We've just died, don't do a briefing now.
		brief_disabled = true;

#if __JHEXEN__
 		if(SV_HxRebornSlotAvailable())
		{ // Use the reborn code if the slot is available
			gameaction = ga_singlereborn;
		}
		else
		{ // Start a new game if there's no reborn info
			gameaction = ga_newgame;
		}
#else
		// reload the level from scratch
		gameaction = ga_loadlevel; 
#endif
    }
    else // Netgame
    {
		if(players[playernum].plr->mo)
		{
			// first dissasociate the corpse 
			players[playernum].plr->mo->player = NULL;   
			players[playernum].plr->mo->dplayer = NULL;
		}

		if(IS_CLIENT)
		{
			G_DummySpawnPlayer(playernum);
			return;
		}

		Con_Printf("G_DoReborn for %i.\n", playernum);

		// spawn at random spot if in death match 
		if (deathmatch) 
		{ 
			G_DeathMatchSpawnPlayer(playernum); 
			return; 
		} 

#if __JHEXEN__
		// Cooperative net-play, retain keys and weapons
		oldKeys = players[playernum].keys;
		oldPieces = players[playernum].pieces;
		for(i = 0; i < NUMWEAPONS; i++)
			oldWeaponowned[i] = players[playernum].weaponowned[i];
#endif
		 
#if __JDOOM__
		assigned = &playerstarts[players[playernum].startspot];
		if(P_CheckSpot(playernum, assigned, true)) 
		{ 
			P_SpawnPlayer(assigned, playernum); 
			return; 
		}
#elif __JHERETIC__
		// Try to spawn at the assigned spot.
		assigned = &playerstarts[players[playernum].startspot];
		if(P_CheckSpot(playernum, assigned, true))
		{
			Con_Printf( "- spawning at assigned spot %i.\n", players[playernum].startspot);
			P_SpawnPlayer(assigned, playernum);
			return;
		}
#elif __JHEXEN__
		foundSpot = false;
		if(P_CheckSpot(playernum,
			P_GetPlayerStart(RebornPosition, playernum), true))
			/* &playerstarts[RebornPosition][playernum] ))*/
		{ 
			// Appropriate player start spot is open
			P_SpawnPlayer(P_GetPlayerStart(RebornPosition, playernum) 
				/* &playerstarts[RebornPosition][playernum] */, playernum);
			foundSpot = true;
		}
		else
		{
			// Try to spawn at one of the other player start spots
			for(i = 0; i < MAXPLAYERS; i++)
			{
				if(P_CheckSpot(playernum, 
					P_GetPlayerStart(RebornPosition, i), true))
					// &playerstarts[RebornPosition][i]))
				{ // Found an open start spot

					P_SpawnPlayer(/*&playerstarts[RebornPosition][i]*/
						P_GetPlayerStart(RebornPosition, i), playernum);
					foundSpot = true;
					break;
				}
			}
		}
		if(foundSpot == false)
		{ // Player's going to be inside something
			P_SpawnPlayer(P_GetPlayerStart(RebornPosition, playernum)
				/*&playerstarts[RebornPosition][playernum]*/, playernum);
		}
		// Restore keys and weapons
		players[playernum].keys = oldKeys;
		players[playernum].pieces = oldPieces;
		for(bestWeapon = 0, i = 0; i < NUMWEAPONS; i++)
		{
			if(oldWeaponowned[i])
			{
				bestWeapon = i;
				players[playernum].weaponowned[i] = true;
			}
		}
		players[playernum].mana[MANA_1] = 25;
		players[playernum].mana[MANA_2] = 25;
		if(bestWeapon)
		{ // Bring up the best weapon
			players[playernum].pendingweapon = bestWeapon;
		}
#endif
#ifndef __JHEXEN__
		Con_Printf( "- force spawning at %i.\n", 
			players[playernum].startspot);

		// Fuzzy returns false if it needs telefragging.
		if(!P_FuzzySpawn(assigned, playernum, true))
		{
			// Spawn at the assigned spot, telefrag whoever's there.
			P_Telefrag(players[playernum].plr->mo);
		}
#endif
    } 
} 

void G_ScreenShot(void) 
{ 
    gameaction = ga_screenshot; 
} 
 
void G_DoScreenShot(void)
{
	int i;
	filename_t name;
	char *numPos;
	
	// Use game mode as the file name base.
	sprintf(name, "%s-", G_Get(DD_GAME_MODE));
	numPos = name + strlen(name);	

	// Find an unused file name.
	for(i = 0; i < 1e6; i++)	// Stop eventually...
	{
		sprintf(numPos, "%03i.tga", i);
		if(!M_FileExists(name)) break;
	}
	M_ScreenShot(name, 24);
	Con_Message("Wrote %s.\n", name);
}

#if __JDOOM__

// DOOM Par Times
int pars[4][10] = 
{ 
    {0}, 
    {0,30,75,120,90,165,180,180,30,165}, 
    {0,90,90,90,120,90,360,240,30,170}, 
    {0,90,45,90,150,90,90,165,30,135} 
}; 

// DOOM II Par Times
int cpars[32] =
{
    30,90,120,120,90,150,120,120,270,90,	//  1-10
    210,150,150,150,210,150,420,150,210,150,	// 11-20
    240,150,180,150,150,300,330,420,300,180,	// 21-30
    120,30					// 31-32
};
 
#endif

#if __JHEXEN__
//==========================================================================
//
// G_StartNewInit
//
//==========================================================================

void G_StartNewInit(void)
{
	SV_HxInitBaseSlot();
	SV_HxClearRebornSlot();
	P_ACSInitNewGame();
	// Default the player start spot group to 0
	RebornPosition = 0;
}

//==========================================================================
//
// G_StartNewGame
//
//==========================================================================

void G_StartNewGame(skill_t skill)
{
	int realMap;

	G_StartNewInit();
	realMap = P_TranslateMap(1);
	if(realMap == -1)
	{
		realMap = 1;
	}
	G_InitNew(TempSkill, 1, realMap);
}

//==========================================================================
//
// G_TeleportNewMap
//
// Only called by the warp cheat code.  Works just like normal map to map
// teleporting, but doesn't do any interlude stuff.
//
//==========================================================================

void G_TeleportNewMap(int map, int position)
{
	gameaction = ga_leavemap;
	LeaveMap = map;
	LeavePosition = position;
}

//==========================================================================
//
// G_DoTeleportNewMap
//
//==========================================================================

void G_DoTeleportNewMap(void)
{
	// Clients trust the server in these things.
	if(IS_CLIENT) 
	{
		gameaction = ga_nothing;
		return;
	}

	SV_HxMapTeleport(LeaveMap, LeavePosition);
	gamestate = GS_LEVEL;
	gameaction = ga_nothing;
	RebornPosition = LeavePosition;

	// Is there a briefing before this map?
	FI_Briefing(gameepisode, gamemap);
}
#endif

//
// G_DoCompleted 
//
 
void G_ExitLevel (void) 
{ 
	if(cyclingMaps && mapCycleNoExit) return;

    secretexit = false; 
    gameaction = ga_completed; 
} 

void G_SecretExitLevel (void) 
{ 
	if(cyclingMaps && mapCycleNoExit) return;

#if __JDOOM__
    // IF NO WOLF3D LEVELS, NO SECRET EXIT!
    if ( (gamemode == commercial)
		&& (W_CheckNumForName("map31")<0))
		secretexit = false;
    else
#endif
		secretexit = true; 
    gameaction = ga_completed; 
} 

#if __JHEXEN__
//==========================================================================
//
// G_Completed
//
// Starts intermission routine, which is used only during hub exits,
// and DeathMatch games.
//==========================================================================

void G_Completed(int map, int position)
{
	if(cyclingMaps && mapCycleNoExit) return;

	if(shareware && map > 4)
	{
		// Not possible in the 4-level demo.
		P_SetMessage(&players[consoleplayer], "PORTAL INACTIVE -- DEMO", 
			true);
		return;
	}

	gameaction = ga_completed;
	LeaveMap = map;
	LeavePosition = position;
}
#endif
 
void G_DoCompleted (void) 
{ 
    int i; 
#if __JHERETIC__
	static int afterSecret[5] = { 7, 5, 5, 5, 4 };
#endif

	// Clear the currently playing script, if any.
	FI_Reset();

	// Is there a debriefing for this map?
	if(FI_Debriefing(gameepisode, gamemap)) return;

    gameaction = ga_nothing; 
	
    for(i=0; i<MAXPLAYERS; i++) 
	{
		if (players[i].plr->ingame) 
		{
#if __JHEXEN__
			G_PlayerExitMap(i);
#else
			G_PlayerFinishLevel (i);        // take away cards and stuff 
#endif
			// Update this client's stats.
			NetSv_SendPlayerState(i, DDSP_ALL_PLAYERS, 
				PSF_FRAGS | PSF_COUNTERS, true);
		}
	}
#if __JHERETIC__
	prevmap = gamemap;
	if(secretexit == true)
	{
		gamemap = 9;
	}
	else if(gamemap == 9)
	{ // Finished secret level
		gamemap = afterSecret[gameepisode-1];
	}
	else if(gamemap == 8)
	{
		gameaction = ga_victory;
		return;
	}
	else
	{
		gamemap++;
	}
#endif
	
#if __JDOOM__
	if (automapactive) AM_Stop (); 

	if ( gamemode != commercial)
		switch(gamemap)
	{
	  case 8:
		  gameaction = ga_victory;
		  return;
	  case 9: 
		  for (i=0 ; i<MAXPLAYERS ; i++) 
			  players[i].didsecret = true; 
		  break;
	}
	
	//#if 0  Hmmm - why?
	if ( (gamemap == 8)
		&& (gamemode != commercial) ) 
	{
		// victory 
		gameaction = ga_victory; 
		return; 
	} 
	
	if ( (gamemap == 9)
		&& (gamemode != commercial) ) 
	{
		// exit secret level 
		for(i = 0; i < MAXPLAYERS; i++) 
			players[i].didsecret = true; 
	} 
	//#endif
		
	wminfo.didsecret = players[consoleplayer].didsecret; 
	wminfo.last = gamemap - 1;
	
	// wminfo.next is 0 biased, unlike gamemap
	if ( gamemode == commercial)
	{
		if (secretexit)
			switch(gamemap)
		{
		  case 15: wminfo.next = 30; break;
		  case 31: wminfo.next = 31; break;
		}
		else
			switch(gamemap)
		{
		  case 31:
		  case 32: wminfo.next = 15; break;
		  default: wminfo.next = gamemap;
		}
	}
	else
	{
		if (secretexit) 
			wminfo.next = 8; 	// go to secret level 
		else if (gamemap == 9) 
		{
			// returning from secret level 
			switch (gameepisode) 
			{ 
			case 1: 
				wminfo.next = 3; 
				break; 
			case 2: 
				wminfo.next = 5; 
				break; 
			case 3: 
				wminfo.next = 6; 
				break; 
			case 4:
				wminfo.next = 2;
				break;
			}                
		} 
		else 
			wminfo.next = gamemap;          // go to next level 
	}
	
	wminfo.maxkills = totalkills; 
	wminfo.maxitems = totalitems; 
	wminfo.maxsecret = totalsecret; 

	G_PrepareWIData();

	// Tell the clients what's going on.
	NetSv_Intermission(IMF_BEGIN, 0, 0);
	gamestate = GS_INTERMISSION; 
	viewactive = false; 
	automapactive = false; 
	WI_Start (&wminfo); 

#elif __JHERETIC__
	// Let the clients know the next level.
	NetSv_SendGameState(0, DDSP_ALL_PLAYERS);
	gamestate = GS_INTERMISSION;
	IN_Start();
#endif

#if __JHEXEN__
	if(LeaveMap == -1 && LeavePosition == -1)
	{
		gameaction = ga_victory;
		return;
	}
	else
	{		
		NetSv_Intermission(IMF_BEGIN, LeaveMap, LeavePosition);
		gamestate = GS_INTERMISSION;
		IN_Start();
	}
#endif
} 

#if __JDOOM__
void G_PrepareWIData(void)
{
	int	i;
	ddmapinfo_t minfo;
	char levid[10];

	wminfo.epsd = gameepisode-1; 
	wminfo.maxfrags = 0; 
	if(gamemode == commercial)
	{
		sprintf(levid, "MAP%02i", gamemap);
		wminfo.partime = 35*cpars[gamemap-1]; 
	}
	else
	{
		sprintf(levid, "E%iM%i", gameepisode, gamemap);
		wminfo.partime = 35*pars[gameepisode][gamemap]; 
	}

	// See if there is a par time definition.
	if(Def_Get(DD_DEF_MAP_INFO, levid, &minfo) && minfo.partime > 0)
		wminfo.partime = 35 * (int) minfo.partime;

	wminfo.pnum = consoleplayer; 
	for (i=0 ; i<MAXPLAYERS ; i++) 
	{ 
		wminfo.plyr[i].in = players[i].plr->ingame; 
		wminfo.plyr[i].skills = players[i].killcount; 
		wminfo.plyr[i].sitems = players[i].itemcount; 
		wminfo.plyr[i].ssecret = players[i].secretcount; 
		wminfo.plyr[i].stime = leveltime; 
		memcpy(wminfo.plyr[i].frags, players[i].frags, 
			sizeof(wminfo.plyr[i].frags)); 
	} 
}
#endif

//
// G_WorldDone 
//
void G_WorldDone (void) 
{ 
    gameaction = ga_worlddone; 

#if __JDOOM__	
    if (secretexit) 
		players[consoleplayer].didsecret = true; 

/*    if ( gamemode == commercial )
    {
		switch (gamemap)
		{
		case 15:
		case 31:
			if (!secretexit)
				break;
		case 6:
		case 11:
		case 20:
		case 30:
			F_StartFinale ();
			break;
		}
    }*/
#endif
} 

//============================================================================
//
// G_DoWorldDone
//
//============================================================================

void G_DoWorldDone (void) 
{   
	gamestate = GS_LEVEL; 
#if __JDOOM__
    gamemap = wminfo.next + 1; 
#endif
    G_DoLoadLevel(); 
    gameaction = ga_nothing; 
    viewactive = true; 
} 

#if __JHEXEN__
//==========================================================================
//
// G_DoSingleReborn
//
// Called by G_Ticker based on gameaction.  Loads a game from the reborn
// save slot.
//
//==========================================================================

void G_DoSingleReborn(void)
{
	gameaction = ga_nothing;
	SV_HxLoadGame(SV_HxGetRebornSlot());
	SB_SetClassData();
}
#endif

//extern boolean setsizeneeded;
//void R_ExecuteSetViewSize (void);

//==========================================================================
//
// G_LoadGame
//
// Can be called by the startup code or the menu task.
//
//==========================================================================

#if __JHEXEN__
void G_LoadGame (int slot) 
{ 
	GameLoadSlot = slot;
#else
void G_LoadGame (char *name) 
{ 
    strcpy(savename, name); 
#endif
    gameaction = ga_loadgame; 
} 
 
//==========================================================================
//
// G_DoLoadGame
//
// Called by G_Ticker based on gameaction.
//
//==========================================================================

void G_DoLoadGame (void) 
{ 
	G_StopDemo();
	FI_Reset();
    gameaction = ga_nothing; 

#if __JHEXEN__
	SV_HxLoadGame(GameLoadSlot);
	if(!netgame)
	{ // Copy the base slot to the reborn slot
		SV_HxUpdateRebornSlot();
	}
	SB_SetClassData();
#else
	SV_LoadGame(savename);
#endif
} 

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string 
//
void
G_SaveGame
( int	slot,
  char*	description ) 
{ 
    savegameslot = slot; 
    strcpy (savedescription, description); 
	gameaction = ga_savegame;
} 
 
//==========================================================================
//
// G_DoSaveGame
//
// Called by G_Ticker based on gameaction.
//
//==========================================================================

void G_DoSaveGame (void) 
{ 
#if __JHEXEN__
	SV_HxSaveGame(savegameslot, savedescription);
	gameaction = ga_nothing;
	savedescription[0] = 0;
	P_SetMessage(&players[consoleplayer], TXT_GAMESAVED, true);

#else
    char	name[100]; 
	
	SV_SaveGameFile(savegameslot, name);
	SV_SaveGame(name, savedescription);		

    gameaction = ga_nothing; 
    savedescription[0] = 0;		 
	
#if __JDOOM__
    P_SetMessage(players + consoleplayer, GGSAVED);
#elif __JHERETIC__
	P_SetMessage(&players[consoleplayer], TXT_GAMESAVED, true);
#endif
#endif
} 
 
#if __JHEXEN__
//==========================================================================
//
// G_DeferredNewGame
//
//==========================================================================

void G_DeferredNewGame(skill_t skill)
{
	TempSkill = skill;
	gameaction = ga_newgame;
}

void G_DoInitNew(void)
{
	SV_HxInitBaseSlot();
	G_InitNew(TempSkill, TempEpisode, TempMap);
	gameaction = ga_nothing;
}
#endif

//
// G_InitNew
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set. 
//
#ifndef __JHEXEN__
skill_t	d_skill; 
int     d_episode; 
int     d_map; 
#endif
 
void
G_DeferedInitNew
( skill_t	skill,
  int		episode,
  int		map) 
{ 
#if __JHEXEN__
	TempSkill = skill;
	TempEpisode = episode;
	TempMap = map;
	gameaction = ga_initnew;
#else
    d_skill = skill; 
    d_episode = episode; 
    d_map = map; 
    gameaction = ga_newgame; 
#endif
} 

void G_DoNewGame (void) 
{
	G_StopDemo();
#ifndef __JHEXEN__
	if(!IS_NETGAME)
	{
	    deathmatch = false;
		respawnparm = false;
		nomonsters = ArgExists("-nomonsters");//false;
	}
    G_InitNew(d_skill, d_episode, d_map); 
#else
	G_StartNewGame(TempSkill);
#endif
    gameaction = ga_nothing; 
} 

/*
 * Returns true if the specified (episode, map) pair can be used.
 * Otherwise the values are adjusted so they are valid.
 */
boolean G_ValidateMap(int *episode, int *map)
{
	boolean ok = true;

    if(*episode < 1) 
	{
		*episode = 1; 
		ok = false;
	}
	if(*map < 1) 
	{
		*map = 1;
		ok = false;
	}
	
#ifdef __JDOOM__
	if(gamemode == shareware)
    {
		// only start episode 1 on shareware
		if(*episode > 1) 
		{
			*episode = 1;
			ok = false;
		}
    }  
    else
    {
		// Allow episodes 1-9.
		if(*episode > 9) 
		{
			*episode = 9;
			ok = false;
		}
    }
    if((*map > 9) && (gamemode != commercial)) 
	{
		*map = 9; 
		ok = false;
	}
	// Check that the map truly exists.
	if(!P_MapExists(*episode, *map))
	{
		// (1,1) should exist always?
		*episode = 1;
		*map = 1;
		ok = false;
	}

#elif __JHERETIC__
	// Up to 9 episodes for testing
	if(*episode > 9) 
	{
		*episode = 9;
		ok = false;
	}
	if(*map > 9) 
	{
		*map = 9;
		ok = false;
	}

#elif __JHEXEN__
	if(*map > 99) 
	{
		*map = 99;
		ok = false;
	}
#endif

	return ok;
}

/*
 * Start a new game.
 */
void G_InitNew(skill_t skill, int episode, int map) 
{ 
    int	i;
#if __JHERETIC__
	int speed; 
#endif
	
	// If there are any InFine scripts running, they must be stopped.
	FI_Reset();

    if (paused) 
    { 
		paused = false; 
		//S_ResumeSound (); 
    } 

	if(skill < sk_baby) skill = sk_baby;
    if(skill > sk_nightmare) skill = sk_nightmare;

	// Make sure that the episode and map numbers are good.
	G_ValidateMap(&episode, &map);
    	
    M_ClearRandom (); 
	
#ifndef __JHEXEN__
#if __JDOOM__
    if(skill == sk_nightmare || respawnparm )
#elif __JHERETIC__
	if(respawnparm)
#endif
		respawnmonsters = true;
    else
		respawnmonsters = false;
#endif
	
#if __JDOOM__
    if (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare) )
    { 
		for(i=S_SARG_RUN1; i<=S_SARG_RUN8; i++)
			states[i].tics = 1;
		for(i=S_SARG_ATK1; i<=S_SARG_ATK3; i++)
			states[i].tics = 4;
		for(i=S_SARG_PAIN; i<=S_SARG_PAIN2; i++)
			states[i].tics = 1;
		mobjinfo[MT_BRUISERSHOT].speed = 20*FRACUNIT; 
		mobjinfo[MT_HEADSHOT].speed = 20*FRACUNIT; 
		mobjinfo[MT_TROOPSHOT].speed = 20*FRACUNIT; 
    } 
    else
    { 
		for(i=S_SARG_RUN1; i<=S_SARG_RUN8; i++)
			states[i].tics = 2;
		for(i=S_SARG_ATK1; i<=S_SARG_ATK3; i++)
			states[i].tics = 8;
		for(i=S_SARG_PAIN; i<=S_SARG_PAIN2; i++)
			states[i].tics = 2;
		mobjinfo[MT_BRUISERSHOT].speed = 15*FRACUNIT; 
		mobjinfo[MT_HEADSHOT].speed = 10*FRACUNIT; 
		mobjinfo[MT_TROOPSHOT].speed = 10*FRACUNIT; 
    } 
#elif __JHERETIC__
	speed = skill == sk_nightmare;
	for(i = 0; MonsterMissileInfo[i].type != -1; i++)
	{
		mobjinfo[MonsterMissileInfo[i].type].speed
			= MonsterMissileInfo[i].speed[speed]<<FRACBITS;
	}
#endif
	
	if(!IS_CLIENT)
	{
	    // force players to be initialized upon first level load         
		for(i = 0; i < MAXPLAYERS; i++) 
		{
			players[i].playerstate = PST_REBORN; 
#if __JHEXEN__
			players[i].worldTimer = 0;
#else
			players[i].didsecret = false;
#endif
		}
	}
	
    usergame = true;                // will be set false if a demo 
    paused = false; 
#if __JDOOM__
    automapactive = false; 
#endif
    viewactive = true; 
    gameepisode = episode; 
    gamemap = map; 
    gameskill = skill; 
	GL_Update(DDUF_BORDER);

	NetSv_UpdateGameConfig();

	// Tell the engine if we want that all players know 
	// where everybody else is.
	Set(DD_SEND_ALL_PLAYERS, !deathmatch);
	
    G_DoLoadLevel(); 

#if __JHEXEN__
	// Initialize the sky.
	P_InitSky(map);
#endif
} 
 
//
// G_PlayDemo 
//
void G_DeferedPlayDemo (char* name) 
{ 
    strcpy(defdemoname, name); 
    gameaction = ga_playdemo; 
} 
 
void G_DoPlayDemo (void) 
{ 
	int lnum = W_CheckNumForName(defdemoname);
	char *lump;
	char buf[128];

	gameaction = ga_nothing;
	// The lump should contain the path of the demo file.
	if(lnum < 0 || W_LumpLength(lnum) != 64) 
	{
		Con_Message("G_DoPlayDemo: invalid demo lump \"%s\".\n", defdemoname);
/*#if __JHEXEN__
		H2_AdvanceDemo();
#elif defined(__JHERETIC__)
		D_AdvanceDemo();
#endif*/
		return;
	}
	lump = W_CacheLumpNum(lnum, PU_CACHE);
	memset(buf, 0, sizeof(buf));
	strcpy(buf, "playdemo ");
	strncat(buf, lump, 64);

	// Start playing the demo.
	if(Con_Execute(buf, false))
	{
		// The demo will begin momentarily.
		gamestate = GS_WAITING;
	}
	else
	{
/*#if __JHEXEN__
		H2_AdvanceDemo();
#elif defined(__JHERETIC__)
		D_AdvanceDemo();
#endif*/
	}
} 

/*
 * Stops both playback and a recording. Called at critical points like 
 * starting a new game, or ending the game in the menu.
 */
void G_StopDemo(void)
{
	Con_Execute("stopdemo", true);
}

void G_DemoEnds(void)
{
	gamestate = GS_WAITING;
	if(singledemo) Sys_Quit();
/*#if __JHEXEN__
	H2_AdvanceDemo();
#elif defined(__JHERETIC__)
	D_AdvanceDemo();
#endif*/

#ifdef __JDOOM__
	FI_DemoEnds();
#endif
}

void G_DemoAborted(void)
{
	gamestate = GS_WAITING;
	// We'll take no further action.

#ifdef __JDOOM__
	FI_DemoEnds();
#endif
}

#if __JHERETIC__ || __JHEXEN__
int CCmdPause(int argc, char **argv)
{
	extern boolean MenuActive;

	if(!MenuActive) sendpause = true;
	return true;
}
#endif

//===========================================================================
// P_Thrust3D
//===========================================================================
void P_Thrust3D(player_t *player, angle_t angle, float lookdir, 
				int forwardmove, int sidemove)
{
	angle_t pitch = LOOKDIR2DEG(lookdir)/360*ANGLE_MAX;
	angle_t sideangle = angle - ANG90;
	mobj_t *mo = player->plr->mo;
	int zmul;
	int x, y, z;

	angle >>= ANGLETOFINESHIFT;
	sideangle >>= ANGLETOFINESHIFT;
	pitch >>= ANGLETOFINESHIFT;
	
	x = FixedMul(forwardmove, finecosine[angle]);
	y = FixedMul(forwardmove, finesine[angle]);
	z = FixedMul(forwardmove, finesine[pitch]);
	
	zmul = finecosine[pitch];
	x = FixedMul(x, zmul) + FixedMul(sidemove, finecosine[sideangle]);
	y = FixedMul(y, zmul) + FixedMul(sidemove, finesine[sideangle]);

	mo->momx += x;
	mo->momy += y;
	mo->momz += z;
}

//===========================================================================
// P_IsCamera
//===========================================================================
boolean P_IsCamera(mobj_t *mo)
{
	// Client mobjs do not have thinkers and thus cannot be cameras.
	return (mo->thinker.function 
		&& mo->player
		&& mo->player->plr->flags & DDPF_CAMERA);
}

//===========================================================================
// P_CameraXYMovement
//===========================================================================
int P_CameraXYMovement(mobj_t *mo)
{
	if(!P_IsCamera(mo)) return false;
#if __JDOOM__
	if(mo->flags & MF_NOCLIP
		// This is a very rough check!
		// Sometimes you get stuck in things.
		|| P_CheckPosition2(mo, mo->x + mo->momx, mo->y + mo->momy, mo->z))
	{
#endif

		P_UnsetThingPosition(mo);
		mo->x += mo->momx;
		mo->y += mo->momy;
		P_SetThingPosition(mo);
		P_CheckPosition(mo, mo->x, mo->y);
		mo->floorz = tmfloorz;
		mo->ceilingz = tmceilingz;

#if __JDOOM__
	}
#endif
	// Friction.
	mo->momx = FixedMul(mo->momx, 0xe800);
	mo->momy = FixedMul(mo->momy, 0xe800);
	return true;
}

//===========================================================================
// P_CameraZMovement
//===========================================================================
int P_CameraZMovement(mobj_t *mo)
{
	if(!P_IsCamera(mo)) return false;
	mo->z += mo->momz;
	mo->momz = FixedMul(mo->momz, 0xe800);
	if(mo->z < mo->floorz + 6*FRACUNIT) 
		mo->z = mo->floorz + 6*FRACUNIT;
	if(mo->z > mo->ceilingz - 6*FRACUNIT)
		mo->z = mo->ceilingz - 6*FRACUNIT;
	return true;
}

//===========================================================================
// P_CameraThink
//	Set appropriate parameters for a camera.
//===========================================================================
void P_CameraThink(player_t *player)
{
	angle_t angle;
	int tp, full, dist;
	mobj_t *mo, *target;

	// If this player is not a camera, get out of here.
	if(!(player->plr->flags & DDPF_CAMERA)) return;

	mo = player->plr->mo;
	player->cheats |= CF_GODMODE;
	if(cfg.cameraNoClip) player->cheats |= CF_NOCLIP;
	player->plr->viewheight = 0;
	mo->flags &= ~(MF_SOLID | MF_SHOOTABLE | MF_PICKUP);

	// How about viewlock?
	if(player->viewlock & 0xff)
	{
		full = (player->viewlock & LOCKF_FULL) != 0;
		tp = (player->viewlock & LOCKF_MASK) - 1;
		target = players[tp].plr->mo;
		if(players[tp].plr->ingame && target) 
		{
			angle = R_PointToAngle2(mo->x, mo->y, target->x, target->y);
			player->plr->clAngle = angle;
			if(full)
			{
				dist = P_ApproxDistance(mo->x - target->x,
					mo->y - target->y);
				angle = R_PointToAngle2(0, 0, target->z + target->height/2 - mo->z,
					dist);
				player->plr->clLookDir = -(angle/(float)ANGLE_MAX * 360.0f - 90);
				if(player->plr->clLookDir > 180)
					player->plr->clLookDir -= 360;
				player->plr->clLookDir *= 110.0f / 85.0f;
				if(player->plr->clLookDir > 110)
					player->plr->clLookDir = 110;
				if(player->plr->clLookDir < -110)
					player->plr->clLookDir = -110;
			}
		}
	}
}

//===========================================================================
// CCmdMakeLocal
//===========================================================================
int CCmdMakeLocal(int argc, char **argv)
{
	int p;
	char buf[20];
		
	if(argc < 2) return false;
	p = atoi(argv[1]);
	if(p < 0 || p >= MAXPLAYERS)
	{	
		Con_Printf("Invalid console number %i.\n", p);
		return false;
	}
	if(players[p].plr->ingame)
	{
		Con_Printf("Player %i is already in the game.\n", p);
		return false;
	}
	players[p].playerstate = PST_REBORN;
	players[p].plr->ingame = true;
	sprintf(buf, "conlocp %i", p);
	Con_Execute(buf, false);
	P_DealPlayerStarts();
	return true;
}

//===========================================================================
// CCmdSetCamera
//===========================================================================
int CCmdSetCamera(int argc, char **argv)
{
	int p;

	if(argc < 2) return false;
	p = atoi(argv[1]);
	if(p < 0 || p >= MAXPLAYERS)
	{
		Con_Printf("Invalid console number %i.\n", p);
		return false;
	}
	players[p].plr->flags ^= DDPF_CAMERA;
	return true;
}

//===========================================================================
// CCmdSetViewLock
//===========================================================================
int CCmdSetViewLock(int argc, char **argv)
{
	int pl = consoleplayer, lock;

	if(!stricmp(argv[0], "lockmode"))
	{
		if(argc < 2) return false;
		lock = atoi(argv[1]);
		if(lock) 
			players[pl].viewlock |= LOCKF_FULL;
		else
			players[pl].viewlock &= ~LOCKF_FULL;
		return true;
	}
	if(argc < 2) return false;
	if(argc >= 3) pl = atoi(argv[2]); // Console number.
	lock = atoi(argv[1]);
	if(lock == pl || lock < 0 || lock >= MAXPLAYERS) lock = -1;
	players[pl].viewlock &= ~LOCKF_MASK;
	players[pl].viewlock |= lock + 1;
	return true;
}

/*
 * CCmdSpawnMobj
 *	Spawns a mobj of the given type at (x,y,z).
 */
int CCmdSpawnMobj(int argc, char **argv)
{
	int type;
	int x, y, z;
	mobj_t *mo;

	if(argc != 5 && argc != 6)
	{
		Con_Printf("Usage: %s (type) (x) (y) (z) (angle)\n", argv[0]);
		Con_Printf("Type must be a defined Thing ID.\n");
		Con_Printf("Z is an offset from the floor, 'floor' or 'ceil'.\n");
		Con_Printf("Angle (0..360) is optional.\n");
		return true;
	}

	if(IS_CLIENT) 
	{
		Con_Printf("%s can't be used by clients.\n", argv[0]);
		return false;
	}
	
	// First try to find the thing.
	if((type = Def_Get(DD_DEF_MOBJ, argv[1], 0)) < 0)
	{
		Con_Printf("Undefined thing type %s.\n", argv[1]);
		return false;
	}

	// The coordinates.
	x = strtod(argv[2], 0) * FRACUNIT;
	y = strtod(argv[3], 0) * FRACUNIT;
	if(!stricmp(argv[4], "floor"))
		z = ONFLOORZ;
	else if(!stricmp(argv[4], "ceil"))
		z = ONCEILINGZ;
	else
	{
		z = strtod(argv[4], 0) * FRACUNIT
			+ R_PointInSubsector(x, y)->sector->floorheight;
	}

	if((mo = P_SpawnMobj(x, y, z, type)) && argc == 6)
	{
		mo->angle = ((int)(strtod(argv[5], 0)/360 * FRACUNIT)) << 16;
	}
	return true;
}

/*
 * CCmdPrintPlayerCoords
 */
int CCmdPrintPlayerCoords(int argc, char **argv)
{
	mobj_t *mo = players[consoleplayer].plr->mo;

	if(!mo || gamestate != GS_LEVEL) return false;
	Con_Printf("Console %i: X=%g Y=%g\n", consoleplayer, 
		FIX2FLT(mo->x), FIX2FLT(mo->y));
	return true;
}
