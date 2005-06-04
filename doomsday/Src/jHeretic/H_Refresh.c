#include "jHeretic/Doomdef.h"
#include "jHeretic/P_local.h"
#include "jHeretic/R_local.h"
#include "jHeretic/h_config.h"

extern float lookOffset;

//int ringFilter = 1;

player_t *viewplayer;

//int           screenblocks=10, setblocks=10;
//boolean       setsizeneeded;

void R_SetViewSize(int blocks, int detail)
{
	cfg.setsizeneeded = true;
	cfg.setblocks = blocks;
	//  setdetail = detail;
}

void R_InitTranslationTables(void)
{
	byte   *translationtables = (byte *) Get(DD_TRANSLATIONTABLES_ADDRESS);
	int     i;

	// Fill out the translation tables
	for(i = 0; i < 256; i++)
	{
		if(i >= 225 && i <= 240)
		{
			translationtables[i] = 114 + (i - 225);	// yellow
			translationtables[i + 256] = 145 + (i - 225);	// red
			translationtables[i + 512] = 190 + (i - 225);	// blue
		}
		else
		{
			translationtables[i] = translationtables[i + 256] =
				translationtables[i + 512] = i;
		}
	}
}

//===========================================================================
// R_SetDoomsdayFlags
//===========================================================================
void R_SetDoomsdayFlags(mobj_t *mo)
{
	// Client mobjs can't be set here.
	if(IS_CLIENT && mo->ddflags & DDMF_REMOTE)
		return;

	// Reset the flags for a new frame.
	mo->ddflags &= DDMF_CLEAR_MASK;

	// Local objects aren't sent to clients.
	if(mo->flags & MF_LOCAL)
		mo->ddflags |= DDMF_LOCAL;
	if(mo->flags & MF_SOLID)
		mo->ddflags |= DDMF_SOLID;
	if(mo->flags & MF_NOGRAVITY)
		mo->ddflags |= DDMF_NOGRAVITY;
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
	if(P_IsCamera(mo))
		mo->ddflags |= DDMF_DONTDRAW;

	// Choose which ddflags to set.
	if(mo->flags2 & MF2_DONTDRAW)
	{
		mo->ddflags |= DDMF_DONTDRAW;
		return;					// No point in checking the other flags.
	}
	// Players get flagged for flying.
	/*if(mo->player && mo->flags2 & MF2_FLY
	   || mo->z < mo->floorz) // Standing on a ledge.
	   mo->ddflags |= DDMF_NOGRAVITY; */

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

	if((mo->flags & MF_VIEWALIGN && !(mo->flags & MF_MISSILE)) ||
	   mo->flags & MF_FLOAT || (mo->flags & MF_MISSILE &&
								!(mo->flags & MF_VIEWALIGN)))
		mo->ddflags |= DDMF_VIEWALIGN;

	mo->ddflags |= mo->flags & MF_TRANSLATION;

	// An offset for the light emitted by this object.
	/*  if(mo->type < NUMMOBJTYPES)
	   off = MobjLightOffsets[mo->type];
	   else
	   off = 0;
	   if(off < 0) off = 8-off;
	   // off must now be in range 0-15.
	   mo->ddflags |= off << DDMF_LIGHTOFFSETSHIFT;         */
}

void R_SetAllDoomsdayFlags()
{
	int     i;
	sector_t *sec = sectors;
	mobj_t *iter;

	// Only visible things are in the sector thinglists, so this is good.
	for(i = 0; i < numsectors; i++, sec++)
		for(iter = sec->thinglist; iter; iter = iter->snext)
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
		   GL_DrawRect(0, 0, 320, 200, .1f, 0, 0, 1); */

		/*gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
		   GL_DrawRect(0, 0, 320, 200, 0, .6f, 0, 1);
		   gl.Func(DGL_BLENDING, DGL_ONE_MINUS_DST_COLOR, DGL_ONE_MINUS_SRC_COLOR);
		   GL_DrawRect(0, 0, 320, 200, 1, 0, 0, 1); */
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
	int     rgba = 0;

	// We have to choose the right color and alpha.
	if(filter >= STARTREDPALS && filter < STARTREDPALS + NUMREDPALS)
		// Red?
		rgba = FMAKERGBA(1, 0, 0, filter / 8.0);	// Full red with filter 8.
	else if(filter >= STARTBONUSPALS && filter < STARTBONUSPALS + NUMBONUSPALS)
		// Light Yellow?
		rgba = FMAKERGBA(1, 1, .5, (filter - STARTBONUSPALS + 1) / 16.0);
	return rgba;
}

void H_SetFilter(int filter)
{
	GL_SetFilter(H_GetFilterColor(filter));
}
