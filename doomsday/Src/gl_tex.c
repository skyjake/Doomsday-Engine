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
 * gl_tex.c: Texture Management
 *
 * Much of this stuff actually belongs in Refresh.
 * This file needs to be split into smaller portions.
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#ifdef UNIX
#	include "de_platform.h"
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_misc.h"

#include "def_main.h"
#include "ui_main.h"

// MACROS ------------------------------------------------------------------

BEGIN_PROF_TIMERS()
	PROF_UPLOAD_START,
	PROF_UPLOAD_STRETCH,
	PROF_UPLOAD_NO_STRETCHING,
	PROF_UPLOAD_STRETCHING,
	PROF_UPLOAD_NEWTEX,
	PROF_UPLOAD_TEXIMAGE,
	PROF_PNG_LOAD,
	PROF_SCALE_1,
	PROF_SCALE_2,
	PROF_SCALE_MAG,
	PROF_SCALE_MIN,
	PROF_SCALE_NO_CHANGE
END_PROF_TIMERS()

#define TEXQ_BEST 8
#define NUM_FLARES 3

#define RGB18(r, g, b) ((r)+((g)<<6)+((b)<<12))

// TYPES -------------------------------------------------------------------

// A translated sprite.
typedef struct {
	int				patch;
	DGLuint			tex;
	unsigned char	*table;
} transspr_t;

// Sky texture topline colors.
typedef struct {
	int				texidx;
	unsigned char	rgb[3];
} skycol_t;

// Model skin.
typedef struct {
	char			path[256];
	DGLuint			tex;
} skintex_t;

// Detail texture instance.
typedef struct dtexinst_s {
	struct dtexinst_s *next;
	int				lump;
	float			contrast;
	DGLuint			tex;
} dtexinst_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void	averageColorIdx(rgbcol_t *sprcol, byte *data, int w, int h,
						byte *palette, boolean has_alpha);
void	averageColorRGB(rgbcol_t *col, byte *data, int w, int h);
byte*	GL_LoadHighResFlat(image_t *img, char *name);
void	GL_DeleteDetailTexture(detailtex_t *dtex);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int		maxTexSize;		// Maximum supported texture size.
extern int		ratioLimit;
extern boolean	palettedTextureExtAvailable;
extern boolean	s3tcAvailable;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean			filloutlines = true;
boolean			loadExtAlways = false;	// Always check for extres (cvar)
boolean			paletted = false;		// Use GL_EXT_paletted_texture (cvar)
boolean			load8bit = false;		// Load textures as 8 bit? (w/paltex)
int				useSmartFilter = 0; 	// Smart filter mode (cvar: 1=hq2x)

// Convert a 18-bit RGB (666) value to a playpal index. 
// FIXME: 256kb - Too big?
byte			pal18to8[262144];	

int				mipmapping = 3, linearRaw = 1, texQuality = TEXQ_BEST; 
int				filterSprites = true;

int				pallump;

// Properties of the current texture.
float			texw = 1, texh = 1;
int				texmask = 0;	
DGLuint			curtex = 0;
detailinfo_t	*texdetail;

skycol_t		*skytop_colors = NULL;
int				num_skytop_colors = 0;

// Names of the dynamic light textures.
DGLuint			lightingTexNames[NUM_LIGHTING_TEXTURES];

int				texMagMode = 1; // Linear.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean	texInited = false;	// Init done.
static boolean	allowMaskedTexEnlarge = false;
static boolean	noHighResTex = false;
static boolean	noHighResPatches = false;
static boolean	highResWithPWAD = false;

// Raw screen lumps (just lump numbers).
static int		*rawlumps, numrawlumps;	
	
// Skinnames will only *grow*. It will never be destroyed, not even
// at resets. The skin textures themselves will be deleted, though.
// This is because we want to have permanent ID numbers for skins,
// and the ID numbers are the same as indices to the skinnames array.
// Created in r_model.c, when registering the skins.
static int		numskinnames;
static skintex_t *skinnames;

// Linked list of detail texture instances. A unique texture is generated
// for each (rounded) contrast level.
static dtexinst_t *dtinstances;

// The translated sprites.
static transspr_t *transsprites;
static int		numtranssprites;

static DGLuint	flaretexnames[NUM_FLARES];

//static boolean	gammaSupport = false;

static int glmode[6] = // Indexed by 'mipmapping'.
{ 
	DGL_NEAREST,
	DGL_LINEAR,
	DGL_NEAREST_MIPMAP_NEAREST,
	DGL_LINEAR_MIPMAP_NEAREST,
	DGL_NEAREST_MIPMAP_LINEAR,
	DGL_LINEAR_MIPMAP_LINEAR
};


// CODE --------------------------------------------------------------------

//===========================================================================
// CeilPow2
//	Finds the power of 2 that is equal to or greater than 
//	the specified number.
//===========================================================================
int CeilPow2(int num)
{
	int cumul;
	for(cumul = 1; num > cumul; cumul <<= 1);
	return cumul;
}

//===========================================================================
// FloorPow2
//	Finds the power of 2 that is less than or equal to
//	the specified number.
//===========================================================================
int FloorPow2(int num)
{
	int fl = CeilPow2(num);
	if(fl > num) fl >>= 1;
	return fl;
}

//===========================================================================
// RoundPow2
//	Finds the power of 2 that is nearest the specified number.
//	In ambiguous cases, the smaller number is returned.
//===========================================================================
int RoundPow2(int num)
{
	int cp2 = CeilPow2(num), fp2 = FloorPow2(num);
	return (cp2-num >= num-fp2)? fp2 : cp2;
}

//===========================================================================
// WeightPow2
//	Weighted rounding. Weight determines the point where the number 
//	is still rounded down (0..1).
//===========================================================================
int WeightPow2(int num, float weight)
{
	int		fp2 = FloorPow2(num);
	float	frac = (num - fp2) / (float) fp2;

	if(frac <= weight) return fp2; else return (fp2<<1);
}


//===========================================================================
// pixBlt
//	Copies a rectangular region of the source buffer to the destination
//	buffer. Doesn't perform clipping, so be careful.
//	Yeah, 13 parameters...
//===========================================================================
void pixBlt(byte *src, int srcWidth, int srcHeight, byte *dest, 
		   int destWidth, int destHeight,
		   int alpha, int srcRegX, int srcRegY, int destRegX, int destRegY,
		   int regWidth, int regHeight)
{
	int	y;	// Y in the copy region.
	int srcNumPels = srcWidth*srcHeight;
	int destNumPels = destWidth*destHeight;

	for(y = 0; y < regHeight; y++) // Copy line by line.
	{
		// The color index data.
		memcpy(dest + destRegX + (y+destRegY)*destWidth,
			src + srcRegX + (y+srcRegY)*srcWidth, 
			regWidth);

		if(alpha)
		{
			// Alpha channel data.
			memcpy(dest + destNumPels + destRegX + (y+destRegY)*destWidth,
				src + srcNumPels + srcRegX + (y+srcRegY)*srcWidth,	
				regWidth);
		}
	}
}

//===========================================================================
// LookupPal18to8
//	Prepare the pal18to8 table.
//	A time-consuming operation (64 * 64 * 64 * 256!).
//===========================================================================
static void LookupPal18to8(byte *palette)
{
	int				r, g, b, i;
	byte			palRGB[3];
	unsigned int	diff, smallestDiff, closestIndex;
		
	for(r = 0; r < 64; r++)
		for(g = 0; g < 64; g++)
			for(b = 0; b < 64; b++)
			{
				// We must find the color index that most closely
				// resembles this RGB combination.
				smallestDiff = -1;
				for(i = 0; i < 256; i++)
				{
					memcpy(palRGB, palette + 3*i, 3);
					diff = (palRGB[0]-(r<<2))*(palRGB[0]-(r<<2)) 
						+ (palRGB[1]-(g<<2))*(palRGB[1]-(g<<2)) 
						+ (palRGB[2]-(b<<2))*(palRGB[2]-(b<<2));
					if(diff < smallestDiff) 
					{
						smallestDiff = diff;
						closestIndex = i;
					}
				}
				pal18to8[RGB18(r, g, b)] = closestIndex;
			}

	if(ArgCheck("-dump_pal18to8"))
	{
		FILE *file = fopen("Pal18to8.lmp", "wb");
		fwrite(pal18to8, sizeof(pal18to8), 1, file);								
		fclose(file);
	}
}

//===========================================================================
// LoadPalette
//===========================================================================
static void LoadPalette()
{
	byte *playpal = W_CacheLumpNum(
		pallump = W_GetNumForName("PLAYPAL"), PU_CACHE);
	byte paldata[256 * 3];
	int	i, c, gammalevel = /*gammaSupport? 0 : */usegamma;

	// Prepare the color table.
	for(i = 0; i < 256; i++)
	{
		// Adjust the values for the appropriate gamma level.
		for(c = 0; c < 3; c++) 
			paldata[i*3 + c] = gammatable[gammalevel][playpal[i*3 + c]];
	}
	gl.Palette(DGL_RGB, paldata);
}

//===========================================================================
// GL_InitPalettedTexture
//	Initializes the paletted texture extension.
//	Returns true iff successful.
//===========================================================================
int GL_InitPalettedTexture()
{
	// Should the extension be used?
	if(!paletted && !ArgCheck("-paltex")) 
		return true;

	gl.Enable(DGL_PALETTED_TEXTURES);

	// Check if the operation was a success.
	if(gl.GetInteger(DGL_PALETTED_TEXTURES) == DGL_FALSE)
	{
		Con_Message("\nPaletted textures init failed!\n");
		return false;
	}
	// Textures must be uploaded as 8-bit, now.
	load8bit = true;
	return true;
}

//===========================================================================
// GL_InitTextureManager
//	This should be cleaned up once and for all.
//===========================================================================
void GL_InitTextureManager(void)
{
	int	i;

	if(novideo) return;
	if(texInited) return; // Don't init again.

	// The -bigmtex option allows the engine to enlarge masked textures
	// that have taller patches than they are themselves.
	allowMaskedTexEnlarge = ArgExists("-bigmtex");

	// Disable the use of 'high resolution' textures?
	noHighResTex = ArgExists("-nohightex");
	noHighResPatches = ArgExists("-nohighpat");

	// Should we allow using external resources with PWAD textures?
	highResWithPWAD = ArgExists("-pwadtex");

	transsprites = 0;
	numtranssprites = 0;

	// Raw screen lump book-keeping.
	rawlumps = 0;
	numrawlumps = 0;

	// The palette lump, for color information (really??!!?!?!).
	pallump = W_GetNumForName("PLAYPAL");

	// Do we need to generate a pal18to8 table?
	if(ArgCheck("-dump_pal18to8"))
		LookupPal18to8(W_CacheLumpName("PLAYPAL", PU_CACHE));

	GL_InitPalettedTexture();

	// DGL needs the palette information regardless of whether the
	// paletted textures are enabled or not.
	LoadPalette();

	// Load the pal18to8 table from the lump PAL18TO8. We need it
	// when resizing textures.
	if((i = W_CheckNumForName("PAL18TO8")) == -1)
		LookupPal18to8(W_CacheLumpNum(pallump, PU_CACHE));
	else
		memcpy(pal18to8, W_CacheLumpNum(i, PU_CACHE), sizeof(pal18to8));

	memset(flaretexnames, 0, sizeof(flaretexnames));

	// Detail textures.
	dtinstances = NULL;

	// System textures loaded in GL_LoadSystemTextures.
	memset(lightingTexNames, 0, sizeof(lightingTexNames));

	// Initialization done.
	texInited = true;

	// Initialize the smart texture filtering routines.
	GL_InitSmartFilter();
}

//===========================================================================
// GL_ShutdownTextureManager
//	Call this if a full cleanup of the textures is required (engine update).
//===========================================================================
void GL_ShutdownTextureManager()
{
	if(!texInited) return;

	PRINT_PROF(	PROF_UPLOAD_START );
	PRINT_PROF(	PROF_UPLOAD_STRETCH );
	PRINT_PROF(	PROF_UPLOAD_NO_STRETCHING );
	PRINT_PROF(	PROF_UPLOAD_STRETCHING );
	PRINT_PROF(	PROF_UPLOAD_NEWTEX );
	PRINT_PROF(	PROF_UPLOAD_TEXIMAGE );
	PRINT_PROF( PROF_PNG_LOAD );
	PRINT_PROF( PROF_SCALE_1 );
	PRINT_PROF( PROF_SCALE_2 );
	PRINT_PROF( PROF_SCALE_MAG );
	PRINT_PROF( PROF_SCALE_MIN );
	PRINT_PROF( PROF_SCALE_NO_CHANGE );

	GL_ClearTextureMemory();

	// Destroy all bookkeeping -- into the shredder, I say!!
	free(skytop_colors);
	skytop_colors = 0;
	num_skytop_colors = 0;

	texInited = false;
}

//===========================================================================
// GL_DestroySkinNames
//	This is called at final shutdown. 
//===========================================================================
void GL_DestroySkinNames(void)
{
	free(skinnames);
	skinnames = 0;
	numskinnames = 0;
}

//===========================================================================
// GL_LoadLightMap
//	Lightmaps should be monochrome images.
//===========================================================================
void GL_LoadLightMap(ded_lightmap_t *map)
{
	image_t image;
	filename_t resource;

	if(map->tex) return; // Already loaded.

	// Default texture name.
	map->tex = lightingTexNames[LST_DYNAMIC];

	if(!strcmp(map->id, "-"))
	{
		// No lightmap, if we don't know where to find the map.
		map->tex = 0;
	}
	else if(map->id[0]) // Not an empty string.
	{
		// Search an external resource.
		if(R_FindResource(RC_LIGHTMAP, map->id, "-ck", resource)
			&& GL_LoadImage(&image, resource, false))
		{
			if(!image.isMasked)
			{
				// An alpha channel is required. If one is not in the
				// image data, we'll generate it. 
				GL_ConvertToAlpha(&image, true);
			}

			map->tex = gl.NewTexture();

			// Upload the texture.
			// No mipmapping or resizing is needed, upload directly.
			gl.Disable(DGL_TEXTURE_COMPRESSION);
			gl.TexImage(image.pixelSize == 2? DGL_LUMINANCE_PLUS_A8
				: image.pixelSize == 3? DGL_RGB : DGL_RGBA, 
				image.width, image.height, 0, image.pixels);
			gl.Enable(DGL_TEXTURE_COMPRESSION);
			GL_DestroyImage(&image);

			gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
			gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
			gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
			gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

			// Copy this to all defs with the same lightmap.
			Def_LightMapLoaded(map->id, map->tex);
		}
	}
}

