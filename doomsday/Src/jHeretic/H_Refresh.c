#include "Doomdef.h"
#include "P_local.h"
#include "R_local.h"
#include "settings.h"

extern float lookOffset;

//int ringFilter = 1;

player_t	*viewplayer;
//int			screenblocks=10, setblocks=10;
//boolean		setsizeneeded;

void R_SetViewSize (int blocks, int detail)
{
	cfg.setsizeneeded = true;
	cfg.setblocks = blocks;
//	setdetail = detail;
}

/*
========================
=
= R_DrawPSprite
=
========================
*/

#if 0
void R_DrawPSprite (pspdef_t *psp)
{
/*	fixed_t		tx;
	int			x1, x2;
	spritedef_t	*sprdef;
	spriteframe_t	*sprframe;
	int			lump;
	boolean		flip;
	vissprite_t	*vis, avis;

	int tempangle;

//
// decide which patch to use
//
#ifdef RANGECHECK
	if ( (unsigned)psp->state->sprite >= numsprites)
		Con_Error ("R_ProjectSprite: invalid sprite number %i "
		, psp->state->sprite);
#endif
	sprdef = &sprites[psp->state->sprite];
#ifdef RANGECHECK
	if ( (psp->state->frame & FF_FRAMEMASK)  >= sprdef->numframes)
		Con_Error ("R_ProjectSprite: invalid sprite frame %i : %i "
		, psp->state->sprite, psp->state->frame);
#endif
	sprframe = &sprdef->spriteframes[ psp->state->frame & FF_FRAMEMASK ];

	lump = sprframe->lump[0];
	flip = (boolean)sprframe->flip[0];

//
// calculate edges of the shape
//
	tx = psp->sx-160*FRACUNIT;

	tx -= spriteoffset[lump];
	if(viewangleoffset)
	{
		tempangle = ((centerxfrac/1024)*(viewangleoffset>>ANGLETOFINESHIFT));
	}
	else
	{
		tempangle = 0;
	}
	x1 = (centerxfrac + FixedMul (tx,pspritescale)+tempangle ) >>FRACBITS;
	if (x1 > viewwidth)
		return;		// off the right side
	tx +=  spritewidth[lump];
	x2 = ((centerxfrac + FixedMul (tx, pspritescale)+tempangle ) >>FRACBITS) - 1;
	if (x2 < 0)
		return;		// off the left side

//
// store information in a vissprite
//
	vis = &avis;
	vis->mobjflags = 0;
	vis->psprite = true;
	vis->texturemid = (BASEYCENTER<<FRACBITS)+FRACUNIT/2-(psp->sy-spritetopoffset[lump]);
	if(viewheight == SCREENHEIGHT)
	{
		vis->texturemid -= PSpriteSY[players[consoleplayer].readyweapon];
	}
	vis->x1 = x1 < 0 ? 0 : x1;
	vis->x2 = x2 >= viewwidth ? viewwidth-1 : x2;
	vis->scale = pspritescale<<detailshift;
	if (flip)
	{
		vis->xiscale = -pspriteiscale;
		vis->startfrac = spritewidth[lump]-1;
	}
	else
	{
		vis->xiscale = pspriteiscale;
		vis->startfrac = 0;
	}
	if (vis->x1 > x1)
		vis->startfrac += vis->xiscale*(vis->x1-x1);
	vis->patch = lump;

	if(viewplayer->powers[pw_invisibility] > 4*32 ||
	viewplayer->powers[pw_invisibility] & 8)
	{
		// Invisibility
		vis->colormap = spritelights[MAXLIGHTSCALE-1];
		vis->mobjflags |= MF_SHADOW;
	}
	else if(fixedcolormap)
	{
		// Fixed color
		vis->colormap = fixedcolormap;
	}
	else if(psp->state->frame & FF_FULLBRIGHT)
	{
		// Full bright
		vis->colormap = colormaps;
	}
	else
	{
		// local light
		vis->colormap = spritelights[MAXLIGHTSCALE-1];
	}
	R_DrawVisSprite(vis, vis->x1, vis->x2);*/

	fixed_t			tx;
	int				x1; 
	spriteinfo_t	sprinfo;
	float			light, alpha;
	int				y;

	// Get the sprite info.
	gi.GetSpriteInfo(psp->state->sprite, psp->state->frame, &sprinfo);

	// Calculate edges of the shape.
	tx = psp->sx - (160 + lookOffset*1300) * FRACUNIT;

	tx -= sprinfo.offset;
	x1 = ((160<<FRACBITS)+tx ) >>FRACBITS;
	// Choose color and alpha.
	light = 1;
	alpha = 1;

/*	if(viewplayer->powers[pw_invulnerability] && viewplayer->class
		== PCLASS_CLERIC)
	{
		if(viewplayer->powers[pw_invulnerability] > 4*32)
		{
			if(viewplayer->plr->mo->flags2 & MF2_DONTDRAW)
			{ // don't draw the psprite
				alpha = .333f;
			}
			else if(viewplayer->plr->mo->flags & MF_SHADOW)
			{
				alpha = .666f;
			}
		}
		else if(viewplayer->powers[pw_invulnerability]&8)
		{
			alpha = .333f;
		}
	}	
	else */
	if(viewplayer->powers[pw_invisibility] > 4*32 || viewplayer->powers[pw_invisibility] & 8)
	{
		// Invisibility
		//vis->colormap = spritelights[MAXLIGHTSCALE-1];
		alpha = .4f;
	}
	if(psp->state->frame & FF_FULLBRIGHT)
	{
		// Full bright
		light = 1;
	}
	else
	{
		// local light
		light = viewplayer->plr->mo->subsector->sector->lightlevel / 255.0;
	}

	// Do some rendering, oh yeah.
	y = -(sprinfo.topOffset>>FRACBITS)+(psp->sy>>FRACBITS);
	if(Get(DD_VIEWWINDOW_HEIGHT) == SCREENHEIGHT)
	{
		//y += PSpriteSY[viewplayer->class][players[consoleplayer].readyweapon] >> FRACBITS;
		y += PSpriteSY[players[consoleplayer].readyweapon] >> FRACBITS;
	}
	else 
	{
		y -= (39*cfg.sbarscale)/40;
		y += (PSpriteSY[players[consoleplayer].readyweapon] >> FRACBITS) * (20-cfg.sbarscale)/20.0f;
	}

	light += .1f;	// Add some extra light.
	GL_SetColorAndAlpha(light, light, light, alpha);
	GL_DrawPSprite(x1, y, 1, sprinfo.flip, sprinfo.lump);
}

