
//**************************************************************************
//**
//** R_DATA.C 
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_network.h"
#include "de_refresh.h"
#include "de_graphics.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int			r_precache_skins = true;
int			r_precache_sprites = false;
int			r_unload_unneeded = false;

lumptexinfo_t *lumptexinfo = 0;
int			numlumptexinfo = 0;
int			numflats;
flat_t		*flats;
int			firstpatch, lastpatch, numpatches;
int			numtextures;
texture_t	**textures;
int			*texturetranslation;	// for global animation

// Glowing textures are always rendered fullbright.
int			r_texglow = true;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// R_FindFlat
//	Returns a flat_t* for the given lump, if one already exists.
//===========================================================================
flat_t *R_FindFlat(int lumpnum)
{
	int i;
	
	for(i = 0; i < numflats; i++)
		if(flats[i].lump == lumpnum) return flats + i;
	return NULL;
}

//===========================================================================
// R_GetFlat
//	Flat management.
//===========================================================================
flat_t *R_GetFlat(int lumpnum)
{
	flat_t *f = R_FindFlat(lumpnum);
	
	// Check if this lump has already been loaded as a flat.
	if(f) return f;

	// Hmm, this is an entirely new flat.
	// FIXME: This kind of reallocation (+1, +1, ...) is inefficient.
	flats = Z_Realloc(flats, sizeof(flat_t) * ++numflats, PU_FLAT);

	// Init the new one.
	f = flats + numflats - 1;
	memset(f, 0, sizeof(*f));
	f->lump = lumpnum;
	f->translation = lumpnum; 
	memset(f->color.rgb, 0xff, 3);
	return f;
}

//===========================================================================
// R_SetFlatTranslation
//===========================================================================
int R_SetFlatTranslation(int flat, int translateTo)
{
	flat_t *f = R_GetFlat(flat);
	int old = f->translation;

	f->translation = translateTo;
	return old;
/*	int old = flats[flat].translation;
	if(flat >= numflats) Con_Error("R_SetFlatTranslation: flat >= numflats!\n");
	flats[flat].translation = translateTo;
	return old;*/
}

//===========================================================================
// R_SetTextureTranslation
//===========================================================================
int R_SetTextureTranslation(int tex, int translateTo)
{
	int old = texturetranslation[tex];
	texturetranslation[tex] = translateTo;
	return old;
}

//===========================================================================
// R_SetAnimGroup
//	Textures/flats in the same animation group are precached at the same
//	time. 'type' can be either DD_TEXTURE or DD_FLAT.
//===========================================================================
void R_SetAnimGroup(int type, int number, int group)
{
	flat_t *flat;

	switch(type)
	{
	case DD_TEXTURE:
		if(number < 0 || number >= numtextures) break;
		textures[number]->group = group;
		break;

	case DD_FLAT:
		if(number < 0 || number >= numlumps) break;
		flat = R_GetFlat(number);
		flat->group = group;
		break;
	}
}

//===========================================================================
// R_InitSwitchAnimGroups
//	Assigns switch texture pairs (SW1/SW2) to their own texture groups.
//	This'll allow them to be precached at the same time.
//===========================================================================
void R_InitSwitchAnimGroups(void)
{
	int i, k, groupCounter = 2000; // Arbitrarily chosen number.

	for(i = 0; i < numtextures; i++)
	{
		// Is this a switch texture?
		if(strnicmp(textures[i]->name, "SW1", 3)) continue;

		// Find the corresponding SW2.
		for(k = 0; k < numtextures; k++)
		{
			// Could this be it?
			if(strnicmp(textures[k]->name, "SW2", 3)) continue;

			if(!strnicmp(textures[k]->name + 3, textures[i]->name + 3, 5))
			{
				// Assign to the same group.
				textures[i]->group = textures[k]->group = groupCounter++;
				break;
			}
		}
	}
}

#if 0 //<----OBSOLETE---->
/*
==============================================================================

						MAPTEXTURE_T CACHING

when a texture is first needed, it counts the number of composite columns
required in the texture and allocates space for a column directory and any
new columns.  The directory will simply point inside other patches if there
is only one patch in a given column, but any columns with multiple patches
will have new column_ts generated.

==============================================================================
*/

/*
===================
=
= R_DrawColumnInCache
=
= Clip and draw a column from a patch into a cached post
=
===================
*/

