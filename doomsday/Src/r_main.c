/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * r_main.c: Refresh Subsystem
 *
 * The refresh daemon has the highest-level rendering code.
 * The view window is handled by refresh. The more specialized
 * rendering code in rend_*.c does things inside the view window.
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_render.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_audio.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// $smoothplane: Maximum speed for a smoothed plane.
#define MAX_SMOOTH_PLANE_MOVE	(64*FRACUNIT)

// TYPES -------------------------------------------------------------------

typedef struct viewer_s {
	fixed_t		x, y, z;
	angle_t		angle;
	float		pitch;
} viewer_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

void R_InitSkyMap(void);

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int			viewangleoffset = 0;
int			validcount = 1;	// increment every time a check is made
int			framecount;		// just for profiling purposes
int			rend_info_tris = 0;
fixed_t		viewx, viewy, viewz;
float		viewfrontvec[3], viewupvec[3], viewsidevec[3];
fixed_t		viewxOffset = 0, viewyOffset = 0, viewzOffset = 0;
angle_t		viewangle;
float		viewpitch;			// player->lookdir, global version
fixed_t		viewcos, viewsin;
ddplayer_t	*viewplayer;
boolean		setsizeneeded;

// Precalculated math tables.
fixed_t		*finecosine = &finesine[FINEANGLES/4];

int			extralight;			// bumped light from gun blasts

int			skyflatnum;
char		skyflatname[9] = "F_SKY";

double		lastSharpFrameTime;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//==========================================================================
// R_InitSkyMap
//==========================================================================
void R_InitSkyMap(void)
{
	skyflatnum = R_FlatNumForName(skyflatname);
}

//===========================================================================
// R_ViewWindow
//	Don't really change anything here, because i might be in the middle of
//	a refresh.  The change will take effect next refresh.
//===========================================================================
void R_ViewWindow(int x, int y, int w, int h)
{
	viewwindowx = x;
	viewwindowy = y;
	viewwidth = w;
	viewheight = h;
}

//===========================================================================
// R_Init
//	One-time initialization of the refresh daemon. Called by DD_Main.
//	GL has not yet been inited.
//===========================================================================
void R_Init(void)
{
	R_InitData();
	// viewwidth / viewheight / detailLevel are set by the defaults
	R_ViewWindow(0, 0, 320, 200);
	R_InitSprites();
	R_InitModels();
	R_InitSkyMap();
	R_InitTranslationTables();
	// Call the game DLL's refresh initialization, if necessary.
	if(gx.R_Init) gx.R_Init();
	Rend_Init();
	framecount = 0;
	R_InitViewBorder();
	Def_PostInit();
}

//===========================================================================
// R_Update
//	Re-initialize almost everything.
//===========================================================================
void R_Update(void)
{
	int i;

	// Stop playing sounds and music.
	Demo_StopPlayback();
	S_Reset();

	// Go back to startup-screen mode.
	Con_StartupInit();
	GL_TotalReset(true, false);
	GL_TotalReset(false, false); // Bring GL back online (no lightmaps yet).
	R_UpdateData();
	R_InitSprites();		// Fully reinitialize sprites.
	R_InitSkyMap();
	R_UpdateTranslationTables();
	// Re-read definitions.
	Def_Read();
	// Now that we've read the defs, we can load lightmaps.
	GL_LoadSystemTextures(true);
	Def_PostInit();
	R_InitModels();			// Defs might've changed.
	for(i = 0; i < DDMAXPLAYERS; i++)
	{
		// States have changed, the states are unknown.
		players[i].psprites[0].stateptr 
			= players[i].psprites[1].stateptr = NULL;
	}
	// The rendeling lists have persistent data that has changed during
	// the re-initialization.
	RL_DeleteLists();
	// Back to the game.
	Con_StartupDone();

#if _DEBUG
	Z_CheckHeap();
#endif
}

//===========================================================================
// R_Shutdown
//	Shutdown the refresh daemon.
//===========================================================================
void R_Shutdown(void)
{
	R_ShutdownModels();
	R_ShutdownData();
	// Most allocated memory goes down with the zone.
}

