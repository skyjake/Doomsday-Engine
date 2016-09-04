/** @file gl_draw.cpp  Basic (Generic) Drawing Routines.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "gl/gl_draw.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <de/GLState>
#include <de/GLInfo>
#include <de/Vector>
#include <de/concurrency.h>
#include <doomsday/console/exec.h>

#include "gl/gl_main.h"
#include "gl/sys_opengl.h"

#include "world/p_players.h"  ///< @todo remove me

#include "api_render.h"
#include "render/viewports.h"

using namespace de;

static bool drawFilter = false;
static Vector4f filterColor;

void GL_DrawRectWithCoords(Rectanglei const &rect, Vector2i const coords[4])
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    LIBGUI_GL.glBegin(GL_QUADS);
        // Top left.
        if(coords) LIBGUI_GL.glTexCoord2i(coords[0].x, coords[0].y);
        LIBGUI_GL.glVertex2f(rect.topLeft.x, rect.topLeft.y);

        // Top Right.
        if(coords) LIBGUI_GL.glTexCoord2i(coords[1].x, coords[1].y);
        LIBGUI_GL.glVertex2f(rect.topRight().x, rect.topRight().y);

        // Bottom right.
        if(coords) LIBGUI_GL.glTexCoord2i(coords[2].x, coords[2].y);
        LIBGUI_GL.glVertex2f(rect.bottomRight.x, rect.bottomRight.y);

        // Bottom left.
        if(coords) LIBGUI_GL.glTexCoord2i(coords[3].x, coords[3].y);
        LIBGUI_GL.glVertex2f(rect.bottomLeft().x, rect.bottomLeft().y);
    LIBGUI_GL.glEnd();
}

void GL_DrawRect(Rectanglei const &rect)
{
    Vector2i coords[4] = { Vector2i(0, 0), Vector2i(1, 0),
                           Vector2i(1, 1), Vector2i(0, 1) };
    GL_DrawRectWithCoords(rect, coords);
}

void GL_DrawRect2(int x, int y, int w, int h)
{
    GL_DrawRect(Rectanglei::fromSize(Vector2i(x, y), Vector2ui(w, h)));
}

void GL_DrawRectfWithCoords(const RectRawf* rect, Point2Rawf coords[4])
{
    if(!rect) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    LIBGUI_GL.glBegin(GL_QUADS);
        // Upper left.
        if(coords) LIBGUI_GL.glTexCoord2dv((GLdouble*)coords[0].xy);
        LIBGUI_GL.glVertex2d(rect->origin.x, rect->origin.y);

        // Upper Right.
        if(coords) LIBGUI_GL.glTexCoord2dv((GLdouble*)coords[1].xy);
        LIBGUI_GL.glVertex2d(rect->origin.x + rect->size.width, rect->origin.y);

        // Lower right.
        if(coords) LIBGUI_GL.glTexCoord2dv((GLdouble*)coords[2].xy);
        LIBGUI_GL.glVertex2d(rect->origin.x + rect->size.width, rect->origin.y + rect->size.height);

        // Lower left.
        if(coords) LIBGUI_GL.glTexCoord2dv((GLdouble*)coords[3].xy);
        LIBGUI_GL.glVertex2d(rect->origin.x, rect->origin.y + rect->size.height);
    LIBGUI_GL.glEnd();
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
    LIBGUI_GL.glColor4f(r, g, b, a);
    GL_DrawRectf2(x, y, w, h);
}

void GL_DrawRectf2TextureColor(double x, double y, double width, double height,
    int texW, int texH, const float topColor[3], float topAlpha,
    const float bottomColor[3], float bottomAlpha)
{
    if(topAlpha <= 0 && bottomAlpha <= 0) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    LIBGUI_GL.glBegin(GL_QUADS);
        // Top color.
        LIBGUI_GL.glColor4f(topColor[0], topColor[1], topColor[2], topAlpha);
        LIBGUI_GL.glTexCoord2f(0, 0);
        LIBGUI_GL.glVertex2f(x, y);
        LIBGUI_GL.glTexCoord2f(width / (float) texW, 0);
        LIBGUI_GL.glVertex2f(x + width, y);

        // Bottom color.
        LIBGUI_GL.glColor4f(bottomColor[0], bottomColor[1], bottomColor[2], bottomAlpha);
        LIBGUI_GL.glTexCoord2f(width / (float) texW, height / (float) texH);
        LIBGUI_GL.glVertex2f(x + width, y + height);
        LIBGUI_GL.glTexCoord2f(0, height / (float) texH);
        LIBGUI_GL.glVertex2f(x, y + height);
    LIBGUI_GL.glEnd();
}

void GL_DrawRectf2Tiled(double x, double y, double w, double h, int tw, int th)
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    LIBGUI_GL.glBegin(GL_QUADS);
        LIBGUI_GL.glTexCoord2f(0, 0);
        LIBGUI_GL.glVertex2d(x, y);
        LIBGUI_GL.glTexCoord2f(w / (float) tw, 0);
        LIBGUI_GL.glVertex2d(x + w, y);
        LIBGUI_GL.glTexCoord2f(w / (float) tw, h / (float) th);
        LIBGUI_GL.glVertex2d(x + w, y + h);
        LIBGUI_GL.glTexCoord2f(0, h / (float) th);
        LIBGUI_GL.glVertex2d(x, y + h);
    LIBGUI_GL.glEnd();
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

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    LIBGUI_GL.glBegin(GL_QUADS);
    if(toph > 0)
    {
        // The top rectangle.
        LIBGUI_GL.glTexCoord2f(txo, tyo);
        LIBGUI_GL.glVertex2f(rect->origin.x, rect->origin.y);
        LIBGUI_GL.glTexCoord2f(txo + (rect->size.width / ftw), tyo);
        LIBGUI_GL.glVertex2f(rect->origin.x + rect->size.width, rect->origin.y);
        LIBGUI_GL.glTexCoord2f(txo + (rect->size.width / ftw), tyo + (toph / fth));
        LIBGUI_GL.glVertex2f(rect->origin.x + rect->size.width, rect->origin.y + toph);
        LIBGUI_GL.glTexCoord2f(txo, tyo + (toph / fth));
        LIBGUI_GL.glVertex2f(rect->origin.x, rect->origin.y + toph);
    }

    if(lefth > 0 && sideh > 0)
    {
        float yoff = toph / fth;

        // The left rectangle.
        LIBGUI_GL.glTexCoord2f(txo, yoff + tyo);
        LIBGUI_GL.glVertex2f(rect->origin.x, rect->origin.y + toph);
        LIBGUI_GL.glTexCoord2f(txo + (lefth / ftw), yoff + tyo);
        LIBGUI_GL.glVertex2f(rect->origin.x + lefth, rect->origin.y + toph);
        LIBGUI_GL.glTexCoord2f(txo + (lefth / ftw), yoff + tyo + sideh / fth);
        LIBGUI_GL.glVertex2f(rect->origin.x + lefth, rect->origin.y + toph + sideh);
        LIBGUI_GL.glTexCoord2f(txo, yoff + tyo + sideh / fth);
        LIBGUI_GL.glVertex2f(rect->origin.x, rect->origin.y + toph + sideh);
    }

    if(righth > 0 && sideh > 0)
    {
        float ox = rect->origin.x + lefth + cutRect->size.width;
        float xoff = (lefth + cutRect->size.width) / ftw;
        float yoff = toph / fth;

        // The left rectangle.
        LIBGUI_GL.glTexCoord2f(xoff + txo, yoff + tyo);
        LIBGUI_GL.glVertex2f(ox, rect->origin.y + toph);
        LIBGUI_GL.glTexCoord2f(xoff + txo + righth / ftw, yoff + tyo);
        LIBGUI_GL.glVertex2f(ox + righth, rect->origin.y + toph);
        LIBGUI_GL.glTexCoord2f(xoff + txo + righth / ftw, yoff + tyo + sideh / fth);
        LIBGUI_GL.glVertex2f(ox + righth, rect->origin.y + toph + sideh);
        LIBGUI_GL.glTexCoord2f(xoff + txo, yoff + tyo + sideh / fth);
        LIBGUI_GL.glVertex2f(ox, rect->origin.y + toph + sideh);
    }

    if(bottomh > 0)
    {
        float oy = rect->origin.y + toph + sideh;
        float yoff = (toph + sideh) / fth;

        LIBGUI_GL.glTexCoord2f(txo, yoff + tyo);
        LIBGUI_GL.glVertex2f(rect->origin.x, oy);
        LIBGUI_GL.glTexCoord2f(txo + rect->size.width / ftw, yoff + tyo);
        LIBGUI_GL.glVertex2f(rect->origin.x + rect->size.width, oy);
        LIBGUI_GL.glTexCoord2f(txo + rect->size.width / ftw, yoff + tyo + bottomh / fth);
        LIBGUI_GL.glVertex2f(rect->origin.x + rect->size.width, oy + bottomh);
        LIBGUI_GL.glTexCoord2f(txo, yoff + tyo + bottomh / fth);
        LIBGUI_GL.glVertex2f(rect->origin.x, oy + bottomh);
    }
    LIBGUI_GL.glEnd();
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
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    LIBGUI_GL.glColor4f(r, g, b, a);
    LIBGUI_GL.glBegin(GL_LINES);
        LIBGUI_GL.glVertex2f(x1, y1);
        LIBGUI_GL.glVertex2f(x2, y2);
    LIBGUI_GL.glEnd();
}

dd_bool GL_FilterIsVisible()
{
    return (drawFilter && filterColor.w > 0);
}

#undef GL_SetFilter
DENG_EXTERN_C void GL_SetFilter(dd_bool enabled)
{
    drawFilter = CPP_BOOL(enabled);
}

#undef GL_ResetViewEffects
DENG_EXTERN_C void GL_ResetViewEffects()
{
    GL_SetFilter(false);
    Con_Executef(CMDS_DDAY, true, "postfx %i none", consolePlayer);
    DD_SetInteger(DD_FULLBRIGHT, false);
}

#undef GL_SetFilterColor
DENG_EXTERN_C void GL_SetFilterColor(float r, float g, float b, float a)
{
    Vector4f newColorClamped(de::clamp(0.f, r, 1.f),
                             de::clamp(0.f, g, 1.f),
                             de::clamp(0.f, b, 1.f),
                             de::clamp(0.f, a, 1.f));

    if(filterColor != newColorClamped)
    {
        filterColor = newColorClamped;

        LOG_AS("GL_SetFilterColor");
        LOGDEV_GL_XVERBOSE("%s") << filterColor.asText();
    }
}

/**
 * @return              Non-zero if the filter was drawn.
 */
