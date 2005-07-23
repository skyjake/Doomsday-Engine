
//**************************************************************************
//**
//** H_ACTION.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "jHeretic/Doomdef.h"
#include "jHeretic/H_Action.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

action_t actions[NUM_ACTIONS + 1] = {
	{"left", 0},
	{"right", 0},
	{"forward", 0},
	{"backward", 0},
	{"strafel", 0},
	{"strafer", 0},
	{"fire", 0},
	{"use", 0},
	{"strafe", 0},
	{"speed", 0},
	{"flyup", 0},
	{"flydown", 0},
	{"falldown", 0},
	{"lookup", 0},
	{"lookdown", 0},
	{"lookcntr", 0},
	{"usearti", 0},
	{"mlook", 0},
	{"jlook", 0},
	{"nextwpn", 0},
	{"prevwpn", 0},
	{"weapon1", 0},
	{"weapon2", 0},
	{"weapon3", 0},
	{"weapon4", 0},
	{"weapon5", 0},
	{"weapon6", 0},
	{"weapon7", 0},
	{"weapon8", 0},
	{"weapon9", 0},
	{"cantdie", 0},
	{"invisib", 0},
	{"health", 0},
	{"sphealth", 0},
	{"tomepwr", 0},
	{"torch", 0},
	{"firebomb", 0},
	{"egg", 0},
	{"flyarti", 0},
	{"teleport", 0},
	{"panic", 0},
	{"demostop", 0},
	{"jump", 0},
	{"mzoomin", 0},
	{"mzoomout", 0},
	{"mpanup", 0},
	{"mpandown", 0},
	{"mpanleft", 0},
	{"mpanright", 0},
	{"", 0}
};

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------
