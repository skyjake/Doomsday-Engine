
//**************************************************************************
//**
//** VIEWPORT.CPP
//**
//** Target:		DGL Driver for Direct3D 8.1
//** Description:	Viewport and scissor
//**
//**************************************************************************

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
