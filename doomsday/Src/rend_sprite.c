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
 * rend_sprite.c: Rendering Map Objects as 2D Sprites
 */

// HEADER FILES ------------------------------------------------------------

#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_play.h"
#include "de_graphics.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float maxSpriteAngle = 60;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// The colors to modify when sprites are lit.
static byte *slRGB1, *slRGB2; 
static fvertex_t slViewVec;
static vissprite_t *slSpr;

// CODE --------------------------------------------------------------------

//===========================================================================
// Rend_Draw3DPlayerSprites
//	Fog is turned off while rendering. It's not feasible to think that the
//	fog would obstruct the player's view of his own weapon.
//===========================================================================
void Rend_Draw3DPlayerSprites(void)
{
	int i;

	// Setup the modelview matrix.
	Rend_ModelViewMatrix(false);

	// Clear Z buffer. This will prevent the psprites from being clipped
	// by nearby polygons.
	gl.Clear(DGL_DEPTH_BUFFER_BIT);

	// Turn off fog.
	if(useFog) gl.Disable(DGL_FOG);

	for(i = 0; i < DDMAXPSPRITES; i++)
	{
		if(!vispsprites[i].type) continue; // Not used.
		Rend_RenderModel(vispsprites + i);
	}

	// Should we turn the fog back on?
	if(useFog) gl.Enable(DGL_FOG);
}

//===========================================================================
// Rend_DrawPlayerSprites
//	Draws 2D player sprites. If they were already drawn 3D, this 
//	won't do anything.
//===========================================================================
void Rend_DrawPlayerSprites(void)
{
	spriteinfo_t info[DDMAXPSPRITES];
	ddpsprite_t *psp;
	int i;
	sector_t *sec         = viewplayer->mo->subsector->sector;
	float offx            = pspOffX / 16.0f;
	float offy            = pspOffY / 16.0f;
	float offScaleY       = weaponOffsetScaleY / 1000.0f;
	byte somethingVisible = false;
	byte isFullBright     = (LevelFullBright != 0);

	// Cameramen have no psprites.
	if(viewplayer->flags & DDPF_CAMERA) return; 

	// Clear sprite info.
	memset(info, 0, sizeof(info));

	// Check for fullbright.
	for(i = 0, psp = viewplayer->psprites; i < DDMAXPSPRITES; i++, psp++)
	{
		// Should this psprite be drawn?
		if(psp->flags & DDPSPF_RENDERED || !psp->stateptr) 
			continue; // No...

		// If one of the psprites is fullbright, both are.
		if(psp->stateptr->frame & FF_FULLBRIGHT) 
			isFullBright = true;

		// Something will be drawn.
		somethingVisible = true;
		psp->flags |= DDPSPF_RENDERED;

		R_GetSpriteInfo(psp->stateptr->sprite, 
			psp->stateptr->frame & FF_FRAMEMASK, &info[i]);
	}

	if(!somethingVisible)
	{
		// No psprite was found, we can get out of here.
		return;
	}

	psp = viewplayer->psprites;

	// If both psprites have identical attributes, they can be combined
	// into one sprite (cvar allowing).
/*	if(rend_hud_combined_sprite 
		&& info[0].realLump > 0 && info[1].realLump > 0
		&& psp[0].alpha == psp[1].alpha
		&& psp[0].light == psp[1].light)
	{
		// Use the sprite info to prepare a single GL texture for
		// both the psprites.
		
		Con_Printf("0:x(%x,%x) off(%x,%x)\n", 
			psp[0].x, psp[0].y, info[0].offset, info[0].topOffset);
		Con_Printf("1:x(%x,%x) off(%x,%x)\n", 
			psp[1].x, psp[1].y, info[1].offset, info[1].topOffset);
		Con_Printf("--\n");
	}
	else*/
	{
		// Draw as separate sprites.
		for(i = 0; i < DDMAXPSPRITES; i++)
		{
			if(!info[i].realLump) continue; // This doesn't exist.

			if(isFullBright)
			{
				GL_SetColorAndAlpha(1, 1, 1, psp[i].alpha);
			}
			else
			{
				const byte *secRGB = R_GetSectorLightColor(sec);
				GL_SetColorAndAlpha(psp[i].light * (secRGB[CR]/255.0f), 
					psp[i].light * (secRGB[CG]/255.0f), 
					psp[i].light * (secRGB[CB]/255.0f), 
					psp[i].alpha);
			}

			GL_DrawPSprite(psp[i].x - info[i].offset + offx, 
				offScaleY * psp[i].y + (1 - offScaleY) * 32
				- info[i].topOffset + offy,
				1, info[i].flip, info[i].lump);
		}
	}
}

