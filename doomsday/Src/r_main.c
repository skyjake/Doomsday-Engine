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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int gridCols, gridRows;
static viewport_t viewports[DDMAXPLAYERS], *currentPort;

// CODE --------------------------------------------------------------------

/*
 * Calculate the placement and dimensions of a specific viewport.
 * Assumes that the grid has already been configured.
 */
void R_ViewPortPlacement(viewport_t *port, int x, int y)
{
	float w = screenWidth / (float) gridCols;
	float h = screenHeight / (float) gridRows;

	port->x = x * w;
	port->y = y * h;
	
	port->width = (x + 1) * w - port->x;
	port->height = (y + 1) * h - port->y;
}

/*
 * Set up a view grid and calculate the viewports.  Set 'numCols' and
 * 'numRows' to zero to just update the viewport coordinates.
 */
void R_SetViewGrid(int numCols, int numRows)
{
	int x, y, p;

	if(numCols > 0 && numRows > 0)
	{
		if(numCols > 16) numCols = 16;
		if(numRows > 16) numRows = 16;
	
		gridCols = numCols;
		gridRows = numRows;
	}

	// Reset all viewports to zero.
	memset(viewports, 0, sizeof(viewports));

	for(p = 0, y = 0; y < gridRows; y++)
	{
		for(x = 0; x < gridCols; x++, p++)
		{
			R_ViewPortPlacement(&viewports[p], x, y);

			// The console number is -1 if the viewport belongs to no
			// one.
			viewports[p].console = P_LocalToConsole(p);
		}
	}
}

/*
 * One-time initialization of the refresh daemon. Called by DD_Main.
 * GL has not yet been inited.
 */
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

/*
 * Re-initialize almost everything.
 */
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

/*
 * Shutdown the refresh daemon.
 */
void R_Shutdown(void)
{
	R_ShutdownModels();
	R_ShutdownData();
	// Most allocated memory goes down with the zone.
}

/*
 * Index of the flat that is used as ceilingpic/floorpic for skies.
 */
void R_InitSkyMap(void)
{
	skyflatnum = R_FlatNumForName(skyflatname);
}

/*
 * Don't really change anything here, because i might be in the middle
 * of a refresh.  The change will take effect next refresh.
 */
void R_ViewWindow(int x, int y, int w, int h)
{
	viewwindowx = x;
	viewwindowy = y;
	viewwidth = w;
	viewheight = h;
}

void R_SetViewPos(viewer_t *v)
{
	viewx = v->x;
	viewy = v->y;
	viewz = v->z;
	viewangle = v->angle;
	viewpitch = v->pitch;
}

/*
 * Prepare rendering the view of the given player.  Also handles
 * smoothing of camera and plane movement.
 */
void R_SetupFrame(ddplayer_t *player)
{
	int tableAngle;
	float yawRad, pitchRad;
	viewer_t viewer;

	// Reset the DGL triangle counter.
	gl.GetInteger(DGL_POLY_COUNT);

	viewplayer = player;

	// Local players use the angles with the lowest latency.
	viewer.angle = (player->flags & DDPF_LOCAL? player->clAngle :
					player->mo->angle) + viewangleoffset;
	viewer.pitch = (player->flags & DDPF_LOCAL? player->clLookDir :
					player->lookdir);
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

	// Calculate the front, up and side unit vectors.  The vectors are
	// in the DGL coordinate system, which is a left-handed one (same
	// as in the game, but Y and Z have been swapped).  Anyone who
	// uses these must note that it might be necessary to fix the
	// aspect ratio of the Y axis by dividing the Y coordinate by 1.2.
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

/*
 * Set the GL viewport.
 */
void R_UseViewPort(viewport_t *port)
{
	if(!port) 
	{
		gl.Viewport(0, 0, screenWidth, screenHeight);		
	}
	else
	{
		currentPort = port;
		gl.Viewport(port->x, port->y, port->width, port->height);
	}
}

/*
 * Render a blank view for the specified player.
 */
void R_RenderBlankView(void)
{
	// Just use the empty console background.
	Con_DrawStartupBackground(320, 200);
}

/*
 * Draw the view of the player inside the view window.
 */
void R_RenderPlayerView(ddplayer_t *player)
{
	extern int psp3d, model_tri_count;
	int i, oldFlags;

	// The mobj must be set or otherwise no view can be rendered.
	if(!player->ingame || !player->mo) return;
	
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
	GL_SwitchTo3DState(true, currentPort);
	Rend_RenderMap();
	
	// Orthogonal projection to the view window.
	GL_Restore2DState(1); 
	Rend_DrawPlayerSprites();		// If the 2D versions are needed.
	
	// Fullscreen viewport.
	GL_Restore2DState(2);
	
	// Do we need to render any 3D psprites?
	if(psp3d)
	{
		GL_SwitchTo3DState(false, currentPort);
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

/*
 * Render all view ports in the viewport grid.
 */
void R_RenderViewPorts(void)
{
	int oldDisplay = displayplayer;
	int x, y, p;

	// Draw a view for all players with a visible viewport.
	for(p = 0, y = 0; y < gridRows; y++)
		for(x = 0; x < gridCols; x++, p++)
		{
			displayplayer = viewports[p].console; 
			R_UseViewPort(viewports + p);
			if(displayplayer < 0)
			{
				R_RenderBlankView();
				continue;
			}

			// Draw the game graphics.
			gx.G_Drawer();
			
			// The colored filter. 
			if(GL_DrawFilter()) BorderNeedRefresh = true;

			// Increment the internal frame count. This does not
			// affect the FPS counter.
			framecount++;
		}

	// Restore things back to normal.
	displayplayer = oldDisplay;
	R_UseViewPort(NULL);
}

int CCmdViewGrid(int argc, char **argv)
{
	if(argc != 3)
	{
		Con_Printf("Usage: %s (cols) (rows)\n", argv[0]);
		return true;
	}

	// Recalculate viewports.
	R_SetViewGrid(strtol(argv[1], NULL, 0), strtol(argv[2], NULL, 0));
	return true;
}
