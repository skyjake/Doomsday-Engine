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
 * viewport.cpp: Viewport and Scissor
 */

// HEADER FILES ------------------------------------------------------------

#include "drD3D.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------


// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean		scissorActive;
Box			scissor, viewport;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// InitViewport
//===========================================================================
void InitViewport(void)
{
	scissorActive = false;
}

//===========================================================================
// Viewport
//	Only updates the D3D viewport.
//===========================================================================
void Viewport(const Box &box)
{
	D3DVIEWPORT8 vp;

	vp.X = box.x;
	vp.Y = box.y;
	vp.Width = box.width;
	vp.Height = box.height;
	vp.MinZ = 0;
	vp.MaxZ = 1;
	dev->SetViewport(&vp);
}

//===========================================================================
// UpdateScissor
//	Updates the projection matrix and viewport.
//===========================================================================
void UpdateScissor(void)
{
	Viewport(scissorActive? scissor : viewport);
	ScissorProjection();
}

//===========================================================================
// EnableScissor
//===========================================================================
void EnableScissor(bool enable)
{
	if(enable)
	{
		scissorActive = true;
		UpdateScissor();
	}
	else
	{
		// Restore the original viewport.
		scissorActive = false;
		UpdateScissor();
	}
}

//===========================================================================
// DG_Viewport
//===========================================================================
void DG_Viewport(int x, int y, int width, int height)
{
	viewport.Set(x, y, width, height);
	Viewport(viewport);

	// Changing the viewport disables scissor.
	bool mustUpdateScissor = false;
	if(scissorActive) mustUpdateScissor = true;
	scissorActive = false;
	scissor = viewport;
	if(mustUpdateScissor) UpdateScissor();
}

//===========================================================================
// DG_Scissor
//===========================================================================
void DG_Scissor(int x, int y, int width, int height)
{
	scissor.Set(x, y, width, height);
	UpdateScissor();
}

//===========================================================================
// DG_ZBias
//===========================================================================
void DG_ZBias(int level)
{
	SetRS(D3DRS_ZBIAS, 2 - level);
}