//===========================================================================
// Rend_RenderMaskedWall
//	A sort of a sprite, I guess... Masked walls must be rendered sorted
//	with sprites, so no artifacts appear when sprites are seen behind 
//	masked walls.
//===========================================================================
void Rend_RenderMaskedWall(vissprite_t *vis)
{
	float color[4];
	boolean withDyn = false;
	int normal, dyn;

	// Do we have a dynamic light to blend with?
	// This only happens when multitexturing is enabled.
	if(vis->data.wall.light)
	{
		if(IS_MUL)
		{
			normal = DGL_TEXTURE1;
			dyn = DGL_TEXTURE0;
		}
		else
		{
			normal = DGL_TEXTURE0;
			dyn = DGL_TEXTURE1;
		}
		
		RL_SelectTexUnits(2);
		gl.SetInteger(DGL_MODULATE_TEXTURE, IS_MUL? 4 : 5); 

		// The dynamic light.
		RL_BindTo(IS_MUL? 0 : 1, vis->data.wall.light->texture);
		RL_FloatRGB(vis->data.wall.light->color, color);
		gl.SetFloatv(DGL_ENV_COLOR, color);

		// The actual texture.
		RL_BindTo(IS_MUL? 1 : 0, vis->data.wall.texture);

		withDyn = true;
	}
	else
	{
		RL_Bind(vis->data.wall.texture);
		normal = DGL_TEXTURE0;
	}

	// Masked walls are sometimes used for special effects like arcs,
	// cobwebs and bottoms of sails. In order for them to look right,
	// we need to disable texture wrapping on the horizontal axis (S).
	// Most masked walls need wrapping, though. What we need to do is
	// look at the texture coordinates and see if they require texture
	// wrapping.
	if(vis->data.wall.masked)
	{
		if(withDyn)
		{
			gl.SetInteger(DGL_ACTIVE_TEXTURE, IS_MUL? 1 : 0);
		}
		if(vis->data.wall.texc[0][VX] < 0 ||
		   vis->data.wall.texc[0][VX] > 1 ||
		   vis->data.wall.texc[1][VX] < 0 ||
		   vis->data.wall.texc[1][VX] > 1)
		{
			// The texcoords are out of the normal [0,1] range.
			gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
		}
		else
		{
			// Visible portion is within the actual [0,1] range. 
			gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		}
	}

	// Draw one quad. This is obviously not a very efficient way to render
	// lots of masked walls, but since 3D models and sprites must be 
	// rendered interleaved with masked walls, there's not much that can be
	// done about this.

	gl.Begin(DGL_QUADS);

	gl.Color4ubv(&vis->data.wall.vertices[0].color);
	gl.MultiTexCoord2f(normal, vis->data.wall.texc[0][VX],
					   vis->data.wall.texc[1][VY]);
	if(withDyn)
	{
		gl.MultiTexCoord2f(dyn, vis->data.wall.light->s[0],
						   vis->data.wall.light->t[1]);
	}
	gl.Vertex3f(vis->data.wall.vertices[0].pos[VX], 
		vis->data.wall.bottom, 
		vis->data.wall.vertices[0].pos[VY]);

	gl.MultiTexCoord2fv(normal, vis->data.wall.texc[0]);
	if(withDyn)
	{
		gl.MultiTexCoord2f(dyn, vis->data.wall.light->s[0],
						   vis->data.wall.light->t[0]);
	}
	gl.Vertex3f(vis->data.wall.vertices[0].pos[VX], 
		vis->data.wall.top, 
		vis->data.wall.vertices[0].pos[VY]);

	gl.Color4ubv(&vis->data.wall.vertices[1].color);
	gl.MultiTexCoord2f(normal, vis->data.wall.texc[1][VX],
					   vis->data.wall.texc[0][VY]);
	if(withDyn)
	{
		gl.MultiTexCoord2f(dyn, vis->data.wall.light->s[1],
						   vis->data.wall.light->t[0]);
	}
	gl.Vertex3f(vis->data.wall.vertices[1].pos[VX], 
		vis->data.wall.top, 
		vis->data.wall.vertices[1].pos[VY]);

	gl.MultiTexCoord2fv(normal, vis->data.wall.texc[1]);
	if(withDyn)
	{
		gl.MultiTexCoord2f(dyn, vis->data.wall.light->s[1],
						   vis->data.wall.light->t[1]);
	}
	gl.Vertex3f(vis->data.wall.vertices[1].pos[VX], 
		vis->data.wall.bottom, 
		vis->data.wall.vertices[1].pos[VY]);

	gl.End();

	if(withDyn)
	{
		// Restore normal GL state.
		RL_SelectTexUnits(1);
		gl.SetInteger(DGL_MODULATE_TEXTURE, 1);
		gl.DisableArrays(true, true, 0x1);
	}
}

