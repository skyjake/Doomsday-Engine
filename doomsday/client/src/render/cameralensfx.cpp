/** @file cameralensfx.cpp  Camera lens effects.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "render/cameralensfx.h"
#include "render/rend_main.h"
#include "render/viewports.h"
#include "render/vignette.h"
#include "gl/gl_draw.h"

static int fxFramePlayerNum;

void LensFx_Init()
{
}

void LensFx_Shutdown()
{
}

void LensFx_BeginFrame(int playerNum)
{
    fxFramePlayerNum = playerNum;
}

void LensFx_EndFrame()
{
    viewdata_t const *vd = R_ViewData(fxFramePlayerNum);

    // The colored filter.
    if(GL_FilterIsVisible())
    {
        GL_DrawFilter();
    }

    Vignette_Render(&vd->window, Rend_FieldOfView());
}
