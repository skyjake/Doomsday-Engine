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
 * texture.c: Texture Handling
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/ 
 */

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>

#include "drOpenGL.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

rgba_t			palette[256];
int				usePalTex;
int				dumpTextures;
int				useCompr;
float			grayMipmapFactor = 1;

#ifdef WIN32
// The color table extension.
PFNGLCOLORTABLEEXTPROC glColorTableEXT = NULL;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// chooseFormat
//	Return the internal texture format. 
//	The compression method is chosen here.
//===========================================================================
GLenum ChooseFormat(int comps)
{
	boolean compress = (useCompr && allowCompression);

	switch(comps)
	{
	case 1:	// Luminance
		return compress? GL_COMPRESSED_LUMINANCE : GL_LUMINANCE;

	case 3: // RGB
		return !compress? 3
			: extS3TC? GL_COMPRESSED_RGB_S3TC_DXT1_EXT
			: GL_COMPRESSED_RGB;

	case 4: // RGBA
		return !compress? 4 
			: extS3TC? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT // >1-bit alpha
			: GL_COMPRESSED_RGBA;
	}
	
	// The fallback.
	return comps;
}

//===========================================================================
// loadPalette
//===========================================================================
void loadPalette(int sharedPalette)
{
	byte	paldata[256*3];
	int		i;

	if(usePalTex == DGL_FALSE) return;

	// Prepare the color table (RGBA -> RGB).
	for(i = 0; i < 256; i++)
		memcpy(paldata + i*3, palette[i].color, 3);

	glColorTableEXT(sharedPalette? GL_SHARED_TEXTURE_PALETTE_EXT
		: GL_TEXTURE_2D, GL_RGB, 256, GL_RGB, GL_UNSIGNED_BYTE, paldata);
}

//===========================================================================
// enablePalTexExt
//===========================================================================
int enablePalTexExt(int enable)
{
	if(!palExtAvailable && !sharedPalExtAvailable) 
	{
		Con_Message("drOpenGL.enablePalTexExt: No paletted texture support.\n");
		return DGL_FALSE;
	}
	if((enable && usePalTex) || (!enable && !usePalTex)) 
		return DGL_TRUE;

	if(!enable && usePalTex)
	{
		usePalTex = DGL_FALSE;
		if(sharedPalExtAvailable)
			glDisable(GL_SHARED_TEXTURE_PALETTE_EXT);
#ifdef WIN32
		glColorTableEXT = NULL;
#endif
		return DGL_TRUE;
	}	

	usePalTex = DGL_FALSE;

#ifdef WIN32
	if((glColorTableEXT = (PFNGLCOLORTABLEEXTPROC) wglGetProcAddress
		("glColorTableEXT")) == NULL)
	{
		Con_Message("drOpenGL.enablePalTexExt: getProcAddress failed.\n");
		return DGL_FALSE;
	}
#endif

	usePalTex = DGL_TRUE;
	if(sharedPalExtAvailable)
	{
		Con_Message("drOpenGL.enablePalTexExt: Using shared tex palette.\n");
		glEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
		loadPalette(true);
	}
	else
	{
		Con_Message("drOpenGL.enablePalTexExt: Using tex palette.\n");
		// Palette will be loaded separately for each texture.
	}
	return DGL_TRUE;
}	

//===========================================================================
// Power2
//===========================================================================
int Power2(int num)
{
	int cumul;
	for(cumul = 1; num > cumul; cumul <<= 1);
	return cumul;
}

//===========================================================================
// DG_NewTexture
//	Create a new texture.
//===========================================================================
DGLuint DG_NewTexture(void)
{
	DGLuint texName;

	// Generate a new texture name and bind it.
	glGenTextures(1, &texName);
	glBindTexture(GL_TEXTURE_2D, texName);
	return texName;
}

//===========================================================================
// setTexAniso
//===========================================================================
void setTexAniso(void)
{
	// Should anisotropic filtering be used?
	if(useAnisotropic)
	{
		// Go with the maximum!
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 
			maxAniso);
	}
}

//===========================================================================
// downMip8
//	Works within the given data, reducing the size of the picture to half 
//	its original. Width and height must be powers of two.
//===========================================================================
void downMip8(byte *in, byte *fadedOut, int width, int height, float fade)
{
	byte *out = in;
	int	x, y, outW = width >> 1, outH = height >> 1;
	float invFade;

	if(fade > 1) fade = 1;
	invFade = 1 - fade;

	if(width == 1 && height == 1)
	{
		// Nothing can be done.
		return;
	}

	if(!outW || !outH) // Limited, 1x2|2x1 -> 1x1 reduction?
	{
		int outDim = width > 1? outW : outH;
		for(x = 0; x < outDim; x++, in += 2)
		{
			*out = (in[0] + in[1]) >> 1;
			*fadedOut++ = (byte) (*out * invFade + 0x80 * fade);
			out++;
		}
	}
	else // Unconstrained, 2x2 -> 1x1 reduction?
	{
		for(y = 0; y < outH; y++, in += width)
			for(x = 0; x < outW; x++, in += 2)
			{
				*out = (in[0] + in[1] + in[width] + in[width + 1]) >> 2;
				*fadedOut++ = (byte) (*out * invFade + 0x80 * fade);
				out++;
			}
	}
}	