//===========================================================================
// Rend_DrawMasked
//	Render sprites, 3D models, masked wall segments and halos, ordered 
//	back to front. Halos are rendered with Z-buffer tests and writes 
//	disabled, so they don't go into walls or interfere with real objects.
//	It means that halos can be partly occluded by objects that are closer
//	to the viewpoint, but that's the price to pay for not having access to
//	the actual Z-buffer per-pixel depth information. The other option would
//	be for halos to shine through masked walls, sprites and models, which
//	looks even worse. (Plus, they are *halos*, not real lens flares...)
//===========================================================================
void Rend_DrawMasked (void)
{
	extern boolean willRenderSprites;
	boolean haloDrawn = false;
	vissprite_t *spr;

	if(!willRenderSprites) return;
	
	R_SortVisSprites ();
	
	if(vissprite_p > vissprites)
	{
		// Draw all vissprites back to front.
		// Sprites look better with Z buffer writes turned off.
		for(spr = vsprsortedhead.next; spr != &vsprsortedhead; 
			spr = spr->next)
		{
			if(spr->type == VSPR_MASKED_WALL)
			{
				// Depth writing is once again needed.
				Rend_RenderMaskedWall(spr);
			}
			else 
			{
				// This is a mobj.
				// There might be a model for this sprite, let's see.
				if(!spr->data.mo.mf)
				{
					if(spr->data.mo.patch >= 0)
					{
						// Render an old fashioned sprite.
						// Ah, the nostalgia...
						if(r_nospritez) gl.Disable(DGL_DEPTH_WRITE);
						Rend_RenderSprite(spr);
						if(r_nospritez) gl.Enable(DGL_DEPTH_WRITE);
					}
				}
				else // It's a sprite and it has a modelframe (it's a 3D model).
				{
					Rend_RenderModel(spr);
				}
				// How about a halo?
				if(spr->data.mo.light) 
				{
					H_RenderHalo(spr, true);
					haloDrawn = true;
				}
			}
		}

		// Draw secondary halos.
		if(haloDrawn && haloMode > 1)
		{
			// Now we can setup the state only once.
			H_SetupState(true);

			for(spr = vsprsortedhead.next ; spr != &vsprsortedhead; 
				spr = spr->next)
			{
				if(spr->type && spr->data.mo.light)
					H_RenderHalo(spr, false);
			}

			// And we're done...
			H_SetupState(false);
		}
	}
}

//===========================================================================
// Rend_SpriteTexCoord
//===========================================================================
void Rend_SpriteTexCoord(int pnum, int x, int y)
{
	// Mode zero (tc[0]) is used with regular sprites.
	gl.TexCoord2f(spritelumps[pnum].tc[0][VX] * x, 
		spritelumps[pnum].tc[0][VY] * y);
}

