/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * viewport.cpp: Viewport and Scissor
 */

// HEADER FILES ------------------------------------------------------------

#include "drd3d.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------


// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean     scissorActive;
Box         scissor, viewport;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void InitViewport(void)
{
    scissorActive = false;
}

/**
 * Only updates the D3D viewport.
 */
void Viewport(const Box &box)
{
    D3DVIEWPORT9 vp;

    vp.X = box.x;
    vp.Y = box.y;
    vp.Width = box.width;
    vp.Height = box.height;
    vp.MinZ = 0;
    vp.MaxZ = 1;
    dev->SetViewport(&vp);
}

/**
 * Updates the projection matrix and viewport.
 */
void UpdateScissor(void)
{
    Viewport(scissorActive? scissor : viewport);
    ScissorProjection();
}

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

void DG_Scissor(int x, int y, int width, int height)
{
    scissor.Set(x, y, width, height);
    UpdateScissor();
}

void DG_ZBias(int level)
{
    SetRS(D3DRS_DEPTHBIAS, 2 - level);
}