/*
========================
=
= R_DrawPlayerSprites
=
========================
*/

void R_DrawPlayerSprites(ddplayer_t *viewplr)
{
	int			i;
	pspdef_t	*psp;

	viewplayer = (player_t*) viewplr->extradata;
	for (i=0, psp=viewplayer->psprites ; i<NUMPSPRITES ; i++,psp++)
		if (psp->state)
			R_DrawPSprite (psp);
}
#endif


void R_InitTranslationTables(void)
{
	byte	*translationtables = (byte*) Get(DD_TRANSLATIONTABLES_ADDRESS);
	int		i;

	// Fill out the translation tables
	for(i = 0; i < 256; i++)
	{
		if(i >= 225 && i <= 240)
		{
			translationtables[i] = 114+(i-225); // yellow
			translationtables[i+256] = 145+(i-225); // red
			translationtables[i+512] = 190+(i-225); // blue
		}
		else
		{
			translationtables[i] = translationtables[i+256] 
				= translationtables[i+512] = i;
		}
	}
}

//===========================================================================
// R_SetDoomsdayFlags
//===========================================================================
void R_SetDoomsdayFlags(mobj_t *mo)
{
	// Client mobjs can't be set here.
	if(IS_CLIENT && mo->ddflags & DDMF_REMOTE) return;
	
	// Reset the flags for a new frame.
	mo->ddflags &= DDMF_CLEAR_MASK;	
	
	// Local objects aren't sent to clients.
	if(mo->flags & MF_LOCAL) mo->ddflags |= DDMF_LOCAL;
	if(mo->flags & MF_SOLID) mo->ddflags |= DDMF_SOLID;
	if(mo->flags & MF_NOGRAVITY) mo->ddflags |= DDMF_NOGRAVITY;
	if(mo->flags2 & MF2_FLOATBOB)
		mo->ddflags |= DDMF_NOGRAVITY | DDMF_BOB;
	if(mo->flags & MF_MISSILE) 
	{
		// Mace death balls are controlled by the server.
		//if(mo->type != MT_MACEFX4)
		mo->ddflags |= DDMF_MISSILE;
	}
	if(mo->flags2 & MF2_FLY) 
		mo->ddflags |= DDMF_FLY | DDMF_NOGRAVITY;

	// $democam: cameramen are invisible
	if(P_IsCamera(mo)) mo->ddflags |= DDMF_DONTDRAW;
	
	// Choose which ddflags to set.
	if(mo->flags2 & MF2_DONTDRAW)
	{
		mo->ddflags |= DDMF_DONTDRAW;
		return; // No point in checking the other flags.
	}
	// Players get flagged for flying.
	/*if(mo->player && mo->flags2 & MF2_FLY
		|| mo->z < mo->floorz) // Standing on a ledge.
		mo->ddflags |= DDMF_NOGRAVITY;*/

	if(mo->flags2 & MF2_LOGRAV)
		mo->ddflags |= DDMF_LOWGRAVITY;

	if(mo->flags2 & MF2_FEETARECLIPPED)
		mo->floorclip = FOOTCLIPSIZE; 
	else
		mo->floorclip = 0;
	
	if(mo->flags & MF_BRIGHTSHADOW)
		mo->ddflags |= DDMF_BRIGHTSHADOW;
	else if(mo->flags & MF_SHADOW)
		mo->ddflags |= DDMF_ALTSHADOW;
	
	if((mo->flags & MF_VIEWALIGN && !(mo->flags & MF_MISSILE)) 
		|| mo->flags & MF_FLOAT 
		|| (mo->flags & MF_MISSILE && !(mo->flags & MF_VIEWALIGN)))
		mo->ddflags |= DDMF_VIEWALIGN;
	
	mo->ddflags |= mo->flags & MF_TRANSLATION;
	
	// An offset for the light emitted by this object.
/*	if(mo->type < NUMMOBJTYPES)
		off = MobjLightOffsets[mo->type];
	else
		off = 0;
	if(off < 0) off = 8-off;
	// off must now be in range 0-15.
	mo->ddflags |= off << DDMF_LIGHTOFFSETSHIFT;			*/
}

