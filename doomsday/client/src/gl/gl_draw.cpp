/**\file gl_draw.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * Basic (Generic) Drawing Routines.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_misc.h"
#include "de_play.h"

#include "gl/sys_opengl.h"
#include "api_render.h"

// MACROS ------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean drawFilter = false;
static float filterColor[4] = { 0, 0, 0, 0 };

// CODE --------------------------------------------------------------------

void GL_DrawRectWithCoords(const RectRaw* rect, Point2Raw coords[4])
{
    if(!rect) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBegin(GL_QUADS);
        // Upper left.
        if(coords) glTexCoord2iv((GLint*)coords[0].xy);
        glVertex2f(rect->origin.x, rect->origin.y);

        // Upper Right.
        if(coords) glTexCoord2iv((GLint*)coords[1].xy);
        glVertex2f(rect->origin.x + rect->size.width, rect->origin.y);

        // Lower right.
        if(coords) glTexCoord2iv((GLint*)coords[2].xy);
        glVertex2f(rect->origin.x + rect->size.width, rect->origin.y + rect->size.height);

        // Lower left.
        if(coords) glTexCoord2iv((GLint*)coords[3].xy);
        glVertex2f(rect->origin.x, rect->origin.y + rect->size.height);
    glEnd();
}

void GL_DrawRect(const RectRaw* rect)
{
    Point2Raw coords[4];
    coords[0].x = 0;
    coords[0].y = 0;
    coords[1].x = 1;
    coords[1].y = 0;
    coords[2].x = 1;
    coords[2].y = 1;
    coords[3].x = 0;
    coords[3].y = 1;
    GL_DrawRectWithCoords(rect, coords);
}

void GL_DrawRect2(int x, int y, int w, int h)
{
    RectRaw rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width = w;
    rect.size.height = h;
    GL_DrawRect(&rect);
}

void GL_DrawRectfWithCoords(const RectRawf* rect, Point2Rawf coords[4])
{
    if(!rect) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBegin(GL_QUADS);
        // Upper left.
        if(coords) glTexCoord2dv((GLdouble*)coords[0].xy);
        glVertex2d(rect->origin.x, rect->origin.y);

        // Upper Right.
        if(coords) glTexCoord2dv((GLdouble*)coords[1].xy);
        glVertex2d(rect->origin.x + rect->size.width, rect->origin.y);

        // Lower right.
        if(coords) glTexCoord2dv((GLdouble*)coords[2].xy);
        glVertex2d(rect->origin.x + rect->size.width, rect->origin.y + rect->size.height);

        // Lower left.
        if(coords) glTexCoord2dv((GLdouble*)coords[3].xy);
        glVertex2d(rect->origin.x, rect->origin.y + rect->size.height);
    glEnd();
}

void GL_DrawRectf(const RectRawf* rect)
{
    Point2Rawf coords[4];
    coords[0].x = 0;
    coords[0].y = 0;
    coords[1].x = 1;
    coords[1].y = 0;
    coords[2].x = 1;
    coords[2].y = 1;
    coords[3].x = 0;
    coords[3].y = 1;
    GL_DrawRectfWithCoords(rect, coords);
}

void GL_DrawRectf2(double x, double y, double w, double h)
{
    RectRawf rect;
    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width = w;
    rect.size.height = h;
    GL_DrawRectf(&rect);
}

void GL_DrawRectf2Color(double x, double y, double w, double h, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    GL_DrawRectf2(x, y, w, h);
}

void GL_DrawRectf2TextureColor(double x, double y, double width, double height,
    int texW, int texH, const float topColor[3], float topAlpha,
    const float bottomColor[3], float bottomAlpha)
{
    if(topAlpha <= 0 && bottomAlpha <= 0) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBegin(GL_QUADS);
        // Top color.
        glColor4f(topColor[0], topColor[1], topColor[2], topAlpha);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        glTexCoord2f(width / (float) texW, 0);
        glVertex2f(x + width, y);

        // Bottom color.
        glColor4f(bottomColor[0], bottomColor[1], bottomColor[2], bottomAlpha);
        glTexCoord2f(width / (float) texW, height / (float) texH);
        glVertex2f(x + width, y + height);
        glTexCoord2f(0, height / (float) texH);
        glVertex2f(x, y + height);
    glEnd();
}

void GL_DrawRectf2Tiled(double x, double y, double w, double h, int tw, int th)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2d(x, y);
        glTexCoord2f(w / (float) tw, 0);
        glVertex2d(x + w, y);
        glTexCoord2f(w / (float) tw, h / (float) th);
        glVertex2d(x + w, y + h);
        glTexCoord2f(0, h / (float) th);
        glVertex2d(x, y + h);
    glEnd();
}

void GL_DrawCutRectfTiled(const RectRawf* rect, int tw, int th, int txoff, int tyoff,
    const RectRawf* cutRect)
{
    float ftw = tw, fth = th;
    float txo = (1.0f / (float)tw) * (float)txoff;
    float tyo = (1.0f / (float)th) * (float)tyoff;

    // We'll draw at max four rectangles.
    float toph = cutRect->origin.y - rect->origin.y;
    float bottomh = rect->origin.y + rect->size.height - (cutRect->origin.y + cutRect->size.height);
    float sideh = rect->size.height - toph - bottomh;
    float lefth = cutRect->origin.x - rect->origin.x;
    float righth = rect->origin.x + rect->size.width - (cutRect->origin.x + cutRect->size.width);

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glBegin(GL_QUADS);
    if(toph > 0)
    {
        // The top rectangle.
        glTexCoord2f(txo, tyo);
        glVertex2f(rect->origin.x, rect->origin.y);
        glTexCoord2f(txo + (rect->size.width / ftw), tyo);
        glVertex2f(rect->origin.x + rect->size.width, rect->origin.y);
        glTexCoord2f(txo + (rect->size.width / ftw), tyo + (toph / fth));
        glVertex2f(rect->origin.x + rect->size.width, rect->origin.y + toph);
        glTexCoord2f(txo, tyo + (toph / fth));
        glVertex2f(rect->origin.x, rect->origin.y + toph);
    }

    if(lefth > 0 && sideh > 0)
    {
        float yoff = toph / fth;

        // The left rectangle.
        glTexCoord2f(txo, yoff + tyo);
        glVertex2f(rect->origin.x, rect->origin.y + toph);
        glTexCoord2f(txo + (lefth / ftw), yoff + tyo);
        glVertex2f(rect->origin.x + lefth, rect->origin.y + toph);
        glTexCoord2f(txo + (lefth / ftw), yoff + tyo + sideh / fth);
        glVertex2f(rect->origin.x + lefth, rect->origin.y + toph + sideh);
        glTexCoord2f(txo, yoff + tyo + sideh / fth);
        glVertex2f(rect->origin.x, rect->origin.y + toph + sideh);
    }

    if(righth > 0 && sideh > 0)
    {
        float ox = rect->origin.x + lefth + cutRect->size.width;
        float xoff = (lefth + cutRect->size.width) / ftw;
        float yoff = toph / fth;

        // The left rectangle.
        glTexCoord2f(xoff + txo, yoff + tyo);
        glVertex2f(ox, rect->origin.y + toph);
        glTexCoord2f(xoff + txo + righth / ftw, yoff + tyo);
        glVertex2f(ox + righth, rect->origin.y + toph);
        glTexCoord2f(xoff + txo + righth / ftw, yoff + tyo + sideh / fth);
        glVertex2f(ox + righth, rect->origin.y + toph + sideh);
        glTexCoord2f(xoff + txo, yoff + tyo + sideh / fth);
        glVertex2f(ox, rect->origin.y + toph + sideh);
    }

    if(bottomh > 0)
    {
        float oy = rect->origin.y + toph + sideh;
        float yoff = (toph + sideh) / fth;

        glTexCoord2f(txo, yoff + tyo);
        glVertex2f(rect->origin.x, oy);
        glTexCoord2f(txo + rect->size.width / ftw, yoff + tyo);
        glVertex2f(rect->origin.x + rect->size.width, oy);
        glTexCoord2f(txo + rect->size.width / ftw, yoff + tyo + bottomh / fth);
        glVertex2f(rect->origin.x + rect->size.width, oy + bottomh);
        glTexCoord2f(txo, yoff + tyo + bottomh / fth);
        glVertex2f(rect->origin.x, oy + bottomh);
    }
    glEnd();
}

void GL_DrawCutRectf2Tiled(double x, double y, double w, double h, int tw, int th,
    int txoff, int tyoff, double cx, double cy, double cw, double ch)
{
    RectRawf rect, cutRect;

    rect.origin.x = x;
    rect.origin.y = y;
    rect.size.width  = w;
    rect.size.height = h;

    cutRect.origin.x = cx;
    cutRect.origin.y = cy;
    cutRect.size.width  = cw;
    cutRect.size.height = ch;

    GL_DrawCutRectfTiled(&rect, tw, th, txoff, tyoff, &cutRect);
}

/**
 * Totally inefficient for a large number of lines.
 */