//===========================================================================
// GL_DeleteLightMap
//===========================================================================
void GL_DeleteLightMap(ded_lightmap_t *map)
{
	if(map->tex != lightingTexNames[LST_DYNAMIC])
	{
		gl.DeleteTextures(1, &map->tex);
	}
	map->tex = 0;
}

//===========================================================================
// GL_LoadSystemTextures
//	Prepares all the system textures (dlight, ptcgens).
//===========================================================================
void GL_LoadSystemTextures(boolean loadLightMaps)
{
	int i, k;
	ded_decor_t *decor;

	if(!texInited) return;

	UI_LoadTextures();

	// Preload lighting system textures.
	GL_PrepareLSTexture(LST_DYNAMIC);
	GL_PrepareLSTexture(LST_GRADIENT);

	if(loadLightMaps)
	{
		// Load lightmaps.
		for(i = 0; i < defs.count.lights.num; i++)
		{
			GL_LoadLightMap(&defs.lights[i].up);
			GL_LoadLightMap(&defs.lights[i].down);
			GL_LoadLightMap(&defs.lights[i].sides);
		}
		for(i = 0, decor = defs.decorations; i < defs.count.decorations.num; 
			i++, decor++)
		{
			for(k = 0; k < DED_DECOR_NUM_LIGHTS; k++)
			{
				if(!R_IsValidLightDecoration(&decor->lights[k]))
					break;
				GL_LoadLightMap(&decor->lights[k].up);
				GL_LoadLightMap(&decor->lights[k].down);
				GL_LoadLightMap(&decor->lights[k].sides);
			}

			// Generate RGB lightmaps for decorations.
			//R_GenerateDecorMap(decor);
		}
	}

	// Load particle textures.
	PG_InitTextures();
}

//===========================================================================
// GL_ClearSystemTextures
//	System textures are loaded at startup and remain in memory all the time.
//	After clearing they must be manually reloaded.
//===========================================================================
void GL_ClearSystemTextures(void)
{
	int i, k;
	ded_decor_t *decor;

	if(!texInited) return;

	for(i = 0; i < defs.count.lights.num; i++)
	{
		GL_DeleteLightMap(&defs.lights[i].up);
		GL_DeleteLightMap(&defs.lights[i].down);
		GL_DeleteLightMap(&defs.lights[i].sides);
	}
	for(i = 0, decor = defs.decorations; i < defs.count.decorations.num; 
		i++, decor++)
	{
		for(k = 0; k < DED_DECOR_NUM_LIGHTS; k++)
		{
			if(!R_IsValidLightDecoration(&decor->lights[k]))
				break;
			GL_DeleteLightMap(&decor->lights[k].up);
			GL_DeleteLightMap(&decor->lights[k].down);
			GL_DeleteLightMap(&decor->lights[k].sides);
		}
	}

	gl.DeleteTextures(NUM_LIGHTING_TEXTURES, lightingTexNames);
	memset(lightingTexNames, 0, sizeof(lightingTexNames));

	UI_ClearTextures();

	// Delete the particle textures.
	PG_ShutdownTextures();
}

//===========================================================================
// GL_ClearRuntimeTextures
//	Runtime textures are not loaded until precached or actually needed.
//	They may be cleared, in which case they will be reloaded when needed.
//===========================================================================
void GL_ClearRuntimeTextures(void)
{
	dtexinst_t *dtex;
	int i;

	if(!texInited) return;

	// The rendering lists contain persistent references to texture names.
	// Which, obviously, can't persist any longer...
	RL_DeleteLists();

	// Textures and sprite lumps.
	for(i = 0; i < numtextures; i++) GL_DeleteTexture(i);
	for(i = 0; i < numspritelumps; i++) GL_DeleteSprite(i);

	// The translated sprite textures.
	for(i = 0; i < numtranssprites; i++)
	{
		gl.DeleteTextures(1, &transsprites[i].tex);
		transsprites[i].tex = 0;
	}

	free(transsprites);
	transsprites = 0;
	numtranssprites = 0;

	// Delete skins.
	for(i = 0; i < numskinnames; i++)
	{
		gl.DeleteTextures(1, &skinnames[i].tex);
		skinnames[i].tex = 0;
	}

	// Delete detail textures.
	i = 0;
	while(dtinstances)
	{
		dtex = dtinstances->next;
		gl.DeleteTextures(1, &dtinstances->tex);
		M_Free(dtinstances);
		dtinstances = dtex;
		i++;
	}
	VERBOSE( Con_Message("GL_ClearRuntimeTextures: %i detail texture "
		"instances.\n", i) );
	for(i = 0; i < defs.count.details.num; i++)
		details[i].gltex = 0;

	// Flare textures.
	gl.DeleteTextures(NUM_FLARES, flaretexnames);
	memset(flaretexnames, 0, sizeof(flaretexnames));

	GL_DeleteRawImages();

	// Delete any remaining lump textures (e.g. flats).
	for(i = 0; i < numlumptexinfo; i++)
	{
		gl.DeleteTextures(2, lumptexinfo[i].tex);
		memset(lumptexinfo[i].tex, 0, sizeof(lumptexinfo[i].tex));
	}
}

//===========================================================================
// GL_ClearTextureMemory
//===========================================================================
void GL_ClearTextureMemory(void)
{
	if(!texInited) return;

	// Delete runtime textures (textures, flats, ...)
	GL_ClearRuntimeTextures();

	// Delete system textures.
	GL_ClearSystemTextures();
}

//===========================================================================
// GL_UpdateGamma
//===========================================================================
void GL_UpdateGamma(void)
{
	/*if(gammaSupport)
	{
		// The driver knows how to update the gamma directly.
		gl.Gamma(DGL_TRUE, gammatable[usegamma]);
	}
	else
	{*/
	LoadPalette();
	GL_ClearRuntimeTextures();
	//}
}

//===========================================================================
// GL_BindTexture
//	Binds the texture if necessary.
//===========================================================================
void GL_BindTexture(DGLuint texname)
{
	/*if(curtex != texname) 
	{*/
	gl.Bind(texname);
	curtex = texname;
	//}
}

//===========================================================================
// PalIdxToRGB
//===========================================================================
void PalIdxToRGB(byte *pal, int idx, byte *rgb)
{
	int c;
	int gammalevel = /*gammaSupport? 0 : */usegamma;

	for(c = 0; c < 3; c++) // Red, green and blue.
		rgb[c] = gammatable[gammalevel][pal[idx*3 + c]];
}

//===========================================================================
// GL_ConvertBuffer
//	in/outformat:
//	1 = palette indices
//	2 = palette indices followed by alpha values
//	3 = RGB 
//	4 = RGBA
//===========================================================================
void GL_ConvertBuffer(int width, int height, int informat, int outformat, 
					  byte *in, byte *out, boolean gamma)
{
	byte	*palette = W_CacheLumpName("playpal", PU_CACHE);
	int		inSize = (informat == 2? 1 : informat);
	int		outSize = (outformat == 2? 1 : outformat);
	int		i, numPixels = width * height, a;
	
	if(informat == outformat) 
	{
		// No conversion necessary.
		memcpy(out, in, numPixels * informat);
		return;
	}
	// Conversion from pal8(a) to RGB(A).
	if(informat <= 2 && outformat >= 3)
	{
		for(i = 0; i < numPixels; i++, in += inSize, out += outSize) 
		{
			// Copy the RGB values in every case.
			if(gamma)
			{
				for(a = 0; a < 3; a++) 
					out[a] = gammatable[usegamma][*(palette + 3*(*in) + a)];
			}
			else
			{
				memcpy(out, palette + 3*(*in), 3);
			}
			// Will the alpha channel be necessary?
			a = 0;
			if(informat == 2) a = in[numPixels*inSize];
			if(outformat == 4) out[3] = a;
		}
	}
	// Conversion from RGB(A) to pal8(a), using pal18to8.
	else if(informat >= 3 && outformat <= 2)
	{
		for(i = 0; i < numPixels; i++, in += inSize, out += outSize)
		{
			// Convert the color value.
			*out = pal18to8[RGB18(in[0]>>2, in[1]>>2, in[2]>>2)];
			// Alpha channel?
			a = 0;
			if(informat == 4) a = in[3];
			if(outformat == 2) out[numPixels*outSize] = a;
		}
	}
	else if(informat == 3 && outformat == 4)
	{
		for(i = 0; i < numPixels; i++, in += inSize, out += outSize)
		{
			memcpy(out, in, 3);
			out[3] = 0xff; // Opaque.

/*			out[0] = 0xff;
			out[1] = 0;
			out[2] = 0;*/
		}
	}
}

//===========================================================================
// scaleLine
//	Len is measured in out units. Comps is the number of components per 
//	pixel, or rather the number of bytes per pixel (3 or 4). The strides 
//	must be byte-aligned anyway, though; not in pixels.
//	FIXME: Probably could be optimized.
//===========================================================================
static void scaleLine(byte *in, int inStride, byte *out, int outStride,
					  int outLen, int inLen, int comps)
{
	int		i, c;
	float	inToOutScale = outLen / (float) inLen;

	if(inToOutScale > 1)	
	{
		// Magnification is done using linear interpolation.
		fixed_t inPosDelta = (FRACUNIT*(inLen-1))/(outLen-1), inPos = inPosDelta;
		byte *col1, *col2;
		int weight, invWeight;

		BEGIN_PROF( PROF_SCALE_MAG );

		// The first pixel.
		memcpy(out, in, comps);
		out += outStride;

		// Step at each out pixel between the first and last ones.
		for(i = 1; i < outLen - 1; i++, out += outStride, inPos += inPosDelta)
		{
			col1 = in + (inPos >> FRACBITS) * inStride;
			col2 = col1 + inStride;
			weight = inPos & 0xffff;
			invWeight = 0x10000 - weight;
			out[0] = (col1[0]*invWeight + col2[0]*weight) >> 16;
			out[1] = (col1[1]*invWeight + col2[1]*weight) >> 16;
			out[2] = (col1[2]*invWeight + col2[2]*weight) >> 16;
			if(comps == 4)
				out[3] = (col1[3]*invWeight + col2[3]*weight) >> 16;
		}

		// The last pixel.
		memcpy(out, in + (inLen - 1)*inStride, comps);

		END_PROF( PROF_SCALE_MAG );
	}
	else if(inToOutScale < 1)
	{
		// Minification needs to calculate the average of each of 
		// the pixels contained by the out pixel.
		unsigned int cumul[4] = {0, 0, 0, 0}, count = 0;
		int outpos = 0;

		BEGIN_PROF( PROF_SCALE_MIN );

		for(i = 0; i < inLen; i++, in += inStride)
		{
			if((int) (i*inToOutScale) != outpos)
			{
				outpos = (int) i*inToOutScale;
				for(c = 0; c < comps; c++) 
				{
					out[c] = cumul[c] / count;
					cumul[c] = 0;
				}
				count = 0;
				out += outStride;
			}
			for(c = 0; c < comps; c++) cumul[c] += in[c];
			count++;
		}
		// Fill in the last pixel, too.
		if(count) for(c = 0; c < comps; c++) out[c] = cumul[c] / count;

		END_PROF( PROF_SCALE_MIN );
	}
	else 
	{
		BEGIN_PROF( PROF_SCALE_NO_CHANGE );

		// No need for scaling.
		if(comps == 3)
		{
			for(i = outLen; i > 0; i--, out += outStride, in += inStride)
			{
				out[0] = in[0];
				out[1] = in[1];
				out[2] = in[2];
			}
		}
		else if(comps == 4)
		{
			for(i = outLen; i > 0; i--, out += outStride, in += inStride)
			{
				out[0] = in[0];
				out[1] = in[1];
				out[2] = in[2];
				out[3] = in[3];
			}
		}

		END_PROF( PROF_SCALE_NO_CHANGE );
	}
}

//===========================================================================
// GL_ScaleBuffer32
//===========================================================================
void GL_ScaleBuffer32
	(byte *in, int inWidth, int inHeight, byte *out, 
	 int outWidth, int outHeight, int comps)
{
	int		i;
	byte	*temp;// = Z_Malloc(outWidth * inHeight * comps, PU_STATIC, 0);

	BEGIN_PROF( PROF_SCALE_1 );

	temp = M_Malloc(outWidth * inHeight * comps);

	// First scale horizontally, to outWidth, into the temporary buffer.
	for(i = 0; i < inHeight; i++)
	{
		scaleLine(in + inWidth*comps*i, comps, temp + outWidth*comps*i, 
			comps, outWidth, inWidth, comps);
	}

	END_PROF( PROF_SCALE_1 );
	BEGIN_PROF( PROF_SCALE_2 );

	// Then scale vertically, to outHeight, into the out buffer.
	for(i = 0; i < outWidth; i++)
	{
		scaleLine(temp + comps*i, outWidth*comps, out + comps*i, 
			outWidth*comps, outHeight, inHeight, comps);
	}

	M_Free(temp);

	END_PROF( PROF_SCALE_2 );
}			