void GL_DrawFilter(void)
{
    viewdata_t const *vd = &DD_Player(displayPlayer)->viewport();

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    LIBGUI_GL.glColor4f(filterColor.x, filterColor.y, filterColor.z, filterColor.w);
    LIBGUI_GL.glBegin(GL_QUADS);
        LIBGUI_GL.glVertex2f(vd->window.topLeft.x, vd->window.topLeft.y);
        LIBGUI_GL.glVertex2f(vd->window.topRight().x, vd->window.topRight().y);
        LIBGUI_GL.glVertex2f(vd->window.bottomRight.x, vd->window.bottomRight.y);
        LIBGUI_GL.glVertex2f(vd->window.bottomLeft().x, vd->window.bottomLeft().y);
    LIBGUI_GL.glEnd();
}

#undef GL_ConfigureBorderedProjection2
DENG_EXTERN_C void GL_ConfigureBorderedProjection2(dgl_borderedprojectionstate_t* bp, int flags,
    int width, int height, int availWidth, int availHeight, scalemode_t overrideMode,
    float stretchEpsilon)
{
    if(!bp) App_Error("GL_ConfigureBorderedProjection2: Invalid 'bp' argument.");

    bp->flags       = flags;
    bp->width       = width; // draw coordinates (e.g., VGA)
    bp->height      = height;
    bp->availWidth  = availWidth; // screen space
    bp->availHeight = availHeight;

    bp->scaleMode = R_ChooseScaleMode2(bp->width, bp->height,
                                       bp->availWidth, bp->availHeight,
                                       overrideMode, stretchEpsilon);

    bp->isPillarBoxed = R_ChooseAlignModeAndScaleFactor(&bp->scaleFactor,
                                                        bp->width, bp->height,
                                                        bp->availWidth, bp->availHeight,
                                                        bp->scaleMode);
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
    DENG_ASSERT(bp != 0);
    if(!bp) return;

    if(SCALEMODE_STRETCH == bp->scaleMode) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    /**
     * Use an orthographic projection in screenspace, translating and
     * scaling the coordinate space using the modelview matrix producing
     * an aspect-corrected space of availWidth x availHeight and centered
     * on the larger of the horizontal and vertical axes.
     */
    LIBGUI_GL.glMatrixMode(GL_PROJECTION);
    LIBGUI_GL.glPushMatrix();
    LIBGUI_GL.glLoadIdentity();
    DGL_Ortho(0, 0, bp->availWidth, bp->availHeight, -1, 1);

    LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
    LIBGUI_GL.glPushMatrix();

    GLState::push();

    if(bp->isPillarBoxed)
    {
        // "Pillarbox":
        int offset = int((bp->availWidth - bp->scaleFactor * bp->width) / 2 + .5f);
        if(bp->flags & BPF_OVERDRAW_CLIP)
        {
            DGL_SetScissor2(offset, 0,
                            int(bp->scaleFactor * bp->width), bp->availHeight);
        }

        LIBGUI_GL.glTranslatef(offset, 0, 0);
        LIBGUI_GL.glScalef(bp->scaleFactor, bp->scaleFactor * 1.2f, 1);
    }
    else
    {
        // "Letterbox":
        int offset = int((bp->availHeight - bp->scaleFactor * 1.2f * bp->height) / 2 + .5f);
        if(bp->flags & BPF_OVERDRAW_CLIP)
        {
            DGL_SetScissor2(0, offset,
                            bp->availWidth, int(bp->scaleFactor * 1.2f * bp->height));
        }

        LIBGUI_GL.glTranslatef(0, offset, 0);
        LIBGUI_GL.glScalef(bp->scaleFactor, bp->scaleFactor * 1.2f, 1);
    }
}

