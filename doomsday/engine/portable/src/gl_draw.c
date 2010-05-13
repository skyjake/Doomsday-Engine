/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2010 Daniel Swanson <danij@dengine.net>
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

static boolean drawFilter = false;
static float filterColor[4] = { 0, 0, 0, 0 };
static boolean usePatchOffset = true;   // A bit of a hack...

// CODE --------------------------------------------------------------------

void GL_UsePatchOffset(boolean enable)
{
    usePatchOffset = enable;
}

/**
 * \todo No need for this special method now. Refactor callers to use the
 * normal DGL drawing methods.
 */
void GL_DrawRawScreen_CS(lumpnum_t lump, float offx, float offy,
                         float scalex, float scaley)
{
    float pixelBorder = 0;
    rawtex_t* raw;

    if(lump < 0 || lump >= numLumps)
        return;

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Setup offset and scale.
    // Scale the offsets to match the resolution.
    glTranslatef(offx * theWindow->width / 320.0f,
                 offy * theWindow->height / 200.0f, 0);
    glScalef(scalex, scaley, 1);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, theWindow->width, theWindow->height, 0, -1, 1);

    GL_SetRawImage(lump, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    raw = R_GetRawTex(lump);
    // Bottom texture coordinate.
    pixelBorder = raw->width * theWindow->width / 320;

    // The first part is rendered in any case.
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(0, 0);
        glTexCoord2f(1, 0);
        glVertex2f(pixelBorder, 0);
        glTexCoord2f(1, 1);
        glVertex2f(pixelBorder, theWindow->height);
        glTexCoord2f(0, 1);
        glVertex2f(0, theWindow->height);
    glEnd();

    // Restore the old projection matrix.
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
}

/**
 * Raw screens are 320 x 200.
 */
void GL_DrawRawScreen(lumpnum_t lump, float offx, float offy)
{
    glColor3f(1, 1, 1);
    GL_DrawRawScreen_CS(lump, offx, offy, 1, 1);
}

/**
 * Drawing with the Current State.
 */
void GL_DrawPatch_CS(int posX, int posY, patchid_t num)
{
    float x = posX, y = posY, w, h;
    patchtex_t* p = R_FindPatchTex(num);

    // Set the texture.
    GL_BindTexture(GL_PreparePatch(p), glmode[texMagMode]);

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
        x += p->extraOffset[VX];
        y += p->extraOffset[VY];
        w += fabs(p->extraOffset[VX])*2;
        h += fabs(p->extraOffset[VY])*2;
    }

    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        glTexCoord2f(1, 0);
        glVertex2f(x + w, y);
        glTexCoord2f(1, 1);
        glVertex2f(x + w, y + h);
        glTexCoord2f(0, 1);
        glVertex2f(x, y + h);
    glEnd();
}

void GL_DrawPatchLitAlpha(int x, int y, float light, float alpha, patchid_t num)
{
    glColor4f(light, light, light, alpha);
    GL_DrawPatch_CS(x, y, num);
}

void GL_DrawPatch(int x, int y, patchid_t num)
{
    if(num < 0)
        return;
    GL_DrawPatchLitAlpha(x, y, 1, 1, num);
}

void GL_DrawFuzzPatch(int x, int y, patchid_t num)
{
    if(num < 0)
        return;
    GL_DrawPatchLitAlpha(x, y, 1, .333f, num);
}

void GL_DrawAltFuzzPatch(int x, int y, patchid_t num)
{
    if(num < 0)
        return;
    GL_DrawPatchLitAlpha(x, y, 1, .666f, num);
}

void GL_DrawShadowedPatch(int x, int y, patchid_t num)
{
    if(num < 0)
        return;
    GL_DrawPatchLitAlpha(x + 2, y + 2, 0, .4f, num);
    GL_DrawPatchLitAlpha(x, y, 1, 1, num);
}

void GL_DrawRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        glTexCoord2f(1, 0);
        glVertex2f(x + w, y);
        glTexCoord2f(1, 1);
        glVertex2f(x + w, y + h);
        glTexCoord2f(0, 1);
        glVertex2f(x, y + h);
    glEnd();
}

void GL_DrawRectTiled(float x, float y, float w, float h, int tw, int th)
{
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0);
        glVertex2f(x, y);
        glTexCoord2f(w / (float) tw, 0);
        glVertex2f(x + w, y);
        glTexCoord2f(w / (float) tw, h / (float) th);
        glVertex2f(x + w, y + h);
        glTexCoord2f(0, h / (float) th);
        glVertex2f(x, y + h);
    glEnd();
}

