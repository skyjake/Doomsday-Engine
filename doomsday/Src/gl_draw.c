/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

/*
 * gl_draw.c: Basic (Generic) Drawing Routines
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>

#include "de_base.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"

// MACROS ------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static int curfilter = 0;	// The current filter (0 = none).
static boolean usePatchOffset = true; // A bit of a hack...

// CODE --------------------------------------------------------------------

void GL_UsePatchOffset(boolean enable)
{
	usePatchOffset = enable;
}

void GL_DrawRawScreen_CS
	(int lump, float offx, float offy, float scalex, float scaley)	
{
	boolean isTwoPart;
	int pixelBorder = 0;
	float tcb = 0;

	if(lump < 0 || lump >= numlumps) return;

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PushMatrix();
	gl.LoadIdentity();

	// Setup offset and scale.
	// Scale the offsets to match the resolution.
	gl.Translatef(offx * screenWidth/320.0f, offy * screenHeight/200.0f, 0);
	gl.Scalef(scalex, scaley, 1);

	gl.MatrixMode(DGL_PROJECTION);
	gl.PushMatrix();
	gl.LoadIdentity();
	gl.Ortho(0, 0, screenWidth, screenHeight, -1, 1);

	GL_SetRawImage(lump, 1);
	isTwoPart = (lumptexinfo[lump].tex[1] != 0);

	if(isTwoPart)
	{
		tcb = lumptexinfo[lump].height / 256.0f;
	}
	else
	{
		// Bottom texture coordinate.
		tcb = 1;
	}
	pixelBorder = lumptexinfo[lump].width[0] * screenWidth / 320;

	// The first part is rendered in any case.
	gl.Begin(DGL_QUADS);
	gl.TexCoord2f(0, 0);
	gl.Vertex2f(0, 0);
	gl.TexCoord2f(1, 0);
	gl.Vertex2f(pixelBorder, 0);
	gl.TexCoord2f(1, tcb);
	gl.Vertex2f(pixelBorder, screenHeight);
	gl.TexCoord2f(0, tcb);
	gl.Vertex2f(0, screenHeight);
	gl.End();

	if(isTwoPart)
	{
		// And the other part.
		GL_SetRawImage(lump, 2);
		gl.Begin(DGL_QUADS);
		gl.TexCoord2f(0, 0);
		gl.Vertex2f(pixelBorder - 1, 0);
		gl.TexCoord2f(1, 0);
		gl.Vertex2f(screenWidth, 0);
		gl.TexCoord2f(1, tcb);
		gl.Vertex2f(screenWidth, screenHeight);
		gl.TexCoord2f(0, tcb);
		gl.Vertex2f(pixelBorder - 1, screenHeight);
		gl.End();
	}

	// Restore the old projection matrix.
	gl.PopMatrix();

	gl.MatrixMode(DGL_MODELVIEW);
	gl.PopMatrix();
}

//===========================================================================
// GL_DrawRawScreen
//	Raw screens are 320 x 200.
//===========================================================================
void GL_DrawRawScreen(int lump, float offx, float offy)	
{
	gl.Color3f(1, 1, 1);
	GL_DrawRawScreen_CS(lump, offx, offy, 1, 1);
}

//===========================================================================
// GL_DrawPatch_CS
//	Drawing with the Current State.
//===========================================================================
void GL_DrawPatch_CS(int x, int y, int lumpnum)
{
	int		w, h;

	// Set the texture.
	GL_SetPatch(lumpnum);

	w = lumptexinfo[lumpnum].width[0];
	h = lumptexinfo[lumpnum].height;
	if(usePatchOffset)
	{
		x += lumptexinfo[lumpnum].offx;
		y += lumptexinfo[lumpnum].offy;
	}

	gl.Begin(DGL_QUADS);
	gl.TexCoord2f(0, 0);
	gl.Vertex2f(x, y);
	gl.TexCoord2f(1, 0);
	gl.Vertex2f(x+w, y);
	gl.TexCoord2f(1, 1);
	gl.Vertex2f(x+w, y+h);
	gl.TexCoord2f(0, 1);
	gl.Vertex2f(x, y+h);
	gl.End();

	// Is there a second part?
	if(GL_GetOtherPart(lumpnum))
	{
		x += w;

		GL_BindTexture(GL_GetOtherPart(lumpnum));
		w = lumptexinfo[lumpnum].width[1];

		gl.Begin(DGL_QUADS);
		gl.TexCoord2f(0, 0);
		gl.Vertex2f(x, y);
		gl.TexCoord2f(1, 0);
		gl.Vertex2f(x+w, y);
		gl.TexCoord2f(1, 1);
		gl.Vertex2f(x+w, y+h);
		gl.TexCoord2f(0, 1);
		gl.Vertex2f(x, y+h);
		gl.End();
	}
}

void GL_DrawPatchLitAlpha(int x, int y, float light, float alpha, int lumpnum)
{
	gl.Color4f(light, light, light, alpha);
	GL_DrawPatch_CS(x, y, lumpnum);
}

void GL_DrawPatch(int x, int y, int lumpnum)
{
	if(lumpnum < 0) return;
	GL_DrawPatchLitAlpha(x, y, 1, 1, lumpnum);
}

void GL_DrawFuzzPatch(int x, int y, int lumpnum)
{
	if(lumpnum < 0) return;
	GL_DrawPatchLitAlpha(x, y, 1, .333f, lumpnum);
}

void GL_DrawAltFuzzPatch(int x, int y, int lumpnum)
{
	if(lumpnum < 0) return;
	GL_DrawPatchLitAlpha(x, y, 1, .666f, lumpnum);
}

void GL_DrawShadowedPatch(int x, int y, int lumpnum)
{
	if(lumpnum < 0) return;
	GL_DrawPatchLitAlpha(x+2, y+2, 0, .4f, lumpnum);
	GL_DrawPatchLitAlpha(x, y, 1, 1, lumpnum);
}

void GL_DrawRect(float x, float y, float w, float h, float r, float g, float b, float a)
{
	gl.Color4f(r, g, b, a);
	gl.Begin(DGL_QUADS);
	gl.TexCoord2f(0, 0);
	gl.Vertex2f(x, y);
	gl.TexCoord2f(1, 0);
	gl.Vertex2f(x+w, y);
	gl.TexCoord2f(1, 1);
	gl.Vertex2f(x+w, y+h);
	gl.TexCoord2f(0, 1);
	gl.Vertex2f(x, y+h);
	gl.End();
}

void GL_DrawRectTiled(int x, int y, int w, int h, int tw, int th)
{
	// Make sure the current texture will be tiled.
	gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
	gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);

	gl.Begin(DGL_QUADS);
	gl.TexCoord2f(0, 0);
	gl.Vertex2f(x, y);
	gl.TexCoord2f(w/(float)tw, 0);
	gl.Vertex2f(x+w, y);
	gl.TexCoord2f(w/(float)tw, h/(float)th);
	gl.Vertex2f(x+w, y+h);
	gl.TexCoord2f(0, h/(float)th);
	gl.Vertex2f(x, y+h);
	gl.End();
}

// The cut rectangle must be inside the other one.
void GL_DrawCutRectTiled(int x, int y, int w, int h, int tw, int th, 
						  int cx, int cy, int cw, int ch)
{
	float ftw = tw, fth = th;
	// We'll draw at max four rectangles.
	int	toph = cy-y, bottomh = y+h-(cy+ch), sideh = h-toph-bottomh,
		lefth = cx-x, righth = x+w-(cx+cw);
	
	gl.Begin(DGL_QUADS);
	if(toph > 0)
	{
		// The top rectangle.
		gl.TexCoord2f(0, 0);
		gl.Vertex2f(x, y);
		gl.TexCoord2f(w/ftw, 0);
		gl.Vertex2f(x+w, y);
		gl.TexCoord2f(w/ftw, toph/fth);
		gl.Vertex2f(x+w, y+toph);
		gl.TexCoord2f(0, toph/fth);
		gl.Vertex2f(x, y+toph);
	}
	if(lefth > 0 && sideh > 0)
	{
		float yoff = toph/fth;
		// The left rectangle.
		gl.TexCoord2f(0, yoff);
		gl.Vertex2f(x, y+toph);
		gl.TexCoord2f(lefth/ftw, yoff);
		gl.Vertex2f(x+lefth, y+toph);
		gl.TexCoord2f(lefth/ftw, yoff+sideh/fth);
		gl.Vertex2f(x+lefth, y+toph+sideh);
		gl.TexCoord2f(0, yoff+sideh/fth);
		gl.Vertex2f(x, y+toph+sideh);
	}
	if(righth > 0 && sideh > 0)
	{
		int ox = x+lefth+cw;
		float xoff = (lefth+cw)/ftw;
		float yoff = toph/fth;
		// The left rectangle.
		gl.TexCoord2f(xoff, yoff);
		gl.Vertex2f(ox, y+toph);
		gl.TexCoord2f(xoff+righth/ftw, yoff);
		gl.Vertex2f(ox+righth, y+toph);
		gl.TexCoord2f(xoff+righth/ftw, yoff+sideh/fth);
		gl.Vertex2f(ox+righth, y+toph+sideh);
		gl.TexCoord2f(xoff, yoff+sideh/fth);
		gl.Vertex2f(ox, y+toph+sideh);
	}
	if(bottomh > 0)
	{
		int oy = y+toph+sideh;
		float yoff = (toph+sideh)/fth;
		gl.TexCoord2f(0, yoff);
		gl.Vertex2f(x, oy);
		gl.TexCoord2f(w/ftw, yoff);
		gl.Vertex2f(x+w, oy);
		gl.TexCoord2f(w/ftw, yoff+bottomh/fth);
		gl.Vertex2f(x+w, oy+bottomh);
		gl.TexCoord2f(0, yoff+bottomh/fth);
		gl.Vertex2f(x, oy+bottomh);
	}
	gl.End();
}

// Totally inefficient for a large number of lines.
void GL_DrawLine(float x1, float y1, float x2, float y2, 
				  float r, float g, float b, float a)
{
	gl.Color4f(r, g, b, a);
	gl.Begin(DGL_LINES);
	gl.Vertex2f(x1, y1);
	gl.Vertex2f(x2, y2);
	gl.End();
}

void GL_SetColor(int palidx)
{
	GL_SetColor2(palidx, 1);
}

void GL_SetColor2(int palidx, float alpha)
{
	byte rgb[4];
	
	if(palidx == -1)	// Invisible?
	{
		gl.Color4f(0, 0, 0, 0);
	}
	else
	{
		PalIdxToRGB(W_CacheLumpNum(pallump, PU_CACHE), palidx, rgb);
		if(alpha < 0) alpha = 0;
		if(alpha > 1) alpha = 1;
		rgb[3] = alpha * 255;
		gl.Color4ubv(rgb);
	}
}

void GL_SetColorAndAlpha(float r, float g, float b, float a)
{
	gl.Color4f(r, g, b, a);
}

void GL_SetFilter(int filterRGBA)
{
	curfilter = filterRGBA;	
}

// Returns nonzero if the filter was drawn.
int GL_DrawFilter()
{
	if(!curfilter) return 0;		// No filter needed.

	// No texture, please.
	gl.Disable(DGL_TEXTURING);

	gl.Color4ub(curfilter&0xff, (curfilter>>8)&0xff, (curfilter>>16)&0xff, 
		(curfilter>>24)&0xff);
	
	gl.Begin(DGL_QUADS);
	gl.Vertex2f(0, 0);
	gl.Vertex2f(320, 0);
	gl.Vertex2f(320, 200);
	gl.Vertex2f(0, 200);
	gl.End();

	gl.Enable(DGL_TEXTURING);
	return 1;
}

//===========================================================================
// GL_DrawPSprite
//===========================================================================
void GL_DrawPSprite(float x, float y, float scale, int flip, int lump)
{
	int	w, h;
	int w2, h2;
	float s, t;
	int pSprMode = 1;
	spritelump_t *slump = spritelumps + lump;

	if(flip) flip = 1; // Make sure it's zero or one.

	GL_SetSprite(lump, pSprMode);
	w = slump->width;
	h = slump->height;
	w2 = CeilPow2(w);
	h2 = CeilPow2(h);

	// Let's calculate texture coordinates.
	// To remove a possible edge artifact, move the corner a bit up/left.
	s = slump->tc[pSprMode][VX] - 0.4f/w2;
	t = slump->tc[pSprMode][VY] - 0.4f/h2;

	gl.Begin(DGL_QUADS);
	
	gl.TexCoord2f(flip * s, 0);
	gl.Vertex2f(x, y);

	gl.TexCoord2f(!flip * s, 0);
	gl.Vertex2f(x + w*scale, y);
	
	gl.TexCoord2f(!flip * s, t);
	gl.Vertex2f(x + w*scale, y + h*scale);
	
	gl.TexCoord2f(flip * s, t);
	gl.Vertex2f(x, y+h*scale);

	gl.End();
}

