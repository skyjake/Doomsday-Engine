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
 * Basic (Generic) Drawing Routines.
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

#include "sys_opengl.h"

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

void GL_DrawRect2(float x, float y, float width, float height, DGLuint tex,
    int texW, int texH, const float topColor[3], float topAlpha,
    const float bottomColor[3], float bottomAlpha)
{
    if(!(topAlpha > 0 || bottomAlpha > 0))
        return;

    if(tex)
    {
        glBindTexture(GL_TEXTURE_2D, tex);
        glEnable(GL_TEXTURE_2D);
    }

    if(tex || topAlpha < 1.0 || bottomAlpha < 1.0)
    {
        glEnable(GL_BLEND);
        GL_BlendMode(BM_NORMAL);
    }
    else
    {
        glDisable(GL_BLEND);
    }

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

    if(tex)
        glDisable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
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

    glColor4fv(filterColor);

    glBegin(GL_QUADS);
        glVertex2f(viewwindowx, viewwindowy);
        glVertex2f(viewwindowx + viewwidth, viewwindowy);
        glVertex2f(viewwindowx + viewwidth, viewwindowy + viewheight);
        glVertex2f(viewwindowx, viewwindowy + viewheight);
    glEnd();

    return 1;
}