//===========================================================================
// GL_DownMipmap32
//	Works within the given data, reducing the size of the picture to half 
//	its original. Width and height must be powers of two.
//===========================================================================
void GL_DownMipmap32(byte *in, int width, int height, int comps)
{
	byte	*out;
	int		x, y, c, outW = width >> 1, outH = height >> 1;

	if(width == 1 && height == 1)
	{
#if _DEBUG
		Con_Error("GL_DownMipmap32 can't be called for a 1x1 image.\n");
#endif
		return;
	}

	if(!outW || !outH) // Limited, 1x2|2x1 -> 1x1 reduction?
	{
		int outDim = width > 1? outW : outH;
		out = in;
		for(x = 0; x < outDim; x++, in += comps*2)
			for(c = 0; c < comps; c++, out++) 
				*out = (in[c] + in[comps+c]) >> 1;
	}
	else // Unconstrained, 2x2 -> 1x1 reduction?
	{
		out = in;
		for(y = 0; y < outH; y++, in += width*comps)
			for(x = 0; x < outW; x++, in += comps*2)
				for(c = 0; c < comps; c++, out++) 
					*out = (in[c] + in[comps + c] + in[comps*width + c] + in[comps*(width+1) + c]) >> 2;
	}
}

/*
 * Determine the optimal size for a texture. Usually the dimensions are
 * scaled upwards to the next power of two. Returns true if noStretch is
 * true and the stretching can be skipped.
 */
boolean GL_OptimalSize(int width, int height, int *optWidth, int *optHeight,
					   boolean noStretch)
{
	if(noStretch)
	{
		*optWidth = CeilPow2(width);
		*optHeight = CeilPow2(height);
		
		// MaxTexSize may prevent using noStretch.
		if(*optWidth > maxTexSize)
		{
			*optWidth = maxTexSize;
			noStretch = false;
		}
		if(*optHeight > maxTexSize)
		{
			*optHeight = maxTexSize;
			noStretch = false;
		}
	}
	else
	{
		// Determine the most favorable size for the texture.
		if(texQuality == TEXQ_BEST)	// The best quality.
		{
			// At the best texture quality *opt, all textures are
			// sized *upwards*, so no details are lost. This takes
			// more memory, but naturally looks better.
			*optWidth = CeilPow2(width);
			*optHeight = CeilPow2(height);
		}
		else if(texQuality == 0)
		{
			// At the lowest quality, all textures are sized down
			// to the nearest power of 2.
			*optWidth = FloorPow2(width);
			*optHeight = FloorPow2(height);
		}
		else 
		{
			// At the other quality *opts, a weighted rounding
			// is used.
			*optWidth = WeightPow2(width, 1 - texQuality/(float)TEXQ_BEST);
			*optHeight = WeightPow2(height, 1 - texQuality/(float)TEXQ_BEST);
		}
	}

	// Hardware limitations may force us to modify the preferred
	// texture size.
	if(*optWidth > maxTexSize) *optWidth = maxTexSize;
	if(*optHeight > maxTexSize) *optHeight = maxTexSize;
	if(ratioLimit)
	{
		if(*optWidth > *optHeight) // Wide texture.
		{
			if(*optHeight < *optWidth / ratioLimit)
				*optHeight = *optWidth / ratioLimit;
		}
		else // Tall texture.
		{
			if(*optWidth < *optHeight / ratioLimit)
				*optWidth = *optHeight / ratioLimit;
		}
	}

	return noStretch;
}

//===========================================================================
// GL_UploadTexture
//	Can be rather time-consuming.
//	Returns the name of the texture. 
//	The texture parameters will NOT be set here.
//	'data' contains indices to the playpal. If 'alphachannel' is true,
//	'data' also contains the alpha values (after the indices).
//===========================================================================
DGLuint GL_UploadTexture
	(byte *data, int width, int height, boolean alphaChannel, 
	 boolean generateMipmaps, boolean RGBData, boolean noStretch)
{
	int	i, levelWidth, levelHeight;	// width and height at the current level
	int comps;
	byte *buffer, *rgbaOriginal, *idxBuffer;
	DGLuint	texName;
	boolean freeOriginal;
	boolean freeBuffer;

	// Number of color components in the destination image.
	comps = (alphaChannel? 4 : 3);

	BEGIN_PROF( PROF_UPLOAD_START );

	// Calculate the real dimensions for the texture, as required by
	// the graphics hardware.
	noStretch = GL_OptimalSize(width, height, &levelWidth, &levelHeight,
							   noStretch);
	
	// Get the RGB(A) version of the original texture.
	if(RGBData)
	{
		// The source image can be used as-is.
		freeOriginal = false;
		rgbaOriginal = data;
	}
	else
	{
		// Convert a paletted source image to truecolor so it can be scaled.
		freeOriginal = true;
		rgbaOriginal = M_Malloc(width * height * comps);
		GL_ConvertBuffer(width, height, alphaChannel? 2 : 1, comps, 
			data, rgbaOriginal,	!load8bit);
	}

	// If smart filtering is enabled, all textures are magnified 2x.
	if(useSmartFilter/* && comps == 3*/)
	{
		byte *filtered = M_Malloc(4 * width * height * 4);

		if(comps == 3)
		{
			// Must convert to RGBA.
			byte *temp = M_Malloc(4 * width * height);
			GL_ConvertBuffer(width, height, 3, 4, rgbaOriginal, temp,
							 !load8bit);
			if(freeOriginal) M_Free(rgbaOriginal);
			rgbaOriginal = temp;
			freeOriginal = true;			
			comps = 4;
			alphaChannel = true;
		}

		GL_SmartFilter2x(rgbaOriginal, filtered, width, height, width * 8);
		width *= 2;
		height *= 2;
		noStretch = GL_OptimalSize(width, height, &levelWidth, &levelHeight,
								   noStretch);

		/*memcpy(filtered, rgbaOriginal, comps * width * height);*/

		// The filtered copy is now the 'original' image data.
		if(freeOriginal) M_Free(rgbaOriginal);
		rgbaOriginal = filtered;
		freeOriginal = true;
	}
	
	END_PROF( PROF_UPLOAD_START );	
	
	BEGIN_PROF( PROF_UPLOAD_STRETCH );

	// Prepare the RGB(A) buffer for the texture: we want a buffer with 
	// power-of-two dimensions. It will be the mipmap level zero.
	// The buffer will be modified in the mipmap generation (if done here).
	if(width == levelWidth && height == levelHeight)
	{
		// No resizing necessary.
		buffer = rgbaOriginal;
		freeBuffer = freeOriginal;
		freeOriginal = false;		
	}
	else
	{
		freeBuffer = true;
		buffer = M_Malloc(levelWidth * levelHeight * comps);
		if(noStretch)
		{
			BEGIN_PROF( PROF_UPLOAD_NO_STRETCHING );

			// Copy the image into a buffer with power-of-two dimensions.
			memset(buffer, 0, levelWidth * levelHeight * comps);
			for(i = 0; i < height; i++) // Copy line by line.
				memcpy(buffer + levelWidth*comps*i, rgbaOriginal + width*comps*i, 
					comps*width);

			END_PROF( PROF_UPLOAD_NO_STRETCHING );
		}
		else
		{
			BEGIN_PROF( PROF_UPLOAD_STRETCHING );

			// Stretch to fit into power-of-two.
			if(width != levelWidth || height != levelHeight)
			{
				GL_ScaleBuffer32(rgbaOriginal, width, height, buffer, levelWidth, 
					levelHeight, comps);
			}

			END_PROF( PROF_UPLOAD_STRETCHING );
		}
	}
	END_PROF( PROF_UPLOAD_STRETCH );

	BEGIN_PROF( PROF_UPLOAD_NEWTEX );

	// The RGB(A) copy of the source image is no longer needed.
	if(freeOriginal) M_Free(rgbaOriginal);
	rgbaOriginal = NULL;
	
	// Generate a new texture name and bind it.
	texName = gl.NewTexture();

	END_PROF( PROF_UPLOAD_NEWTEX );

	if(load8bit)
	{
		int canGenMips;
		gl.GetIntegerv(DGL_PALETTED_GENMIPS, &canGenMips);

		// Prepare the palette indices buffer, to be handed over to DGL. 
		idxBuffer = M_Malloc(levelWidth * levelHeight * (alphaChannel? 2 : 1));

		// Since this is a paletted texture, we may need to manually
		// generate the mipmaps.
		for(i = 0; levelWidth || levelHeight; i++)
		{
			if(!levelWidth) levelWidth = 1;
			if(!levelHeight) levelHeight = 1;
		
			// Convert to palette indices.
			GL_ConvertBuffer(levelWidth, levelHeight, comps,
				alphaChannel? 2 : 1, buffer, idxBuffer, false);

			// Upload it.
			if(gl.TexImage(alphaChannel? DGL_COLOR_INDEX_8_PLUS_A8 
				: DGL_COLOR_INDEX_8, levelWidth, levelHeight, 
				generateMipmaps && canGenMips? DGL_TRUE 
				: generateMipmaps? -i : DGL_FALSE, idxBuffer) != DGL_OK)
			{
				Con_Error("GL_UploadTexture: TexImage failed (%i x %i) as 8-bit, alpha:%i\n",
					levelWidth, levelHeight, alphaChannel);
			}			

			// If no mipmaps need to generated, quit now.
			if(!generateMipmaps || canGenMips) break;
			
			if(levelWidth > 1 || levelHeight > 1)
				GL_DownMipmap32(buffer, levelWidth, levelHeight, comps);

			// Move on.
			levelWidth >>= 1;
			levelHeight >>= 1;
		}

		M_Free(idxBuffer);
	}
	else
	{
		BEGIN_PROF( PROF_UPLOAD_TEXIMAGE );

		// DGL knows how to generate mipmaps for RGB(A) textures.
		if(gl.TexImage(alphaChannel? DGL_RGBA : DGL_RGB, levelWidth, 
			levelHeight, generateMipmaps? DGL_TRUE : DGL_FALSE, 
			buffer) != DGL_OK)
		{
			Con_Error("GL_UploadTexture: TexImage failed (%i x %i), alpha:%i\n",
				levelWidth, levelHeight, alphaChannel);
		}

		END_PROF( PROF_UPLOAD_TEXIMAGE );
	}

	if(freeBuffer) M_Free(buffer);
	
	return texName;
}

//===========================================================================
// GL_GetDetailInstance
//	The contrast is rounded.
//===========================================================================
dtexinst_t *GL_GetDetailInstance(int lump, float contrast)
{
	dtexinst_t *i;

	// Round off the contrast to nearest 0.1.
	contrast = (int)((contrast + .05) * 10) / 10.0;

	for(i = dtinstances; i; i = i->next)
	{
		if(i->lump == lump && i->contrast == contrast)
			return i;
	}

	// Create a new instance.
	i = M_Malloc(sizeof(dtexinst_t));
	i->next = dtinstances;
	dtinstances = i;
	i->lump = lump;
	i->contrast = contrast;
	i->tex = 0;
	return i;
}

//===========================================================================
// GL_LoadDetailTexture
//	Detail textures are grayscale images.
//===========================================================================
DGLuint GL_LoadDetailTexture(int num, float contrast)
{
	byte *lumpData, *image;
	int w = 256, h = 256;
	dtexinst_t *inst;
		
	if(num < 0) return 0; // No such lump?!
	lumpData = W_CacheLumpNum(num, PU_STATIC);

	// Apply the global detail contrast factor.
	contrast *= detailFactor;

	// Have we already got an instance of this texture loaded?
	inst = GL_GetDetailInstance(num, contrast);
	if(inst->tex) return inst->tex;

	// Detail textures are faded to gray depending on the contrast factor.
	// The texture is also progressively faded towards gray when each
	// mipmap level is loaded.
	gl.SetInteger(DGL_GRAY_MIPMAP, contrast * 255);

	// First try loading it as a PCX image.
	if(PCX_MemoryGetSize(lumpData, &w, &h))
	{
		// Nice...
		image = M_Malloc(w*h*3);
		PCX_MemoryLoad(lumpData, W_LumpLength(num), w, h, image);
		inst->tex = gl.NewTexture();
		// Make faded mipmaps.
		if(!gl.TexImage(DGL_RGB, w, h, DGL_GRAY_MIPMAP, image))
		{
			Con_Error("GL_LoadDetailTexture: %-8s (%ix%i): not powers of two.\n",
				lumpinfo[num].name, w, h);
		}
	}
	else // It must be a raw image.
	{
		// How big is it?
		if(lumpinfo[num].size != 256 * 256)
		{
			if(lumpinfo[num].size != 128 * 128) 
			{
				if(lumpinfo[num].size != 64 * 64)
				{
					Con_Message("GL_LoadDetailTexture: Must be 256x256, 128x128 or 64x64.\n");
					W_ChangeCacheTag(num, PU_CACHE);
					return 0;
				}
				w = h = 64;
			}
			else w = h = 128;
		}
		image = M_Malloc(w * h);
		memcpy(image, W_CacheLumpNum(num, PU_CACHE), w * h);
		inst->tex = gl.NewTexture();
		// Make faded mipmaps.
		gl.TexImage(DGL_LUMINANCE, w, h, DGL_GRAY_MIPMAP, image);
	}

	// Set texture parameters.
	gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR_MIPMAP_LINEAR);
	gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
	gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
	gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);
	
	// Free allocated memory.
	M_Free(image);
	W_ChangeCacheTag(num, PU_CACHE);
	return inst->tex;
}

//===========================================================================
// GL_PrepareDetailTexture
//	This is only called when loading a wall texture or a flat 
//	(not too time-critical).
//===========================================================================
DGLuint GL_PrepareDetailTexture
	(int index, boolean is_wall_texture, ded_detailtexture_t **dtdef)
{
	int i;
	detailtex_t *dt;

	// Search through the assignments. 
	for(i = defs.count.details.num - 1; i >= 0; i--)
	{
		dt = &details[i];

		// Is there a detail texture assigned for this?
		if(dt->detail_lump < 0) continue;

		if((is_wall_texture && index == dt->wall_texture) ||
		   (!is_wall_texture && index == dt->flat_lump))
		{
			if(dtdef) *dtdef = defs.details + i;

			// Hey, a match. Load this?
			if(!dt->gltex)
			{
				dt->gltex = GL_LoadDetailTexture(dt->detail_lump, 
					defs.details[i].strength);
			}
			return dt->gltex;
		}
	}
	return 0; // There is no detail texture for this.
}

