
//**************************************************************************
//**
//** R_THINGS.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_play.h"
#include "de_render.h"
#include "de_graphics.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define MAX_FRAMES 128

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int gametic;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float			weaponOffsetScale = 0.3183f;	// 1/Pi
float			weaponFOVShift = 45;
int				alwaysAlign = 0;
int				r_nospritez = false;
int				pspOffX = 0, pspOffY = 0;
int				r_use_srvo = 2, r_use_srvo_angle = true; 
				// srvo1 = models only, srvo2 = sprites + models

int				psp3d;

// variables used to look up and range check thing_t sprites patches
spritedef_t		*sprites = 0;
int				numsprites;

spriteframe_t	sprtemp[MAX_FRAMES];
int				maxframe;
char			*spritename;

spritelump_t	*spritelumps;
int				numspritelumps;

vissprite_t		vissprites[MAXVISSPRITES], *vissprite_p;
vissprite_t		vispsprites[DDMAXPSPRITES];
vissprite_t		overflowsprite;
int				newvissprite;

int				r_maxmodelz = 1500;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*

Sprite rotation 0 is facing the viewer, rotation 1 is one angle turn 
CLOCKWISE around the axis. This is not the same as the angle, which 
increases counter clockwise (protractor).

*/

int LevelFullBright = false;

/*
===============================================================================

						INITIALIZATION FUNCTIONS

===============================================================================
*/

//===========================================================================
// R_InitSpriteLumps
//===========================================================================
void R_InitSpriteLumps(void)
{
	patch_t *patch;
	spritelump_t *sl;
	int i;
	char buf[64];

	sprintf(buf, "R_Init: Initializing %i sprites...", numspritelumps);
	Con_InitProgress(buf, numspritelumps);

	for(i = 0, sl = spritelumps; i < numspritelumps; i++, sl++)
	{
		if(!(i%50)) Con_Progress(i, PBARF_SET | PBARF_DONTSHOW);		

		patch = W_CacheLumpNum(sl->lump, PU_CACHE);
		sl->width = SHORT(patch->width);
		sl->height = SHORT(patch->height);
		sl->offset = SHORT(patch->leftoffset);
		sl->topoffset = SHORT(patch->topoffset);
	}

	Con_HideProgress();
}

//===========================================================================
// R_NewSpriteLump
//	Returns the new sprite lump number.
//===========================================================================
int R_NewSpriteLump(int lump)
{
	spritelump_t *newlist, *ptr;
	int i;

	// Is this lump already entered?
	for(i=0; i<numspritelumps; i++)
		if(spritelumps[i].lump == lump)
			return i;

	newlist = Z_Malloc(sizeof(spritelump_t) * ++numspritelumps, PU_SPRITE, 0);
	if(numspritelumps > 1)
	{
		memcpy(newlist, spritelumps, sizeof(spritelump_t) * (numspritelumps-1));
		Z_Free(spritelumps);
	}
	spritelumps = newlist;
	ptr = spritelumps + numspritelumps-1;
	memset(ptr, 0, sizeof(spritelump_t));
	ptr->lump = lump;
	return numspritelumps-1;
}