void R_DrawColumnInCache (column_t *patch, byte *cache, int originy, 
						  int cacheheight)
{
	int		count, position;
	byte	*source, *dest;
	
	dest = (byte *)cache + 3;
	
	while (patch->topdelta != 0xff)
	{
		source = (byte *)patch + 3;
		count = patch->length;
		position = originy + patch->topdelta;
		if (position < 0)
		{
			count += position;
			position = 0;
		}
		if (position + count > cacheheight)
			count = cacheheight - position;
		if (count > 0)
			memcpy (cache + position, source, count);
		
		patch = (column_t *)(  (byte *)patch + patch->length + 4);
	}
}


/*
===================
=
= R_GenerateComposite
=
===================
*/

void R_GenerateComposite (int texnum)
{
	byte		*block;
	texture_t	*texture;
	texpatch_t	*patch;	
	patch_t		*realpatch;
	int			x, x1, x2;
	int			i;
	column_t	*patchcol;
	short		*collump;
	unsigned short *colofs;
	
	texture = textures[texnum];
	block = Z_Malloc (texturecompositesize[texnum], PU_REFRESHTEX, 
		&texturecomposite[texnum]);	
	collump = texturecolumnlump[texnum];
	colofs = texturecolumnofs[texnum];
		
//
// composite the columns together
//
	patch = texture->patches;
		
	for (i=0 , patch = texture->patches; i<texture->patchcount ; i++, patch++)
	{
		realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
		x1 = patch->originx;
		x2 = x1 + SHORT(realpatch->width);

		if (x1<0)
			x = 0;
		else
			x = x1;
		if (x2 > texture->width)
			x2 = texture->width;

		for ( ; x<x2 ; x++)
		{
			if (collump[x] >= 0)
				continue;		// column does not have multiple patches
			patchcol = (column_t *)((byte *)realpatch + 
				LONG(realpatch->columnofs[x-x1]));
			R_DrawColumnInCache (patchcol, block + colofs[x], patch->originy,
				texture->height);
		}
						
	}

// now that the texture has been built, it is purgable
	Z_ChangeTag (block, PU_CACHE);
}

//===========================================================================
// R_GenerateLookup
//===========================================================================
void R_GenerateLookup (int texnum)
{
	texture_t	*texture;
	byte		*patchcount;		// [texture->width]
	texpatch_t	*patch;	
	patch_t		*realpatch;
	int			x, x1, x2;
	int			i;
	short		*collump;
	unsigned short	*colofs;
	
	texture = textures[texnum];

	texturecomposite[texnum] = 0;	// composited not created yet
	texturecompositesize[texnum] = 0;
	collump = texturecolumnlump[texnum];
	colofs = texturecolumnofs[texnum];
	
//
// count the number of columns that are covered by more than one patch
// fill in the lump / offset, so columns with only a single patch are
// all done
//
	patchcount = (byte *) Z_Malloc(texture->width, PU_STATIC, 0);
	memset (patchcount, 0, texture->width);
	patch = texture->patches;
		
	for (i=0 , patch = texture->patches; i<texture->patchcount ; i++, patch++)
	{
		realpatch = W_CacheLumpNum (patch->patch, PU_CACHE);
		x1 = patch->originx;
		x2 = x1 + SHORT(realpatch->width);
		if (x1 < 0)
			x = 0;
		else
			x = x1;
		if (x2 > texture->width)
			x2 = texture->width;
		for ( ; x<x2 ; x++)
		{
			patchcount[x]++;
			collump[x] = patch->patch;
			colofs[x] = LONG(realpatch->columnofs[x-x1])+3;
		}
	}
	
	for (x=0 ; x<texture->width ; x++)
	{
		if (!patchcount[x])
		{
			ST_Message ("R_GenerateLookup: column without a patch (%s)\n", 
				texture->name);
			free(patchcount);
			return;
		}
		//Con_Error ("R_GenerateLookup: column without a patch");
		if (patchcount[x] > 1)
		{
			collump[x] = -1;	// use the cached block
			colofs[x] = texturecompositesize[texnum];
			if (texturecompositesize[texnum] > 0x10000-texture->height)
				Con_Error ("R_GenerateLookup: texture %i is >64k",texnum);
			texturecompositesize[texnum] += texture->height;
		}
	}	
	Z_Free(patchcount);
}