void GL_DrawLine(float x1, float y1, float x2, float y2, float r, float g,
                 float b, float a)
{
    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glColor4f(r, g, b, a);
    glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    glEnd();
}

boolean GL_FilterIsVisible(void)
{
    return (0 != drawFilter && filterColor[CA] > 0);
}

#undef GL_SetFilter
DENG_EXTERN_C void GL_SetFilter(boolean enabled)
{
    drawFilter = enabled;
}

#undef GL_SetFilterColor
DENG_EXTERN_C void GL_SetFilterColor(float r, float g, float b, float a)
{
    filterColor[CR] = MINMAX_OF(0, r, 1);
    filterColor[CG] = MINMAX_OF(0, g, 1);
    filterColor[CB] = MINMAX_OF(0, b, 1);
    filterColor[CA] = MINMAX_OF(0, a, 1);
}

/**
 * @return              Non-zero if the filter was drawn.
 */
void GL_DrawFilter(void)
{
    const viewdata_t* vd = R_ViewData(displayPlayer);
    assert(NULL != vd);

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glColor4fv(filterColor);
    glBegin(GL_QUADS);
        glVertex2f(vd->window.origin.x, vd->window.origin.y);
        glVertex2f(vd->window.origin.x + vd->window.size.width, vd->window.origin.y);
        glVertex2f(vd->window.origin.x + vd->window.size.width, vd->window.origin.y + vd->window.size.height);
        glVertex2f(vd->window.origin.x, vd->window.origin.y + vd->window.size.height);
    glEnd();
}

