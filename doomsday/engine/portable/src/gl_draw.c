/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2008 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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
 * gl_draw.c: Basic (Generic) Drawing Routines
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_dgl.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int curfilter = 0;       // The current filter (0 = none).
static boolean usePatchOffset = true;   // A bit of a hack...

// CODE --------------------------------------------------------------------

void GL_UsePatchOffset(boolean enable)
{
    usePatchOffset = enable;
}

void GL_DrawRawScreen_CS(lumpnum_t lump, float offx, float offy,
                         float scalex, float scaley)
{
    boolean             isTwoPart;
    float               pixelBorder = 0;
    float               tcb = 0;
    rawtex_t           *raw;

    if(lump < 0 || lump >= numLumps)
        return;

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PushMatrix();
    DGL_LoadIdentity();

    // Setup offset and scale.
    // Scale the offsets to match the resolution.
    DGL_Translatef(offx * theWindow->width / 320.0f, offy * theWindow->height / 200.0f,
                  0);
    DGL_Scalef(scalex, scaley, 1);

    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PushMatrix();
    DGL_LoadIdentity();
    DGL_Ortho(0, 0, theWindow->width, theWindow->height, -1, 1);

    GL_SetRawImage(lump, false, DGL_CLAMP, DGL_CLAMP);
    raw = R_GetRawTex(lump);
    isTwoPart = (raw->tex2 != 0);

    if(isTwoPart)
    {
        tcb = raw->height / 256.0f;
    }
    else
    {
        // Bottom texture coordinate.
        tcb = 1;
    }
    pixelBorder = raw->width * theWindow->width / 320;

    // The first part is rendered in any case.
    DGL_Begin(DGL_QUADS);
    DGL_TexCoord2f(0, 0);
    DGL_Vertex2f(0, 0);
    DGL_TexCoord2f(1, 0);
    DGL_Vertex2f(pixelBorder, 0);
    DGL_TexCoord2f(1, tcb);
    DGL_Vertex2f(pixelBorder, theWindow->height);
    DGL_TexCoord2f(0, tcb);
    DGL_Vertex2f(0, theWindow->height);
    DGL_End();

    if(isTwoPart)
    {
        // And the other part.
        GL_SetRawImage(lump, true, DGL_CLAMP, DGL_CLAMP);
        DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0);
        DGL_Vertex2f(pixelBorder, 0);
        DGL_TexCoord2f(1, 0);
        DGL_Vertex2f(theWindow->width, 0);
        DGL_TexCoord2f(1, tcb);
        DGL_Vertex2f(theWindow->width, theWindow->height);
        DGL_TexCoord2f(0, tcb);
        DGL_Vertex2f(pixelBorder, theWindow->height);
        DGL_End();
    }

    // Restore the old projection matrix.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_PopMatrix();

    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_PopMatrix();
}

/**
 * Raw screens are 320 x 200.
 */
void GL_DrawRawScreen(lumpnum_t lump, float offx, float offy)
{
    DGL_Color3f(1, 1, 1);
    GL_DrawRawScreen_CS(lump, offx, offy, 1, 1);
}

/**
 * Drawing with the Current State.
 */