//===========================================================================
// R_GetColumn
//	FIXME: This routine should be gotten rid of! That way texture lookup
//	generation could be forgotten for good.
//===========================================================================
byte *R_GetColumn (int tex, int col)
{
	int	lump, ofs;
	
	col &= texturewidthmask[tex];
	lump = texturecolumnlump[tex][col];
	ofs = texturecolumnofs[tex][col];
	if (lump > 0)	
		return (byte *)W_CacheLumpNum(lump, PU_CACHE) + ofs;
	if (!texturecomposite[tex])
		R_GenerateComposite (tex);
	return texturecomposite[tex] + ofs;
}

#endif //<----OBSOLETE---->

//===========================================================================
// R_InitTextures
//	Initializes the texture list with the textures from the world map.
//===========================================================================
void R_InitTextures (void)
{
	maptexture_t *mtexture;
	texture_t	*texture;
	mappatch_t	*mpatch;
	texpatch_t	*patch;
	int			i,j;
	int			*maptex, *maptex2, *maptex1;
	char		name[9], *names, *name_p;
	int			*patchlookup;
	int			nummappatches;
	int			offset, maxoff, maxoff2;
	int			numtextures1, numtextures2;
	int			*directory;
	char		buf[64];

	// Load the patch names from the PNAMES lump.
	name[8] = 0;
	names = W_CacheLumpName("PNAMES", PU_REFRESHTEX);
	nummappatches = LONG ( *((int *)names) );
	name_p = names+4;
	patchlookup = Z_Malloc(nummappatches * sizeof(*patchlookup), PU_STATIC, 0);
	for (i=0 ; i<nummappatches ; i++)
	{
		strncpy (name,name_p+i*8, 8);
		patchlookup[i] = W_CheckNumForName (name);
	}
	Z_Free (names);

	// Load the map texture definitions from TEXTURE1/2.
	maptex = maptex1 = W_CacheLumpName ("TEXTURE1", PU_REFRESHTEX);
	numtextures1 = LONG(*maptex);
	maxoff = W_LumpLength (W_GetNumForName ("TEXTURE1"));
	directory = maptex+1;

	if (W_CheckNumForName ("TEXTURE2") != -1)
	{
		maptex2 = W_CacheLumpName ("TEXTURE2", PU_REFRESHTEX);
		numtextures2 = LONG(*maptex2);
		maxoff2 = W_LumpLength (W_GetNumForName ("TEXTURE2"));
	}
	else
	{
		maptex2 = NULL;
		numtextures2 = 0;
		maxoff2 = 0;
	}
	numtextures = numtextures1 + numtextures2;

	// FIXME: Surely not all of these are still needed?
	textures = Z_Malloc (numtextures*4, PU_REFRESHTEX, 0);
#if 0
	texturecolumnlump = Z_Malloc (numtextures*4, PU_REFRESHTEX, 0);
	texturecolumnofs = Z_Malloc (numtextures*4, PU_REFRESHTEX, 0);
	texturecomposite = Z_Malloc (numtextures*4, PU_REFRESHTEX, 0);
	texturecompositesize = Z_Malloc (numtextures*4, PU_REFRESHTEX, 0);
	texturewidthmask = Z_Malloc (numtextures*4, PU_REFRESHTEX, 0);
	textureheight = Z_Malloc (numtextures*4, PU_REFRESHTEX, 0);
#endif

	sprintf(buf, "R_Init: Initializing %i textures...", numtextures);
	Con_InitProgress(buf, numtextures);

	for (i=0 ; i<numtextures ; i++, directory++)
	{
		Con_Progress(1, PBARF_DONTSHOW);

		if (i == numtextures1)
		{	// Start looking in second texture file.
			maptex = maptex2;
			maxoff = maxoff2;
			directory = maptex+1;
		}

		offset = LONG(*directory);
		if (offset > maxoff)
			Con_Error ("R_InitTextures: bad texture directory");
		mtexture = (maptexture_t *) ( (byte *)maptex + offset);
		texture = textures[i] = Z_Calloc (sizeof(texture_t) 
			+ sizeof(texpatch_t)*(SHORT(mtexture->patchcount)-1), 
			PU_REFRESHTEX, 0);
		texture->width = SHORT(mtexture->width);
		texture->height = SHORT(mtexture->height);
		texture->flags = mtexture->masked? TXF_MASKED : 0;

		texture->patchcount = SHORT(mtexture->patchcount);
		memcpy (texture->name, mtexture->name, 8);
		mpatch = &mtexture->patches[0];
		patch = &texture->patches[0];
		for (j=0 ; j<texture->patchcount ; j++, mpatch++, patch++)
		{
			patch->originx = SHORT(mpatch->originx);
			patch->originy = SHORT(mpatch->originy);
			patch->patch = patchlookup[SHORT(mpatch->patch)];
			if(patch->patch == -1)
			{
				Con_Error("R_InitTextures: Missing patch in texture %s", 
					texture->name);
			}
		}		
#if 0
		texturecolumnlump[i] = Z_Malloc (texture->width*2, PU_REFRESHTEX,0);
		texturecolumnofs[i] = Z_Malloc (texture->width*2, PU_REFRESHTEX,0);

		j = 1;
		while (j*2 <= texture->width)
			j<<=1;
		texturewidthmask[i] = j-1;
		textureheight[i] = texture->height<<FRACBITS;
#endif
	}

	Z_Free (maptex1);
	if (maptex2)
		Z_Free (maptex2);

#if 0
	//
	// Precalculate whatever possible.
	//		
	// FIXME: Surely texture lookup generation is no longer required?
	for (i = 0; i < numtextures; i++)
	{
		R_GenerateLookup (i);
		DD_Progress(1, PBARF_DONTSHOW);
	}
#endif

	Con_HideProgress();

	// Translation table for global animation.
	texturetranslation = Z_Malloc ((numtextures+1)*4, PU_REFRESHTEX, 0);
	for (i=0 ; i<numtextures ; i++)
		texturetranslation[i] = i;

	Z_Free(patchlookup);

	// Assign switch texture pairs (SW1/SW2) to their own texture groups.
	// This'll allow them to be precached at the same time.
	R_InitSwitchAnimGroups();
}