//===========================================================================
// GL_BindTexFlat
//	No translation is done.
//===========================================================================
unsigned int GL_BindTexFlat(flat_t *fl)
{
	DGLuint name;
	byte *flatptr;
	int lump = fl->lump, width, height, pixSize = 3;
	boolean RGBData = false, freeptr = false;
	ded_detailtexture_t *def;
	image_t image;
	boolean hasExternal = false;

	if(lump >= numlumps	|| lump == skyflatnum)
	{
		// The sky flat is not a real flat at all.
		GL_BindTexture(0);	
		return 0;
	}

	// Is there a high resolution version?
	if((loadExtAlways || highResWithPWAD || W_IsFromIWAD(lump))
		&& (flatptr = GL_LoadHighResFlat(&image, lumpinfo[lump].name)) 
			!= NULL)
	{
		RGBData = true;
		freeptr = true;
		width   = image.width;
		height  = image.height;
		pixSize = image.pixelSize;

		hasExternal = true;
	}
	else
	{
		if(lumpinfo[lump].size < 4096) return 0; // Too small.
		// Get a pointer to the texture data.
		flatptr = W_CacheLumpNum(lump, PU_CACHE);
		width = height = 64;
	}	
	// Is there a detail texture for this?
	if((fl->detail.tex = GL_PrepareDetailTexture(fl->lump, false, &def)))
	{
		// The width and height could be used for something meaningful.
		fl->detail.width = 128;
		fl->detail.height = 128;
		fl->detail.scale = def->scale;
		fl->detail.strength = def->strength;
		fl->detail.maxdist = def->maxdist;
	}

	// Load the texture.
	name = GL_UploadTexture(flatptr, width, height, pixSize == 4, 
		true, RGBData, false);

	// Average color for glow planes.
	if(RGBData)
	{
		averageColorRGB(&fl->color, flatptr, width, height);
	}
	else
	{
		averageColorIdx(&fl->color, flatptr, width, height, 
			W_CacheLumpNum(pallump, PU_CACHE), false);
	}
	
	// Set the parameters.
	gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
	gl.TexParameter(DGL_MAG_FILTER, glmode[texMagMode]);
	
	if(freeptr) M_Free(flatptr);

	// Is there a surface decoration for this flat?
	fl->decoration = Def_GetDecoration(lump, false, hasExternal);

	// The name of the texture is returned.
	return name;
}

//===========================================================================
// GL_PrepareFlat
//===========================================================================
unsigned int GL_PrepareFlat(int idx)
{
	return GL_PrepareFlat2(idx, true);
}

//===========================================================================
// GL_PrepareFlat
//	Returns the OpenGL name of the texture.
//	(idx is really a lumpnum)
//===========================================================================
unsigned int GL_PrepareFlat2(int idx, boolean translate)
{
	flat_t *flat = R_GetFlat(idx);

	// Get the translated one?
	if(translate && flat->translation.current != idx)
	{
		flat = R_GetFlat(flat->translation.current);
	}

	if(!lumptexinfo[flat->lump].tex[0])
	{
		// The flat isn't yet bound with OpenGL.
		lumptexinfo[flat->lump].tex[0] = GL_BindTexFlat(flat);
	}
	texw = texh = 64;
	texmask = false;
	texdetail = (r_detail && flat->detail.tex? &flat->detail : 0);
	return lumptexinfo[flat->lump].tex[0];
}

//===========================================================================
// GL_GetFlatColor
//	'fnum' is really a lump number.
//===========================================================================
void GL_GetFlatColor(int fnum, unsigned char *rgb)
{
	flat_t *flat = R_GetFlat(fnum);
	memcpy(rgb, flat->color.rgb, 3);
}

//===========================================================================
// GL_SetFlat
//===========================================================================
void GL_SetFlat(int idx)
{
	gl.Bind(curtex = GL_PrepareFlat2(idx, false));
}

//===========================================================================
// DrawRealPatch
//	The buffer must have room for the alpha values.
//	Returns true if the buffer really has alpha information.
//	If !checkForAlpha, returns false.
//	Origx and origy set the origin of the patch.
//
//  Modified to allow taller masked textures - GMJ Aug 2002
//
//===========================================================================
static int DrawRealPatch(byte *buffer, byte *palette, int texwidth,
						 int texheight, patch_t *patch, int origx, int origy,
						 boolean maskZero, unsigned char *transtable,
						 boolean checkForAlpha)
{
	int			count;
	int			col;
	column_t	*column;
	byte		*destTop, *destAlphaTop = NULL;
	byte		*dest1, *dest2;
	byte		*source;
	int			w, i, bufsize = texwidth*texheight;
	int			x, y, top;	// Keep track of pos for clipping.

	col = 0;
	destTop = buffer + origx;
	destAlphaTop = buffer + origx + bufsize;
	w = SHORT(patch->width);
	x = origx;
	for(; col < w; col++, destTop++, destAlphaTop++, x++)
	{
		column = (column_t *)((byte *)patch+LONG(patch->columnofs[col]));
		top = -1;
		// Step through the posts in a column
		while(column->topdelta != 0xff)
		{
			source = (byte *)column+3;

			if(x < 0 || x >= texwidth) break; // Out of bounds. 
			
			if (column->topdelta <= top)
				top += column->topdelta;
			else
				top = column->topdelta;

			if (!column->length) goto nextpost;

			y = origy + top;
			dest1 = destTop + y*texwidth;
			dest2 = destAlphaTop + y*texwidth;

			count = column->length;
			while(count--)
			{
				unsigned char palidx = *source++;
				// Do we need to make a translation?
				if(transtable) palidx = transtable[palidx];
				
				// Is the destination within bounds?
				if(y >= 0 && y < texheight)
				{
					if(!maskZero || palidx)
						*dest1 = palidx;
					
					if(maskZero)
						*dest2 = palidx? 0xff : 0;
					else
						*dest2 = 0xff;
				}

				// One row down.
				dest1 += texwidth; 				
				dest2 += texwidth; 
				y++;
			}
nextpost:
			column = (column_t *)((byte *)column+column->length+4);
		}
	}
	if(checkForAlpha)
	{
		// Scan through the RGBA buffer and check for sub-0xff alpha.
		source = buffer + texwidth*texheight;
		for(i=0, count=0; i<texwidth*texheight; i++)
		{
			if(source[i] < 0xff) 
			{
				//----- <HACK> -----
				// 'Small' textures tolerate no alpha.
				if(texwidth <= 128 || texheight < 128) return true;
				// Big ones can have a single alpha pixel (ZZZFACE3!).
				if(count++ > 1) return true; // Has alpha data.
				//----- </HACK> -----
			}
		}
	}
	return false; // Doesn't have alpha data.
}

//===========================================================================
// TranslatePatch
//	Translate colors in the specified patch.
//===========================================================================
void TranslatePatch(patch_t *patch, byte *transTable)
{
	int			count;
	int			col = 0;
	column_t	*column;
	byte		*source;
	int			w = SHORT(patch->width);

	for(; col < w; col++)
	{
		column = (column_t *)((byte *)patch + LONG(patch->columnofs[col]));
		// Step through the posts in a column
		while(column->topdelta != 0xff)
		{
			source = (byte*) column+3;
			count = column->length;
			while(count--)
			{
				*source = transTable[*source];
				source++;
			}
			column = (column_t *)((byte *)column + column->length+4);
		}
	}
}

//===========================================================================
// GL_ConvertToLuminance
//	Converts the image data to grayscale luminance in-place.
//===========================================================================
void GL_ConvertToLuminance(image_t *image)
{
	int p, total = image->width * image->height;
	byte *ptr = image->pixels;

	if(image->pixelSize == 1) 
	{
		// No need to convert anything.
		return;
	}

	// Average the RGB colors.
	for(p = 0; p < total; p++, ptr += image->pixelSize)
	{
		image->pixels[p] = (ptr[0] + ptr[1] + ptr[2]) / 3;
	}
	image->pixelSize = 1;
}

//===========================================================================
// GL_ConvertToAlpha
//===========================================================================
void GL_ConvertToAlpha(image_t *image, boolean makeWhite)
{
	int p, total = image->width * image->height;

	GL_ConvertToLuminance(image);
	for(p = 0; p < total; p++)
	{
		// Move the average color to the alpha channel, make the
		// actual color white.
		image->pixels[total + p] = image->pixels[p];
		if(makeWhite) image->pixels[p] = 255;
	}
	image->pixelSize = 2;
}

//===========================================================================
// GL_LoadImage
//	Loads PCX, TGA and PNG images. The returned buffer must be freed 
//	with M_Free. Color keying is done if "-ck." is found in the filename.
//	The allocated memory buffer always has enough space for 4-component
//	colors.
//===========================================================================
byte *GL_LoadImage(image_t *img, const char *imagefn, boolean useModelPath)
{
	DFILE	*file;
	byte	*ckdest, ext[40], *in, *out;
	int		format, i, numpx; 

	// Clear any old values.
	memset(img, 0, sizeof(*img));

	if(useModelPath) 
	{
		if(!R_FindModelFile(imagefn, img->fileName))
			return NULL; // Not found.
	}
	else
	{
		strcpy(img->fileName, imagefn);
	}

	// We know how to load PCX, TGA and PNG.
	M_GetFileExt(img->fileName, ext);
	if(!strcmp(ext, "pcx"))
	{
		img->pixels = PCX_AllocLoad(img->fileName, &img->width, 
			&img->height, NULL);
		img->pixelSize = 3; // PCXs can't be masked.
		img->originalBits = 8;
	}
	else if(!strcmp(ext, "tga"))
	{
		if(!TGA_GetSize(img->fileName, &img->width, &img->height)) 
			return NULL; 

		file = F_Open(img->fileName, "rb");
		if(!file) return NULL;	
		
		// Allocate a big enough buffer and read in the image.
		img->pixels = M_Malloc(4 * img->width * img->height);
		format = TGA_Load32_rgba8888(file, img->width, img->height, 
			img->pixels);
		if(format == TGA_TARGA24)
		{
			img->pixelSize = 3;
			img->originalBits = 24;
		}
		else
		{
			img->pixelSize = 4;
			img->originalBits = 32;
		}
		F_Close(file);
	}
	else if(!strcmp(ext, "png"))
	{
		BEGIN_PROF( PROF_PNG_LOAD );

		img->pixels = PNG_Load(img->fileName, &img->width, &img->height, 
			&img->pixelSize);
		img->originalBits = 8 * img->pixelSize;

		END_PROF( PROF_PNG_LOAD );

		if(img->pixels == NULL) return NULL;
	}

	VERBOSE( Con_Message("LoadImage: %s (%ix%i)\n", M_Pretty(img->fileName), 
		img->width, img->height) );

	numpx = img->width * img->height;

	// How about some color-keying?
	if(GL_IsColorKeyed(img->fileName))
	{
		// We must allocate a new buffer if the loaded image has three
		// color componenets.
		if(img->pixelSize < 4)
		{
			ckdest = M_Malloc(4 * img->width * img->height);
			for(in = img->pixels, out = ckdest, i = 0; i < numpx; 
				i++, in += img->pixelSize, out += 4)
			{
				if(GL_ColorKey(in))
					memset(out, 0, 4);	// Totally black.
				else
				{
					memcpy(out, in, 3); // The color itself.
					out[CA] = 255;		// Opaque.
				}
			}
			M_Free(img->pixels);
			img->pixels = ckdest;
		}
		else // We can do the keying in-buffer.
		{
			// This preserves the alpha values of non-keyed pixels.
			for(i = 0; i < img->height; i++)
				GL_DoColorKeying(img->pixels + 4*i*img->width, img->width);
		}
		// Color keying is done; now we have 4 bytes per pixel.
		img->pixelSize = 4;
		img->originalBits = 32; // Effectively...
	}

	// Any alpha pixels?
	img->isMasked = false;
	if(img->pixelSize == 4)
		for(i = 0, in = img->pixels; i < numpx; i++, in += 4)
			if(in[3] < 255)
			{
				img->isMasked = true;
				break;
			}
	
	return img->pixels;
}

//===========================================================================
// GL_LoadImageCK
//	First sees if there is a color-keyed version of the given image. If
//	there is it is loaded. Otherwise the 'regular' version is loaded.
//===========================================================================
byte *GL_LoadImageCK(image_t *img, const char *name, boolean useModelPath)
{
	char keyFileName[256];
	byte *pixels;
	char *ptr;

	strcpy(keyFileName, name);

	// Append the "-ck" and try to load.
	ptr = strrchr(keyFileName, '.');
	if(ptr)
	{
		memmove(ptr + 3, ptr, strlen(ptr) + 1);
		ptr[0] = '-';
		ptr[1] = 'c';
		ptr[2] = 'k';
		if((pixels = GL_LoadImage(img, keyFileName, useModelPath)) != NULL) 
			return pixels;
	}
	return GL_LoadImage(img, name, useModelPath);
}

//===========================================================================
// GL_DestroyImage
//	Frees all memory associated with the image.
//===========================================================================
void GL_DestroyImage(image_t *img)
{
	M_Free(img->pixels);
	img->pixels = NULL;
}

//===========================================================================
// GL_LoadHighRes
//	Name must end in \0.
//===========================================================================
byte *GL_LoadHighRes
	(image_t *img, char *name, char *prefix, boolean allowColorKey)
{
	filename_t resource, fileName;

	// Form the resource name.
	sprintf(resource, "%s%s", prefix, name);

	if(!R_FindResource(RC_TEXTURE, resource, 
		allowColorKey? "-ck" : NULL, fileName))
	{
		// There is no such external resource file.
		return NULL;	
	}

	return GL_LoadImage(img, fileName, false);
}

//===========================================================================
// GL_LoadTexture
//	Use this when loading custom textures from the Data\*\Textures dir.
//	The returned buffer must be freed with M_Free.
//===========================================================================
byte *GL_LoadTexture(image_t *img, char *name)
{
	return GL_LoadHighRes(img, name, "", true);
}

//===========================================================================
// GL_LoadHighResTexture
//	Use this when loading high-res wall textures.
//	The returned buffer must be freed with M_Free.
//===========================================================================
byte *GL_LoadHighResTexture(image_t *img, char *name)
{
	if(noHighResTex) return NULL;
	return GL_LoadTexture(img, name);
}

