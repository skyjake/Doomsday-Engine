/** @file vignette.cpp Renders a vignette for the player view.
 * @ingroup render
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include <de/vector1.h>
#include <math.h>

#include "render/vignette.h"

static byte  vignetteEnabled  = true;
static float vignetteDarkness = 1.0f;
static float vignetteWidth    = 1.0f;

void Vignette_Register(void)
{
    C_VAR_BYTE ("rend-vignette",          &vignetteEnabled,  0,          0, 1);
    C_VAR_FLOAT("rend-vignette-darkness", &vignetteDarkness, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-vignette-width",    &vignetteWidth,    0,          0, 2);
}

void Vignette_Render(const RectRaw* viewRect, float fov)
{
    const int DIVS = 60;
    vec2f_t vec;
    float cx, cy, outer, inner;
    float alpha;
    int i;

    if(!vignetteEnabled) return;

    // Center point.
    cx = viewRect->origin.x + viewRect->size.width/2;
    cy = viewRect->origin.y + viewRect->size.height/2;

    // Radius.
    V2f_Set(vec, viewRect->size.width/2, viewRect->size.height/2);
    outer = V2f_Length(vec) + 1; // Extra pixel to account for a possible gap.
    if(fov < 100)
    {
        // Small FOV angles cause the vignette to be thinner/lighter.
        outer *= (1 + 100.f/fov) / 2;
    }
    inner = outer * vignetteWidth * .32f;
    if(fov > 100)
    {
        // High FOV angles cause the vignette to be wider.
        inner *= 100.f/fov;
    }

    alpha = vignetteDarkness * .6f;
    if(fov > 100)
    {
        // High FOV angles cause the vignette to be darker.
        alpha *= fov/100.f;
    }

    GL_BindTextureUnmanaged(GL_PrepareLSTexture(LST_CAMERA_VIGNETTE), GL_REPEAT, GL_CLAMP_TO_EDGE);
    glEnable(GL_TEXTURE_2D);

    glBegin(GL_TRIANGLE_STRIP);
    for(i = 0; i <= DIVS; ++i)
    {
        float ang = (float)(2 * de::PI * i) / (float)DIVS;
        float dx = cos(ang);
        float dy = sin(ang);

        glColor4f(0, 0, 0, alpha);
        glTexCoord2f(0, 1);
        glVertex2f(cx + outer * dx, cy + outer * dy);

        glColor4f(0, 0, 0, 0);
        glTexCoord2f(0, 0);
        glVertex2f(cx + inner * dx, cy + inner * dy);
    }
    glEnd();

    glDisable(GL_TEXTURE_2D);
}
