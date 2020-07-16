/** @file busyvisual.cpp  Busy Mode visualizer.
 *
 * @authors Copyright © 2007-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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
#include "ui/busyvisual.h"

#include <cmath>
#include <de/legacy/concurrency.h>
#include <de/legacy/timer.h>
#include <de/glstate.h>
#include <de/glinfo.h>
#include <doomsday/doomsdayapp.h>
#include <doomsday/console/var.h>
#include "gl/gl_main.h"
#include "ui/widgets/busywidget.h"

static void releaseScreenshotTexture()
{
    ClientWindow::main().busy().releaseTransitionFrame();
}

void BusyVisual_PrepareResources()
{
    BusyTask *task = DoomsdayApp::app().busyMode().currentTask();
    if (!task) return;

    if (task->mode & BUSYF_STARTUP)
    {
        // Not in startup, so take a copy of the current frame contents.
        releaseScreenshotTexture();
    }
}

// Transition effect -----------------------------------------------------------

using namespace de;

#define DOOMWIPESINE_NUMSAMPLES 320

static void seedDoomWipeSine(void);

int rTransition = (int) TS_CROSSFADE; ///< cvar Default transition style.
int rTransitionTics = 28; ///< cvar Default transition duration (in tics).

typedef struct {
    dd_bool inProgress; /// @c true= a transition is presently being animated.
    transitionstyle_t style; /// Style of transition (cross-fade, wipe, etc...).
    uint startTime; /// Time at the moment the transition began (in 35hz tics).
    uint tics; /// Time duration of the animation.
    float position; /// Animation interpolation point [0..1].
} transitionstate_t;

static transitionstate_t transition;

static float doomWipeSine[DOOMWIPESINE_NUMSAMPLES];
static float doomWipeSamples[SCREENWIDTH+1];

void Con_TransitionRegister(void)
{
    C_VAR_INT("con-transition", &rTransition, 0, FIRST_TRANSITIONSTYLE, LAST_TRANSITIONSTYLE);
    C_VAR_INT("con-transition-tics", &rTransitionTics, 0, 0, 60);
}

void Con_TransitionConfigure(void)
{
    transition.tics = rTransitionTics;
    transition.style = (transitionstyle_t) rTransition;
    if (transition.style == TS_DOOM || transition.style == TS_DOOMSMOOTH)
    {
        seedDoomWipeSine();
    }
    transition.inProgress = true;
}

void Con_TransitionBegin(void)
{
    transition.startTime = Timer_Ticks();
    transition.position = 0;
}

dd_bool Con_TransitionInProgress(void)
{
    return transition.inProgress;
}

static void Con_EndTransition(void)
{
    if (!transition.inProgress) return;

    releaseScreenshotTexture();
    transition.inProgress = false;
}

void Con_TransitionTicker(timespan_t /*ticLength*/)
{
    if (isDedicated) return;
    if (!Con_TransitionInProgress()) return;

    transition.position = (float)(Timer_Ticks() - transition.startTime) / transition.tics;
    if (transition.position >= 1)
    {
        Con_EndTransition();
    }
}

static void seedDoomWipeSine(void)
{
    int i;

    doomWipeSine[0] = (128 - RNG_RandByte()) / 128.f;
    for (i = 1; i < DOOMWIPESINE_NUMSAMPLES; ++i)
    {
        float r = (128 - RNG_RandByte()) / 512.f;
        doomWipeSine[i] = MINMAX_OF(-1, doomWipeSine[i-1] + r, 1);
    }
}

static float sampleDoomWipeSine(float point)
{
    float sample = doomWipeSine[(uint)ROUND((DOOMWIPESINE_NUMSAMPLES-1) * MINMAX_OF(0, point, 1))];
    float offset = SCREENHEIGHT * transition.position * transition.position;
    return offset + (SCREENHEIGHT/2) * (transition.position + sample) * transition.position * transition.position;
}