//===========================================================================
// R_UpdateTextures
//===========================================================================
void R_UpdateTextures (void)
{
	Z_FreeTags(PU_REFRESHTEX, PU_REFRESHTEX);
	R_InitTextures();		
}

//===========================================================================
// R_TextureFlags
//===========================================================================
int R_TextureFlags(int texture)
{
	if(!r_texglow) return 0;
	texture = texturetranslation[texture];
	if(!texture) return 0;
	return textures[texture]->flags;
}

//===========================================================================
// R_FlatFlags
//===========================================================================
int R_FlatFlags(int flat)
{
	flat_t *fl = R_GetFlat(flat);

	if(!r_texglow) return 0;
	return fl->flags;
}

//===========================================================================
// R_InitFlats
//===========================================================================
void R_InitFlats (void)
{
	flats = 0;
	numflats = 0;
}

//===========================================================================
// R_UpdateFlats
//===========================================================================
void R_UpdateFlats (void)
{
	Z_FreeTags(PU_FLAT, PU_FLAT);
	R_InitFlats();
}

//===========================================================================
// R_InitLumpTexInfo
//===========================================================================
void R_InitLumpTexInfo(void)
{
	if(lumptexinfo) Z_Free(lumptexinfo);

	// Allocate one info per lump.
	numlumptexinfo = numlumps;
	lumptexinfo = Z_Calloc(sizeof(*lumptexinfo) * numlumps, PU_STATIC, 0);
}

//===========================================================================
// R_InitData
//	Locates all the lumps that will be used by all views.
//	Must be called after W_Init.
//===========================================================================
void R_InitData(void)
{
	R_InitTextures();
	R_InitFlats();
	R_InitLumpTexInfo();
	Cl_InitTranslations();
}

//===========================================================================
// R_UpdateData
//===========================================================================
void R_UpdateData(void)
{
	R_UpdateTextures();
	R_UpdateFlats();
	R_InitLumpTexInfo();
	Cl_InitTranslations();
}

//===========================================================================
// R_InitTranslationTables
//===========================================================================
void R_InitTranslationTables(void)
{
	int i;
	byte *transLump;

	// Allocate translation tables
	translationtables = Z_Malloc(256*3*(/*MAXPLAYERS*/8-1)+255, 
		PU_REFRESHTRANS, 0);
	translationtables = (byte *)(((int)translationtables+255)&~255);

	for(i = 0; i < 3*(/*MAXPLAYERS*/8-1); i++)
	{
		// If this can't be found, it's reasonable to expect that the game dll
		// will initialize the translation tables as it wishes.
		if(W_CheckNumForName("trantbl0") < 0) break;

		transLump = W_CacheLumpNum(W_GetNumForName("trantbl0")+i, PU_STATIC);
		memcpy(translationtables+i*256, transLump, 256);
		Z_Free(transLump);
	}
}

