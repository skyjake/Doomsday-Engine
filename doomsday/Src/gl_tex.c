
//**************************************************************************
//**
//** GL_TEX.C
//**
//** A lot of this stuff actually belongs in Refresh.
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>
#include <math.h>

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_refresh.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#define TEXQ_BEST	8
#define NUM_FLARES	3

#define RGB18(r, g, b) ((r)+((g)<<6)+((b)<<12))

// TYPES -------------------------------------------------------------------

// A translated sprite.
typedef struct
{
	int				patch;
	DGLuint			tex;
	unsigned char	*table;
} transspr_t;

// Sky texture topline colors.
typedef struct
{
	int				texidx;
	unsigned char	rgb[3];
} skycol_t;

// Model skin.
typedef struct
{
	char			path[256];
	DGLuint			tex;
} skintex_t;

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void	averageColorIdx(rgbcol_t *sprcol, byte *data, int w, int h, byte *palette, boolean has_alpha);
void	averageColorRGB(rgbcol_t *col, byte *data, int w, int h);
byte *	GL_LoadHighResFlat(char *name, int *width, int *height, int *pixsize);
void	GL_DeleteDetailTexture(detailtex_t *dtex);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int		maxTexSize;		// Maximum supported texture size.
extern int		ratioLimit;
extern boolean	palettedTextureExtAvailable;
extern boolean	s3tcAvailable;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean			filloutlines = true;
boolean			paletted = false;	// Use GL_EXT_paletted_texture (cvar)
boolean			load8bit = false;	// Load textures as 8 bit? (with paltex)

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

//texsize_t		*lumptexsizes;	// Sizes for all the lumps. 

skycol_t		*skytop_colors = NULL;
int				num_skytop_colors = 0;

DGLuint			dltexname;	// Name of the dynamic light texture.
DGLuint			glowtexname;

char			hiTexPath[256], hiTexPath2[256];

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean	texInited = false;	// Init done.
static boolean	allowMaskedTexEnlarge = false;
static boolean	noHighResTex = false;

// Raw screen lumps (just lump numbers).
static int		*rawlumps, numrawlumps;	
	
// Skinnames will only *grow*. It will never be destroyed, not even
// at resets. The skin textures themselves will be deleted, though.
// This is because we want to have permanent ID numbers for skins,
// and the ID numbers are the same as indices to the skinnames array.
// Created in r_model.c, when registering the skins.
static int		numskinnames;
static skintex_t *skinnames;

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
//	Yeah, 13 parameters.
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

	// The -bigmtex option allows the engine to enlarge masked textures
	// that have taller patches than they are themselves.
	allowMaskedTexEnlarge = ArgCheck("-bigmtex") > 0;

	// The -nohitex option disables the high resolution TGA textures.
	noHighResTex = ArgCheck("-nohightex") > 0;

	// The -texdir option specifies a path to look for TGA textures.
	if(ArgCheckWith("-texdir", 1))
	{
		M_TranslatePath(ArgNext(), hiTexPath);
		Dir_ValidDir(hiTexPath);
	}
	if(ArgCheckWith("-texdir2", 1))
	{
		M_TranslatePath(ArgNext(), hiTexPath2);
		Dir_ValidDir(hiTexPath2);
	}

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

	// System textures loaded in GL_LoadSystemTextures.
	dltexname = glowtexname = 0;

	// Initialization done.
	texInited = true;
}

//===========================================================================
// GL_ShutdownTextureManager
//	Call this if a full cleanup of the textures is required (engine update).
//===========================================================================
void GL_ShutdownTextureManager()
{
	if(!texInited) return;

	GL_ClearTextureMemory();

	// Destroy all bookkeeping -- into the shredder, I say!!
	free(skytop_colors);
	skytop_colors = 0;
	num_skytop_colors = 0;

	texInited = false;
}

#if 0
//===========================================================================
// GL_ResetTextureManager
//	Shutdown and reinit is a better option.
//===========================================================================
void GL_ResetTextureManager(void)
{
	int i;

	// Textures haven't been initialized, nothing to do here.
	if(!texInited) return;	

	if(texnames)
	{
		gl.DeleteTextures(numtextures, texnames);
		memset(texnames, 0, sizeof(DGLuint)*numtextures);
	}
	if(texmasked)	
		memset(texmasked, 0, numtextures);

	if(spritenames)
	{
		gl.DeleteTextures(numspritelumps, spritenames);
		memset(spritenames, 0, sizeof(DGLuint)*numspritelumps);
		memset(spritecolors, 0, sizeof(rgbcol_t)*numspritelumps);
	}

	// Delete the translated sprite textures.
	for(i = 0; i < numtranssprites; i++)
	{
		gl.DeleteTextures(1, &transsprites[i].tex);
	}
	free(transsprites);
	transsprites = 0;
	numtranssprites = 0;

	free(skytop_colors);
	skytop_colors = 0;
	num_skytop_colors = 0;

	gl.DeleteTextures(1, &dltexname);
	gl.DeleteTextures(1, &glowtexname);
	dltexname = glowtexname = 0;
	gl.SetInteger(DGL_LIGHT_TEXTURE, dltexname = GL_PrepareLightTexture());
	gl.SetInteger(DGL_GLOW_TEXTURE, glowtexname = GL_PrepareGlowTexture());
	
	if(flaretexnames)
	{
		gl.DeleteTextures(NUM_FLARES, flaretexnames);
		memset(flaretexnames, 0, sizeof(flaretexnames));
	}

	// Delete model skins.
	for(i = 0; i < numskinnames; i++)
		if(skinnames[i].tex)
		{
			gl.DeleteTextures(1, &skinnames[i].tex);
			skinnames[i].tex = 0; // It's gone.
		}

	// Detail textures.
	for(i = 0; i < defs.num_details; i++)
		if(details[i].gltex != ~0)
		{
			gl.DeleteTextures(1, &details[i].gltex);
			details[i].gltex = ~0;
		}
}
#endif

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