//===========================================================================
// GL_LoadHighResFlat
//	The returned buffer must be freed with M_Free.
//===========================================================================
byte *GL_LoadHighResFlat(image_t *img, char *name)
{
	if(noHighResTex) return NULL;
	return GL_LoadHighRes(img, name, "Flat-", false);
}

//===========================================================================
// GL_LoadGraphics
//	Set grayscale to 2 to include an alpha channel. Set to 3 to make the
//	actual pixel colors all white.
//===========================================================================
DGLuint GL_LoadGraphics(const char *name, gfxmode_t mode)
{
	image_t image;
	filename_t fileName;
	DGLuint texture = 0;

	if(R_FindResource(RC_GRAPHICS, name, NULL, fileName)
		&& GL_LoadImage(&image, fileName, false))
	{
		// Too big for us?
		if(image.width > maxTexSize || image.height > maxTexSize)
		{
			int newWidth  = MIN_OF(image.width, maxTexSize);
			int newHeight = MIN_OF(image.height, maxTexSize);
			byte *temp = M_Malloc(newWidth * newHeight * image.pixelSize);
			GL_ScaleBuffer32(image.pixels, image.width, image.height,
				temp, newWidth, newHeight, image.pixelSize);
			M_Free(image.pixels);
			image.pixels = temp;
			image.width = newWidth;
			image.height = newHeight;
		}

		// Force it to grayscale?
		if(mode == LGM_GRAYSCALE_ALPHA || mode == LGM_WHITE_ALPHA)
		{
			GL_ConvertToAlpha(&image, mode == LGM_WHITE_ALPHA);
		}
		else if(mode == LGM_GRAYSCALE)
		{
			GL_ConvertToLuminance(&image);
		}

		texture = gl.NewTexture();
		if(image.width < 128 && image.height < 128)
		{
			// Small textures will never be compressed.
			gl.Disable(DGL_TEXTURE_COMPRESSION);
		}
		gl.TexImage(
			  image.pixelSize == 2? DGL_LUMINANCE_PLUS_A8
			: image.pixelSize == 3? DGL_RGB
			: image.pixelSize == 4? DGL_RGBA
			: DGL_LUMINANCE,
			image.width, image.height, 0, image.pixels);
		gl.Enable(DGL_TEXTURE_COMPRESSION);
		gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

		GL_DestroyImage(&image);
	}
	return texture;
}

//===========================================================================
// GL_BufferTexture
//	Renders the given texture into the buffer.
//===========================================================================
boolean GL_BufferTexture(texture_t *tex, byte *buffer, int width, int height,
						 int *has_big_patch)
{
	int			i, len = width * height;
	boolean		alphaChannel;
	byte		*palette = W_CacheLumpNum(pallump, PU_STATIC);
	patch_t		*patch;

	// Clear the buffer.
	memset(buffer, 0, 2 * len);

	// By default zero is put in the big patch height.
	if(has_big_patch) *has_big_patch = 0;
	
	// Draw all the patches. Check for alpha pixels after last patch has
	// been drawn.
	for(i = 0; i < tex->patchcount; i++)
	{
		patch = W_CacheLumpNum(tex->patches[i].patch, PU_CACHE);
		// Check for big patches?
		if(patch->height > tex->height 
			&& has_big_patch
			&& *has_big_patch < patch->height)
		{ 
			*has_big_patch = patch->height;
		}
		// Draw the patch in the buffer.
		alphaChannel = DrawRealPatch(buffer, palette, width, 
			height, patch, tex->patches[i].originx, tex->patches[i].originy, 
			false, 0, i == tex->patchcount-1);
	}
	W_ChangeCacheTag(pallump, PU_CACHE);
	return alphaChannel;
}

//===========================================================================
// GL_PrepareTexture
//	Returns the DGL texture name.
//===========================================================================
unsigned int GL_PrepareTexture(int idx)
{
	return GL_PrepareTexture2(idx, true);
}

//===========================================================================
// GL_PrepareTexture2
//	Returns the DGL texture name.
//===========================================================================
unsigned int GL_PrepareTexture2(int idx, boolean translate)
{
	ded_detailtexture_t *def;
	int			originalIndex = idx;
	texture_t	*tex;
	boolean		alphaChannel = false, RGBData = false;
	int			i;
	image_t		image;
	boolean		hasExternal = false;

	if(idx == 0)
	{
		// No texture?
		texw = 1;
		texh = 1;
		texmask = 0;
		texdetail = 0;
		return 0;
	}
	if(translate) 
	{
		idx = texturetranslation[idx].current;
	}
	tex = textures[idx];
	if(!textures[idx]->tex)
	{
		// Try to load a high resolution version of this texture.
		if((loadExtAlways || highResWithPWAD || !R_IsCustomTexture(idx))
			&& GL_LoadHighResTexture(&image, tex->name) != NULL)
		{
			// High resolution texture loaded.
			RGBData = true;
			alphaChannel = (image.pixelSize == 4);
			hasExternal = true;
		}
		else
		{
			image.width = tex->width;
			image.height = tex->height;
			image.pixels = M_Malloc(2 * image.width * image.height);
			image.isMasked = GL_BufferTexture(tex, image.pixels, image.width, 
				image.height, &i);
			
			// The -bigmtex option allows the engine to resize masked 
			// textures whose patches are too big to fit the texture.
			if(allowMaskedTexEnlarge && image.isMasked && i)
			{
				// Adjust height to fit the largest patch.
				tex->height = image.height = i; 
				// Get a new buffer.
				M_Free(image.pixels);
				image.pixels = M_Malloc(2 * image.width * image.height);
				image.isMasked = GL_BufferTexture(tex, image.pixels, 
					image.width, image.height, 0);
			}
			// "alphaChannel" and "masked" are the same thing for indexed
			// images.
			alphaChannel = image.isMasked;
		}

		// Load a detail texture (if one is defined).
		if((textures[idx]->detail.tex 
			= GL_PrepareDetailTexture(idx, true, &def)))
		{
			// The width and height could be used for something meaningful.
			textures[idx]->detail.width = 128;
			textures[idx]->detail.height = 128;
			textures[idx]->detail.scale = def->scale;
			textures[idx]->detail.strength = def->strength;
			textures[idx]->detail.maxdist = def->maxdist;
		}
		
		textures[idx]->tex = GL_UploadTexture(image.pixels, image.width, 
			image.height, alphaChannel, true, RGBData, false);

		// Set texture parameters.
		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, glmode[texMagMode]);

		textures[idx]->masked = (image.isMasked != 0);

		/*if(alphaChannel)
		{
			// Don't tile masked textures vertically.
			gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		}*/

		GL_DestroyImage(&image);

		// Is there a decoration for this surface?
		textures[idx]->decoration = Def_GetDecoration(idx, true, hasExternal);
	}
	return GL_GetTextureInfo2(originalIndex, translate);
}

//===========================================================================
// GL_GetTextureInfo
//	Returns the texture name, if it has been prepared.
//===========================================================================
DGLuint GL_GetTextureInfo(int index)
{
	return GL_GetTextureInfo2(index, true);
}

//===========================================================================
// GL_GetTextureInfo2
//	Returns the texture name, if it has been prepared.
//===========================================================================
DGLuint GL_GetTextureInfo2(int index, boolean translate)
{
	texture_t *tex;

	if(!index) return 0;
	
	// Translate the texture.
	if(translate)
	{
		index = texturetranslation[index].current;
	}
	tex = textures[index];

	// Set the global texture info variables.
	texw = tex->width;
	texh = tex->height;
	texmask = tex->masked;
	texdetail = (r_detail && tex->detail.tex? &tex->detail : 0);
	return tex->tex;
}

//===========================================================================
// GL_SetTexture
//===========================================================================
void GL_SetTexture(int idx)
{
	gl.Bind(GL_PrepareTexture(idx));
}

//===========================================================================
// LineAverageRGB
//===========================================================================
int LineAverageRGB(byte *imgdata, int width, int height, int line, 
				   byte *rgb, byte *palette, boolean has_alpha)
{
	byte	*start = imgdata + width*line;
	byte	*alphaStart = start + width*height;
	int		i, c, count = 0;
	int		integerRGB[3] = {0,0,0};
	byte	col[3];

	for(i = 0; i < width; i++)
	{
		// Not transparent?
		if(alphaStart[i] > 0 || !has_alpha)
		{
			count++;
			// Ignore the gamma level.
			memcpy(col, palette + 3*start[i], 3);
			for(c = 0; c < 3; c++) integerRGB[c] += col[c];
		}
	}
	// All transparent? Sorry...
	if(!count) return 0;

	// We're going to make it!
	for(c = 0; c < 3; c++) rgb[c] = integerRGB[c]/count;
	return 1;	// Successful.
}

//===========================================================================
// ImageAverageRGB
//	The imgdata must have alpha info, too.
//===========================================================================
void ImageAverageRGB(byte *imgdata, int width, int height, 
					 byte *rgb, byte *palette)
{
	int	i, c, integerRGB[3] = {0,0,0}, count = 0;

	for(i = 0; i < height; i++)
	{
		if(LineAverageRGB(imgdata, width, height, i, rgb, palette, true))
		{
			count++;
			for(c = 0; c < 3; c++) integerRGB[c] += rgb[c];
		}
	}
	if(count)	// If there were pixels...
		for(c = 0; c < 3; c++) 
			rgb[c] = integerRGB[c]/count;
}

//===========================================================================
// ColorOutlines
//	Fills the empty pixels with reasonable color indices.
//	This gets rid of the black outlines.
//	Not a very efficient algorithm...
//===========================================================================
static void ColorOutlines(byte *buffer, int width, int height)
{
	int		numpels = width*height;
	byte	*ptr;
	int		i, k, a, b;

	for(k = 0; k < height; k++)
		for(i = 0; i < width; i++)
			// Solid pixels spread around...
			if(buffer[numpels + i + k*width])
			{
				for(b = -1; b <= 1; b++)
					for(a = -1; a <= 1; a++)
					{
						// First check that the pixel is OK.
						if((!a && !b) || i+a < 0 || k+b < 0 
						   || i+a >= width || k+b >= height) continue;

						ptr = buffer + i+a + (k+b)*width;
						if(!ptr[numpels]) // An alpha pixel?
							*ptr = buffer[i + k*width];
					}
			}
}

//===========================================================================
// GL_BufferSkyTexture
//	Draws the given sky texture in a buffer. The returned buffer must be 
//	freed by the caller. Idx must be a valid texture number.
//===========================================================================
void GL_BufferSkyTexture(int idx, byte **outbuffer, int *width, int *height, 
						 boolean zeroMask)
{
	texture_t	*tex = textures[idx];	
	byte		*imgdata; 
	byte		*palette = W_CacheLumpNum(pallump, PU_CACHE);
	int			i, numpels;

	*width = tex->width;
	*height = tex->height;

	if(tex->patchcount > 1)
	{
		numpels = tex->width * tex->height;
		imgdata = M_Calloc(2 * numpels);
		/*for(i = 0; i < tex->width; i++)
		{
			colptr = R_GetColumn(idx, i);
			for(k = 0; k < tex->height; k++)
			{
				if(!zeroMask)
					imgdata[k*tex->width + i] = colptr[k];
				else if(colptr[k])
				{
					byte *imgpos = imgdata + (k*tex->width + i);
					*imgpos = colptr[k];
					imgpos[numpels] = 0xff;	// Not transparent, this pixel.
				}
			}
		}*/
		for(i = 0; i < tex->patchcount; i++)
		{
			DrawRealPatch(imgdata, palette, tex->width, tex->height,
				W_CacheLumpNum(tex->patches[i].patch, PU_CACHE),
				tex->patches[i].originx, tex->patches[i].originy, 
				zeroMask, 0, false);
		}
	}
	else 
	{
		patch_t *patch = W_CacheLumpNum(tex->patches[0].patch, PU_CACHE);
		int bufHeight = patch->height > tex->height? patch->height 
			: tex->height;
		if(bufHeight > *height)
		{
			// Heretic sky textures are reported to be 128 tall, even if the
			// data is 200. We'll adjust the real height of the texture up to
			// 200 pixels (remember Caldera?).
			*height = bufHeight;
			if(*height > 200) *height = 200;
		}
		// Allocate a large enough buffer. 
		numpels = tex->width * bufHeight;
		imgdata = M_Calloc(2 * numpels);
		DrawRealPatch(imgdata, palette, tex->width, bufHeight, patch, 
			0, 0, zeroMask, 0, false);
	}
	if(zeroMask && filloutlines) ColorOutlines(imgdata, *width, *height);
	*outbuffer = imgdata;
}

//===========================================================================
// GL_PrepareSky
//	Sky textures are usually 256 pixels wide.
//===========================================================================
unsigned int GL_PrepareSky(int idx, boolean zeroMask)
{
	return GL_PrepareSky2(idx, zeroMask, true);
}

//===========================================================================
// GL_PrepareSky2
//	Sky textures are usually 256 pixels wide.
//===========================================================================
unsigned int GL_PrepareSky2(int idx, boolean zeroMask, boolean translate)
{
	boolean	RGBData, alphaChannel;
	image_t image;

	if(idx >= numtextures) return 0;
/*
#if _DEBUG
	if(idx != texturetranslation[idx])
		Con_Error("Skytex: %d, translated: %d\n", idx, texturetranslation[idx]);
#endif
*/
	if(translate)
	{
		idx = texturetranslation[idx].current;
	}
	
	if(!textures[idx]->tex)
	{
		// Try to load a high resolution version of this texture.
		if((loadExtAlways || highResWithPWAD || !R_IsCustomTexture(idx))
			&& GL_LoadHighResTexture(&image, textures[idx]->name) != NULL)
		{
			// High resolution texture loaded.
			RGBData = true;
			alphaChannel = (image.pixelSize == 4);
		}
		else
		{
			RGBData = false;
			GL_BufferSkyTexture(idx, &image.pixels, &image.width, 
				&image.height, zeroMask);
			alphaChannel = image.isMasked = zeroMask;
		}

		// Upload it.
		textures[idx]->tex = GL_UploadTexture(image.pixels, image.width, 
			image.height, alphaChannel, true, RGBData, false);
		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);

		// Do we have a masked texture?
		textures[idx]->masked = (image.isMasked != 0);

		// Free the buffer.
		GL_DestroyImage(&image);
	}
	texw = textures[idx]->width;
	texh = textures[idx]->height;
	texmask = textures[idx]->masked;
	texdetail = 0;
	return textures[idx]->tex;
}