void GL_DrawPatch_CS(int posX, int posY, lumpnum_t lump)
{
    float               x = posX;
    float               y = posY;
    float               w, h;
    patchtex_t*         p = R_GetPatchTex(lump);

    if(!p)
        return;

    // Set the texture.
    GL_BindTexture(GL_PreparePatch(p->lump), glmode[texMagMode]);

    w = (float) p->width;
    h = (float) p->height;

    if(usePatchOffset)
    {
        x += (float) p->offX;
        y += (float) p->offY;
    }

    if(p->extraOffset[VX])
    {
        // This offset is used only for the extra borders in the
        // "upscaled and sharpened" patches, so we can tweak the values
        // to our liking a bit more.
        x += p->extraOffset[VX] * .75f;
        y += p->extraOffset[VY] * .75f;
        w -= fabs(p->extraOffset[VX]) / 2;
        h -= fabs(p->extraOffset[VY]) / 2;
    }

    DGL_Begin(DGL_QUADS);
    DGL_TexCoord2f(0, 0);
    DGL_Vertex2f(x, y);
    DGL_TexCoord2f(1, 0);
    DGL_Vertex2f(x + w, y);
    DGL_TexCoord2f(1, 1);
    DGL_Vertex2f(x + w, y + h);
    DGL_TexCoord2f(0, 1);
    DGL_Vertex2f(x, y + h);
    DGL_End();

    // Is there a second part?
    if(GL_GetPatchOtherPart(lump))
    {
        x += (float) p->width2;

        GL_BindTexture(GL_GetPatchOtherPart(lump), glmode[texMagMode]);
        w = (float) p->width2;

        DGL_Begin(DGL_QUADS);
        DGL_TexCoord2f(0, 0);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(1, 0);
        DGL_Vertex2f(x + w, y);
        DGL_TexCoord2f(1, 1);
        DGL_Vertex2f(x + w, y + h);
        DGL_TexCoord2f(0, 1);
        DGL_Vertex2f(x, y + h);
        DGL_End();
    }
}

void GL_DrawPatchLitAlpha(int x, int y, float light, float alpha,
                          lumpnum_t lump)
{
    DGL_Color4f(light, light, light, alpha);
    GL_DrawPatch_CS(x, y, lump);
}

void GL_DrawPatch(int x, int y, lumpnum_t lump)
{
    if(lump < 0)
        return;
    GL_DrawPatchLitAlpha(x, y, 1, 1, lump);
}

void GL_DrawFuzzPatch(int x, int y, lumpnum_t lump)
{
    if(lump < 0)
        return;
    GL_DrawPatchLitAlpha(x, y, 1, .333f, lump);
}

void GL_DrawAltFuzzPatch(int x, int y, lumpnum_t lump)
{
    if(lump < 0)
        return;
    GL_DrawPatchLitAlpha(x, y, 1, .666f, lump);
}

void GL_DrawShadowedPatch(int x, int y, lumpnum_t lump)
{
    if(lump < 0)
        return;
    GL_DrawPatchLitAlpha(x + 2, y + 2, 0, .4f, lump);
    GL_DrawPatchLitAlpha(x, y, 1, 1, lump);
}

void GL_DrawRect(float x, float y, float w, float h, float r, float g,
                 float b, float a)
{
    DGL_Color4f(r, g, b, a);
    DGL_Begin(DGL_QUADS);
    DGL_TexCoord2f(0, 0);
    DGL_Vertex2f(x, y);
    DGL_TexCoord2f(1, 0);
    DGL_Vertex2f(x + w, y);
    DGL_TexCoord2f(1, 1);
    DGL_Vertex2f(x + w, y + h);
    DGL_TexCoord2f(0, 1);
    DGL_Vertex2f(x, y + h);
    DGL_End();
}

void GL_DrawRectTiled(int x, int y, int w, int h, int tw, int th)
{
    // Make sure the current texture will be tiled.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    DGL_Begin(DGL_QUADS);
    DGL_TexCoord2f(0, 0);
    DGL_Vertex2f(x, y);
    DGL_TexCoord2f(w / (float) tw, 0);
    DGL_Vertex2f(x + w, y);
    DGL_TexCoord2f(w / (float) tw, h / (float) th);
    DGL_Vertex2f(x + w, y + h);
    DGL_TexCoord2f(0, h / (float) th);
    DGL_Vertex2f(x, y + h);
    DGL_End();
}

/**
 * The cut rectangle must be inside the other one.
 */