//===========================================================================
// R_InstallSpriteLump
//	 Local function for R_InitSprites.
//===========================================================================
void R_InstallSpriteLump (int lump, unsigned frame, unsigned rotation, boolean flipped)
{
	int		splump = R_NewSpriteLump(lump);
	int		r;

	if(frame >= 30 || rotation > 8)
		//Con_Error ("R_InstallSpriteLump: Bad frame characters in lump %i", lump);
		return;

	if( (int) frame > maxframe)
		maxframe = frame;

	if(rotation == 0)
	{
		// The lump should be used for all rotations.
/*		if(sprtemp[frame].rotate == false)
			Con_Error ("R_InitSprites: Sprite %s frame %c has multip rot=0 lump",
				spritename, 'A'+frame);
		if (sprtemp[frame].rotate == true)
			Con_Error ("R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump"
			, spritename, 'A'+frame);*/

		sprtemp[frame].rotate = false;
		for(r=0; r<8; r++)
		{
			sprtemp[frame].lump[r] = splump; //lump - firstspritelump;
			sprtemp[frame].flip[r] = (byte) flipped;
		}
		return;
	}

// the lump is only used for one rotation
/*	if (sprtemp[frame].rotate == false)
		Con_Error ("R_InitSprites: Sprite %s frame %c has rotations and a rot=0 lump"
		, spritename, 'A'+frame);*/

	sprtemp[frame].rotate = true;

	rotation--;		// make 0 based
/*	if(sprtemp[frame].lump[rotation] != -1)
		Con_Error ("R_InitSprites: Sprite %s : %c : %c has two lumps mapped to it"
		,spritename, 'A'+frame, '1'+rotation);*/

	sprtemp[frame].lump[rotation] = splump; //lump - firstspritelump;
	sprtemp[frame].flip[rotation] = (byte) flipped;
}

//===========================================================================
// R_InitSpriteDefs
//	Pass a null terminated list of sprite names (4 chars exactly) to be used
//	Builds the sprite rotation matrixes to account for horizontally flipped
//	sprites.  Will report an error if the lumps are inconsistant.
//
//	Sprite lump names are 4 characters for the actor, a letter for the frame,
//	and a number for the rotation, A sprite that is flippable will have an
//	additional letter/number appended.  The rotation character can be 0 to
//	signify no rotations.
//===========================================================================
void R_InitSpriteDefs(void)
{
	int		i, l, intname, frame, rotation;
	boolean	in_sprite_block;

	numspritelumps = 0;
	numsprites = count_sprnames.num;
	
	// Check that some sprites are defined.
	if(!numsprites) return;

	sprites = Z_Malloc(numsprites * sizeof(*sprites), PU_SPRITE, NULL);

	// Scan all the lump names for each of the names, noting the highest
	// frame letter. Just compare 4 characters as ints.
	for (i=0 ; i<numsprites ; i++)
	{
		spritename = sprnames[i].name; 
		memset(sprtemp, -1, sizeof(sprtemp));

		maxframe = -1;
		intname = *(int*) spritename;

		//
		// scan the lumps, filling in the frames for whatever is found
		//
		in_sprite_block = false;
		for(l=0; l<numlumps; l++)
		{
			char *name = lumpinfo[l].name;
			if(!strnicmp(name, "S_START", 7))
			{
				// We've arrived at *a* sprite block.
				in_sprite_block = true;
				continue;
			}
			else if(!strnicmp(name, "S_END", 5))
			{
				// The sprite block ends.
				in_sprite_block = false;
				continue;
			}
			if(!in_sprite_block) continue;
			// Check that the first four letters match the sprite name.
			if( *(int*) name == intname)
			{
				// Check that the name is valid.
				if(!name[4] || !name[5] || (name[6] && !name[7])) 
					continue; // This is not a sprite frame.
				// Indices 5 and 7 must be numbers (0-8).
				if(name[5] < '0' || name[5] > '8') continue;
				if(name[7] && (name[7] < '0' || name[7] > '8')) continue;
				frame = name[4] - 'A';
				rotation = name[5] - '0';
				R_InstallSpriteLump(l, frame, rotation, false);
				if(name[6])
				{
					frame = name[6] - 'A';
					rotation = name[7] - '0';
					R_InstallSpriteLump(l, frame, rotation, true);
				}
			}
		}

		//
		// check the frames that were found for completeness
		//
		if (maxframe == -1)
		{
			sprites[i].numframes = 0;
			//Con_Error ("R_InitSprites: No lumps found for sprite %s", namelist[i]);
		}

		maxframe++;
		for (frame = 0 ; frame < maxframe ; frame++)
		{
			switch (sprtemp[frame].rotate)
			{
			case -1:	// no rotations were found for that frame at all
				Con_Error ("R_InitSprites: No patches found for %s frame %c",
					spritename, frame+'A');
			case 0:	// only the first rotation is needed
				break;

			case 1:	// must have all 8 frames
				for (rotation=0 ; rotation<8 ; rotation++)
					if (sprtemp[frame].lump[rotation] == -1)
						Con_Error ("R_InitSprites: Sprite %s frame %c is missing rotations",
							spritename, frame+'A');
			}
		}

		// The model definitions might have a larger max frame for this 
		// sprite.
		/*highframe = R_GetMaxMDefSpriteFrame(spritename) + 1;
		if(highframe > maxframe) 
		{
			maxframe = highframe;
			for(l=0; l<maxframe; l++)
				for(frame=0; frame<8; frame++)
					if(sprtemp[l].lump[frame] == -1)
						sprtemp[l].lump[frame] = 0;
		}*/
			
		// Allocate space for the frames present and copy sprtemp to it.
		sprites[i].numframes = maxframe;
		sprites[i].spriteframes =
			Z_Malloc (maxframe * sizeof(spriteframe_t), PU_SPRITE, NULL);
		memcpy (sprites[i].spriteframes, sprtemp, 
			maxframe * sizeof(spriteframe_t));
		// The possible model frames are initialized elsewhere.
		//sprites[i].modeldef = -1;
	}
}