//===========================================================================
// GL_GetSkyColor
//	Return a skycol_t for texidx.
//===========================================================================
skycol_t *GL_GetSkyColor(int texidx)
{
	int			i, width, height;
	skycol_t	*skycol;
	byte		*imgdata, *pald;

	if(texidx < 0 || texidx >= numtextures) return NULL;

	// Try to find a skytop color for this.
	for(i = 0; i < num_skytop_colors; i++)
		if(skytop_colors[i].texidx == texidx)
			return skytop_colors + i;

	// There was no skycol for the specified texidx!
	skytop_colors = realloc(skytop_colors, sizeof(skycol_t) 
		* ++num_skytop_colors);
	skycol = skytop_colors + num_skytop_colors-1;
	skycol->texidx = texidx;

	// Calculate the color.
	pald = W_CacheLumpNum(pallump, PU_STATIC);
	GL_BufferSkyTexture(texidx, &imgdata, &width, &height, false);
	LineAverageRGB(imgdata, width, height, 0, skycol->rgb, pald, false);
	M_Free(imgdata); // Free the temp buffer created by GL_BufferSkyTexture.
	W_ChangeCacheTag(pallump, PU_CACHE);
	return skycol;
}

//===========================================================================
// GL_GetSkyTopColor
//	Returns the sky fadeout color of the given texture.
//===========================================================================
void GL_GetSkyTopColor(int texidx, byte *rgb)
{
	skycol_t *skycol = GL_GetSkyColor(texidx);

	if(!skycol) 
	{
		// Must be an invalid texture.
		memset(rgb, 0, 3); // Default to black.
	}
	else memcpy(rgb, skycol->rgb, 3);
}

//===========================================================================
// GL_NewTranslatedSprite
//===========================================================================
transspr_t *GL_NewTranslatedSprite(int pnum, unsigned char *table)
{
	transspr_t *news;
	
	transsprites = realloc(transsprites, sizeof(transspr_t) 
		* ++numtranssprites);
	news = transsprites + numtranssprites-1;
	news->patch = pnum;
	news->tex = 0;
	news->table = table;
	return news;
}

//===========================================================================
// GL_GetTranslatedSprite
//===========================================================================
transspr_t *GL_GetTranslatedSprite(int pnum, unsigned char *table)
{
	int i;

	for(i = 0; i < numtranssprites; i++)
		if(transsprites[i].patch == pnum && transsprites[i].table == table)
			return transsprites + i;
	return 0;
}

//===========================================================================
// amplify
//	The given RGB color is scaled uniformly so that the highest component 
//	becomes one.
//===========================================================================
void amplify(byte *rgb)
{
	int i, max = 0;

	for(i = 0; i < 3; i++) if(rgb[i] > max) max = rgb[i];
	if(max)
	{
		for(i = 0; i < 3; i++) rgb[i] *= 255.0f / max;
	}
}

//===========================================================================
// averageColorIdx
//	Used by flares and dynamic lights. The resulting average color is 
//	amplified to be as bright as possible.
//===========================================================================
void averageColorIdx(rgbcol_t *col, byte *data, int w, int h, 
					 byte *palette, boolean has_alpha)
{
	int				i;
	unsigned int	r, g, b, count;
	byte			*alphaStart = data + w*h, rgb[3];

	// First clear them.
	memset(col->rgb, 0, sizeof(col->rgb));
	r = g = b = count = 0;
	for(i = 0; i < w*h; i++)
	{
		if(alphaStart[i] || !has_alpha)
		{
			count++;
			memcpy(rgb, palette + 3*data[i], 3);
			r += rgb[0];
			g += rgb[1];
			b += rgb[2];
		}
	}
	if(!count) return;	// Line added by GMJ 22/07/01
	col->rgb[0] = r/count;
	col->rgb[1] = g/count;
	col->rgb[2] = b/count;

	// Make it glow (average colors are used with flares and dynlights).
	amplify(col->rgb);
}

//===========================================================================
// averageColorRGB
//===========================================================================
void averageColorRGB(rgbcol_t *col, byte *data, int w, int h)
{
	int i, count = w*h;
	unsigned int cumul[3];

	if(!count) return;
	memset(cumul, 0, sizeof(cumul));
	for(i = 0; i < count; i++)
	{
		cumul[0] += *data++;
		cumul[1] += *data++;
		cumul[2] += *data++;
	}
	for(i = 0; i < 3; i++) col->rgb[i] = cumul[i]/count;
	amplify(col->rgb);
}

//==========================================================================
// GL_CalcLuminance
//	Calculates the properties of a dynamic light that the given sprite
//	frame casts. 
//	2003-05-30 (skyjake): Modified to handle pixel sizes 1 (==2), 3 and 4.
//==========================================================================
void GL_CalcLuminance(int pnum, byte *buffer, int width, int height, 
					  int pixelsize)
{
	byte *palette = pixelsize == 1? W_CacheLumpNum(pallump, PU_CACHE) : NULL;
	spritelump_t *slump = spritelumps + pnum;
	int			 i, k, c, cnt = 0, poscnt = 0;
	byte		 rgb[3], *src, *alphasrc = NULL;
	int			 limit = 0xc0, poslimit = 0xe0, collimit = 0xc0;
	int			 average[3], avcnt = 0, lowavg[3], lowcnt = 0;
	rgbcol_t	 *sprcol = &slump->color;

	slump->flarex = slump->flarey = 0;
	memset(average, 0, sizeof(average));
	memset(lowavg, 0, sizeof(lowavg));
	src = buffer;
	
	if(pixelsize == 1)
	{
		// In paletted mode, the alpha channel follows the actual image.
		alphasrc = buffer + width*height;
	}

	for(k = 0; k < height; k++)
		for(i = 0; i < width; i++, src += pixelsize, alphasrc++)
		{
			// Alpha pixels don't count.
			if(pixelsize == 1)
			{
				if(*alphasrc < 255) continue;
			}
			else if(pixelsize == 4)
			{
				if(src[3] < 255) continue;
			}

			// Bright enough?
			if(pixelsize == 1)
			{
				memcpy(rgb, palette + (*src * 3), 3);
			}
			else if(pixelsize >= 3)
			{
				memcpy(rgb, src, 3);
			}

			if(rgb[0] > poslimit || rgb[1] > poslimit || rgb[2] > poslimit)
			{
				// This pixel will participate in calculating the average
				// center point.
				slump->flarex += i;
				slump->flarey += k;
				poscnt++;
			}

			// Bright enough to affect size?
			if(rgb[0] > limit || rgb[1] > limit || rgb[2] > limit) cnt++;

			// How about the color of the light?
			if(rgb[0] > collimit || rgb[1] > collimit || rgb[2] > collimit)
			{
				avcnt++;
				for(c = 0; c < 3; c++) average[c] += rgb[c];
			}
			else
			{
				lowcnt++;
				for(c = 0; c < 3; c++) lowavg[c] += rgb[c];
			}
		}
	if(!poscnt)
	{
		slump->flarex = slump->width/2.0f;
		slump->flarey = slump->height/2.0f;
	}
	else
	{
		// Get the average.
		slump->flarex /= poscnt;
		slump->flarey /= poscnt;
	}
	// The color.
	if(!avcnt)
	{
		if(!lowcnt) 
		{
			// Doesn't the thing have any pixels??? Use white light.
			memset(sprcol->rgb, 0xff, 3);
		}
		else
		{
			// Low-intensity color average.
			for(c = 0; c < 3; c++) sprcol->rgb[c] = lowavg[c] / lowcnt;
		}
	}
	else
	{
		// High-intensity color average.
		for(c = 0; c < 3; c++) sprcol->rgb[c] = average[c] / avcnt;
	}
	// Amplify color.
	amplify(sprcol->rgb);
	// How about the size of the light source?
	slump->lumsize = (2*cnt+avcnt)/3.0f / 70.0f;
	if(slump->lumsize > 1) slump->lumsize = 1;
}

//==========================================================================
// GL_SetTexCoords
//	Calculates texture coordinates based on the given dimensions. The
//	coordinates are calculated as width/CeilPow2(width), or 1 if the
//	CeilPow2 would go over maxTexSize.
//==========================================================================
void GL_SetTexCoords(float *tc, int wid, int hgt)
{
	int pw = CeilPow2(wid), ph = CeilPow2(hgt);

	if(pw > maxTexSize || ph > maxTexSize)
		tc[VX] = tc[VY] = 1;
	else
	{
		tc[VX] = wid / (float) pw;
		tc[VY] = hgt / (float) ph;
	}
}

/*
 * Uploads the sprite in the buffer and sets the appropriate texture 
 * parameters.
 */
unsigned int GL_PrepareSpriteBuffer
	(int pnum, image_t *image, boolean isPsprite)
{
	unsigned int texture = 0;

	if(!isPsprite)
	{
		// Calculate light source properties.
		GL_CalcLuminance(pnum, image->pixels, image->width, image->height, 
			image->pixelSize);
	}
		
	if(image->pixelSize == 1 && filloutlines) 
		ColorOutlines(image->pixels, image->width, image->height);

	texture = GL_UploadTexture(image->pixels, image->width, image->height, 
		image->pixelSize != 3, true, image->pixelSize > 1, true);

	gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
	gl.TexParameter(DGL_MAG_FILTER, filterSprites? DGL_LINEAR : DGL_NEAREST);
	gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
	gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

	// Determine coordinates for the texture.
	GL_SetTexCoords(spritelumps[pnum].tc[isPsprite], image->width, 
		image->height);

	return texture;
}

//===========================================================================
// GL_PrepareTranslatedSprite
//===========================================================================
unsigned int GL_PrepareTranslatedSprite(int pnum, int tmap, int tclass)
{
	byte *table = translationtables-256 + tclass*((8-1)*256) + tmap*256;
	transspr_t *tspr = GL_GetTranslatedSprite(pnum, table);
	image_t image;

	if(!tspr)
	{
		filename_t resource, fileName;
		patch_t *patch = W_CacheLumpNum(spritelumps[pnum].lump, PU_CACHE);

		// Compose a resource name.
		if(tclass || tmap)
		{
			sprintf(resource, "%s-table%i%i", 
				lumpinfo[spritelumps[pnum].lump].name, tclass, tmap);
		}
		else 
		{
			// Not actually translated? Use the normal resource.
			strcpy(resource, lumpinfo[spritelumps[pnum].lump].name);
		}

		if(!noHighResPatches
			&& R_FindResource(RC_PATCH, resource, "-ck", fileName)
			&& GL_LoadImage(&image, fileName, false) != NULL)
		{
			// The buffer is now filled with the data.
		}
		else
		{
			// Must load from the normal lump.
			image.width     = patch->width;
			image.height    = patch->height;
			image.pixelSize = 1;
			image.pixels    = M_Calloc(2 * image.width * image.height);
		
			DrawRealPatch(image.pixels, 
				W_CacheLumpNum(pallump, PU_CACHE), 
				image.width, image.height, patch, 0, 0, false, table, false);
		}

		tspr = GL_NewTranslatedSprite(pnum, table);
		tspr->tex = GL_PrepareSpriteBuffer(pnum, &image, false);

		GL_DestroyImage(&image);
	}
	return tspr->tex;
}

//===========================================================================
// GL_PrepareSprite
//	Spritemodes:
//	0 = Normal sprite
//	1 = Psprite (HUD)
//===========================================================================
unsigned int GL_PrepareSprite(int pnum, int spriteMode)
{
	DGLuint *texture;
	int lumpNum;
	spritelump_t *slump;

	if(pnum < 0) return 0;

	slump = &spritelumps[pnum];
	lumpNum = slump->lump;

	// Normal sprites and HUD sprites are stored separately.
	// This way a sprite can be used both in the game and the HUD, and
	// the textures can be different. (Very few sprites are used both
	// in the game and the HUD.)
	texture = (spriteMode == 0? &slump->tex : &slump->hudtex);

	if(!*texture)
	{
		image_t image;
		filename_t hudResource, fileName;
		patch_t *patch = W_CacheLumpNum(lumpNum, PU_CACHE);

		// Compose a resource for the psprite.
		if(spriteMode == 1)
			sprintf(hudResource, "%s-hud", lumpinfo[lumpNum].name);
		
		// Is there an external resource for this image? For HUD sprites,
		// first try the HUD version of the resource.
		if(!noHighResPatches
			&& ( (spriteMode == 1 && R_FindResource(RC_PATCH, hudResource, 
					"-ck", fileName))
				|| R_FindResource(RC_PATCH, lumpinfo[lumpNum].name, "-ck", 
					fileName) )
			&& GL_LoadImage(&image, fileName, false) != NULL)
		{
			// A high-resolution version of this sprite has been found.
			// The buffer has been filled with the data we need.
		}
		else
		{
			// There's no name for this patch, load it in.
			image.width     = patch->width;
			image.height    = patch->height;
			image.pixels    = M_Calloc(2 * image.width * image.height);
			image.pixelSize = 1;

			DrawRealPatch(image.pixels, W_CacheLumpNum(pallump, PU_CACHE), 
				image.width, image.height, patch, 0, 0, false, 0, false);
		}

		*texture = GL_PrepareSpriteBuffer(pnum, &image, spriteMode == 1);
		GL_DestroyImage(&image);
	}

	return *texture;
}

