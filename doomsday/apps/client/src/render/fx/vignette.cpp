/** @file vignette.cpp Renders a vignette for the player view.
 * @ingroup render
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de_platform.h"
#include "render/fx/vignette.h"
#include "clientapp.h"
#include "gl/gl_main.h"
#include "gl/gl_texmanager.h"
#include "render/rend_main.h"
#include "world/clientworld.h"

#include <de/glinfo.h>
#include <de/legacy/vector1.h>
#include <doomsday/console/var.h>
#include <cmath>

using namespace de;

namespace fx {

static byte  vignetteEnabled  = true;
static float vignetteDarkness = 1.0f;
static float vignetteWidth    = 1.0f;

static void Vignette_Render(const Rectanglei &viewRect, float fov)
{
    const int DIVS = 60;
    vec2f_t vec;
    float cx, cy, outer, inner;
    float alpha;
    int i;

    if (!vignetteEnabled) return;

    // Center point.
    cx = viewRect.left() + viewRect.width()  / 2.f;
    cy = viewRect.top()  + viewRect.height() / 2.f;

    // Radius.
    V2f_Set(vec, viewRect.width() / 2.f, viewRect.height() / 2.f);
    outer = V2f_Length(vec) + 1; // Extra pixel to account for a possible gap.
    if (fov < 100)
    {
        // Small FOV angles cause the vignette to be thinner/lighter.
        outer *= (1 + 100.f/fov) / 2;
    }
    inner = outer * vignetteWidth * .32f;
    if (fov > 100)
    {
        // High FOV angles cause the vignette to be wider.
        inner *= 100.f/fov;
    }

    alpha = vignetteDarkness * .6f;
    if (fov > 100)
    {
        // High FOV angles cause the vignette to be darker.
        alpha *= fov/100.f;
    }

    GL_BindTextureUnmanaged(
        GL_PrepareLSTexture(LST_CAMERA_VIGNETTE), gfx::Repeat, gfx::ClampToEdge);
    DGL_Enable(DGL_TEXTURE_2D);

    DGL_Begin(DGL_TRIANGLE_STRIP);
    for (i = 0; i <= DIVS; ++i)
    {
        float ang = (float)(2 * de::PI * i) / (float)DIVS;
        float dx = cos(ang);
        float dy = sin(ang);

        DGL_Color4f(0, 0, 0, alpha);
        DGL_TexCoord2f(0, 0, 1);
        DGL_Vertex2f(cx + outer * dx, cy + outer * dy);

        DGL_Color4f(0, 0, 0, 0);
        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(cx + inner * dx, cy + inner * dy);
    }
    DGL_End();

    DGL_Disable(DGL_TEXTURE_2D);
}

Vignette::Vignette(int console) : ConsoleEffect(console)
{}

void Vignette::draw()
{
    if (ClientApp::world().hasMap())
    {
        /// @todo Field of view should be console-specific.

        Vignette_Render(viewRect(), Rend_FieldOfView());
    }
}

void Vignette::consoleRegister()
{
    C_VAR_BYTE ("rend-vignette",          &vignetteEnabled,  0,          0, 1);
    C_VAR_FLOAT("rend-vignette-darkness", &vignetteDarkness, CVF_NO_MAX, 0, 0);
    C_VAR_FLOAT("rend-vignette-width",    &vignetteWidth,    0,          0, 2);
}

} // namespace fx