//===========================================================================
// R_GetSpriteInfo
//===========================================================================
void R_GetSpriteInfo(int sprite, int frame, spriteinfo_t *sprinfo)
{
	spritedef_t	*sprdef;
	spriteframe_t *sprframe;
	spritelump_t *sprlump;

#ifdef RANGECHECK
	if( (unsigned) sprite >= (unsigned) numsprites)
		Con_Error( "R_GetSpriteInfo: invalid sprite number %i.\n", sprite);
#endif

	sprdef = &sprites[sprite];

#ifdef RANGECHECK
	if((frame & FF_FRAMEMASK) >= sprdef->numframes)
		Con_Error( "R_ProjectSprite: invalid sprite frame %i : %i.\n", sprite, frame);
#endif

	sprframe = &sprdef->spriteframes[ frame & FF_FRAMEMASK ];
	sprlump = spritelumps + sprframe->lump[0];

	sprinfo->numFrames = sprdef->numframes;
	sprinfo->lump = sprframe->lump[0];
	sprinfo->realLump = sprlump->lump;
	sprinfo->flip = sprframe->flip[0];
	sprinfo->offset = sprlump->offset;
	sprinfo->topOffset = sprlump->topoffset;
	sprinfo->width = sprlump->width;
	sprinfo->height = sprlump->height;
}

//===========================================================================
// R_VisualRadius
//	Returns the radius of the mobj as it would visually appear to be.
//===========================================================================
int R_VisualRadius(mobj_t *mo)
{
	modeldef_t *mf, *nextmf;
	spriteinfo_t sprinfo;

	// If models are being used, use the model's radius.
	if(useModels)
	{
		R_CheckModelFor(mo, &mf, &nextmf);
		if(mf)
		{
			// Returns the model's radius!
			return mf->visualradius;
		}
	}

	// Use the sprite frame's width.
	R_GetSpriteInfo(mo->sprite, mo->frame, &sprinfo);
	return sprinfo.width/2;
}

/*
===============================================================================

							GAME FUNCTIONS

===============================================================================
*/

//===========================================================================
// R_InitSprites
//===========================================================================
void R_InitSprites(void)
{
	// Free all previous sprite memory.
	Z_FreeTags(PU_SPRITE, PU_SPRITE);
	R_InitSpriteDefs();
	R_InitSpriteLumps();
}	

//===========================================================================
// R_ClearSprites
//	Called at frame start.
//===========================================================================
void R_ClearSprites(void)
{
	vissprite_p = vissprites;
}

//===========================================================================
// R_NewVisSprite
//===========================================================================
vissprite_t *R_NewVisSprite (void)
{
	if (vissprite_p == &vissprites[MAXVISSPRITES])
		return &overflowsprite;
	vissprite_p++;
	return vissprite_p-1;
}