//===========================================================================
// R_UpdateTranslationTables
//===========================================================================
void R_UpdateTranslationTables (void)
{
	Z_FreeTags(PU_REFRESHTRANS, PU_REFRESHTRANS);
	R_InitTranslationTables();
}

//===========================================================================
// R_FlatNumForName
//===========================================================================
int	R_FlatNumForName (char *name)
{
	int		i;
	char	namet[9];

	i = W_CheckNumForName (name);
	if (i == -1)
	{
		namet[8] = 0;
		memcpy (namet, name,8);
		Con_Error ("R_FlatNumForName: %.8s not found", namet);
	}
	return i; //R_GetFlatIndex(i);//i - firstflat;
}

//===========================================================================
// R_CheckTextureNumForName
//===========================================================================
int	R_CheckTextureNumForName (char *name)
{
	int		i;
	
	if (name[0] == '-')		// no texture marker
		return 0;
		
	for (i=0 ; i<numtextures ; i++)
		if (!strncasecmp (textures[i]->name, name, 8) )
			return i;
		
	return -1;
}

//===========================================================================
// R_TextureNumForName
//===========================================================================
int	R_TextureNumForName (char *name)
{
	int		i;

	i = R_CheckTextureNumForName(name);
	if(i == -1) Con_Error("R_TextureNumForName: %.8s not found!\n", name);
	return i;
}

//===========================================================================
// R_TextureNameForNum
//===========================================================================
char* R_TextureNameForNum(int num)
{
	if(num < 0 || num > numtextures-1) return NULL;
	return textures[num]->name;
}

//===========================================================================
// R_IsCustomTexture
//	Returns true if the texture is probably not from the original game.
//===========================================================================
boolean R_IsCustomTexture(int texture)
{
	int i, lump;

	// First check the texture definitions.
	lump = W_CheckNumForName("TEXTURE1");
	if(lump >= 0 && !W_IsFromIWAD(lump)) return true;

	lump = W_CheckNumForName("TEXTURE2");
	if(lump >= 0 && !W_IsFromIWAD(lump)) return true;

	// Go through the patches.
	for(i = 0; i < textures[texture]->patchcount; i++)
	{
		if(!W_IsFromIWAD(textures[texture]->patches[i].patch))
			return true;
	}

	// This is most likely from the original game data.
	return false;
}

//===========================================================================
// R_IsAllowedDecoration
//	Returns true if the given decoration works under the specified 
//	circumstances.
//===========================================================================
boolean R_IsAllowedDecoration
	(ded_decor_t *def, int index, boolean hasExternal)
{
	if(hasExternal)
		return (def->flags & DCRF_EXTERNAL) != 0;

	if(def->is_texture)
	{
		// Is it probably an original texture?
		if(!R_IsCustomTexture(index))
			return !(def->flags & DCRF_NO_IWAD);
	}
	else
	{
		if(W_IsFromIWAD(index))
			return !(def->flags & DCRF_NO_IWAD);
	}

	return (def->flags & DCRF_PWAD) != 0;
}

//===========================================================================
// R_PrecacheFlat
//	Prepares the specified flat and all the other flats in the same
//	animation group. Has the consequence that all lumps inside the 
//	F_START...F_END block obtain a flat_t record.
//===========================================================================
void R_PrecacheFlat(int num)
{
	int i;
	flat_t *f = R_GetFlat(num), *test;

	if(f->group)
	{
		// Iterate over all flats. We assume that all flats have been 
		// enclosed inside an F_START...F_END block, per the specs.
		// Note that the WAD loader will merge all F_START...F_END blocks
		// in the loaded files into one continuous range.
		for(i = 0; i < numlumps; i++)
			if(lumpinfo[i].group == LGT_FLATS)
			{
				test = R_FindFlat(i);
				if(test && test->group == f->group)
					GL_BindTexture(GL_PrepareFlat(i));
			}
	}
	else
	{
		GL_BindTexture(GL_PrepareFlat(num));
	}
}