#if 0
//===========================================================================
// GL_ResetLumpTexData
//===========================================================================
void GL_ResetLumpTexData()
{
	if(!texInited) return;

	// Free the raw lumps book-keeping table.
	GL_DeleteRawImages();

	/*gl.DeleteTextures(mynumlumps, lumptexnames);
	gl.DeleteTextures(mynumlumps, lumptexnames2);
	memset(lumptexnames, 0, sizeof(DGLuint)*mynumlumps);
	memset(lumptexnames2, 0, sizeof(DGLuint)*mynumlumps);
	memset(lumptexsizes, 0, sizeof(texsize_t)*mynumlumps);*/
}
#endif

//===========================================================================
// GL_LoadSystemTextures
//	Prepares all the system textures (dlight, ptcgens).
//===========================================================================
void GL_LoadSystemTextures(void)
{
	if(!texInited) return;

	dltexname = GL_PrepareLightTexture();
	glowtexname = GL_PrepareGlowTexture();

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
	if(!texInited) return;

	gl.DeleteTextures(1, &dltexname);
	gl.DeleteTextures(1, &glowtexname);
	dltexname = 0;
	glowtexname = 0;

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
	for(i = 0; i < defs.count.details.num; i++)
		GL_DeleteDetailTexture(details + i);

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
	if(curtex != texname) 
	{
		gl.Bind(texname);
		curtex = texname;
	}
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
	int		inSize = informat==2? 1 : informat;
	int		outSize = outformat==2? 1 : outformat;
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
			out[3] = 0;
		}
	}
}

//===========================================================================
// scaleLine
//	Len is in measured in out units. Comps is the number of components per 
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
		// The first pixel.
		memcpy(out, in, comps);
		out += outStride;
		// Step at each out pixel between the first and last ones.
		for(i=1; i<outLen-1; i++, out += outStride, inPos += inPosDelta)
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
		memcpy(out, in + (inLen-1)*inStride, comps);
	}
	else if(inToOutScale < 1)
	{
		// Minification needs to calculate the average of each of 
		// the pixels contained by the out pixel.
		unsigned int cumul[4] = {0, 0, 0, 0}, count = 0;
		int outpos = 0;
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
	}
	else 
	{
		// No need for scaling.
		for(i = 0; i < outLen; i++, out += outStride, in += inStride)
			memcpy(out, in, comps);
	}
}

