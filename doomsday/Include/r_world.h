//===========================================================================
// R_WORLD.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_WORLD_H__
#define __DOOMSDAY_REFRESH_WORLD_H__

extern int leveltic;				// Restarts when a new map is set up.

// Map Info flags.
#define MIF_FOG				0x1		// Fog is used in the level.

const char *R_GetCurrentLevelID(void);
void R_SetupLevel(char *level_id, int flags);
void R_SetupFog(void);
void R_SetupSky(void);
void R_SetSectorLinks(sector_t *sec);
sector_t *R_GetLinkedSector(sector_t *startsec, boolean getfloor);
void R_UpdatePlanes(void);
void R_ClearSectorFlags(void);
void R_SkyFix(void);

#endif 