void GL_DrawCutRectTiled(int x, int y, int w, int h, int tw, int th,
                         int txoff, int tyoff, int cx, int cy, int cw,
                         int ch)
{
    float               ftw = tw, fth = th;
    float               txo = (1.0f / (float)tw) * (float)txoff;
    float               tyo = (1.0f / (float)th) * (float)tyoff;

    // We'll draw at max four rectangles.
    int     toph = cy - y, bottomh = y + h - (cy + ch), sideh =
        h - toph - bottomh, lefth = cx - x, righth = x + w - (cx + cw);

    DGL_Begin(DGL_QUADS);
    if(toph > 0)
    {
        // The top rectangle.
        DGL_TexCoord2f(txo, tyo);
        DGL_Vertex2f(x, y);
        DGL_TexCoord2f(txo + (w / ftw), tyo);
        DGL_Vertex2f(x + w, y );
        DGL_TexCoord2f(txo + (w / ftw), tyo + (toph / fth));
        DGL_Vertex2f(x + w, y + toph);
        DGL_TexCoord2f(txo, tyo + (toph / fth));
        DGL_Vertex2f(x, y + toph);
    }

    if(lefth > 0 && sideh > 0)
    {
        float               yoff = toph / fth;

        // The left rectangle.
        DGL_TexCoord2f(txo, yoff + tyo);
        DGL_Vertex2f(x, y + toph);
        DGL_TexCoord2f(txo + (lefth / ftw), yoff + tyo);
        DGL_Vertex2f(x + lefth, y + toph);
        DGL_TexCoord2f(txo + (lefth / ftw), yoff + tyo + sideh / fth);
        DGL_Vertex2f(x + lefth, y + toph + sideh);
        DGL_TexCoord2f(txo, yoff + tyo + sideh / fth);
        DGL_Vertex2f(x, y + toph + sideh);
    }

    if(righth > 0 && sideh > 0)
    {
        int                 ox = x + lefth + cw;
        float               xoff = (lefth + cw) / ftw;
        float               yoff = toph / fth;

        // The left rectangle.
        DGL_TexCoord2f(xoff + txo, yoff + tyo);
        DGL_Vertex2f(ox, y + toph);
        DGL_TexCoord2f(xoff + txo + righth / ftw, yoff + tyo);
        DGL_Vertex2f(ox + righth, y + toph);
        DGL_TexCoord2f(xoff + txo + righth / ftw, yoff + tyo + sideh / fth);
        DGL_Vertex2f(ox + righth, y + toph + sideh);
        DGL_TexCoord2f(xoff + txo, yoff + tyo + sideh / fth);
        DGL_Vertex2f(ox, y + toph + sideh);
    }

    if(bottomh > 0)
    {
        int                 oy = y + toph + sideh;
        float               yoff = (toph + sideh) / fth;

        DGL_TexCoord2f(txo, yoff + tyo);
        DGL_Vertex2f(x, oy);
        DGL_TexCoord2f(txo + w / ftw, yoff + tyo);
        DGL_Vertex2f(x + w, oy);
        DGL_TexCoord2f(txo + w / ftw, yoff + tyo + bottomh / fth);
        DGL_Vertex2f(x + w, oy + bottomh);
        DGL_TexCoord2f(txo, yoff + tyo + bottomh / fth);
        DGL_Vertex2f(x, oy + bottomh);
    }
    DGL_End();
}

/**
 * Totally inefficient for a large number of lines.
 */
void GL_DrawLine(float x1, float y1, float x2, float y2, float r, float g,
                 float b, float a)
{
    DGL_Color4f(r, g, b, a);
    DGL_Begin(DGL_LINES);
    DGL_Vertex2f(x1, y1);
    DGL_Vertex2f(x2, y2);
    DGL_End();
}

void GL_SetFilter(int filterRGBA)
{
    curfilter = filterRGBA;
}

/**
 * @return              Non-zero if the filter was drawn.
 */
int GL_DrawFilter(void)
{
    if(!curfilter)
        return 0; // No filter needed.

    // No texture, please.
    DGL_Disable(DGL_TEXTURING);

    DGL_Color4ub(curfilter & 0xff, (curfilter >> 8) & 0xff,
                (curfilter >> 16) & 0xff, (curfilter >> 24) & 0xff);

    DGL_Begin(DGL_QUADS);
    DGL_Vertex2f(viewwindowx, viewwindowy);
    DGL_Vertex2f(viewwindowx + viewwidth, viewwindowy);
    DGL_Vertex2f(viewwindowx + viewwidth, viewwindowy + viewheight);
    DGL_Vertex2f(viewwindowx, viewwindowy + viewheight);
    DGL_End();

    DGL_Enable(DGL_TEXTURING);
    return 1;
}