//===========================================================================
// ScaleBuffer32
//===========================================================================
static void ScaleBuffer32(byte *in, int inWidth, int inHeight,
						  byte *out, int outWidth, int outHeight, int comps)
{
	int		i;
	byte	*temp = Z_Malloc(outWidth * inHeight * comps, PU_STATIC, 0);

	// First scale horizontally, to outWidth, into the temporary buffer.
	for(i = 0; i < inHeight; i++)
	{
		scaleLine(in + inWidth*comps*i, comps, temp + outWidth*comps*i, comps, outWidth, 
			inWidth, comps);
	}
	// Then scale vertically, to outHeight, into the out buffer.
	for(i = 0; i < outWidth; i++)
	{
		scaleLine(temp + comps*i, outWidth*comps, out + comps*i, outWidth*comps, outHeight,
			inHeight, comps);
	}
	Z_Free(temp);
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
		int outDim = width > 1? width : height;
		out = in;
		for(x=0; x<outDim; x++, in += comps*2)
			for(c=0; c<comps; c++, out++) 
				*out = (in[c] + in[comps+c]) >> 1;
	}
	else // Unconstrained, 2x2 -> 1x1 reduction?
	{
		out = in;
		for(y=0; y<outH; y++, in += width*comps)
			for(x=0; x<outW; x++, in += comps*2)
				for(c=0; c<comps; c++, out++) 
					*out = (in[c] + in[comps + c] + in[comps*width + c] + in[comps*(width+1) + c]) >> 2;
	}
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

	if(noStretch)
	{
		levelWidth = CeilPow2(width);
		levelHeight = CeilPow2(height);
		// MaxTexSize may prevent using noStretch.
		if(levelWidth > maxTexSize)
		{
			levelWidth = maxTexSize;
			noStretch = false;
		}
		if(levelHeight > maxTexSize)
		{
			levelHeight = maxTexSize;
			noStretch = false;
		}
	}
	else
	{
		// Determine the most favorable size for the texture.
		if(texQuality == TEXQ_BEST)	// The best quality.
		{
			// At the best texture quality level, all textures are
			// sized *upwards*, so no details are lost. This takes
			// more memory, but naturally looks better.
			levelWidth = CeilPow2(width);
			levelHeight = CeilPow2(height);
		}
		else if(texQuality == 0)
		{
			// At the lowest quality, all textures are sized down
			// to the nearest power of 2.
			levelWidth = FloorPow2(width);
			levelHeight = FloorPow2(height);
		}
		else 
		{
			// At the other quality levels, a weighted rounding
			// is used.
			levelWidth = WeightPow2(width, 1 - texQuality/(float)TEXQ_BEST);
			levelHeight = WeightPow2(height, 1 - texQuality/(float)TEXQ_BEST);
		}
	}

	// Hardware limitations may force us to modify the preferred
	// texture size.
	if(levelWidth > maxTexSize) levelWidth = maxTexSize;
	if(levelHeight > maxTexSize) levelHeight = maxTexSize;
	if(ratioLimit)
	{
		if(levelWidth > levelHeight) // Wide texture.
		{
			if(levelHeight < levelWidth/ratioLimit)
				levelHeight = levelWidth / ratioLimit;
		}
		else // Tall texture.
		{
			if(levelWidth < levelHeight/ratioLimit)
				levelWidth = levelHeight / ratioLimit;
		}
	}
	
	// Number of color components in the destination image.
	comps = alphaChannel? 4 : 3;

	// Get the RGB(A) version of the original texture.
	//rgbaOriginal = Z_Malloc(width * height * (alphaChannel? 4 : 3), PU_STATIC, 0);
	if(RGBData)
	{
		// The source image can be used as-is.
		freeOriginal = false;
		rgbaOriginal = data;

		//memcpy(rgbaOriginal, data, width * height * (alphaChannel? 4 : 3));
	}
	else
	{
		// Convert a paletted source image to truecolor so it can be scaled.
		freeOriginal = true;
		rgbaOriginal = M_Malloc(width * height * comps);
		GL_ConvertBuffer(width, height, alphaChannel? 2 : 1, comps, 
			data, rgbaOriginal,	!load8bit);
	}
	
	// Prepare the RGB(A) buffer for the texture: we want a buffer with 
	// power-of-two dimensions. It will be the mipmap level zero.
	// The buffer will be modified by the mipmap generation (if done here).
	buffer = M_Malloc(levelWidth * levelHeight * comps);
	if(noStretch)
	{
		// Copy the image into a buffer with power-of-two dimensions.
		memset(buffer, 0, levelWidth * levelHeight * comps);
		for(i = 0; i < height; i++) // Copy line by line.
			memcpy(buffer + levelWidth*comps*i, rgbaOriginal + width*comps*i, 
				comps*width);
	}
	else
	{
		// Stretch to fit into power-of-two.
		if(width != levelWidth || height != levelHeight)
		{
			//buffer = M_Malloc(levelWidth * levelHeight * comps);
			ScaleBuffer32(rgbaOriginal, width, height, buffer, levelWidth, 
				levelHeight, comps);
		}
		else
		{
			// We can use the input data as-is.
			//buffer = rgbaOriginal;
			memcpy(buffer, rgbaOriginal, width * height * comps);
		}
	}

	// The RGB(A) copy of the source image is no longer needed.
	if(freeOriginal) M_Free(rgbaOriginal);
	rgbaOriginal = NULL;
	
	// Generate a new texture name and bind it.
	texName = gl.NewTexture();

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
		// DGL knows how to generate mipmaps for RGB(A) textures.
		if(gl.TexImage(alphaChannel? DGL_RGBA : DGL_RGB, levelWidth, 
			levelHeight, generateMipmaps? DGL_TRUE : DGL_FALSE, 
			buffer) != DGL_OK)
		{
			Con_Error("GL_UploadTexture: TexImage failed (%i x %i), alpha:%i\n",
				levelWidth, levelHeight, alphaChannel);
		}
	}

	M_Free(buffer);
	
	return texName;
}

//===========================================================================
// GL_LoadDetailTexture
//	Detail textures are 128x128 grayscale 8-bit raw data.
//===========================================================================
DGLuint GL_LoadDetailTexture(int num)
{
	DGLuint dtex;
	byte *lumpData, *image;
	int w = 256, h = 256;
		
	if(num < 0) return 0; // No such lump?!
	lumpData = W_CacheLumpNum(num, PU_STATIC);

	// First try loading it as a PCX image.
	if(PCX_MemoryGetSize(lumpData, &w, &h))
	{
		// Nice...
		image = M_Malloc(w*h*3);
		PCX_MemoryLoad(lumpData, W_LumpLength(num), w, h, image);
		dtex = gl.NewTexture();
		if(!gl.TexImage(DGL_RGB, w, h, DGL_TRUE, image))
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
					Con_Message("GL_LoadDetailTexture: Must be 128x128 or 64x64.\n");
					W_ChangeCacheTag(num, PU_CACHE);
					return 0;
				}
				w = h = 64;
			}
			else w = h = 128;
		}
		image = M_Malloc(w * h);
		memcpy(image, W_CacheLumpNum(num, PU_CACHE), w * h);
		dtex = gl.NewTexture();
		// Make mipmap levels.
		gl.TexImage(DGL_LUMINANCE, w, h, DGL_TRUE, image);
	}

	// Set texture parameters.
	gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR_MIPMAP_LINEAR);
	gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
	gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
	gl.TexParameter(DGL_WRAP_T, DGL_REPEAT);
	
	// Free allocated memory.
	M_Free(image);
	W_ChangeCacheTag(num, PU_CACHE);
	return dtex;
}