#undef GL_ConfigureBorderedProjection2
DENG_EXTERN_C void GL_ConfigureBorderedProjection2(dgl_borderedprojectionstate_t* bp, int flags,
    int width, int height, int availWidth, int availHeight, scalemode_t overrideMode,
    float stretchEpsilon)
{
    if(!bp) Con_Error("GL_ConfigureBorderedProjection2: Invalid 'bp' argument.");

    bp->flags  = flags;
    bp->width  = width;
    bp->height = height;
    bp->availWidth  = availWidth;
    bp->availHeight = availHeight;

    bp->scaleMode = R_ChooseScaleMode2(bp->width, bp->height, bp->availWidth,
        bp->availHeight, overrideMode, stretchEpsilon);
    bp->alignHorizontal = R_ChooseAlignModeAndScaleFactor(&bp->scaleFactor,
        bp->width, bp->height, bp->availWidth, bp->availHeight, bp->scaleMode);

    bp->scissorState = 0;
    bp->scissorRegion.origin.x = bp->scissorRegion.origin.y = 0;
    bp->scissorRegion.size.width = bp->scissorRegion.size.height = 0;
}

#undef GL_ConfigureBorderedProjection
DENG_EXTERN_C void GL_ConfigureBorderedProjection(dgl_borderedprojectionstate_t* bp, int flags,
    int width, int height, int availWidth, int availHeight, scalemode_t overrideMode)
{
    GL_ConfigureBorderedProjection2(bp, flags, width, height, availWidth,
        availHeight, overrideMode, DEFAULT_SCALEMODE_STRETCH_EPSILON);
}