//===========================================================================
// R_PrecacheTexture
//	Prepares the specified texture and all the other textures in the
//	same animation group.
//===========================================================================
void R_PrecacheTexture(int num)
{
	int i;

	if(textures[num]->group)
	{
		for(i = 0; i < numtextures; i++)
			if(textures[i]->group == textures[num]->group)
				GL_BindTexture(GL_PrepareTexture(i));
	}
	else
	{
		// Just this one texture.
		GL_BindTexture(GL_PrepareTexture(num));
	}
}

//===========================================================================
// R_PrecacheLevel
//	Prepare all relevant skins, textures, flats and sprites.
//	Doesn't unload anything, though (so that if there's enough
//	texture memory it will be used more efficiently). That much trust
//	is placed in the GL/D3D drivers. The prepared textures are also bound
//	here once so they should be ready for use ASAP.
//===========================================================================
void R_PrecacheLevel (void)
{
	char			*texturepresent;
	char			*spritepresent;
	int				i,j,k, lump, mocount;
	thinker_t		*th;
	spriteframe_t	*sf;
	float			starttime;

	// Don't precache when playing demo.
	if(isDedicated || playback) 
	{
		Con_HideProgress();
		return;
	}
	
	Con_InitProgress("Setting up level: Precaching...", -1);

	starttime = Sys_GetSeconds();

	// Precache flats.
	for(i = 0; i < numsectors; i++)
	{
		R_PrecacheFlat( SECTOR_PTR(i)->floorpic );
		R_PrecacheFlat( SECTOR_PTR(i)->ceilingpic );
		if(i % SAFEDIV(numsectors, 10) == 0) Con_Progress(1, PBARF_DONTSHOW);
	}

	// Precache textures.
	texturepresent = Z_Malloc(numtextures, PU_STATIC, 0);
	memset (texturepresent, 0, numtextures);
	
	for(i = 0 ; i < numsides ; i++)
	{
		texturepresent[ SIDE_PTR(i)->toptexture ] = 1;
		texturepresent[ SIDE_PTR(i)->midtexture ] = 1;
		texturepresent[ SIDE_PTR(i)->bottomtexture ] = 1;
	}
	
	// FIXME: Precache sky textures!

	for(i = 0; i < numtextures; i++)
		if(texturepresent[i])
		{
			R_PrecacheTexture(i);
			if(i % SAFEDIV(numtextures, 10) == 0) Con_Progress(1, PBARF_DONTSHOW);
		}
	
	// Precache sprites.
	spritepresent = Z_Malloc(numsprites, PU_STATIC, 0);
	memset (spritepresent, 0, numsprites);
	
	for(th = thinkercap.next, mocount = 0; th != &thinkercap; th = th->next)
	{
		if(th->function != gx.MobjThinker) continue;
		spritepresent[((mobj_t *)th)->sprite] = 1;
		mocount++;
	}

	// Precache skins?
	if(useModels && r_precache_skins)
	{
		for(i = 0, th = thinkercap.next; th != &thinkercap; th = th->next)
		{
			if(th->function != gx.MobjThinker) continue;
			// Advance progress bar.
			if(++i % SAFEDIV(mocount, 10) == 0) 
				Con_Progress(2, PBARF_DONTSHOW);
			// Precache all the skins for the mobj.
			R_PrecacheSkinsForMobj( (mobj_t*) th);
		}
	}
	
	if(r_precache_sprites)
	{
		for(i = 0; i < numsprites ; i++)
		{
			if(i % SAFEDIV(numsprites, 10) == 0) 
				Con_Progress(1, PBARF_DONTSHOW);
			
			if (!spritepresent[i] || !useModels)
				continue;
			for(j = 0; j < sprites[i].numframes; j++)
			{
				sf = &sprites[i].spriteframes[j];
				for (k = 0; k < 8; k++)
				{
					lump = spritelumps[sf->lump[k]].lump;
					GL_BindTexture(GL_PrepareSprite(sf->lump[k], 0));
				}
			}
		}
	}

	/*ST_Message("PrecacheLevel: %i bytes of cache used.\n  Flats:%i, textures:%i, sprites:%i.\n",
		flatmemory + texturememory + spritememory, 
		flatmemory, texturememory, spritememory);*/

	//Z_Free(flatpresent);
	Z_Free(texturepresent);
	Z_Free(spritepresent);

	if(verbose)
	{
		Con_Message("Precaching took %.2f seconds.\n", 
			Sys_GetSeconds() - starttime);
	}

	// Done!
	Con_Progress(100, PBARF_SET);
	Con_HideProgress();
}