/**
 * The cut rectangle must be inside the other one.
 */
void GL_DrawCutRectTiled(float x, float y, float w, float h, int tw, int th,
                         int txoff, int tyoff, float cx, float cy, float cw,
                         float ch)
{
    float               ftw = tw, fth = th;
    float               txo = (1.0f / (float)tw) * (float)txoff;
    float               tyo = (1.0f / (float)th) * (float)tyoff;

    // We'll draw at max four rectangles.
    float     toph = cy - y, bottomh = y + h - (cy + ch), sideh =
        h - toph - bottomh, lefth = cx - x, righth = x + w - (cx + cw);

    glBegin(GL_QUADS);
    if(toph > 0)
    {
        // The top rectangle.
        glTexCoord2f(txo, tyo);
        glVertex2f(x, y);
        glTexCoord2f(txo + (w / ftw), tyo);
        glVertex2f(x + w, y );
        glTexCoord2f(txo + (w / ftw), tyo + (toph / fth));
        glVertex2f(x + w, y + toph);
        glTexCoord2f(txo, tyo + (toph / fth));
        glVertex2f(x, y + toph);
    }

    if(lefth > 0 && sideh > 0)
    {
        float               yoff = toph / fth;

        // The left rectangle.
        glTexCoord2f(txo, yoff + tyo);
        glVertex2f(x, y + toph);
        glTexCoord2f(txo + (lefth / ftw), yoff + tyo);
        glVertex2f(x + lefth, y + toph);
        glTexCoord2f(txo + (lefth / ftw), yoff + tyo + sideh / fth);
        glVertex2f(x + lefth, y + toph + sideh);
        glTexCoord2f(txo, yoff + tyo + sideh / fth);
        glVertex2f(x, y + toph + sideh);
    }

    if(righth > 0 && sideh > 0)
    {
        float               ox = x + lefth + cw;
        float               xoff = (lefth + cw) / ftw;
        float               yoff = toph / fth;

        // The left rectangle.
        glTexCoord2f(xoff + txo, yoff + tyo);
        glVertex2f(ox, y + toph);
        glTexCoord2f(xoff + txo + righth / ftw, yoff + tyo);
        glVertex2f(ox + righth, y + toph);
        glTexCoord2f(xoff + txo + righth / ftw, yoff + tyo + sideh / fth);
        glVertex2f(ox + righth, y + toph + sideh);
        glTexCoord2f(xoff + txo, yoff + tyo + sideh / fth);
        glVertex2f(ox, y + toph + sideh);
    }

    if(bottomh > 0)
    {
        float               oy = y + toph + sideh;
        float               yoff = (toph + sideh) / fth;

        glTexCoord2f(txo, yoff + tyo);
        glVertex2f(x, oy);
        glTexCoord2f(txo + w / ftw, yoff + tyo);
        glVertex2f(x + w, oy);
        glTexCoord2f(txo + w / ftw, yoff + tyo + bottomh / fth);
        glVertex2f(x + w, oy + bottomh);
        glTexCoord2f(txo, yoff + tyo + bottomh / fth);
        glVertex2f(x, oy + bottomh);
    }
    glEnd();
}

/**
 * Totally inefficient for a large number of lines.
 */
void GL_DrawLine(float x1, float y1, float x2, float y2, float r, float g,
                 float b, float a)
{
    glColor4f(r, g, b, a);
    glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
    glEnd();
}

void GL_SetFilter(boolean enabled)
{
    drawFilter = enabled;
}

void GL_SetFilterColor(float r, float g, float b, float a)
{
    filterColor[CR] = MINMAX_OF(0, r, 1);
    filterColor[CG] = MINMAX_OF(0, g, 1);
    filterColor[CB] = MINMAX_OF(0, b, 1);
    filterColor[CA] = MINMAX_OF(0, a, 1);
}

/**
 * @return              Non-zero if the filter was drawn.
 */
int GL_DrawFilter(void)
{
    if(!drawFilter)
        return 0; // No filter needed.

    // No texture, please.
    glDisable(GL_TEXTURE_2D);

    glColor4fv(filterColor);

    glBegin(GL_QUADS);
        glVertex2f(viewwindowx, viewwindowy);
        glVertex2f(viewwindowx + viewwidth, viewwindowy);
        glVertex2f(viewwindowx + viewwidth, viewwindowy + viewheight);
        glVertex2f(viewwindowx, viewwindowy + viewheight);
    glEnd();

    glEnable(GL_TEXTURE_2D);
    return 1;
}