//===========================================================================
// R_InterpolateViewer
//===========================================================================
void R_InterpolateViewer
	(viewer_t *start, viewer_t *end, float pos, viewer_t *out)
{
	float inv = 1 - pos;

	out->x = inv*start->x + pos*end->x;
	out->y = inv*start->y + pos*end->y;
	out->z = inv*start->z + pos*end->z;
	out->angle = start->angle + pos*((int)end->angle - (int)start->angle);
	out->pitch = inv*start->pitch + pos*end->pitch;
}

//===========================================================================
// R_SetViewPos
//===========================================================================
void R_SetViewPos(viewer_t *v)
{
	viewx = v->x;
	viewy = v->y;
	viewz = v->z;
	viewangle = v->angle;
	viewpitch = v->pitch;
}

//===========================================================================
// R_CheckViewerLimits
//	The components whose difference is too large for interpolation will be 
//	snapped to the sharp values.
//===========================================================================
void R_CheckViewerLimits(viewer_t *src, viewer_t *dst)
{
#define MAXMOVE	(FRACUNIT*32)
	if(abs(dst->x - src->x) > MAXMOVE
		|| abs(dst->y - src->y) > MAXMOVE) 
	{
		src->x = dst->x;
		src->y = dst->y;
		src->z = dst->z;
	}
	if(abs((int)dst->angle - (int)src->angle) >= ANGLE_45) 
		src->angle = dst->angle;
}

//===========================================================================
// R_SetupFrame
//	Prepare rendering the view of the given player.
//	Also handles smoothing of camera and plane movement.
//===========================================================================
void R_SetupFrame(ddplayer_t *player)
{
	int tableAngle;
	float yawRad, pitchRad;
	viewer_t viewer;

	// Reset the DGL triangle counter.
	gl.GetInteger(DGL_POLY_COUNT);

	viewplayer = player;

	viewer.angle = (isClient? player->clAngle : player->mo->angle)
		+ viewangleoffset;
	viewer.pitch = isClient? player->clLookDir : player->lookdir;
	viewer.x = player->mo->x + viewxOffset;
	viewer.y = player->mo->y + viewyOffset;
	viewer.z = player->viewz + viewzOffset;

	// Check that the viewz doesn't go too high or low.
	if(viewer.z > player->mo->ceilingz - 4*FRACUNIT)
	{
		viewer.z = player->mo->ceilingz - 4*FRACUNIT;
	}
	if(viewer.z < player->mo->floorz + 4*FRACUNIT)
	{
		viewer.z = player->mo->floorz + 4*FRACUNIT;
	}

	R_SetViewPos(&viewer);

	extralight = player->extralight;
	tableAngle = viewangle >> ANGLETOFINESHIFT;
	viewsin = finesine[tableAngle];
	viewcos = finecosine[tableAngle];
	validcount++;
	if(BorderNeedRefresh)
	{
		R_DrawViewBorder();
		BorderNeedRefresh = false;
		BorderTopRefresh = false;
		UpdateState |= I_FULLSCRN;
	}
	if(BorderTopRefresh)
	{
		if(viewwindowx > 0)
		{
			R_DrawTopBorder();
		}
		BorderTopRefresh = false;
		UpdateState |= I_MESSAGES;
	}

	// Calculate the front, up and side unit vectors.
	// The vectors are in the DGL coordinate system, which is a left-handed
	// one (same as in the game, but Y and Z have been swapped). Anyone
	// who uses these must note that it might be necessary to fix the aspect
	// ratio of the Y axis by dividing the Y coordinate by 1.2.
	yawRad = viewangle/(float)ANGLE_MAX * 2*PI;
	pitchRad = viewpitch * 85/110.f/180 * PI;

	// The front vector.
	viewfrontvec[VX] = cos(yawRad) * cos(pitchRad);
	viewfrontvec[VZ] = sin(yawRad) * cos(pitchRad);
	viewfrontvec[VY] = sin(pitchRad);

	// The up vector.
	viewupvec[VX] = -cos(yawRad) * sin(pitchRad);
	viewupvec[VZ] = -sin(yawRad) * sin(pitchRad);
	viewupvec[VY] = cos(pitchRad);

	// The side vector is the cross product of the front and up vectors.
	M_CrossProduct(viewfrontvec, viewupvec, viewsidevec);
}