#undef GL_EndBorderedProjection
DENG_EXTERN_C void GL_EndBorderedProjection(dgl_borderedprojectionstate_t* bp)
{
    DENG_ASSERT(bp != 0);
    if(!bp) return;

    if(SCALEMODE_STRETCH == bp->scaleMode) return;

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    GLState::pop().apply();

    LIBGUI_GL.glMatrixMode(GL_MODELVIEW);
    LIBGUI_GL.glPopMatrix();

    if(bp->flags & BPF_OVERDRAW_MASK)
    {
        // It shouldn't be necessary to bind the "not-texture" but the game
        // may have left whatever GL texture state it was using on. As this
        // isn't cleaned up until drawing control returns to the engine we
        // must explicitly disable it here.
        GL_SetNoTexture();
        LIBGUI_GL.glColor4f(0, 0, 0, 1);

        if(bp->isPillarBoxed)
        {
            // "Pillarbox":
            int w = int((bp->availWidth - bp->scaleFactor * bp->width) / 2 + .5f);
            GL_DrawRectf2(0, 0, w, bp->availHeight);
            GL_DrawRectf2(bp->availWidth - w, 0, w, bp->availHeight);
        }
        else
        {
            // "Letterbox":
            int h = int((bp->availHeight - bp->scaleFactor * 1.2f * bp->height) / 2 + .5f);
            GL_DrawRectf2(0, 0, bp->availWidth, h);
            GL_DrawRectf2(0, bp->availHeight - h, bp->availWidth, h);
        }
    }

    LIBGUI_GL.glMatrixMode(GL_PROJECTION);
    LIBGUI_GL.glPopMatrix();
}