//===========================================================================
// R_ProjectPlayerSprites
//	If 3D models are found for psprites, here we will create vissprites
//	for them.
//===========================================================================
void R_ProjectPlayerSprites(void)
{
	int				i;
	modeldef_t		*mf, *nextmf;
	ddpsprite_t		*psp;
	vissprite_t		*vis;
	mobj_t			dummy;

	psp3d = false;

	// Cameramen have no psprites.
	if(viewplayer->flags & DDPF_CAMERA) return; 

	for(i = 0, psp = viewplayer->psprites; i < DDMAXPSPRITES; i++, psp++)
	{
		vis = vispsprites + i;
		psp->flags &= ~DDPSPF_RENDERED;
		vis->issprite = false;
		if(!useModels || !psp->stateptr) continue;

		// Is there a model for this frame?
		// Setup a dummy for the call to R_CheckModelFor.
		dummy.state = psp->stateptr;
		dummy.tics = psp->tics;

		vis->mo.inter = R_CheckModelFor(&dummy, &mf, &nextmf);
		if(!mf)
		{
			// No, draw a 2D sprite instead (in R_DrawPlayerSprites).
			continue;
		}
		// Mark this sprite rendered.
		psp->flags |= DDPSPF_RENDERED;
	
		// There are 3D psprites.
		psp3d = true;

		vis->issprite = true;
		vis->distance = 4;
		vis->mo.mf = mf;
		vis->mo.nextmf = nextmf;
		vis->mo.flags = 0;
		vis->mo.gx = viewx;
		vis->mo.gy = viewy;
		vis->mo.v1[VX] = FIX2FLT(viewx);
		vis->mo.v1[VY] = FIX2FLT(viewy);
		// 32 is the raised weapon height.
		vis->mo.gzt = vis->mo.gz = viewz;
		vis->mo.viewaligned = true; 
		vis->mo.secfloor = -1.0e6;
		vis->mo.secceil = 1.0e6;
		vis->mo.class = 0;
		vis->mo.floorclip = 0;
		// Offsets to rotation angles.
		vis->mo.v2[VX] = psp->x * weaponOffsetScale - 90;
		vis->mo.v2[VY] = (32-psp->y) * weaponOffsetScale;
		// Is the FOV shift in effect?
		if(weaponFOVShift > 0 && fieldOfView > 90)
			vis->mo.v2[VY] -= weaponFOVShift * (fieldOfView - 90)/90;
		// Real rotation angles.
		vis->mo.yaw = viewangle / (float) ANGLE_MAX * -360 
			+ vis->mo.v2[VX] + 90;
		vis->mo.pitch = viewpitch*85/110 + vis->mo.v2[VY];
		vis->mo.flip = false;
		if(psp->light < 1)
			vis->mo.lightlevel = (psp->light-.1f) * 255;
		else
			vis->mo.lightlevel = 255;
		vis->mo.alpha = psp->alpha;
		memcpy(vis->mo.rgb, viewplayer->mo->subsector->sector->rgb, 3);
		memset(vis->mo.visoff, 0, sizeof(vis->mo.visoff));
	}
}