#undef GL_BeginBorderedProjection
DENG_EXTERN_C void GL_BeginBorderedProjection(dgl_borderedprojectionstate_t* bp)
{
    if(!bp)
    {
#if _DEBUG
        Con_Message("Warning: GL_BeginBorderedProjection: Invalid 'bp' argument, ignoring.\n");
#endif
        return;
    }

    if(SCALEMODE_STRETCH == bp->scaleMode) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    /**
     * Use an orthographic projection in screenspace, translating and
     * scaling the coordinate space using the modelview matrix producing
     * an aspect-corrected space of availWidth x availHeight and centered
     * on the larger of the horizontal and vertical axes.
     */
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    DGL_Ortho(0, 0, bp->availWidth, bp->availHeight, -1, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    if(bp->alignHorizontal)
    {
        // "Pillarbox":
        if(bp->flags & BPF_OVERDRAW_CLIP)
        {
            int w = .5f + (bp->availWidth - bp->width * bp->scaleFactor) / 2;
            bp->scissorState = DGL_GetInteger(DGL_SCISSOR_TEST);
            DGL_Scissor(&bp->scissorRegion);
            DGL_SetScissor2(w, 0, bp->width * bp->scaleFactor, bp->availHeight);
            DGL_Enable(DGL_SCISSOR_TEST);
        }

        glTranslatef((float)bp->availWidth/2, 0, 0);
        //glScalef(1/1.2f, 1, 1); // Aspect correction.
        glScalef(bp->scaleFactor, bp->scaleFactor, 1);
        glTranslatef(-bp->width/2, 0, 0);
    }
    else
    {
        // "Letterbox":
        if(bp->flags & BPF_OVERDRAW_CLIP)
        {
            int h = .5f + (bp->availHeight - bp->height * bp->scaleFactor) / 2;
            bp->scissorState = DGL_GetInteger(DGL_SCISSOR_TEST);
            DGL_Scissor(&bp->scissorRegion);
            DGL_SetScissor2(0, h, bp->availWidth, bp->height * bp->scaleFactor);
            DGL_Enable(DGL_SCISSOR_TEST);
        }

        glTranslatef(0, (float)bp->availHeight/2, 0);
        //glScalef(1, 1.2f, 1); // Aspect correction.
        glScalef(bp->scaleFactor, bp->scaleFactor, 1);
        glTranslatef(0, -bp->height/2, 0);
    }
}

#undef GL_EndBorderedProjection
DENG_EXTERN_C void GL_EndBorderedProjection(dgl_borderedprojectionstate_t* bp)
{
    if(!bp)
    {
#if _DEBUG
        Con_Message("Warning: GL_EndBorderedProjection: Invalid 'bp' argument, ignoring.\n");
#endif
        return;
    }

    if(SCALEMODE_STRETCH == bp->scaleMode) return;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    if(bp->flags & BPF_OVERDRAW_CLIP)
    {
        if(!bp->scissorState)
            DGL_Disable(DGL_SCISSOR_TEST);
        DGL_SetScissor(&bp->scissorRegion);
    }

    if(bp->flags & BPF_OVERDRAW_MASK)
    {
        // It shouldn't be necessary to bind the "not-texture" but the game
        // may have left whatever GL texture state it was using on. As this
        // isn't cleaned up until drawing control returns to the engine we
        // must explicitly disable it here.
        GL_SetNoTexture();
        glColor4f(0, 0, 0, 1);

        if(bp->alignHorizontal)
        {
            // "Pillarbox":
            int w = .5f + (bp->availWidth  - bp->width  * bp->scaleFactor) / 2;
            GL_DrawRectf2(0, 0, w, bp->availHeight);
            GL_DrawRectf2(bp->availWidth - w, 0, w, bp->availHeight);
        }
        else
        {
            // "Letterbox":
            int h = .5f + (bp->availHeight - bp->height * bp->scaleFactor) / 2;
            GL_DrawRectf2(0, 0, bp->availWidth, h);
            GL_DrawRectf2(0, bp->availHeight - h, bp->availWidth, h);
        }
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}
