
//**************************************************************************
//**
//** R_COMMON.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#ifdef __JDOOM__
#include "doomdef.h"
#include "d_items.h"
#endif

#ifdef __JHERETIC__
#include "Doomdef.h"
#endif

#ifdef __JHEXEN__
#include "h2def.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// R_PrecachePSprites
//===========================================================================
void R_PrecachePSprites(void)
{
	int i;
#ifdef __JHEXEN__
	int k;
#endif

	for(i = 0; i < NUMWEAPONS; i++)
	{
#ifdef __JDOOM__
		R_PrecacheSkinsForState(weaponinfo[i].upstate);
		R_PrecacheSkinsForState(weaponinfo[i].downstate);
		R_PrecacheSkinsForState(weaponinfo[i].readystate);
		R_PrecacheSkinsForState(weaponinfo[i].atkstate);
		R_PrecacheSkinsForState(weaponinfo[i].flashstate);
#endif

#ifdef __JHERETIC__
		R_PrecacheSkinsForState(wpnlev1info[i].upstate);
		R_PrecacheSkinsForState(wpnlev1info[i].downstate);
		R_PrecacheSkinsForState(wpnlev1info[i].readystate);
		R_PrecacheSkinsForState(wpnlev1info[i].atkstate);
		R_PrecacheSkinsForState(wpnlev1info[i].holdatkstate);
		R_PrecacheSkinsForState(wpnlev1info[i].flashstate);

		R_PrecacheSkinsForState(wpnlev2info[i].upstate);
		R_PrecacheSkinsForState(wpnlev2info[i].downstate);
		R_PrecacheSkinsForState(wpnlev2info[i].readystate);
		R_PrecacheSkinsForState(wpnlev2info[i].atkstate);
		R_PrecacheSkinsForState(wpnlev2info[i].holdatkstate);
		R_PrecacheSkinsForState(wpnlev2info[i].flashstate);
#endif

#ifdef __JHEXEN__
		k = players[consoleplayer].class;
		R_PrecacheSkinsForState(WeaponInfo[i][k].upstate);
		R_PrecacheSkinsForState(WeaponInfo[i][k].downstate);
		R_PrecacheSkinsForState(WeaponInfo[i][k].readystate);
		R_PrecacheSkinsForState(WeaponInfo[i][k].atkstate);
		R_PrecacheSkinsForState(WeaponInfo[i][k].holdatkstate);
		R_PrecacheSkinsForState(WeaponInfo[i][k].flashstate);
#endif		
	}
}

