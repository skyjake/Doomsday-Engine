
//**************************************************************************
//**
//** D_ACTION.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "doomdef.h"
#include "D_Action.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

action_t actions[NUM_ACTIONS + 1] =
{
	{ "left", 0 },
	{ "right", 0 },
	{ "forward", 0 },
	{ "backward", 0 },
	{ "strafel", 0 },
	{ "strafer", 0 },
	{ "fire", 0 },
	{ "use", 0 },
	{ "strafe", 0 },
	{ "speed", 0 },
	{ "weap1", 0 },		// Fist.
	{ "weapon2", 0 },
	{ "weap3", 0 },		// Shotgun.
	{ "weapon4", 0 },
	{ "weapon5", 0 },
	{ "weapon6", 0 },
	{ "weapon7", 0 },
	{ "weapon8", 0 },	// Chainsaw
	{ "weapon9", 0 },	// Super sg.
	{ "nextwpn", 0 },
	{ "prevwpn", 0 },
	{ "mlook", 0 },
	{ "jlook", 0 },
	{ "lookup", 0 },
	{ "lookdown", 0 },
	{ "lookcntr", 0 },
	{ "jump", 0 },
	{ "demostop", 0 },
	{ "weapon1", 0 },	// Weapon cycle 1: fist/chain saw
	{ "weapon3", 0 },	// Weapon cycle 2: shotgun/super shotgun
	{ "", 0 }			// A terminator.
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------