//===========================================================================
// R_ProjectSprite
//	Generates a vissprite for a thing if it might be visible.
//===========================================================================
void R_ProjectSprite (mobj_t *thing)
{
	sector_t	*sect = thing->subsector->sector;
	fixed_t		trx,try;
	spritedef_t	*sprdef;
	spriteframe_t *sprframe;
	int			i, lump;
	unsigned	rot;
	boolean		flip;
	vissprite_t	*vis;
	angle_t		ang;
	float		v1[2], v2[2];
	float		sinrv, cosrv, thangle;	// rv = real value
	boolean		align;
	modeldef_t	*mf = NULL, *nextmf = NULL;
	float		interp, distance;
	
	if(thing->ddflags & DDMF_DONTDRAW)
	{ 
		// Never make a vissprite when DDMF_DONTDRAW is set.
		return;
	}

	// Transform the origin point.
	trx = thing->x - viewx;
	try = thing->y - viewy;

	// Decide which patch to use for sprite reletive to player.

#ifdef RANGECHECK
	if ((unsigned)thing->sprite >= (unsigned)numsprites)
		Con_Error ("R_ProjectSprite: invalid sprite number %i ",thing->sprite);
#endif
	sprdef = &sprites[thing->sprite];
#ifdef RANGECHECK
	if ( (thing->frame&FF_FRAMEMASK) >= sprdef->numframes )
		Con_Error ("R_ProjectSprite: invalid sprite frame %i : %i "
		,thing->sprite, thing->frame);
#endif
	sprframe = &sprdef->spriteframes[ thing->frame & FF_FRAMEMASK ];

	// Calculate edges of the shape.
	v1[VX] = FIX2FLT(thing->x);
	v1[VY] = FIX2FLT(thing->y);

	distance = Rend_PointDist2D(v1);

	// Check for a 3D model.
	if(useModels)
	{
		interp = R_CheckModelFor(thing, &mf, &nextmf);
		if(mf && !(mf->flags & MFF_NO_DISTANCE_CHECK)
			&& r_maxmodelz 
			&& distance > r_maxmodelz)
		{
			// Don't use a 3D model.
			mf = nextmf = NULL;
			interp = -1;
		}
	}

	if(sprframe->rotate && !mf)
	{	
		// Choose a different rotation based on player view.
		ang = R_PointToAngle (thing->x, thing->y);
		rot = (ang - thing->angle + (unsigned)(ANG45/2)*9)>>29;
		lump = sprframe->lump[rot];
		flip = (boolean)sprframe->flip[rot];
	}
	else
	{	
		// Use single rotation for all views.
		lump = sprframe->lump[0];
		flip = (boolean)sprframe->flip[0];
	}

	// Align to the view plane?
	if(thing->ddflags & DDMF_VIEWALIGN)
		align = true;
	else
		align = false;

	if(alwaysAlign == 1) align = true;
	
	if(!mf)
	{
		if(align || alwaysAlign == 3)
		{
			// The sprite should be fully aligned to view plane.
			sinrv = -FIX2FLT(viewcos);
			cosrv = FIX2FLT(viewsin);
		}
		else
		{
			thangle = BANG2RAD(bamsAtan2(FIX2FLT(try)*10, FIX2FLT(trx)*10)) - PI/2;
			sinrv = sin(thangle);
			cosrv = cos(thangle);
		}	
		
		//if(alwaysAlign == 2) align = true;
		
		v1[VX] -= cosrv * spritelumps[lump].offset; 
		v1[VY] -= sinrv * spritelumps[lump].offset; 
		v2[VX] = v1[VX] + cosrv * spritelumps[lump].width;
		v2[VY] = v1[VY] + sinrv * spritelumps[lump].width;
		
		if(!align && alwaysAlign != 2 && alwaysAlign != 3)
			// Check for visibility.
			if(!C_CheckViewRelSeg(v1[VX], v1[VY], v2[VX], v2[VY]))
				return;	// Isn't visible.
	}
	else
	{
		// Models need to be visibility-checked, too.
		float off[2];
		thangle = BANG2RAD(bamsAtan2(FIX2FLT(try)*10, FIX2FLT(trx)*10)) - PI/2;
		sinrv = sin(thangle);
		cosrv = cos(thangle);
		off[VX] = cosrv * (thing->radius>>FRACBITS);
		off[VY] = sinrv * (thing->radius>>FRACBITS);
		if(!C_CheckViewRelSeg(v1[VX]-off[VX], v1[VY]-off[VY], 
			v1[VX]+off[VX], v1[VY]+off[VY])) return;	// Can't be visible.
		// Viewaligning means scaling down Z with models.
		align = false;
	}
	//
	// Store information in a vissprite.
	//
	vis = R_NewVisSprite ();
	vis->issprite = true;
	vis->distance = distance;
	vis->mo.light = thing->light;
	vis->mo.mf = mf;
	vis->mo.nextmf = nextmf;
	vis->mo.inter = interp;
	vis->mo.flags = thing->ddflags;
	vis->mo.id = thing->thinker.id;
	vis->mo.selector = thing->selector;
	vis->mo.gx = thing->x;
	vis->mo.gy = thing->y;
	vis->mo.gz = thing->z;
	vis->mo.gzt = thing->z + ((fixed_t)spritelumps[lump].topoffset << FRACBITS); 

	memcpy(vis->mo.rgb, sect->rgb, 3);

	vis->mo.viewaligned = align;

	vis->mo.secfloor = FIX2FLT(thing->subsector->sector->floorheight);
	vis->mo.secceil = FIX2FLT(thing->subsector->sector->ceilingheight);

	if(thing->ddflags & DDMF_TRANSLATION)
		vis->mo.class = (thing->ddflags>>DDMF_CLASSTRSHIFT) & 0x3;
	else
		vis->mo.class = 0;

	// Foot clipping.
	vis->mo.floorclip = thing->floorclip;
	
	// The start and end vertices.
	vis->mo.v1[VX] = v1[VX];
	vis->mo.v1[VY] = v1[VY];
	if(!mf)
	{
		vis->mo.v2[VX] = v2[VX];
		vis->mo.v2[VY] = v2[VY];
	}
	else
	{
		// Determine the rotation angles (in degrees).
		if(mf->sub[0].flags & MFF_ALIGN_YAW)
		{
			vis->mo.yaw = 90 - thangle / PI * 180;
		}
		else if(mf->sub[0].flags & MFF_SPIN)
		{
			vis->mo.yaw = 70 * gametic/35.0f + (int)thing % 360;
		}
		else if(mf->sub[0].flags & MFF_MOVEMENT_YAW)
		{
			vis->mo.yaw = BANG2DEG(bamsAtan2(-100*FIX2FLT(thing->momy), 
				100*FIX2FLT(thing->momx)));
		}
		else
		{
			vis->mo.yaw = (r_use_srvo_angle && !netgame && !playback? 
				(thing->visangle<<16) : thing->angle) 
				/ (float) ANGLE_MAX*-360;
		}

		// How about a unique offset?
		if(mf->sub[0].flags & MFF_IDANGLE)
		{
			// Multiply with an arbitrary factor.
			vis->mo.yaw += (thing->thinker.id * 27) % 360; 
		}

		if(mf->sub[0].flags & MFF_ALIGN_PITCH)
		{
			vis->mo.pitch = -BANG2DEG(bamsAtan2(FIX2FLT
				((vis->mo.gz + vis->mo.gzt)/2 - viewz)*10,
				distance*10));			
		}
		else if(mf->sub[0].flags & MFF_MOVEMENT_PITCH)
		{
			vis->mo.pitch = BANG2DEG(bamsAtan2(100*FIX2FLT(thing->momz), 
				100*P_AccurateDistance(thing->momx, thing->momy)));
		}
		else
			vis->mo.pitch = 0;
	}
	vis->mo.flip = flip;
	vis->mo.patch = lump;

	// Set light level.
	if((LevelFullBright || thing->frame & FF_FULLBRIGHT) 
		&& (!mf || !(mf->sub[0].flags & MFF_DIM)))
	{
		vis->mo.lightlevel = -1;
	}
	else
	{
		// Diminished light.
		vis->mo.lightlevel = sect->lightlevel;
	}
	
	// The three highest bits of the selector are used for an alpha level.
	// 0 = opaque (alpha -1)
	// 1 = 1/8 transparent
	// 4 = 1/2 transparent
	// 7 = 7/8 transparent
	i = thing->selector >> DDMOBJ_SELECTOR_SHIFT;
	if(i & 0xe0)
		vis->mo.alpha = 1 - ((i&0xe0) >> 5)/8.0f;
	else
		vis->mo.alpha = -1;

	// Short-range visual offsets.
	if((vis->mo.mf && r_use_srvo > 0 || !vis->mo.mf && r_use_srvo > 1)
		&& thing->state 
		&& thing->tics >= 0)
	{
		float mul = thing->tics / (float) thing->state->tics;				
		for(i = 0; i < 3; i++)
			vis->mo.visoff[i] = FIX2FLT(thing->srvo[i]<<8) * mul;
	}
	else
	{
		// Reset the visual offset.
		memset(vis->mo.visoff, 0, sizeof(vis->mo.visoff));
	}

	// Glowing floor and ceiling.
	vis->mo.hasglow = false;
	if(useWallGlow)
	{
		if(R_FlatFlags(sect->ceilingpic) & TXF_GLOW)
		{
			GL_GetFlatColor(sect->ceilingpic, vis->mo.ceilglow);
			for(i=0; i<3; i++) vis->mo.ceilglow[i] *= dlFactor;
			vis->mo.hasglow = true;
		}
		else
		{
			memset(vis->mo.ceilglow, 0, sizeof(vis->mo.ceilglow));
		}
		if(R_FlatFlags(sect->floorpic) & TXF_GLOW)
		{
			GL_GetFlatColor(sect->floorpic, vis->mo.floorglow);
			for(i=0; i<3; i++) vis->mo.floorglow[i] *= dlFactor;
			vis->mo.hasglow = true;
		}
		else
		{
			memset(vis->mo.floorglow, 0, sizeof(vis->mo.floorglow));
		}
	}
}