//===========================================================================
// GL_DeleteSprite
//===========================================================================
void GL_DeleteSprite(int spritelump)
{
	if(spritelump < 0 || spritelump >= numspritelumps) return;

	gl.DeleteTextures(1, &spritelumps[spritelump].tex);
	spritelumps[spritelump].tex = 0;

	if(spritelumps[spritelump].hudtex)
	{
		gl.DeleteTextures(1, &spritelumps[spritelump].hudtex);
		spritelumps[spritelump].hudtex = 0;
	}
}

//===========================================================================
// GL_GetSpriteColor
//===========================================================================
void GL_GetSpriteColor(int pnum, unsigned char *rgb)
{
	if(pnum > numspritelumps-1) return;
	memcpy(rgb, &spritelumps[pnum].color, 3);
}
 
//===========================================================================
// GL_SetSprite
//	0 = Normal sprite
//	1 = Psprite (HUD)
//===========================================================================
void GL_SetSprite(int pnum, int spriteType)
{
	GL_BindTexture(GL_PrepareSprite(pnum, spriteType));
}

//===========================================================================
// GL_SetTranslatedSprite
//===========================================================================
void GL_SetTranslatedSprite(int pnum, int tmap, int tclass)
{
	GL_BindTexture(GL_PrepareTranslatedSprite(pnum, tmap, tclass));
}

//===========================================================================
// GL_NewRawLump
//===========================================================================
void GL_NewRawLump(int lump)
{
	rawlumps = realloc(rawlumps, sizeof(int) * ++numrawlumps);
	rawlumps[numrawlumps-1] = lump;
}

//===========================================================================
// GL_GetOtherPart
//===========================================================================
DGLuint GL_GetOtherPart(int lump)
{
	return lumptexinfo[lump].tex[1];
}

/*
 * Sets texture parameters for raw image textures (parts).
 */
void GL_SetRawImageParams(void)
{
	gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
	gl.TexParameter(DGL_MAG_FILTER, linearRaw? DGL_LINEAR : DGL_NEAREST);
	gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
	gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
}

/*
 * Prepares and uploads a raw image texture from the given lump.
 * Called only by GL_SetRawImage(), so params are valid.
 */
void GL_SetRawImageLump(int lump, int part)
{
	int		i, k, c, idx;
	byte	*dat1, *dat2, *palette, *lumpdata, *image;
	int		height, assumedWidth = 320;
	boolean need_free_image = true;
	boolean rgbdata;
	int		comps;
	lumptexinfo_t *info = lumptexinfo + lump;

	// Load the raw image data.
	// It's most efficient to create two textures for it (256 + 64 = 320).
	lumpdata = W_CacheLumpNum(lump, PU_STATIC);
	height = 200;

	// Try to load it as a PCX image first.
	image = M_Malloc(3 * 320 * 200);
	if(PCX_MemoryLoad(lumpdata, lumpinfo[lump].size, 320, 200, image))
	{
		rgbdata = true;
		comps = 3;
	}
	else
	{
		// PCX load failed. It must be an old-fashioned raw image.
		need_free_image = false;
		M_Free(image);
		height = lumpinfo[lump].size / 320;				
		rgbdata = false;
		comps = 1;
		image = lumpdata;
	}

	// Two pieces:
	dat1 = M_Malloc(comps * 256 * 256);
	dat2 = M_Malloc(comps * 64 * 256);

	if(height < 200 && part == 2) goto dont_load; // What is this?!
	if(height < 200) assumedWidth = 256;

	memset(dat1, 0, comps * 256 * 256);
	memset(dat2, 0, comps * 64 * 256);
	palette = W_CacheLumpNum(pallump, PU_CACHE);

	// Image data loaded, divide it into two parts.
	for(k = 0; k < height; k++)
		for(i = 0; i < 256; i++)
		{
			idx = k*assumedWidth + i;
			// Part one.
			for(c = 0; c < comps; c++)
				dat1[(k*256 + i)*comps + c] = image[idx*comps + c];
			// We can setup part two at the same time.
			if(i < 64 && part) 
				for(c = 0; c < comps; c++)
				{
					dat2[(k*64 + i)*comps + c] 
						= image[(idx + 256)*comps + c];
				}
		}

	// Upload part one.
	info->tex[0] = GL_UploadTexture(dat1, 256, 
		assumedWidth < 320? height : 256, false, false, rgbdata, false);
	GL_SetRawImageParams();
	
	if(part)
	{
		// And the other part.
		info->tex[1] = GL_UploadTexture(dat2, 64, 256, false, false, 
			rgbdata, false);
		GL_SetRawImageParams();

		// Add it to the list.
		GL_NewRawLump(lump);
	}

	info->width[0] = 256;
	info->width[1] = 64;
	info->height = height;

dont_load:
	if(need_free_image) M_Free(image);
	M_Free(dat1);
	M_Free(dat2);
	W_ChangeCacheTag(lump, PU_CACHE);
}

/*
 * Raw images are always 320x200.
 * Part is either 1 or 2. Part 0 means only the left side is loaded.
 * No splittex is created in that case. Once a raw image is loaded
 * as part 0 it must be deleted before the other part is loaded at the
 * next loading. Part can also contain the width and height of the 
 * texture. 
 *
 * 2003-05-30 (skyjake): External resources can be larger than 320x200,
 * but they're never split into two parts. 
 */
unsigned int GL_SetRawImage(int lump, int part)
{
	DGLuint texId = 0;
	image_t	image;
	lumptexinfo_t *info = lumptexinfo + lump;
	
	// Check the part.
	if(part < 0 || part > 2 || lump >= numlumps) return texId; 

	if(!info->tex[0])
	{
		// First try to find an external resource.
		filename_t fileName;
		if(R_FindResource(RC_PATCH, lumpinfo[lump].name, NULL, fileName)
			&& GL_LoadImage(&image, fileName, false) != NULL)
		{
			// We have the image in the buffer. We'll upload it as one
			// big texture.
			info->tex[0] = GL_UploadTexture(image.pixels, image.width, 
				image.height, image.pixelSize == 4, false, true, false);
			GL_SetRawImageParams();

			info->width[0] = 320;
			info->width[1] = 0;
			info->tex[1] = 0;
			info->height = 200;

			GL_DestroyImage(&image);						
		}				
		else
		{
			// Must load the old-fashioned data lump.
			GL_SetRawImageLump(lump, part);
		}
	}
	
	// Bind the part that was asked for.
	if(!info->tex[1])
	{
		// There's only one part, so we'll bind it.
		texId = info->tex[0];
	}
	else
	{
		texId = info->tex[part <= 1? 0 : 1];
	}
	gl.Bind(texId);

	// We don't track the current texture with raw images.
	curtex = 0;

	return texId;
}

/*
 * Loads and sets up a patch using data from the specified lump.
 */
void GL_PrepareLumpPatch(int lump)
{
	patch_t *patch = W_CacheLumpNum(lump, PU_CACHE);
	int numpels = patch->width * patch->height, alphaChannel;
	byte *buffer;

	if(!numpels) return; // This won't do!

	// Allocate memory for the patch.
	buffer = M_Calloc(2 * numpels);

	alphaChannel = DrawRealPatch(buffer, W_CacheLumpNum(pallump, PU_CACHE),
		patch->width, patch->height, patch, 0, 0, false, 0, true);
	if(filloutlines) ColorOutlines(buffer, patch->width, patch->height);
	
	// See if we have to split the patch into two parts.
	// This is done to conserve the quality of wide textures
	// (like the status bar) on video cards that have a pitifully
	// small maximum texture size. ;-)
	if(patch->width > maxTexSize) 
	{
		// The width of the first part is maxTexSize.
		int part2width = patch->width - maxTexSize;
		byte *tempbuff = M_Malloc(2 * MAX_OF(maxTexSize, part2width) 
			* patch->height);
		
		// We'll use a temporary buffer for doing to splitting.
		// First, part one.
		pixBlt(buffer, patch->width, patch->height, tempbuff, maxTexSize, 
			patch->height, alphaChannel, 0, 0, 0, 0, 
			maxTexSize, patch->height);
		lumptexinfo[lump].tex[0] = GL_UploadTexture(tempbuff, maxTexSize, 
			patch->height, alphaChannel, false, false, false);
		
		gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		
		// Then part two.
		pixBlt(buffer, patch->width, patch->height, tempbuff, part2width, patch->height,
			alphaChannel, maxTexSize, 0, 0, 0, part2width, patch->height);
		lumptexinfo[lump].tex[1] = GL_UploadTexture(tempbuff, part2width, patch->height, 
			alphaChannel, false, false, false);
		
		gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		
		GL_BindTexture(lumptexinfo[lump].tex[0]);
		
		lumptexinfo[lump].width[0] = maxTexSize;
		lumptexinfo[lump].width[1] = patch->width - maxTexSize;
		
		M_Free(tempbuff);
	}
	else // We can use the normal one-part method.
	{
		// Generate a texture.
		lumptexinfo[lump].tex[0] = GL_UploadTexture(buffer, patch->width, patch->height, 
			alphaChannel, false, false, false);
		gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		
		lumptexinfo[lump].width[0] = patch->width;
		lumptexinfo[lump].width[1] = 0;
	}
	M_Free(buffer);
}

//===========================================================================
// GL_SetPatch
//	No mipmaps are generated for regular patches.
//===========================================================================
void GL_SetPatch(int lump)	
{
	if(lump >= numlumps) return;

	if(!lumptexinfo[lump].tex[0])
	{
		patch_t *patch = W_CacheLumpNum(lump, PU_CACHE);
		filename_t fileName;
		image_t image;

		// Let's first try the resource locator and see if there is a
		// 'high-resolution' version available.
		if(!noHighResPatches
			&& (loadExtAlways || highResWithPWAD || W_IsFromIWAD(lump))
			&& R_FindResource(RC_PATCH, lumpinfo[lump].name, "-ck", fileName)
			&& GL_LoadImage(&image, fileName, false))
		{
			// This is our texture! No mipmaps are generated.
			lumptexinfo[lump].tex[0] = GL_UploadTexture(image.pixels, 
				image.width, image.height, image.pixelSize == 4, false, 
				true, false);

			// The original image is no longer needed.
			GL_DestroyImage(&image);

			// Set the texture parameters.
			gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
			gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
			gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
			gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

			lumptexinfo[lump].width[0] = patch->width;
			lumptexinfo[lump].width[1] = 0;
			lumptexinfo[lump].tex[1] = 0;
		}
		else
		{
			// Use data from the normal lump.
			GL_PrepareLumpPatch(lump);
		}		

		// The rest of the size information.
		lumptexinfo[lump].height = patch->height;
		lumptexinfo[lump].offx = -patch->leftoffset;
		lumptexinfo[lump].offy = -patch->topoffset;
	}
	else
	{
		GL_BindTexture(lumptexinfo[lump].tex[0]);
	}
	curtex = lumptexinfo[lump].tex[0];
}

/*
 * You should use Disable(DGL_TEXTURING) instead of this.
 */
void GL_SetNoTexture(void)
{
	gl.Bind(0);
	curtex = 0;
}

/*
 * Prepare a texture used in the lighting system. 'which' must be one
 * of the LST_* constants.
 */
DGLuint	GL_PrepareLSTexture(lightingtex_t which)
{
	int i;
	
	switch(which)
	{
	case LST_DYNAMIC:
		// The dynamic light map is a 64x64 grayscale 8-bit image.		
		if(!lightingTexNames[LST_DYNAMIC])
		{
			// We need to generate the texture, I see.
			byte *data = W_CacheLumpName("DLIGHT", PU_CACHE);
			byte *image, *alpha;
			if(!data) 
			{
				Con_Error("GL_SetLightTexture: DLIGHT not found.\n");
			}

			// Prepare the data by adding an alpha channel.
			image = Z_Malloc(64 * 64 * 2, PU_STATIC, 0);
			memset(image, 255, 64 * 64); // The colors are just white.
			alpha = image + 64 * 64;
			memcpy(alpha, data, 64 * 64);

			// The edges of the light texture must be fully transparent,
			// to prevent all unwanted repeating.
			for(i = 0; i < 64; i++)
			{
				// Top and bottom row.
				alpha[i] = 0;
				alpha[i + 64 * 63] = 0;
				
				// Leftmost and rightmost column.
				alpha[i * 64] = 0;
				alpha[63 + i * 64] = 0;
			}

/*#ifdef _DEBUG
  for(i = 28; i < 34; i++)
  {
  alpha[i] = 64;
  alpha[i + 64 * 63] = 64;
  alpha[i * 64] = 5;
  alpha[63 + i * 64] = 64;
  }
  #endif*/

			// We don't want to compress the flares (banding would be
			// noticeable).
			gl.Disable(DGL_TEXTURE_COMPRESSION);

			lightingTexNames[LST_DYNAMIC] = gl.NewTexture();
		
			// No mipmapping or resizing is needed, upload directly.
			gl.TexImage(DGL_LUMINANCE_PLUS_A8, 64, 64, 0, image);
			gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
			gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
			gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
			gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

			// Enable texture compression as usual.
			gl.Enable(DGL_TEXTURE_COMPRESSION);

			Z_Free(image);
		}
		// Set the info.
		texw = texh = 64;
		texmask = 0;
		return lightingTexNames[LST_DYNAMIC];

	case LST_GRADIENT:
		if(!lightingTexNames[LST_GRADIENT])
		{
			lightingTexNames[LST_GRADIENT] =
				GL_LoadGraphics("WallGlow", LGM_WHITE_ALPHA);
			
			gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
			gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
			gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
			gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		}
		// Global tex variables not set! (scalable texture)
		return lightingTexNames[LST_GRADIENT];

	case LST_RADIO_CO: // closed/open
	case LST_RADIO_CC: // closed/closed
		// FakeRadio corner shadows.
		if(!lightingTexNames[which])
		{
			lightingTexNames[which] =
				GL_LoadGraphics(which == LST_RADIO_CO? "RadioCO" : "RadioCC",
								LGM_WHITE_ALPHA);

			gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
			gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
			gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
			gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		}
		// Global tex variables not set! (scalable texture)
		return lightingTexNames[which];
		
	default:
		// Failed to prepare anything.
		return 0;
	}
}