//===========================================================================
// grayMipmap
//===========================================================================
int grayMipmap(int format, int width, int height, void *data)
{
	byte *image, *in, *out, *faded;
	int i, numLevels, w, h, size = width * height, res;
	float invFactor = 1 - grayMipmapFactor;

	// Buffer used for the faded texture.
	faded = malloc(size / 4);
	image = malloc(size);

	// Initial fading.
	if(format == DGL_LUMINANCE)
	{
		for(i = 0, in = data, out = image; i < size; i++)
		{
			// Result is clamped to [0,255].
			res = (int) (*in++ * grayMipmapFactor + 0x80 * invFactor);
			if(res < 0) res = 0;
			if(res > 255) res = 255;
			*out++ = res;
		}
	}
	else if(format == DGL_RGB)
	{
		for(i = 0, in = data, out = image; i < size; i++, in += 3)
		{
			// Result is clamped to [0,255].
			res = (int) (*in * grayMipmapFactor + 0x80 * invFactor);
			if(res < 0) res = 0;
			if(res > 255) res = 255;
			*out++ = res;
		}
	}

	// How many levels will there be?
	for(numLevels = 0, w = width, h = height; w > 1 || h > 1; 
		w >>= 1, h >>= 1, numLevels++);

	// We do not want automatical mipmaps.
	if(extGenMip)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);

	// Upload the first level right away.
	glTexImage2D(GL_TEXTURE_2D, 0, ChooseFormat(1), width, height, 0, 
		GL_LUMINANCE, GL_UNSIGNED_BYTE, image);

	// Generate all mipmaps levels.
	for(i = 0, w = width, h = height; i < numLevels; i++)
	{
		downMip8(image, faded, w, h, (i * 1.75f)/numLevels);

		// Go down one level.
		if(w > 1) w >>= 1;
		if(h > 1) h >>= 1;

		glTexImage2D(GL_TEXTURE_2D, i + 1, ChooseFormat(1), w, h, 0, 
			GL_LUMINANCE, GL_UNSIGNED_BYTE, faded);
	} 

	// Do we need to free the temp buffer?
	free(faded);
	free(image);

	setTexAniso();
	return DGL_OK;
}