/*
========================
=
= R_AddSprites
=
========================
*/

void R_AddSprites (sector_t *sec)
{
	mobj_t *thing;
	spriteinfo_t spriteInfo;
	fixed_t visibleTop;

	if (sec->validcount == validcount)
		return;		// already added

	sec->validcount = validcount;

	for (thing = sec->thinglist ; thing ; thing = thing->snext)
	{
		R_ProjectSprite (thing);

		// Hack: Sprites have a tendency to extend into the ceiling in
		// sky sectors. Here we will raise the skyfix dynamically, at 
		// runtime, to make sure that no sprites get clipped by the sky.
		R_GetSpriteInfo(thing->sprite, thing->frame, &spriteInfo);
		visibleTop = thing->z + (spriteInfo.height << FRACBITS);

		if(sec->ceilingpic == skyflatnum 
			&& visibleTop > sec->ceilingheight + (sec->skyfix << FRACBITS))
		{
			// Raise sector skyfix.
			sec->skyfix = ((visibleTop - sec->ceilingheight) 
				>> FRACBITS) + 16; // Add some leeway.

			// This'll adjust all adjacent sectors.
			R_SkyFix();
		}
	}
}

/*
========================
=
= R_SortVisSprites
=
========================
*/

vissprite_t	vsprsortedhead;

