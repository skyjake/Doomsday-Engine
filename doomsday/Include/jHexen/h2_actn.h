#ifndef __JHEXEN_ACTIONS_H__
#define __JHEXEN_ACTIONS_H__

#include "h2def.h"

// These must correspond the action_t's in the actions array!
// Things that are needed in building ticcmds should be here.
typedef enum 
{
	// Game controls.
	A_TURNLEFT,
	A_TURNRIGHT,
	A_FORWARD,
	A_BACKWARD,
	A_STRAFELEFT,
	A_STRAFERIGHT,
	A_JUMP,
	A_FIRE,
	A_USE,
	A_STRAFE,
	A_SPEED,
	A_FLYUP,
	A_FLYDOWN,
	A_FLYCENTER,
	A_LOOKUP,
	A_LOOKDOWN,
	A_LOOKCENTER,
	A_USEARTIFACT,
	A_MLOOK,
	A_JLOOK,
	A_NEXTWEAPON,
	A_PREVIOUSWEAPON,
	A_WEAPON1,
	A_WEAPON2,
	A_WEAPON3,
	A_WEAPON4,

	// Item hotkeys.
	A_PANIC,
	A_TORCH,
	A_HEALTH,
	A_MYSTICURN,
	A_KRATER,
	A_SPEEDBOOTS,
	A_BLASTRADIUS,
	A_TELEPORT,
	A_TELEPORTOTHER,
	A_POISONBAG,
	A_INVULNERABILITY,
	A_DARKSERVANT,
	A_EGG,

	// Game system actions.
	A_STOPDEMO,		

	NUM_ACTIONS
} h2action_t;

// This is the actions array.
extern action_t actions[NUM_ACTIONS+1];

#endif