//===========================================================================
// DG_TexImage
//	'width' and 'height' must be powers of two.
//	Give a negative 'genMips' to set a specific mipmap level.
//===========================================================================
int DG_TexImage(int format, int width, int height, int genMips, void *data)
{
	int mipLevel = 0;
	byte *bdata = data;

	// Negative genMips values mean that the specific mipmap level is 
	// being uploaded.
	if(genMips < 0) 
	{
		mipLevel = -genMips;
		genMips = 0;
	}

	// Can't operate on the null texture.
	if(!data) return DGL_FALSE;

	// Check that the texture dimensions are valid.
	if(width != Power2(width) || height != Power2(height))
		return DGL_FALSE;

	if(width > maxTexSize || height > maxTexSize)
		return DGL_FALSE;

	// Special fade-to-gray luminance texture? (used for details)
	if(genMips == DGL_GRAY_MIPMAP)
	{
		return grayMipmap(format, width, height, data);
	}

	// Automatic mipmap generation?
	if(extGenMip && genMips)
		glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);

	// Paletted texture?
	if(usePalTex && format == DGL_COLOR_INDEX_8) 
	{
		if(genMips && !extGenMip)
		{
			// Build mipmap textures.
			gluBuild2DMipmaps(GL_TEXTURE_2D, GL_COLOR_INDEX8_EXT,
				width, height, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, data);
		}
		else
		{
			// The texture has no mipmapping.
			glTexImage2D(GL_TEXTURE_2D, mipLevel, GL_COLOR_INDEX8_EXT, 
				width, height, 0, GL_COLOR_INDEX, GL_UNSIGNED_BYTE, data);
		}
		// Load palette, too (if not shared).
		if(!sharedPalExtAvailable) loadPalette(false);
	}
	else // Use true color textures.
	{
		int alphachannel = (format == DGL_RGBA) 
			|| (format == DGL_COLOR_INDEX_8_PLUS_A8)
			|| (format == DGL_LUMINANCE_PLUS_A8);
		int i, colorComps = alphachannel? 4 : 3;
		int numPixels = width * height;
		byte *buffer;
		byte *pixel, *in;
		int needFree = DGL_FALSE;
		int loadFormat = GL_RGBA;

		// Convert to either RGB or RGBA, if necessary.
		if(format == DGL_RGBA)
		{
			buffer = data;
		}
		// A bug in Nvidia's drivers? Very small RGBA textures don't load
		// properly.
		else if(format == DGL_RGB && width > 2 && height > 2)
		{
			buffer = data;
			loadFormat = GL_RGB;
		}
		else // Needs converting. FIXME: This adds some overhead.
		{
			needFree = DGL_TRUE;
			buffer = malloc(numPixels * 4);
			if(!buffer) return DGL_FALSE;

			switch(format)
			{
			case DGL_RGB:
				for(i = 0, pixel = buffer, in = bdata; 
					i < numPixels; i++, pixel += 4)
				{
					pixel[CR] = *in++;
					pixel[CG] = *in++;
					pixel[CB] = *in++;
					pixel[CA] = 255;
				}
				break;

			case DGL_COLOR_INDEX_8:
				loadFormat = GL_RGB;
				for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 3)
					memcpy(pixel, palette[bdata[i]].color, 3);
				break;

			case DGL_COLOR_INDEX_8_PLUS_A8:
				for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 4)
				{
					memcpy(pixel, palette[bdata[i]].color, 3);
					pixel[CA] = bdata[numPixels+i];
				}
				break;

			case DGL_LUMINANCE:
				loadFormat = GL_RGB;
				for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 3)
					pixel[CR] = pixel[CG] = pixel[CB] = bdata[i];
				break;

			case DGL_LUMINANCE_PLUS_A8:
				for(i = 0, pixel = buffer; i < numPixels; i++, pixel += 4)
				{
					pixel[CR] = pixel[CG] = pixel[CB] = bdata[i];
					pixel[CA] = bdata[numPixels+i];
				}
				break;

			default:
				free(buffer);
				Con_Error("LoadTexture: Unknown format %x.\n", format);
				break;
			}
		}
		if(genMips && !extGenMip)
		{
			// Build all mipmap levels.
			gluBuild2DMipmaps(GL_TEXTURE_2D, ChooseFormat(colorComps), 
				width, height, loadFormat, GL_UNSIGNED_BYTE, buffer);
		}
		else		
		{
			// The texture has no mipmapping, just one level.
			glTexImage2D(GL_TEXTURE_2D, mipLevel, ChooseFormat(colorComps), 
				width, height, 0, loadFormat, GL_UNSIGNED_BYTE, buffer);
		}

		/*if(dumpTextures && mipmap && loadFormat == GL_RGB)
		{
			char fn[100];
			sprintf(fn, "%03ic%iw%03ih%03i_m%i.tga",
				currentTex, colorComps, width, height, mipmap);
			TGA_Save24_rgb888(fn, width, height, buffer);
		}*/

		if(needFree) free(buffer);
	}
	
	setTexAniso();
	return DGL_OK;
}

//===========================================================================
// DG_DeleteTextures
//===========================================================================
void DG_DeleteTextures(int num, DGLuint *names)
{
	if(!num || !names) return;
	glDeleteTextures(num, names);
}

//===========================================================================
// DG_TexParameter
//===========================================================================
void DG_TexParameter(int pname, int param)
{
	GLenum mlevs[] = { GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST,
		GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR,
		GL_LINEAR_MIPMAP_LINEAR };

	switch(pname)
	{
	default:
		glTexParameteri(GL_TEXTURE_2D, pname==DGL_MIN_FILTER? GL_TEXTURE_MIN_FILTER
			: pname==DGL_MAG_FILTER? GL_TEXTURE_MAG_FILTER
			: pname==DGL_WRAP_S? GL_TEXTURE_WRAP_S
			: GL_TEXTURE_WRAP_T,
		
			(param>=DGL_NEAREST && param<=DGL_LINEAR_MIPMAP_LINEAR)? mlevs[param-DGL_NEAREST]
			: param==DGL_CLAMP? GL_CLAMP
			: GL_REPEAT);
		break;
	}
}

//===========================================================================
// DG_GetTexParameterv
//===========================================================================
void DG_GetTexParameterv(int level, int pname, int *v)
{
	switch(pname)
	{
	case DGL_WIDTH:
		glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, v);
		break;

	case DGL_HEIGHT:
		glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, v);
		break;
	}
}

//===========================================================================
// DG_Palette
//===========================================================================
void DG_Palette(int format, void *data)
{
	unsigned char	*ptr = data;
	int				i, size = (format==DGL_RGBA? 4 : 3);

	for(i = 0; i < 256; i++, ptr += size)
	{
		palette[i].color[CR] = ptr[CR];
		palette[i].color[CG] = ptr[CG];
		palette[i].color[CB] = ptr[CB];
		palette[i].color[CA] = format==DGL_RGBA? ptr[CA] : 0xff;
	}
	loadPalette(sharedPalExtAvailable);
}

//===========================================================================
// DG_Bind
//===========================================================================
int	DG_Bind(DGLuint texture)
{
	glBindTexture(GL_TEXTURE_2D, texture);
#ifdef _DEBUG
	CheckError();
#endif
	return 0;
}

