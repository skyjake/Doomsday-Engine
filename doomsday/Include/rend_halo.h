//===========================================================================
// REND_HALO.H
//	Halo == flare.
//===========================================================================
#ifndef __DOOMSDAY_RENDER_HALO_H__
#define __DOOMSDAY_RENDER_HALO_H__

#include "con_decl.h"

extern int		haloOccludeSpeed;
extern int		haloMode, haloBright, haloSize;
extern float	haloFadeMax, haloFadeMin, minHaloSize;

//void		H_Clear();

// Prepares the list of halos to render. Goes through the luminousList.
//void		H_InitForNewFrame();

// This must be called when the renderer is in player sprite rendering mode:
// 2D projection to the game view. 
//void		H_Render();

void	H_Register(void);
void	H_SetupState(boolean dosetup);
void	H_RenderHalo(vissprite_t *sourcevis, boolean primary);

// Console commands.
D_CMD( FlareConfig );

#endif 