//===========================================================================
// GL_PrepareDetailTexture
//===========================================================================
DGLuint GL_PrepareDetailTexture(int index, boolean is_wall_texture,
								ded_detailtexture_t **dtdef)
{
	int i, k;
	DGLuint tex;

	// Search through the assignments. 
	for(i = defs.count.details.num - 1; i >= 0; i--)
	{
		if(is_wall_texture && index == details[i].wall_texture
			|| !is_wall_texture && index == details[i].flat_lump)
		{
			if(dtdef) *dtdef = defs.details + i;
			// Hey, a match. Load this?
			if(details[i].gltex == ~0)
			{
				tex = GL_LoadDetailTexture(details[i].detail_lump);
				// FIXME: This is a bit questionable...
				// Set the texture ID for all the definitions that use it.
				// This way the texture gets loaded only once.
				for(k = 0; k < defs.count.details.num; k++)
					if(details[k].detail_lump == details[i].detail_lump)
						details[k].gltex = tex;
			}
			return details[i].gltex;
		}
	}
	return 0; // There is no detail texture for this.
}

//===========================================================================
// GL_DeleteDetailTexture
//===========================================================================
void GL_DeleteDetailTexture(detailtex_t *dtex)
{
	int i;
	DGLuint texname = dtex->gltex;

	if(texname == ~0) return; // Not loaded.
	gl.DeleteTextures(1, &texname);

	// Remove all references to this texture.
	for(i = 0; i < defs.count.details.num; i++)
		if(details[i].gltex == texname)
			details[i].gltex = ~0;
}

