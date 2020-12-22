/** @file gl_draw.cpp  Basic (Generic) Drawing Routines.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/glstate.h>
#include <de/glinfo.h>
#include <de/vector.h>
#include <de/legacy/concurrency.h>
#include <doomsday/console/exec.h>

#include "gl/gl_main.h"
#include "gl/sys_opengl.h"

#include "world/p_players.h"  ///< @todo remove me

#include "api_render.h"
#include "render/viewports.h"

using namespace de;

void GL_DrawRectWithCoords(const Rectanglei &rect, Vec2i const coords[4])
{
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_Begin(DGL_QUADS);
        // Top left.
        if(coords) DGL_TexCoord2f(0, coords[0].x, coords[0].y);
        DGL_Vertex2f(rect.topLeft.x, rect.topLeft.y);

        // Top Right.
        if(coords) DGL_TexCoord2f(0, coords[1].x, coords[1].y);
        DGL_Vertex2f(rect.topRight().x, rect.topRight().y);

        // Bottom right.
        if(coords) DGL_TexCoord2f(0, coords[2].x, coords[2].y);
        DGL_Vertex2f(rect.bottomRight.x, rect.bottomRight.y);

        // Bottom left.
        if(coords) DGL_TexCoord2f(0, coords[3].x, coords[3].y);
        DGL_Vertex2f(rect.bottomLeft().x, rect.bottomLeft().y);
    DGL_End();
}

void GL_DrawRect(const Rectanglei &rect)
{
    Vec2i coords[4] = { Vec2i(0, 0), Vec2i(1, 0),
                           Vec2i(1, 1), Vec2i(0, 1) };
    GL_DrawRectWithCoords(rect, coords);
}

void GL_DrawRect2(int x, int y, int w, int h)
{
    GL_DrawRect(Rectanglei::fromSize(Vec2i(x, y), Vec2ui(w, h)));
}

void GL_DrawRectfWithCoords(const RectRawf* rect, Point2Rawf coords[4])
{
    if(!rect) return;

    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_Begin(DGL_QUADS);
        // Upper left.
        if(coords) DGL_TexCoord2f(0, coords[0].x, coords[0].y);
        DGL_Vertex2f(rect->origin.x, rect->origin.y);

        // Upper Right.
        if(coords) DGL_TexCoord2f(0, coords[1].x, coords[1].y);
        DGL_Vertex2f(rect->origin.x + rect->size.width, rect->origin.y);

        // Lower right.
        if(coords) DGL_TexCoord2f(0, coords[2].x, coords[2].y);
        DGL_Vertex2f(rect->origin.x + rect->size.width, rect->origin.y + rect->size.height);

        // Lower left.
        if(coords) DGL_TexCoord2f(0, coords[3].x, coords[3].y);
        DGL_Vertex2f(rect->origin.x, rect->origin.y + rect->size.height);
    DGL_End();
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
    DGL_Color4f(r, g, b, a);
    GL_DrawRectf2(x, y, w, h);
}

void GL_DrawRectf2TextureColor(double x, double y, double width, double height,
    int texW, int texH, const float topColor[3], float topAlpha,
    const float bottomColor[3], float bottomAlpha)
{
    if(topAlpha <= 0 && bottomAlpha <= 0) return;

    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_Begin(DGL_QUADS);
        // Top color.
        DGL_Color4f(topColor[0], topColor[1], topColor[2], topAlpha);
        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, width / (float) texW, 0);
        DGL_Vertex2f(x + width, y);

        // Bottom color.
        DGL_Color4f(bottomColor[0], bottomColor[1], bottomColor[2], bottomAlpha);
        DGL_TexCoord2f(0, width / (float) texW, height / (float) texH);
        DGL_Vertex2f(x + width, y + height);
        DGL_TexCoord2f(0, 0, height / (float) texH);
        DGL_Vertex2f(x, y + height);
    DGL_End();
}

void GL_DrawRectf2Tiled(double x, double y, double w, double h, int tw, int th)
{
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(0, w / (float) tw, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(0, w / (float) tw, h / (float) th);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 0, h / (float) th);
        DGL_Vertex2f(x, y + h);
    DGL_End();
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

    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_Begin(DGL_QUADS);
    if(toph > 0)
    {
        // The top rectangle.
        DGL_TexCoord2f(0, txo, tyo);
        DGL_Vertex2f(rect->origin.x, rect->origin.y);
        DGL_TexCoord2f(0, txo + (rect->size.width / ftw), tyo);
        DGL_Vertex2f(rect->origin.x + rect->size.width, rect->origin.y);
        DGL_TexCoord2f(0, txo + (rect->size.width / ftw), tyo + (toph / fth));
        DGL_Vertex2f(rect->origin.x + rect->size.width, rect->origin.y + toph);
        DGL_TexCoord2f(0, txo, tyo + (toph / fth));
        DGL_Vertex2f(rect->origin.x, rect->origin.y + toph);
    }

    if(lefth > 0 && sideh > 0)
    {
        float yoff = toph / fth;

        // The left rectangle.
        DGL_TexCoord2f(0, txo, yoff + tyo);
        DGL_Vertex2f(rect->origin.x, rect->origin.y + toph);
        DGL_TexCoord2f(0, txo + (lefth / ftw), yoff + tyo);
        DGL_Vertex2f(rect->origin.x + lefth, rect->origin.y + toph);
        DGL_TexCoord2f(0, txo + (lefth / ftw), yoff + tyo + sideh / fth);
        DGL_Vertex2f(rect->origin.x + lefth, rect->origin.y + toph + sideh);
        DGL_TexCoord2f(0, txo, yoff + tyo + sideh / fth);
        DGL_Vertex2f(rect->origin.x, rect->origin.y + toph + sideh);
    }

    if(righth > 0 && sideh > 0)
    {
        float ox = rect->origin.x + lefth + cutRect->size.width;
        float xoff = (lefth + cutRect->size.width) / ftw;
        float yoff = toph / fth;

        // The left rectangle.
        DGL_TexCoord2f(0, xoff + txo, yoff + tyo);
        DGL_Vertex2f(ox, rect->origin.y + toph);
        DGL_TexCoord2f(0, xoff + txo + righth / ftw, yoff + tyo);
        DGL_Vertex2f(ox + righth, rect->origin.y + toph);
        DGL_TexCoord2f(0, xoff + txo + righth / ftw, yoff + tyo + sideh / fth);
        DGL_Vertex2f(ox + righth, rect->origin.y + toph + sideh);
        DGL_TexCoord2f(0, xoff + txo, yoff + tyo + sideh / fth);
        DGL_Vertex2f(ox, rect->origin.y + toph + sideh);
    }

    if(bottomh > 0)
    {
        float oy = rect->origin.y + toph + sideh;
        float yoff = (toph + sideh) / fth;

        DGL_TexCoord2f(0, txo, yoff + tyo);
        DGL_Vertex2f(rect->origin.x, oy);
        DGL_TexCoord2f(0, txo + rect->size.width / ftw, yoff + tyo);
        DGL_Vertex2f(rect->origin.x + rect->size.width, oy);
        DGL_TexCoord2f(0, txo + rect->size.width / ftw, yoff + tyo + bottomh / fth);
        DGL_Vertex2f(rect->origin.x + rect->size.width, oy + bottomh);
        DGL_TexCoord2f(0, txo, yoff + tyo + bottomh / fth);
        DGL_Vertex2f(rect->origin.x, oy + bottomh);
    }
    DGL_End();
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
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_Color4f(r, g, b, a);
    DGL_Begin(DGL_LINES);
        DGL_Vertex2f(x1, y1);
        DGL_Vertex2f(x2, y2);
    DGL_End();
}

#undef GL_ResetViewEffects
DE_EXTERN_C void GL_ResetViewEffects()
{
    GL_SetFilter(false);
    Con_Executef(CMDS_DDAY, true, "postfx %i none", consolePlayer);
    DD_SetInteger(DD_RENDER_FULLBRIGHT, false);
}

#undef GL_ConfigureBorderedProjection2
DE_EXTERN_C void GL_ConfigureBorderedProjection2(dgl_borderedprojectionstate_t* bp, int flags,
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
DE_EXTERN_C void GL_ConfigureBorderedProjection(dgl_borderedprojectionstate_t* bp, int flags,
    int width, int height, int availWidth, int availHeight, scalemode_t overrideMode)
{
    GL_ConfigureBorderedProjection2(bp, flags, width, height, availWidth,
        availHeight, overrideMode, DEFAULT_SCALEMODE_STRETCH_EPSILON);
}

#undef GL_BeginBorderedProjection
DE_EXTERN_C void GL_BeginBorderedProjection(dgl_borderedprojectionstate_t* bp)
{
    DE_ASSERT(bp != 0);
    if (!bp) return;

    if (bp->scaleMode == SCALEMODE_STRETCH) return;

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    /**
     * Use an orthographic projection in screenspace, translating and
     * scaling the coordinate space using the modelview matrix producing
     * an aspect-corrected space of availWidth x availHeight and centered
     * on the larger of the horizontal and vertical axes.
     */
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, bp->availWidth, bp->availHeight, -1, 1);

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();

    DGL_PushState();

    if (bp->isPillarBoxed)
    {
        // "Pillarbox":
        int offset = int((bp->availWidth - bp->scaleFactor * bp->width) / 2 + .5f);
        if (bp->flags & BPF_OVERDRAW_CLIP)
        {
            DGL_SetScissor2(offset, 0,
                            int(bp->scaleFactor * bp->width), bp->availHeight);
        }

        DGL_Translatef(offset, 0, 0);
        DGL_Scalef(bp->scaleFactor, bp->scaleFactor * 1.2f, 1);
    }
    else
    {
        // "Letterbox":
        int offset = int((bp->availHeight - bp->scaleFactor * 1.2f * bp->height) / 2 + .5f);
        if (bp->flags & BPF_OVERDRAW_CLIP)
        {
            DGL_SetScissor2(0, offset,
                            bp->availWidth, int(bp->scaleFactor * 1.2f * bp->height));
        }

        DGL_Translatef(0, offset, 0);
        DGL_Scalef(bp->scaleFactor, bp->scaleFactor * 1.2f, 1);
    }
}

#undef GL_EndBorderedProjection
DE_EXTERN_C void GL_EndBorderedProjection(dgl_borderedprojectionstate_t* bp)
{
    DE_ASSERT(bp != 0);
    if(!bp) return;

    if (SCALEMODE_STRETCH == bp->scaleMode) return;

    DE_ASSERT_IN_RENDER_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    DGL_PopState();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();

    if (bp->flags & BPF_OVERDRAW_MASK)
    {
        // It shouldn't be necessary to bind the "not-texture" but the game
        // may have left whatever GL texture state it was using on. As this
        // isn't cleaned up until drawing control returns to the engine we
        // must explicitly disable it here.
        GL_SetNoTexture();
        DGL_Color4f(0, 0, 0, 1);

        if (bp->isPillarBoxed)
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

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();
}