//===========================================================================
// Rend_SpriteLighter
//===========================================================================
static boolean Rend_SpriteLighter(lumobj_t *lum, fixed_t dist)
{
	int			i, temp;
	float		fdist = FIX2FLT(dist);
	fvertex_t	lightVec = 
	{ 
		FIX2FLT(slSpr->data.mo.gx - lum->thing->x), 
		FIX2FLT(slSpr->data.mo.gy - lum->thing->y) 
	};
	float		directness, side, inleft, inright, zfactor;
	
	if(!fdist) return true;
	if(slRGB1[0] == 0xff && slRGB1[1] == 0xff && slRGB1[2] == 0xff 
		&& slRGB2[0] == 0xff && slRGB2[1] == 0xff && slRGB2[2] == 0xff)
		return false; // No point in continuing, light is already white.

	zfactor = (FIX2FLT(slSpr->data.mo.gz + slSpr->data.mo.gzt)/2 
		- (FIX2FLT(lum->thing->z) + lum->center)) / dlMaxRad;
	if(zfactor < 0) zfactor = -zfactor;
	if(zfactor > 1) return true; // Too high or low.
	zfactor = 1-zfactor;
	// Enlarge the full-lit area.
	zfactor *= 2;
	if(zfactor > 1) zfactor = 1;
	
	lightVec.x /= fdist;
	lightVec.y /= fdist;

	// Also include the effect of the distance to zfactor.
	fdist /= (lum->radius*2) /*dlMaxRad*/;
	fdist = 1-fdist;
	//fdist *= 4;
	if(fdist > 1) fdist = 1;
	zfactor *= fdist;

	// Now the view vector and light vector are normalized.
	directness = slViewVec.x * lightVec.x + slViewVec.y * lightVec.y; // Dot product.
	side = -slViewVec.y * lightVec.x + slViewVec.x * lightVec.y;
	// If side > 0, the light comes from the right.
	if(directness > 0)
	{
		// The light comes from the front.
		if(side > 0)
		{
			inright = 1;
			inleft = directness;
		}
		else 
		{
			inleft = 1;
			inright = directness;
		}
	}
	else
	{
		// The light comes from the back.
		if(side > 0)
		{
			inright = side;
			inleft = 0;
		}
		else 
		{
			inleft = -side;
			inright = 0;
		}
	}
	inright *= zfactor;
	inleft *= zfactor;
	if(inleft > 0)
	{
		for(i=0; i<3; i++)
		{
			temp = slRGB1[i] + inleft*lum->rgb[i];
			if(temp > 0xff) temp = 0xff;
			slRGB1[i] = temp;
		}
	}
	if(inright > 0)
	{
		for(i=0; i<3; i++)
		{
			temp = slRGB2[i] + inright*lum->rgb[i];
			if(temp > 0xff) temp = 0xff;
			slRGB2[i] = temp;
		}
	}
	return true;
}

//===========================================================================
// Rend_ScaledAmbientLight
//===========================================================================
void Rend_ScaledAmbientLight(byte *out, byte *ambient, float mul)
{
	int i;

	if(mul < 0) mul = 0;
	if(mul > 1) mul = 1;
	for(i=0; i<3; i++)
		if(out[i] < ambient[i]*mul)
			out[i] = ambient[i]*mul;
}