static void sampleDoomWipe(void)
{
    int i;
    for (i = 0; i <= SCREENWIDTH; ++i)
    {
        doomWipeSamples[i] = MAX_OF(0, sampleDoomWipeSine((float) i / SCREENWIDTH));
    }
}

void Con_DrawTransition(void)
{
    if (isDedicated) return;
    if (!Con_TransitionInProgress()) return;

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, SCREENWIDTH, SCREENHEIGHT, -1, 1);

    DE_ASSERT(ClientWindow::main().busy().transitionFrame() != 0);

    const GLuint texScreenshot = ClientWindow::main().busy().transitionFrame()->glName();

    GL_BindTextureUnmanaged(texScreenshot, gfx::ClampToEdge, gfx::ClampToEdge);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    DGL_PushState();
    DGL_Disable(DGL_ALPHA_TEST);
    DGL_Enable(DGL_TEXTURE_2D);

    switch (transition.style)
    {
    case TS_DOOMSMOOTH: {

        float topAlpha, s, div, colWidth = 1.f / SCREENWIDTH;
        int x, y, h, i;

        sampleDoomWipe();
        div = 1 - .25f * transition.position;
        topAlpha = 1 - transition.position;
        topAlpha *= topAlpha;

        h = SCREENHEIGHT * (1 - div);
        x = 0;
        s = 0;

        DGL_Begin(DGL_TRIANGLE_STRIP);
        for (i = 0; i <= SCREENWIDTH; ++i, x++, s += colWidth)
        {
            y = doomWipeSamples[i];

            DGL_Color4f(1, 1, 1, 1);
            DGL_TexCoord2f(0, s, div); DGL_Vertex2f(x, y + h);
            DGL_Color4f(1, 1, 1, topAlpha);
            DGL_TexCoord2f(0, s, 1); DGL_Vertex2f(x, y);
        }
        DGL_End();

        x = 0;
        s = 0;

        DGL_Color4f(1, 1, 1, 1);
        DGL_Begin(DGL_TRIANGLE_STRIP);
        for (i = 0; i <= SCREENWIDTH; ++i, x++, s += colWidth)
        {
            y = doomWipeSamples[i] + h;

            DGL_TexCoord2f(0, s, 0);   DGL_Vertex2f(x, y + (SCREENHEIGHT - h));
            DGL_TexCoord2f(0, s, div); DGL_Vertex2f(x, y);
        }
        DGL_End();
        break;
      }
    case TS_DOOM: {
        // As above but drawn with whole pixel columns.
        float s = 0, colWidth = 1.f / SCREENWIDTH;
        int x = 0, y, i;

        sampleDoomWipe();

        DGL_Color4f(1, 1, 1, 1);
        DGL_Begin(DGL_QUADS);
        for (i = 0; i <= SCREENWIDTH; ++i, x++, s+= colWidth)
        {
            y = doomWipeSamples[i];

            DGL_TexCoord2f(0, s, 1); DGL_Vertex2f(x, y);
            DGL_TexCoord2f(0, s+colWidth, 1); DGL_Vertex2f(x+1, y);
            DGL_TexCoord2f(0, s+colWidth, 0); DGL_Vertex2f(x+1, y+SCREENHEIGHT);
            DGL_TexCoord2f(0, s, 0); DGL_Vertex2f(x, y+SCREENHEIGHT);
        }
        DGL_End();
        break;
      }
    case TS_CROSSFADE:
        DGL_Color4f(1, 1, 1, 1 - transition.position);

        DGL_Begin(DGL_QUADS);
            DGL_TexCoord2f(0, 0, 1); DGL_Vertex2f(0, 0);
            DGL_TexCoord2f(0, 0, 0); DGL_Vertex2f(0, SCREENHEIGHT);
            DGL_TexCoord2f(0, 1, 0); DGL_Vertex2f(SCREENWIDTH, SCREENHEIGHT);
            DGL_TexCoord2f(0, 1, 1); DGL_Vertex2f(SCREENWIDTH, 0);
        DGL_End();
        break;
    }

    GL_SetNoTexture();
    DGL_PopState();

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}