void R_SortVisSprites (void)
{
	int			i, count;
	vissprite_t	*ds, *best = 0;
	vissprite_t	unsorted;
	float		bestdist;

	count = vissprite_p - vissprites;

	unsorted.next = unsorted.prev = &unsorted;
	if(!count) return;

	for(ds=vissprites; ds<vissprite_p; ds++)
	{
		ds->next = ds+1;
		ds->prev = ds-1;
	}
	vissprites[0].prev = &unsorted;
	unsorted.next = &vissprites[0];
	(vissprite_p-1)->next = &unsorted;
	unsorted.prev = vissprite_p-1;

	//
	// Pull the vissprites out by distance.
	//
	vsprsortedhead.next = vsprsortedhead.prev = &vsprsortedhead;
	for(i=0; i<count; i++)
	{
		bestdist = 0;
		for(ds=unsorted.next ; ds!= &unsorted ; ds=ds->next)
		{
			if(ds->distance >= bestdist)
			{
				bestdist = ds->distance;
				best = ds;
			}
		}
		best->next->prev = best->prev;
		best->prev->next = best->next;
		best->next = &vsprsortedhead;
		best->prev = vsprsortedhead.prev;
		vsprsortedhead.prev->next = best;
		vsprsortedhead.prev = best;
	}
}