//===========================================================================
// Rend_RenderSprite
//===========================================================================
void Rend_RenderSprite(vissprite_t *spr)
{
	int			patch = spr->data.mo.patch;
	float		bot, top;
	int			i, sprh;
	float		v1[2];
	DGLubyte	alpha;
	boolean		additiveBlending = false, flip, restoreMatrix = false;
	boolean		usingSRVO = false, restoreZ = false;
	rendpoly_t	tempquad;

	// Do we need to translate any of the colors?
	if(spr->data.mo.flags & DDMF_TRANSLATION)
	{
		// We need to prepare a translated version of the sprite.
		GL_SetTranslatedSprite(patch, 
			(spr->data.mo.flags & DDMF_TRANSLATION) >> DDMF_TRANSSHIFT, 
			spr->data.mo.class);
	}
	else
	{
		// Set the texture. No translation required.
		GL_SetSprite(patch, 0);
	}
	sprh = spritelumps[patch].height;

	// Set the lighting and alpha.
	if(missileBlend && spr->data.mo.flags & DDMF_BRIGHTSHADOW)
	{
		alpha = 204;	// 80 %.
		additiveBlending = true;
	}
	else if(spr->data.mo.flags & DDMF_SHADOW)
		alpha = 51;//85;		// One third.
	else if(spr->data.mo.flags & DDMF_ALTSHADOW)
		alpha = 160;	// Two thirds.
	else
		alpha = 255;

	// Sprite has a custom alpha multiplier?
	if(spr->data.mo.alpha >= 0) alpha *= spr->data.mo.alpha;

	if(spr->data.mo.lightlevel < 0)
	{
		gl.Color4ub(0xff, 0xff, 0xff, alpha);
	}
	else
	{
		v1[VX] = Q_FIX2FLT(spr->data.mo.gx);
		v1[VY] = Q_FIX2FLT(spr->data.mo.gy);
		tempquad.vertices[0].dist = Rend_PointDist2D(v1);
		tempquad.numvertices = 1;
		RL_VertexColors(&tempquad, 
			r_ambient > spr->data.mo.lightlevel? r_ambient : spr->data.mo.lightlevel,
			spr->data.mo.rgb);

		// Add extra light using dynamic lights.
		if(litSprites)
		{
			float len;
			memcpy(&tempquad.vertices[1].color, &tempquad.vertices[0].color, 3);
			// Global variables as parameters... ugly.
			slSpr = spr;
			slRGB1 = tempquad.vertices[0].color.rgba;
			slRGB2 = tempquad.vertices[1].color.rgba;
			slViewVec.x = FIX2FLT(spr->data.mo.gx - viewx);
			slViewVec.y = FIX2FLT(spr->data.mo.gy - viewy);
			len = sqrt(slViewVec.x*slViewVec.x + slViewVec.y*slViewVec.y);
			if(len)
			{
				slViewVec.x /= len;
				slViewVec.y /= len;
				DL_RadiusIterator(spr->data.mo.subsector, spr->data.mo.gx, spr->data.mo.gy, 
					dlMaxRad << FRACBITS, Rend_SpriteLighter);
			}
			// Check floor and ceiling for glow. They count as ambient light.
			if(spr->data.mo.hasglow)
			{
				len = 1 - (FIX2FLT(spr->data.mo.gz) - spr->data.mo.secfloor) / glowHeight;
				for(i = 0; i < 2; i++)
					Rend_ScaledAmbientLight(tempquad.vertices[i].color.rgba,
						spr->data.mo.floorglow, len);

				len = 1 - (spr->data.mo.secceil - FIX2FLT(spr->data.mo.gzt)) / glowHeight;
				for(i = 0; i < 2; i++)
					Rend_ScaledAmbientLight(tempquad.vertices[i].color.rgba,
						spr->data.mo.ceilglow, len);
			}				
		}
		gl.Color4ub(tempquad.vertices[0].color.rgba[CR],
			tempquad.vertices[0].color.rgba[CG],
			tempquad.vertices[0].color.rgba[CB],
			alpha);
	}

	// We must find the correct positioning using the sector floor and ceiling
	// heights as an aid.
	top = FIX2FLT(spr->data.mo.gzt);
	// Sprite fits in, adjustment possible?
	if(sprh < spr->data.mo.secceil - spr->data.mo.secfloor)	
	{
		// Check top.
		if(spr->data.mo.flags & DDMF_FITTOP && top > spr->data.mo.secceil) 
			top = spr->data.mo.secceil;
		// Check bottom.
		if(!(spr->data.mo.flags & DDMF_NOFITBOTTOM) && top-sprh < spr->data.mo.secfloor)
			top = spr->data.mo.secfloor+sprh;
	}
	// Adjust by the floor clip.
	top -= FIX2FLT(spr->data.mo.floorclip);
	bot = top - sprh;

	// Should the texture be flipped?
	flip = spr->data.mo.flip;

	// Should we apply a SRVO?
	if(spr->data.mo.visoff[VX] || spr->data.mo.visoff[VY] || spr->data.mo.visoff[VZ])
	{
		usingSRVO = true;
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PushMatrix();
		gl.Translatef(spr->data.mo.visoff[VX], spr->data.mo.visoff[VZ], 
			spr->data.mo.visoff[VY]);
	}

	// Do we need to do some aligning?
	if(spr->data.mo.viewaligned || alwaysAlign >= 2)
	{
		float centerx = FIX2FLT(spr->data.mo.gx), centery = FIX2FLT(spr->data.mo.gy);
		float centerz = (top+bot)/2;
		// We must set up a modelview transformation matrix.
		restoreMatrix = true;
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PushMatrix();
		// Rotate around the center of the sprite.
		gl.Translatef(centerx, centerz, centery);
		if(!spr->data.mo.viewaligned)
		{
			float s_dx = spr->data.mo.v1[VX] - spr->data.mo.v2[VX];
			float s_dy = spr->data.mo.v1[VY] - spr->data.mo.v2[VY];
			if(alwaysAlign == 2) // restricted camera alignment
			{
				float dx = centerx - vx, dy = centery - vz;	
				float spriteAngle = BANG2DEG
					( bamsAtan2(centerz - vy, sqrt(dx*dx + dy*dy)) );
				if(spriteAngle > 180) spriteAngle -= 360;
				if(fabs(spriteAngle) > maxSpriteAngle)
				{
					float turnAngle = spriteAngle>0? spriteAngle-maxSpriteAngle 
						: spriteAngle+maxSpriteAngle;
					// Rotate along the sprite edge.
					gl.Rotatef(turnAngle, s_dx, 0, s_dy);			
				}
			}
			else // restricted view plane alignment
			{
				// This'll do, for now... Really it should notice both the
				// sprite angle and vpitch.
				gl.Rotatef(vpitch/2, s_dx, 0, s_dy);
			}
		}
		else
		{
			// Normal rotation perpendicular to the view plane.
			gl.Rotatef(vpitch, viewsidex, 0, viewsidey);
		}
		gl.Translatef(-centerx, -centerz, -centery);
	}

	if(additiveBlending)
	{
		// Change the blending mode.
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
	}

	// Transparent sprites shouldn't be written to the Z buffer.
	if(alpha < 250 || additiveBlending) 
	{
		restoreZ = true;
		gl.Disable(DGL_DEPTH_WRITE);
	}

	// Render the sprite.
	gl.Begin(DGL_QUADS);
	Rend_SpriteTexCoord(patch, flip, 1);
	gl.Vertex3f(spr->data.mo.v1[VX], bot, spr->data.mo.v1[VY]);
	Rend_SpriteTexCoord(patch, flip, 0);
	gl.Vertex3f(spr->data.mo.v1[VX], top, spr->data.mo.v1[VY]);

	if(litSprites && spr->data.mo.lightlevel >= 0)
	{
		gl.Color4ub(tempquad.vertices[1].color.rgba[CR],
			tempquad.vertices[1].color.rgba[CG],
			tempquad.vertices[1].color.rgba[CB],
			alpha);
	}
	Rend_SpriteTexCoord(patch, !flip, 0);
	gl.Vertex3f(spr->data.mo.v2[VX], top, spr->data.mo.v2[VY]);
	Rend_SpriteTexCoord(patch, !flip, 1);
	gl.Vertex3f(spr->data.mo.v2[VX], bot, spr->data.mo.v2[VY]);
	gl.End();

/*	// This mirroring would be excellent if it were properly clipped.
	{
		gl.Disable(DGL_DEPTH_TEST);

		// Draw a "test mirroring".
		gl.Begin(DGL_QUADS);
		gl.Color4ub(tempquad.vertices[0].color.rgb[0],
			tempquad.vertices[0].color.rgb[1],
			tempquad.vertices[0].color.rgb[2],
			alpha/3);
		gl.TexCoord2f(flip, 0);
		gl.Vertex3f(spr->v1[VX], 2*spr->secfloor - top, spr->v1[VY]);
		gl.TexCoord2f(flip, 1);
		gl.Vertex3f(spr->v1[VX], 2*spr->secfloor - bot, spr->v1[VY]);
		gl.TexCoord2f(!flip, 1);
		gl.Vertex3f(spr->v2[VX], 2*spr->secfloor - bot, spr->v2[VY]);
		gl.TexCoord2f(!flip, 0);
		gl.Vertex3f(spr->v2[VX], 2*spr->secfloor - top, spr->v2[VY]);
		gl.End();

		gl.Enable(DGL_DEPTH_TEST);
	}*/

	if(restoreMatrix)
	{
		// Restore the original modelview matrix.
		gl.PopMatrix();
	}

	if(usingSRVO) gl.PopMatrix();

	if(additiveBlending)
	{
		// Change to normal blending.
		gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
	}

	if(restoreZ)
	{
		// Enable Z-writing again.
		gl.Enable(DGL_DEPTH_WRITE);
	}
}