void R_SetAllDoomsdayFlags()
{
	int			i;
	sector_t	*sec = sectors;
	mobj_t		*iter;

	// Only visible things are in the sector thinglists, so this is good.
	for(i=0; i<numsectors; i++, sec++)
		for(iter=sec->thinglist; iter; iter=iter->snext)
			R_SetDoomsdayFlags(iter);
}

void R_DrawRingFilter()
{
	gl.Disable(DGL_TEXTURING);
	if(cfg.ringFilter == 1)
	{
		gl.Func(DGL_BLENDING, DGL_SRC_COLOR, DGL_SRC_COLOR);
		GL_DrawRect(0, 0, 320, 200, .5f, .35f, .1f, 1);
		
		/*gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
		GL_DrawRect(0, 0, 320, 200, 1, .7f, 0, 1);
		gl.Func(DGL_BLENDING, DGL_ONE, DGL_DST_COLOR);
		GL_DrawRect(0, 0, 320, 200, .1f, 0, 0, 1);*/

		/*gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
		GL_DrawRect(0, 0, 320, 200, 0, .6f, 0, 1);
		gl.Func(DGL_BLENDING, DGL_ONE_MINUS_DST_COLOR, DGL_ONE_MINUS_SRC_COLOR);
		GL_DrawRect(0, 0, 320, 200, 1, 0, 0, 1);*/
	}
	else 
	{
		gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_SRC_COLOR);
		GL_DrawRect(0, 0, 320, 200, 0, 0, .6f, 1);
	}
	// Restore the normal rendering state.
	gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
	gl.Enable(DGL_TEXTURING);

}

#define FMAKERGBA(r,g,b,a) ( (byte)(0xff*r) + ((byte)(0xff*g)<<8) + ((byte)(0xff*b)<<16) + ((byte)(0xff*a)<<24) )

int H_GetFilterColor(int filter)
{
	int rgba = 0;

	// We have to choose the right color and alpha.
	if(filter >= STARTREDPALS && filter < STARTREDPALS+NUMREDPALS) 
		// Red?
		rgba = FMAKERGBA(1, 0, 0, filter/8.0);	// Full red with filter 8.
	else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS+NUMBONUSPALS) 
		// Light Yellow?
		rgba = FMAKERGBA(1, 1, .5, (filter-STARTBONUSPALS+1)/16.0); 
	return rgba;
}

void H_SetFilter(int filter)
{
	GL_SetFilter(H_GetFilterColor(filter));		
}