//===========================================================================
// GL_BindTexFlat
//===========================================================================
unsigned int GL_BindTexFlat(flat_t *fl)
{
	DGLuint name;
	byte *flatptr;
	int lump = fl->lump, width, height, pixSize = 3;
	boolean RGBData = false, freeptr = false;
	ded_detailtexture_t *def;

	if(lump >= numlumps	|| lump == skyflatnum)
	{
		// The sky flat is not a real flat at all.
		GL_BindTexture(0);	
		return 0;
	}

	// Is there a high resolution version?
	if((flatptr = GL_LoadHighResFlat(lumpinfo[lump].name, &width, 
		&height, &pixSize)) != NULL)
	{
		RGBData = true;
		freeptr = true;
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
	gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
	
	if(freeptr) M_Free(flatptr);

	// The name of the texture is returned.
	return name;
}

//===========================================================================
// GL_PrepareFlat
//	Returns the OpenGL name of the texture.
//	(idx is really a lumpnum)
//===========================================================================
unsigned int GL_PrepareFlat(int idx)
{
	flat_t *flat = R_GetFlat(idx);

	// Get the translated one?
	if(flat->translation != idx)
		flat = R_GetFlat(flat->translation); 

	if(!lumptexinfo[flat->lump].tex[0])
	{
		// The flat isn't yet bound with OpenGL.
		lumptexinfo[flat->lump].tex[0] = GL_BindTexFlat(flat);
	}
	texw = texh = 64;
	texmask = false;
	texdetail = flat->detail.tex? &flat->detail : 0;
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
	gl.Bind(curtex = GL_PrepareFlat(idx));
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
// GL_LoadImage
//	Loads PCX, TGA and PNG images. The returned buffer must be freed 
//	with M_Free. Color keying is done if "-ck." is found in the filename.
//===========================================================================
byte *GL_LoadImage(const char *imagefn, int *width, int *height,
				   int *pixsize, boolean *masked, boolean usemodelpath)
{
	DFILE	*file;
	char	filename[256];
	byte	*buffer, *ckdest, ext[40], *in, *out;
	int		format, i, numpx; 

	if(usemodelpath) 
	{
		if(!R_FindModelFile(imagefn, filename))
			return NULL; // Not found.
	}
	else
		strcpy(filename, imagefn);

	// We know how to load PCX, TGA and PNG.
	M_GetFileExt(filename, ext);
	if(!strcmp(ext, "pcx"))
	{
		if(!PCX_GetSize(filename, width, height)) return NULL;
		buffer = M_Malloc(4 * (*width) * (*height));
		PCX_Load(filename, *width, *height, buffer);
		*pixsize = 3; // PCXs can't be masked.
	}
	else if(!strcmp(ext, "tga"))
	{
		if(!TGA_GetSize(filename, width, height)) return NULL; 

		file = F_Open(filename, "rb");
		if(!file) return NULL;	
		
		// Allocate a big enough buffer and read in the image.
		buffer = M_Malloc(4 * (*width) * (*height));
		format = TGA_Load32_rgba8888(file, *width, *height, buffer);
		*pixsize = (format == TGA_TARGA24? 3 : 4);
		F_Close(file);
	}
	else if(!strcmp(ext, "png"))
	{
		buffer = PNG_Load(filename, width, height, pixsize);
		if(!buffer) return NULL;
	}

	if(verbose) 
		Con_Message("LoadImage: %s (%ix%i)\n", filename, *width, *height);

	numpx = (*width) * (*height);

	// How about some color-keying?
	if(GL_IsColorKeyed(filename))
	{
		// We must allocate a new buffer if the loaded image has three
		// color componenets.
		if(*pixsize < 4)
		{
			ckdest = M_Malloc(4 * (*width) * (*height));
			for(in = buffer, out = ckdest, i = 0; i < numpx; 
				i++, in += *pixsize, out += 4)
			{
				if(GL_ColorKey(in))
					memset(out, 0, 4);	// Totally black.
				else
				{
					memcpy(out, in, 3); // The color itself.
					out[CA] = 255;		// Opaque.
				}
			}
			M_Free(buffer);
			buffer = ckdest;
		}
		else // We can do the keying in-buffer.
		{
			// This preserves the alpha values of non-keyed pixels.
			for(i = 0; i < *height; i++)
				GL_DoColorKeying(buffer + 4*i*(*width), *width);
		}
		// Color keying is done; now we have 4 bytes per pixel.
		*pixsize = 4;
	}

	// Any alpha pixels?
	*masked = false;
	if(*pixsize == 4)
		for(i = 0, in = buffer; i < numpx; i++, in += 4)
			if(in[3] < 255)
			{
				*masked = true;
				break;
			}
	return buffer;
}

//===========================================================================
// GL_LoadImageCK
//	First sees if there is a color-keyed version of the given image. If
//	there is it is loaded. Otherwise the 'regular' version is loaded.
//===========================================================================
byte *GL_LoadImageCK(const char *name, int *width, int *height, int *pixsize, 
					 boolean *masked, boolean usemodelpath)
{
	char fn[256], *ptr;
	byte *img;

	strcpy(fn, name);
	// Append the "-ck" and try to load.
	ptr = strrchr(fn, '.');
	if(ptr)
	{
		memmove(ptr + 3, ptr, strlen(ptr) + 1);
		ptr[0] = '-';
		ptr[1] = 'c';
		ptr[2] = 'k';
		if((img = GL_LoadImage(fn, width, height, pixsize, masked, 
			usemodelpath)) != NULL) return img;
	}
	return GL_LoadImage(name, width, height, pixsize, masked, usemodelpath);
}

//===========================================================================
// GL_LoadHighRes
//===========================================================================
byte *GL_LoadHighRes(char *name, char *path, char *altPath, char *prefix,
					 int *width, int *height, int *pixSize, boolean *masked,
					 boolean allowColorKey)
{
	char filename[256], *formats[3] = { "png", "tga", "pcx" };
	byte *buf;
	int p, f;

	if(noHighResTex) return NULL;
	for(f = 0; f < 3; f++)
		for(p = 0; p < 2; p++)
		{
			// Form the file name.
			sprintf(filename, "%s%s%.8s.%s", !p? altPath : path, prefix, 
				name, formats[f]);
			if(allowColorKey)
			{
				if((buf = GL_LoadImageCK(filename, width, height, pixSize, 
					masked, false)) != NULL) return buf;
			}
			else
			{
				if((buf = GL_LoadImage(filename, width, height, pixSize, 
					masked, false)) != NULL) return buf;
			}
		}
	return NULL;
}

//===========================================================================
// GL_LoadHighResTexture
//	The returned buffer must be freed with M_Free.
//===========================================================================
byte *GL_LoadHighResTexture
	(char *name, int *width, int *height, int *pixsize, boolean *masked)
{
	return GL_LoadHighRes(name, hiTexPath, hiTexPath2, "", width, height,
		pixsize, masked, true);
}

//===========================================================================
// GL_LoadHighResFlat
//	The returned buffer must be freed with M_Free.
//===========================================================================
byte *GL_LoadHighResFlat(char *name, int *width, int *height, int *pixsize)
{
	boolean masked;
	
	// FIXME: Why no subdir named "Flats"?
	return GL_LoadHighRes(name, hiTexPath, hiTexPath2, "Flat-", width, 
		height, pixsize, &masked, false);
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
	ded_detailtexture_t *def;
	byte		*buffer = NULL;
	texture_t	*tex;
	boolean		alphaChannel = false, masked = false, RGBData = false;
	int			i, width, height;

	if(idx == 0)
	{
		// No texture?
		texw = 1;
		texh = 1;
		texmask = 0;
		texdetail = 0;
		return 0;
	}
	idx = texturetranslation[idx];
	tex = textures[idx];
	if(!textures[idx]->tex)
	{
		// Try to load a high resolution version of this texture.
		if((buffer = GL_LoadHighResTexture(tex->name,
			&width, &height, &i, &masked)) != NULL)
		{
			// High resolution texture loaded.
			RGBData = true;
			alphaChannel = (i == 4);
		}
		else
		{
			width = tex->width;
			height = tex->height;
			buffer = M_Malloc(2 * width * height);
			masked = GL_BufferTexture(tex, buffer, width, height, &i);
			
			// The -bigmtex option allows the engine to resize masked 
			// textures whose patches are too big to fit the texture.
			if(allowMaskedTexEnlarge && masked && i)
			{
				// Adjust height to fit the largest patch.
				tex->height = height = i; 
				// Get a new buffer.
				M_Free(buffer);
				buffer = M_Malloc(2 * width * height);
				masked = GL_BufferTexture(tex, buffer, width, height, 0);
			}
			// "alphaChannel" and "masked" are the same thing for indexed
			// images.
			alphaChannel = masked;
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
		
		textures[idx]->tex = GL_UploadTexture(buffer, width, height, 
			alphaChannel, true, RGBData, false);

		// Set texture parameters.
		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);

		textures[idx]->masked = (masked != 0);

		if(alphaChannel)
		{
			// Don't tile masked textures vertically.
			//---DEBUG---
			//gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		}
		M_Free(buffer);
	}
	texw = textures[idx]->width;
	texh = textures[idx]->height;
	texmask = textures[idx]->masked;
	texdetail = textures[idx]->detail.tex? &textures[idx]->detail : 0;
	return textures[idx]->tex;
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
						if(!a && !b || i+a < 0 || k+b < 0 
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
	int		i, width, height;
	byte	*imgdata;
	boolean	RGBData, masked, alphaChannel;

	if(idx > numtextures-1) return 0;

#if _DEBUG
	if(idx != texturetranslation[idx])
		Con_Error("Skytex: %d, translated: %d\n", idx, texturetranslation[idx]);
#endif
	
	idx = texturetranslation[idx];
	
	if(!textures[idx]->tex)
	{
		// Try to load a high resolution version of this texture.
		if((imgdata = GL_LoadHighResTexture(textures[idx]->name,
			&width, &height, &i, &masked)) != NULL)
		{
			// High resolution texture loaded.
			RGBData = true;
			alphaChannel = (i == 4);
		}
		else
		{
			RGBData = false;
			GL_BufferSkyTexture(idx, &imgdata, &width, &height, zeroMask);
			alphaChannel = masked = zeroMask;
		}

		// Upload it.
		textures[idx]->tex = GL_UploadTexture(imgdata, width, height, alphaChannel, 
			true, RGBData, false);
		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);

		// Free the buffer.
		M_Free(imgdata);

		// Do we have a masked texture?
		textures[idx]->masked = (masked != 0);
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
//==========================================================================
void GL_CalcLuminance(int pnum, byte *buffer, int width, int height)
{
	byte			*palette = W_CacheLumpNum(pallump, PU_CACHE);
	spritelump_t	*slump = spritelumps + pnum;
	int				i, k, c, cnt = 0, poscnt = 0;
	byte			rgb[3], *src, *alphasrc;
	int				limit = 0xc0, poslimit = 0xe0, collimit = 0xc0;
	int				average[3], avcnt = 0, lowavg[3], lowcnt = 0;
	rgbcol_t		*sprcol = &slump->color;

	slump->flarex = slump->flarey = 0;
	memset(average, 0, sizeof(average));
	memset(lowavg, 0, sizeof(lowavg));
	src = buffer;
	alphasrc = buffer + width*height;

	for(k = 0; k < height; k++)
		for(i = 0; i < width; i++, src++, alphasrc++)
		{
			if(*alphasrc < 255) continue;
			// Bright enough?
			memcpy(rgb, palette + (*src * 3), 3);
			if(rgb[0] > poslimit || rgb[1] > poslimit || rgb[2] > poslimit)
			{
				// Include in the count.
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
				for(c=0; c<3; c++) average[c] += rgb[c];
			}
			else
			{
				lowcnt++;
				for(c=0; c<3; c++) lowavg[c] += rgb[c];
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

//===========================================================================
// GL_PrepareTranslatedSprite
//===========================================================================
unsigned int GL_PrepareTranslatedSprite(int pnum, unsigned char *table)
{
	transspr_t *tspr = GL_GetTranslatedSprite(pnum, table);

	if(!tspr)
	{
		// There's no name for this patch, load it in.
		patch_t		*patch = W_CacheLumpNum(spritelumps[pnum].lump, PU_CACHE);
		int			bufsize = 2 * patch->width * patch->height;
		byte		*palette = W_CacheLumpNum(pallump, PU_CACHE);
		byte		*buffer = M_Calloc(bufsize);
		
		DrawRealPatch(buffer, W_CacheLumpNum(pallump, PU_CACHE), 
			patch->width, patch->height, patch, 0, 0, false, table, false);

		// Calculate light source properties.
		GL_CalcLuminance(pnum, buffer, patch->width, patch->height);

		if(filloutlines) ColorOutlines(buffer, patch->width, patch->height);

		tspr = GL_NewTranslatedSprite(pnum, table);
		tspr->tex = GL_UploadTexture(buffer, patch->width, patch->height, true, true, false, true);
		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, filterSprites? DGL_LINEAR : DGL_NEAREST);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

		M_Free(buffer);

		// Determine coordinates for the texture.
		GL_SetTexCoords(spritelumps[pnum].tc, patch->width, patch->height);
	}
	return tspr->tex;
}

//===========================================================================
// GL_PrepareSprite
//===========================================================================
unsigned int GL_PrepareSprite(int pnum)
{
	if(pnum < 0) return 0;

	if(!spritelumps[pnum].tex)
	{
		// There's no name for this patch, load it in.
		byte	*palette = W_CacheLumpNum(pallump, PU_CACHE);
		patch_t *patch = W_CacheLumpNum(spritelumps[pnum].lump, PU_CACHE);
		int		bufsize = 2 * patch->width * patch->height;
		byte	*buffer = M_Calloc(bufsize);

#if _DEBUG
		if(patch->width > 512 || patch->height > 512) 
		{
			Con_Error("GL_PrepareSprite: Bad patch (too big?!)\n  Assumed lump: %8s",
				lumpinfo[spritelumps[pnum].lump].name);
		}
#endif

		DrawRealPatch(buffer, W_CacheLumpNum(pallump, PU_CACHE), 
			patch->width, patch->height, patch, 0, 0, false, 0, false);

		// Calculate light source properties.
		GL_CalcLuminance(pnum, buffer, patch->width, patch->height);
		
		if(filloutlines) ColorOutlines(buffer, patch->width, patch->height);

		spritelumps[pnum].tex = GL_UploadTexture(buffer, patch->width, patch->height,
			true, true, false, true);
		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, filterSprites? DGL_LINEAR : DGL_NEAREST);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

		M_Free(buffer);

		// Determine coordinates for the texture.
		GL_SetTexCoords(spritelumps[pnum].tc, patch->width, patch->height);

		/*CON_Printf("sp%i: x=%f y=%f\n", pnum, spritelumps[pnum].tc[VX],
			spritelumps[pnum].tc[VY]);*/
	}
	return spritelumps[pnum].tex;
}

//===========================================================================
// GL_DeleteSprite
//===========================================================================
void GL_DeleteSprite(int spritelump)
{
	if(spritelump < 0 || spritelump >= numspritelumps) return;

	gl.DeleteTextures(1, &spritelumps[spritelump].tex);
	spritelumps[spritelump].tex = 0;
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
//===========================================================================
void GL_SetSprite(int pnum)
{
	GL_BindTexture(GL_PrepareSprite(pnum));
}

//===========================================================================
// GL_SetTranslatedSprite
//===========================================================================
void GL_SetTranslatedSprite(int pnum, unsigned char *trans)
{
	GL_BindTexture(GL_PrepareTranslatedSprite(pnum, trans));
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

//===========================================================================
// GL_SetRawImage
//	Raw images are always 320x200.
//	Part is either 1 or 2. Part 0 means only the left side is loaded.
//	No splittex is created in that case. Once a raw image is loaded
//	as part 0 it must be deleted before the other part is loaded at the
//	next loading. Part can also contain the width and height of the 
//	texture. 
//===========================================================================
void GL_SetRawImage(int lump, int part)
{
	int		i, k, c, idx;
	byte	*dat1, *dat2, *palette, *image, *lumpdata;
	int		height, assumedWidth = 320;
	boolean need_free_image = true;
	boolean rgbdata;
	int		comps;
	
	// Check the part.
	if(part < 0 || part > 2 || lump > numlumps-1) return; 

	if(!lumptexinfo[lump].tex[0])
	{
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
		lumptexinfo[lump].tex[0] = GL_UploadTexture(dat1, 256, 
			assumedWidth < 320? height : 256, false, false, rgbdata, false);
		gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
		gl.TexParameter(DGL_MAG_FILTER, linearRaw? DGL_LINEAR : DGL_NEAREST);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
		
		if(part)
		{
			// And the other part.
			lumptexinfo[lump].tex[1] = GL_UploadTexture(dat2, 64, 256, 
				false, false, rgbdata, false);
			gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
			gl.TexParameter(DGL_MAG_FILTER, linearRaw? DGL_LINEAR : DGL_NEAREST);
			gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
			gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

			// Add it to the list.
			GL_NewRawLump(lump);
		}

		lumptexinfo[lump].width[0] = 256;
		lumptexinfo[lump].width[1] = 64;
		lumptexinfo[lump].height = height;

dont_load:
		if(need_free_image) M_Free(image);
		M_Free(dat1);
		M_Free(dat2);
		W_ChangeCacheTag(lump, PU_CACHE);
	}
	// Bind the part that was asked for.
	if(part <= 1) gl.Bind(lumptexinfo[lump].tex[0]);
	if(part == 2) gl.Bind(lumptexinfo[lump].tex[1]);
	// We don't track the current texture with raw images.
	curtex = 0;
}

//===========================================================================
// GL_SetPatch
//	No mipmaps are generated for regular patches.
//===========================================================================
void GL_SetPatch(int lump)	
{
	if(lump > numlumps-1) return;
	if(!lumptexinfo[lump].tex[0])
	{
		// Load the patch.
		patch_t	*patch = W_CacheLumpNum(lump, PU_CACHE);
		int		numpels = patch->width * patch->height;
		byte	*buffer = M_Calloc(2 * numpels);
		int		alphaChannel;
		
		if(!numpels) // This won't do!
		{
			M_Free(buffer);
			return;
		}

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
			byte *tempbuff = M_Malloc(2 * maxTexSize * patch->height);
			
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
		// The rest of the size information.
		lumptexinfo[lump].height = patch->height;
		lumptexinfo[lump].offx = -patch->leftoffset;
		lumptexinfo[lump].offy = -patch->topoffset;

		M_Free(buffer);
	}
	else
	{
		GL_BindTexture(lumptexinfo[lump].tex[0]);
	}
	curtex = lumptexinfo[lump].tex[0];
}

//===========================================================================
// GL_SetNoTexture
//	You should use Disable(DGL_TEXTURING) instead of this.
//===========================================================================
void GL_SetNoTexture(void)
{
	gl.Bind(0);
	curtex = 0;
}

//===========================================================================
// GL_PrepareLightTexture
//	The dynamic light map is a 64x64 grayscale 8-bit image.
//===========================================================================
DGLuint GL_PrepareLightTexture(void)
{
	if(!dltexname)
	{
		// We need to generate the texture, I see.
		byte *image = W_CacheLumpName("DLIGHT", PU_CACHE);
		if(!image) 
			Con_Error("GL_SetLightTexture: DLIGHT not found.\n");
		dltexname = gl.NewTexture();
		// No mipmapping or resizing is needed, upload directly.
		gl.TexImage(DGL_LUMINANCE, 64, 64, 0, image);
		gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
	}
	// Set the info.
	texw = texh = 64;
	texmask = 0;
	return dltexname;
}

//===========================================================================
// GL_PrepareGlowTexture
//===========================================================================
DGLuint GL_PrepareGlowTexture(void)
{
	if(!glowtexname)
	{
		// Generate the texture.
		byte *image = W_CacheLumpName("WDLIGHT", PU_CACHE);
		if(!image)
			Con_Error("GL_PrepareGlowTexture: no wdlight texture.\n");
		glowtexname = gl.NewTexture();
		// Upload it.
		gl.TexImage(DGL_LUMINANCE, 4, 32, 0, image);
		gl.TexParameter(DGL_MIN_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
	}
	// Set the info.
	texw = 4;
	texh = 32;
	texmask = 0;
	return glowtexname;
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
	w = h = flare==2? 128 : 64;

	if(!flaretexnames[flare])
	{
		byte *image = W_CacheLumpName(flare==0? "FLARE" : flare==1? "BRFLARE" : "BIGFLARE", PU_CACHE);
		if(!image)
			Con_Error("GL_PrepareFlareTexture: flare texture %i not found!\n", flare);

		flaretexnames[flare] = gl.NewTexture();
		gl.TexImage(DGL_LUMINANCE, w, h, 0, image);
		gl.TexParameter(DGL_MIN_FILTER, DGL_NEAREST);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);
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
		for(i = 0; i < numflats; i++)
			if(lumptexinfo[flats[i].lump].tex[0]) // Is the texture loaded?
			{
				gl.Bind(lumptexinfo[flats[i].lump].tex[0]);
				gl.TexParameter(DGL_MIN_FILTER, minMode);
				gl.TexParameter(DGL_MAG_FILTER, magMode);
			}
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
	GL_SetTextureParams(glmode[mipmode], DGL_LINEAR, true, false);
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
		Con_Message("SkinTex: %s => %i\n", skin, st - skinnames);
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
		// Load the texture. R_LoadSkin allocates enough memory with Z_Malloc.
		image = R_LoadSkin(mdl, skin, &width, &height, &size);
		if(!image)
		{
			Con_Error("GL_PrepareSkin: %s not found.\n", mdl->skins[skin].name);
		}

		st->tex = GL_UploadTexture(image, width, height, 
			size == 4, true, true, false);

		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_CLAMP);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

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
	//const char *skinp = md->def->sub[sub].shinyskin;
	int pxsize, width, height;
	boolean masked;
	byte *image;
	model_t *mdl = modellist[md->sub[sub].model];
	skintex_t *stp = GL_GetSkinTexByIndex(md->sub[sub].shinyskin);

	if(!stp) return 0;	// Does not have a shiny skin.
	if(!stp->tex)
	{
		// Load in the texture. 
		image = GL_LoadImageCK(stp->path, &width, &height, &pxsize, &masked, true);

		stp->tex = GL_UploadTexture(image, width, height, pxsize == 4, 
			true, true, false);

		gl.TexParameter(DGL_MIN_FILTER, glmode[mipmapping]);
		gl.TexParameter(DGL_MAG_FILTER, DGL_LINEAR);
		gl.TexParameter(DGL_WRAP_S, DGL_REPEAT);
		gl.TexParameter(DGL_WRAP_T, DGL_CLAMP);

		// We don't need the image data any more.
		M_Free(image);
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
	GL_LoadSystemTextures();
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