//===========================================================================
// R_RenderPlayerView
//	Draw the view of the player inside the view window.
//===========================================================================
void R_RenderPlayerView(ddplayer_t *player)
{
	extern int psp3d, model_tri_count;
	int i, oldFlags;

	// Setup for rendering the frame.
	R_SetupFrame(player);
	R_ClearSprites ();
	R_ProjectPlayerSprites();	// Only if 3D models exists for them.
	PG_InitForNewFrame();

	// Hide the viewplayer's mobj.
	oldFlags = player->mo->ddflags;
	player->mo->ddflags |= DDMF_DONTDRAW;

	// Go to wireframe mode?
	if(renderWireframe) gl.Enable(DGL_WIREFRAME_MODE);

	// GL is in 3D transformation state only during the frame.
	GL_SwitchTo3DState(true);
	Rend_RenderMap();
	// Orthogonal projection to the view window.
	GL_Restore2DState(1); 
	Rend_DrawPlayerSprites();		// If the 2D versions are needed.
	// Fullscreen viewport.
	GL_Restore2DState(2); 
	// Do we need to render any 3D psprites?
	if(psp3d)
	{
		GL_SwitchTo3DState(false);
		Rend_Draw3DPlayerSprites();
		GL_Restore2DState(2);		// Restore viewport.
	}
	// Original matrices and state: back to normal 2D.
	GL_Restore2DState(3); 

	// Back from wireframe mode?
	if(renderWireframe) gl.Disable(DGL_WIREFRAME_MODE);

	// Now we can show the viewplayer's mobj again.
	player->mo->ddflags = oldFlags;

	// Should we be counting triangles?
	if(rend_info_tris)
	{
		// This count includes all triangles drawn since R_SetupFrame.
		i = gl.GetInteger(DGL_POLY_COUNT);
		Con_Printf("Tris: %-4i (Mdl=%-4i)\n", i, model_tri_count); 
		model_tri_count = 0;
	}
	if(rend_info_lums)
	{
		Con_Printf("LumObjs: %-4i\n", numLuminous);
	}
}


//===========================================================================
// >>> OBSOLETE >>>
//===========================================================================
#if 0 
/*
=================
=
= R_InitPointToAngle
=
=================
*/

void R_InitPointToAngle (void)
{
// now getting from tables.c
#if 0
	int	i;
	long	t;
	float	f;
//
// slope (tangent) to angle lookup
//
	for (i=0 ; i<=SLOPERANGE ; i++)
	{
		f = atan( (float)i/SLOPERANGE )/(3.141592657*2);
		t = 0xffffffff*f;
		tantoangle[i] = t;
	}
#endif
}

//=============================================================================

/*
=================
=
= R_InitTables
=
=================
*/

void R_InitTables (void)
{
// now getting from tables.c
#if 0
	int		i;
	float		a, fv;
	int			t;

//
// viewangle tangent table
//
	for (i=0 ; i<FINEANGLES/2 ; i++)
	{
		a = (i-FINEANGLES/4+0.5)*PI*2/FINEANGLES;
		fv = FRACUNIT*tan (a);
		t = fv;
		finetangent[i] = t;
	}

//
// finesine table
//
	for (i=0 ; i<5*FINEANGLES/4 ; i++)
	{
// OPTIMIZE: mirror...
		a = (i+0.5)*PI*2/FINEANGLES;
		t = FRACUNIT*sin (a);
		finesine[i] = t;
	}
#endif

}

/*
=================
=
= R_InitTextureMapping
=
=================
*/

void R_InitTextureMapping (void)
{
}

//=============================================================================

/*
====================
=
= R_InitLightTables
=
= Only inits the zlight table, because the scalelight table changes
= with view size
=
====================
*/

#define		DISTMAP	2

void R_InitLightTables (void)
{
}

#endif 
//===========================================================================
// <<< OBSOLETE <<<
//===========================================================================