//===========================================================================
// GL_PrepareFlareTexture
//===========================================================================
DGLuint GL_PrepareFlareTexture(int flare)
{
	int		w, h;

	// There are three flare textures.
	if(flare < 0 || flare >= NUM_FLARES) return 0;

	// What size? Hardcoded dimensions... argh.
	w = h = (flare == 2? 128 : 64);

	if(!flaretexnames[flare])
	{
		byte *image = W_CacheLumpName(flare==0? "FLARE" 
			: flare==1? "BRFLARE" : "BIGFLARE", PU_CACHE);
		if(!image)
			Con_Error("GL_PrepareFlareTexture: flare texture %i not found!\n", flare);

		// We don't want to compress the flares (banding would be noticeable).
		gl.Disable(DGL_TEXTURE_COMPRESSION);

		flaretexnames[flare] = gl.NewTexture();
		gl.TexImage(DGL_LUMINANCE, w, h, 0, image);
		gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

		// Allow texture compression as usual.
		gl.Enable(DGL_TEXTURE_COMPRESSION);
	}
	texmask = 0;
	texw = w;
	texh = h;
	return flaretexnames[flare];
}

//===========================================================================
// GL_GetLumpTexWidth
//===========================================================================
int GL_GetLumpTexWidth(int lump)
{
	return lumptexinfo[lump].width[0];
}

//===========================================================================
// GL_GetLumpTexHeight
//===========================================================================
int GL_GetLumpTexHeight(int lump)
{
	return lumptexinfo[lump].height;
}

//===========================================================================
// GL_SetTextureParams
//	Updates the textures, flats and sprites (gameTex) or the user 
//	interface textures (patches and raw screens).
//===========================================================================
void GL_SetTextureParams(int minMode, int magMode, int gameTex, int uiTex)
{
	int	i, k;
	flat_t **flats, **ptr;
	
	if(gameTex)
	{
		// Textures.
		for(i = 0; i < numtextures; i++)
			if(textures[i]->tex)	// Is the texture loaded?
			{
				gl.Bind(textures[i]->tex);
				gl.TexParameter(DGL_MIN_FILTER, minMode);
				gl.TexParameter(DGL_MAG_FILTER, magMode);
			}
		// Flats.
		flats = R_CollectFlats(NULL);
		for(ptr = flats; *ptr; ptr++)
			if(lumptexinfo[(*ptr)->lump].tex[0]) // Is the texture loaded?
			{
				gl.Bind(lumptexinfo[(*ptr)->lump].tex[0]);
				gl.TexParameter(DGL_MIN_FILTER, minMode);
				gl.TexParameter(DGL_MAG_FILTER, magMode);
			}
		Z_Free(flats);
		// Sprites.
		for(i = 0; i < numspritelumps; i++)
			if(spritelumps[i].tex)
			{
				gl.Bind(spritelumps[i].tex);
				gl.TexParameter(DGL_MIN_FILTER, minMode);
				gl.TexParameter(DGL_MAG_FILTER, magMode);
			}
		// Translated sprites.
		for(i = 0; i < numtranssprites; i++)
		{	
			gl.Bind(transsprites[i].tex);
			gl.TexParameter(DGL_MIN_FILTER, minMode);
			gl.TexParameter(DGL_MAG_FILTER, magMode);
		}
	}
	if(uiTex)
	{
		for(i = 0; i < numlumps; i++)
			for(k = 0; k < 2; k++)
				if(lumptexinfo[i].tex[k])
				{
					gl.Bind(lumptexinfo[i].tex[k]);
					gl.TexParameter(DGL_MIN_FILTER, minMode);
					gl.TexParameter(DGL_MAG_FILTER, magMode);
				}
	}
}

//===========================================================================
// GL_UpdateTexParams
//===========================================================================
void GL_UpdateTexParams(int mipmode)
{
	mipmapping = mipmode;
	GL_SetTextureParams(glmode[mipmode], glmode[texMagMode], true, false);
}

//===========================================================================
// GL_LowRes
//===========================================================================
void GL_LowRes()
{
	// Set everything as low as they go.
	GL_SetTextureParams(DGL_NEAREST, DGL_NEAREST, true, true);
}

//===========================================================================
// GL_DeleteRawImages
//	To save texture memory, delete all raw image textures. Raw images are
//	used as interlude backgrounds, title screens, etc. Called from 
//	DD_SetupLevel.
//===========================================================================
void GL_DeleteRawImages(void)
{
	int i;

	for(i = 0; i < numrawlumps; i++)
	{
		gl.DeleteTextures(2, lumptexinfo[rawlumps[i]].tex);
		lumptexinfo[rawlumps[i]].tex[0] 
			= lumptexinfo[rawlumps[i]].tex[1] = 0;
	}

	free(rawlumps);
	rawlumps = 0;
	numrawlumps = 0;
}

//===========================================================================
// GL_UpdateRawScreenParams
//	Updates the raw screen smoothing (linear magnification).
//===========================================================================
void GL_UpdateRawScreenParams(int smoothing)
{
	int		i;
	int		glmode = smoothing? DGL_LINEAR : DGL_NEAREST;

	linearRaw = smoothing;
	for(i = 0; i < numrawlumps; i++)
	{
		// First part 1.
		gl.Bind(lumptexinfo[rawlumps[i]].tex[0]);
		gl.TexParameter(DGL_MAG_FILTER, glmode);
		// Then part 2.
		gl.Bind(lumptexinfo[rawlumps[i]].tex[1]);
		gl.TexParameter(DGL_MAG_FILTER, glmode);
	}
}

//===========================================================================
// GL_TextureFilterMode
//===========================================================================
void GL_TextureFilterMode(int target, int parm)
{
	if(target == DD_TEXTURES) GL_UpdateTexParams(parm);
	if(target == DD_RAWSCREENS) GL_UpdateRawScreenParams(parm);
}

//===========================================================================
// GL_DeleteTexture
//	Deletes a texture. Only for textures (not for sprites, flats, etc.).
//===========================================================================
void GL_DeleteTexture(int texidx)
{
	if(texidx < 0 || texidx >= numtextures) return;

	if(textures[texidx]->tex)
	{
		gl.DeleteTextures(1, &textures[texidx]->tex);
		textures[texidx]->tex = 0;
	}
}

//===========================================================================
// GL_GetTextureName
//===========================================================================
unsigned int GL_GetTextureName(int texidx)
{
	return textures[texidx]->tex;
}

//===========================================================================
// GL_GetSkinTex
//===========================================================================
skintex_t *GL_GetSkinTex(const char *skin)
{
	int i;
	skintex_t *st;
	char realpath[256];

	if(!skin[0]) return NULL;

	// Convert the given skin file to a full pathname.
	_fullpath(realpath, skin, 255);

	for(i = 0; i < numskinnames; i++)
		if(!stricmp(skinnames[i].path, realpath))
			return skinnames + i;

	// We must allocate a new skintex_t.
	skinnames = realloc(skinnames, sizeof(*skinnames) * ++numskinnames);
	st = skinnames + (numskinnames-1);
	strcpy(st->path, realpath);
	st->tex = 0; // Not yet prepared.

	if(verbose)
	{
		Con_Message("SkinTex: %s => %i\n", M_Pretty(skin), st - skinnames);
	}
	return st;	
}

//===========================================================================
// GL_GetSkinTexByIndex
//===========================================================================
skintex_t *GL_GetSkinTexByIndex(int id)
{
	if(id < 0 || id >= numskinnames) return NULL; // No such thing, pal.
	return skinnames + id;
}

//===========================================================================
// GL_GetSkinTexIndex
//===========================================================================
int GL_GetSkinTexIndex(const char *skin)
{
	skintex_t *sk = GL_GetSkinTex(skin);
	if(!sk) return -1;	// 'S no good, fellah!
	return sk - skinnames;
}

//===========================================================================
// GL_PrepareSkin
//===========================================================================
unsigned int GL_PrepareSkin(model_t *mdl, int skin)
{
	int		width, height, size;
	byte	*image;
	skintex_t *st;

	if(skin < 0 || skin >= mdl->info.numSkins) skin = 0;
	st = GL_GetSkinTexByIndex(mdl->skins[skin].id);
	if(!st) return 0; // Urgh.

	// If the texture has already been loaded, we don't need to
	// do anything.
	if(!st->tex)
	{
		// Load the texture. R_LoadSkin allocates enough memory with M_Malloc.
		image = R_LoadSkin(mdl, skin, &width, &height, &size);
		if(!image)
		{
			Con_Error("GL_PrepareSkin: %s not found.\n", mdl->skins[skin].name);
		}

		if(!mdl->allowTexComp)
		{
			// This will prevent texture compression.
			gl.Disable(DGL_TEXTURE_COMPRESSION);
		}

		st->tex = GL_UploadTexture(image, width, height, 
			size == 4, true, true, false);

		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

		// Compression can be enabled again.
		gl.Enable(DGL_TEXTURE_COMPRESSION);

		// We don't need the image data any more.
		M_Free(image);
	}
	return st->tex;
}

//===========================================================================
// GL_PrepareShinySkin
//===========================================================================
unsigned int GL_PrepareShinySkin(modeldef_t *md, int sub)
{
//	model_t *mdl = modellist[md->sub[sub].model];
	skintex_t *stp = GL_GetSkinTexByIndex(md->sub[sub].shinyskin);
	image_t image;

	if(!stp) return 0;	// Does not have a shiny skin.
	if(!stp->tex)
	{
		// Load in the texture. 
		if(!GL_LoadImageCK(&image, stp->path, true))
		{
			Con_Error("GL_PrepareShinySkin: Failed to load '%s'.\n", 
				stp->path);
		}

		stp->tex = GL_UploadTexture(image.pixels, image.width, image.height, 
			image.pixelSize == 4, true, true, false);

		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

		// We don't need the image data any more.
		GL_DestroyImage(&image);
	}
	return stp->tex;
}

//===========================================================================
// GL_IsColorKeyed
//	Returns true if the given path name refers to an image, which should
//	be color keyed. Color keying is done for both (0,255,255) and 
//	(255,0,255).
//===========================================================================
boolean GL_IsColorKeyed(const char *path)
{
	char buf[256];

	strcpy(buf, path);
	strlwr(buf);
	return strstr(buf, "-ck.") != NULL;
}

//===========================================================================
// GL_ColorKey
//	Returns true if the given color is either (0,255,255) or (255,0,255).
//===========================================================================
boolean GL_ColorKey(byte *color)
{
	return color[CB] == 0xff 
		&& ((color[CR] == 0xff && color[CG] == 0)
			|| (color[CR] == 0 && color[CG] == 0xff));
}

//===========================================================================
// GL_DoColorKeying
//	Buffer must be RGBA. Doesn't touch the non-keyed pixels.
//===========================================================================
void GL_DoColorKeying(byte *rgbaBuf, int width)
{
	int i;

	for(i = 0; i < width; i++, rgbaBuf += 4)
		if(GL_ColorKey(rgbaBuf))
			rgbaBuf[3] = rgbaBuf[2] = rgbaBuf[1] = rgbaBuf[0] = 0;
}

//--------------------------------------------------------------------------

//===========================================================================
// CCmdLowRes
//===========================================================================
int CCmdLowRes(int argc, char **argv)
{
	GL_LowRes();
	return true;
}

#ifdef _DEBUG
int CCmdTranslateFont(int argc, char **argv)
{
	char	name[32];
	int		i, lump, size;
	patch_t	*patch;
	byte	redToWhite[256];

	if(argc < 3) return false;

	// Prepare the red-to-white table.
	for(i=0; i<256; i++)
	{
		if(i == 176) redToWhite[i] = 168; // Full red -> white.
		else if(i == 45) redToWhite[i] = 106;
		else if(i == 46) redToWhite[i] = 107;
		else if(i == 47) redToWhite[i] = 108; 
		else if (i >= 177 && i <= 191)
		{
			redToWhite[i] = 80 + (i-177)*2;
		}
		else redToWhite[i] = i; // No translation for this.
	}

	// Translate everything.
	for(i=0; i<256; i++)
	{
		sprintf(name, "%s%.3d", argv[1], i);
		if((lump = W_CheckNumForName(name)) != -1)
		{
			Con_Message( "%s...\n", name);
			size = W_LumpLength(lump);
			patch = (patch_t*) Z_Malloc(size, PU_STATIC, 0);
			memcpy(patch, W_CacheLumpNum(lump, PU_CACHE), size);
			TranslatePatch(patch, redToWhite);
			sprintf(name, "%s%.3d.lmp", argv[2], i);
			M_WriteFile(name, patch, size);
			Z_Free(patch);
		}
	}
	return true;
}
#endif

int CCmdResetTextures(int argc, char **argv)
{
	GL_ClearTextureMemory();
	GL_LoadSystemTextures(true);
	Con_Printf("All DGL textures deleted.\n");
	return true;
}

int CCmdMipMap(int argc, char **argv)
{
	if(argc != 2)
	{
		Con_Printf( "Usage: %s (0-5)\n", argv[0]);
		Con_Printf( "0 = GL_NEAREST\n");
		Con_Printf( "1 = GL_LINEAR\n");
		Con_Printf( "2 = GL_NEAREST_MIPMAP_NEAREST\n");
		Con_Printf( "3 = GL_LINEAR_MIPMAP_NEAREST\n");
		Con_Printf( "4 = GL_NEAREST_MIPMAP_LINEAR\n");
		Con_Printf( "5 = GL_LINEAR_MIPMAP_LINEAR\n");
		return true;
	}
	GL_UpdateTexParams(strtol(argv[1], NULL, 0));
	return true;
}

int CCmdSmoothRaw(int argc, char **argv)
{
	if(argc != 2)
	{
		Con_Printf( "Usage: %s (0-1)\n", argv[0]);
		Con_Printf( "Set the rendering mode of fullscreen images.\n");
		Con_Printf( "0 = GL_NEAREST\n");
		Con_Printf( "1 = GL_LINEAR\n");
		return true;
	}
	GL_UpdateRawScreenParams(strtol(argv[1], NULL, 0));	
	return true;
}

