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
 * state.cpp: Controlling of the Direct3D Rendering State
 */

// HEADER FILES ------------------------------------------------------------

#include "drD3D.h"

// MACROS ------------------------------------------------------------------

#define DISABLE_STAGE(S) \
	SetTSS(S, D3DTSS_COLOROP, D3DTOP_DISABLE); \
	SetTSS(S, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern HWND		hwnd;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// InitState
//===========================================================================
void InitState(void)
{
	DWORD dw;
	int i;
	float f;

	// Default alpha blending.
	SetRS(D3DRS_ALPHABLENDENABLE, TRUE);
	SetRS(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	SetRS(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	// Enable alpha test.
	SetRS(D3DRS_ALPHATESTENABLE, TRUE);
	SetRS(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
	SetRS(D3DRS_ALPHAREF, 1);

	// Setup fog.
	SetRS(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	SetRS(D3DRS_FOGSTART, 0);
	f = 2100;
	SetRS(D3DRS_FOGEND, *(DWORD*)&f);
	SetRS(D3DRS_FOGCOLOR, 0x8a8a8a);

	// Should we allow dithering?
	if(!ArgExists("-nodither")) SetRS(D3DRS_DITHERENABLE, TRUE);
	
	SetRS(D3DRS_LIGHTING, FALSE);
	SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
	SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	SetTSS(0, D3DTSS_ALPHAARG2, D3DTA_TEXTURE);
	SetTSS(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

	SetTSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	SetTSS(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
	SetTSS(1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);

	// Default state diagnose.
#define DS(cn)	dev->GetRenderState(cn, &dw); DP(#cn" = 0x%x", dw);
#define DSf(cn)	dev->GetRenderState(cn, &dw); DP(#cn" = %g", *(float*)&dw);

	DS(D3DRS_ZENABLE);
	DS(D3DRS_FILLMODE);
	DS(D3DRS_SHADEMODE);
	DS(D3DRS_LINEPATTERN);
	DS(D3DRS_ZWRITEENABLE);
	DS(D3DRS_ALPHATESTENABLE);
	DS(D3DRS_LASTPIXEL);
	DS(D3DRS_SRCBLEND);
	DS(D3DRS_DESTBLEND);
	DS(D3DRS_BLENDOP);
	DS(D3DRS_CULLMODE);
	DS(D3DRS_ZFUNC);
	DS(D3DRS_ALPHAREF);
	DS(D3DRS_ALPHAFUNC);
	DS(D3DRS_DITHERENABLE);
	DS(D3DRS_ALPHABLENDENABLE);
	DS(D3DRS_FOGENABLE);
	DS(D3DRS_SPECULARENABLE);
	DS(D3DRS_ZVISIBLE);
	DS(D3DRS_FOGCOLOR);
	DS(D3DRS_FOGTABLEMODE);
	DSf(D3DRS_FOGSTART);
	DSf(D3DRS_FOGEND);
	DSf(D3DRS_FOGDENSITY);
	DS(D3DRS_EDGEANTIALIAS);
	DS(D3DRS_ZBIAS);
	DS(D3DRS_RANGEFOGENABLE);
	DS(D3DRS_STENCILENABLE);
	DS(D3DRS_STENCILFAIL);
	DS(D3DRS_STENCILZFAIL);
	DS(D3DRS_STENCILPASS);
	DS(D3DRS_STENCILFUNC);
	DS(D3DRS_STENCILREF);
	DS(D3DRS_STENCILMASK);
	DS(D3DRS_STENCILWRITEMASK);
	DS(D3DRS_TEXTUREFACTOR);
	DS(D3DRS_WRAP0);
	DS(D3DRS_WRAP1);
	DS(D3DRS_WRAP2);
	DS(D3DRS_WRAP3);
	DS(D3DRS_WRAP4);
	DS(D3DRS_WRAP5);
	DS(D3DRS_WRAP6);
	DS(D3DRS_WRAP7);
	DS(D3DRS_CLIPPING);
	DS(D3DRS_LIGHTING);
	DS(D3DRS_AMBIENT);
	DS(D3DRS_FOGVERTEXMODE);
	DS(D3DRS_COLORVERTEX);
	DS(D3DRS_LOCALVIEWER);
	DS(D3DRS_NORMALIZENORMALS);
	DS(D3DRS_DIFFUSEMATERIALSOURCE);
	DS(D3DRS_SPECULARMATERIALSOURCE);
	DS(D3DRS_AMBIENTMATERIALSOURCE);
	DS(D3DRS_EMISSIVEMATERIALSOURCE);
	DS(D3DRS_VERTEXBLEND);
	DS(D3DRS_CLIPPLANEENABLE);
	DS(D3DRS_SOFTWAREVERTEXPROCESSING);
	DSf(D3DRS_POINTSIZE);
	DSf(D3DRS_POINTSIZE_MIN);
	DSf(D3DRS_POINTSIZE_MAX);
	DS(D3DRS_POINTSPRITEENABLE);
	DS(D3DRS_POINTSCALEENABLE);
	DSf(D3DRS_POINTSCALE_A);
	DSf(D3DRS_POINTSCALE_B);
	DSf(D3DRS_POINTSCALE_C);
	DS(D3DRS_MULTISAMPLEANTIALIAS);
	DS(D3DRS_MULTISAMPLEMASK);
	DS(D3DRS_PATCHEDGESTYLE);
	DSf(D3DRS_PATCHSEGMENTS);
	DS(D3DRS_DEBUGMONITORTOKEN);
	DS(D3DRS_INDEXEDVERTEXBLENDENABLE);
	DS(D3DRS_COLORWRITEENABLE);
	DSf(D3DRS_TWEENFACTOR);
	DS(D3DRS_POSITIONORDER);
	DS(D3DRS_NORMALORDER);

	for(i = 0; (unsigned int) i < caps.MaxTextureBlendStages; i++)
	{
#undef DS
#define DS(cn)	dev->GetTextureStageState(i, cn, &dw); DP("  " #cn " = 0x%x", dw)
		DP("Texture blending stage %i:", i);
		DS(D3DTSS_COLOROP);
		DS(D3DTSS_COLORARG1);
		DS(D3DTSS_COLORARG2);
		DS(D3DTSS_ALPHAOP);
		DS(D3DTSS_ALPHAARG1);
		DS(D3DTSS_ALPHAARG2);
		DS(D3DTSS_BUMPENVMAT00);
		DS(D3DTSS_BUMPENVMAT01);
		DS(D3DTSS_BUMPENVMAT10);
		DS(D3DTSS_BUMPENVMAT11);
		DS(D3DTSS_TEXCOORDINDEX);
		DS(D3DTSS_ADDRESSU);
		DS(D3DTSS_ADDRESSV);
		DS(D3DTSS_BORDERCOLOR);
		DS(D3DTSS_MAGFILTER);
		DS(D3DTSS_MINFILTER);
		DS(D3DTSS_MIPFILTER);
		DS(D3DTSS_MIPMAPLODBIAS);
		DS(D3DTSS_MAXMIPLEVEL);
		DS(D3DTSS_MAXANISOTROPY);
		DS(D3DTSS_BUMPENVLSCALE);
		DS(D3DTSS_BUMPENVLOFFSET);
		DS(D3DTSS_TEXTURETRANSFORMFLAGS);
		DS(D3DTSS_ADDRESSW);
		DS(D3DTSS_COLORARG0);
		DS(D3DTSS_ALPHAARG0);
		DS(D3DTSS_RESULTARG);
	}
}

//===========================================================================
// DG_Enable
//===========================================================================
int DG_Enable(int cap)
{
	switch(cap)
	{
	case DGL_TEXTURE0:
	case DGL_TEXTURE1:
		// Enable a texture unit.
		ActiveTexture(cap - DGL_TEXTURE0);
		TextureOperatingMode(DGL_TRUE);
		break;

	case DGL_TEXTURING:
		TextureOperatingMode(DGL_TRUE);
		break;

	case DGL_BLENDING:
		SetRS(D3DRS_ALPHABLENDENABLE, TRUE);
		break;

	case DGL_DEPTH_TEST:
		SetRS(D3DRS_ZENABLE, D3DZB_TRUE);
		break;

	case DGL_ALPHA_TEST:
		SetRS(D3DRS_ALPHATESTENABLE, TRUE);
		break;

	case DGL_CULL_FACE:
		SetRS(D3DRS_CULLMODE, D3DCULL_CCW);
		break;

	case DGL_FOG:
		SetRS(D3DRS_FOGENABLE, TRUE);
		break;

	case DGL_SCISSOR_TEST:
		EnableScissor(true);
		break;

	case DGL_COLOR_WRITE:
		break;

	case DGL_DEPTH_WRITE:
		SetRS(D3DRS_ZWRITEENABLE, TRUE);
		break;

	case DGL_PALETTED_TEXTURES:
		break;

	case DGL_TEXTURE_COMPRESSION:
		// Sure, whatever...
		break;

	default:
		// What was that all about?
		Con_Error("DG_Enable: Unknown cap=0x%x\n", cap);
		return DGL_FALSE;
	}
	// Enabling was successful.
	return DGL_TRUE;
}

//===========================================================================
// DG_Disable
//===========================================================================
void DG_Disable(int cap)
{
	switch(cap)
	{
	case DGL_TEXTURE0:
	case DGL_TEXTURE1:
		// Disable the texture unit.
		ActiveTexture(cap - DGL_TEXTURE0);
		TextureOperatingMode(DGL_FALSE);

		// Implicit disabling of texcoord array.
		DG_DisableArrays(0, 0, 1 << (cap - DGL_TEXTURE0));
		break;
	
	case DGL_TEXTURING:
		TextureOperatingMode(DGL_FALSE);
		break;

	case DGL_BLENDING:
		SetRS(D3DRS_ALPHABLENDENABLE, FALSE);
		break;

	case DGL_DEPTH_TEST:
		SetRS(D3DRS_ZENABLE, D3DZB_FALSE);
		break;

	case DGL_ALPHA_TEST:
		SetRS(D3DRS_ALPHATESTENABLE, FALSE);
		break;

	case DGL_CULL_FACE:
		SetRS(D3DRS_CULLMODE, D3DCULL_NONE);
		break;

	case DGL_FOG:
		SetRS(D3DRS_FOGENABLE, FALSE);
		break;

	case DGL_SCISSOR_TEST:
		EnableScissor(false);
		break;

	case DGL_COLOR_WRITE:
		break;

	case DGL_DEPTH_WRITE:
		SetRS(D3DRS_ZWRITEENABLE, FALSE);
		break;

	case DGL_PALETTED_TEXTURES:
		break;

	case DGL_TEXTURE_COMPRESSION:
		// Sure, whatever...
		break;

	default:
		Con_Error("DG_Disable: Unknown cap=0x%x\n", cap);
		break;
	}
}

//===========================================================================
// DG_GetInteger
//===========================================================================
int	DG_GetInteger(int name)
{
	int values[10];

	DG_GetIntegerv(name, values);
	return values[0];
}

//===========================================================================
// DG_GetIntegerv
//===========================================================================
int	DG_GetIntegerv(int name, int *v)
{
	DWORD dw;

	switch(name)
	{
	case DGL_VERSION:
		*v = DGL_VERSION_NUM;
		break;

	case DGL_MAX_TEXTURE_SIZE:
		*v = maxTexSize;
		break;

	case DGL_MAX_TEXTURE_UNITS:
		*v = maxTextures;
		break;

	case DGL_MODULATE_ADD_COMBINE:
		*v = availMulAdd;
		break;

	case DGL_PALETTED_TEXTURES:
		*v = DGL_TRUE;
		break;

	case DGL_PALETTED_GENMIPS:
		// We are able to generate mipmaps for paletted textures.
		*v = DGL_TRUE;
		break;

	case DGL_POLY_COUNT:
		*v = 0; /*triCounter;*/
		//triCounter = 0;
		break;

	case DGL_SCISSOR_TEST:
		*v = scissorActive;
		break;

	case DGL_SCISSOR_BOX:
		v[0] = scissor.x;
		v[1] = scissor.y;
		v[2] = scissor.width;
		v[3] = scissor.height;
		break;

	case DGL_FOG:
		dev->GetRenderState(D3DRS_FOGENABLE, &dw);
		*v = (dw != 0);
		break;

	case DGL_R:
		*v = (currentVertex.color >> 16) & 0xff;
		break;

	case DGL_G:
		*v = (currentVertex.color >> 8) & 0xff;
		break;

	case DGL_B:
		*v = currentVertex.color & 0xff;
		break;

	case DGL_A:
		*v = (currentVertex.color >> 24) & 0xff;
		break;

	case DGL_RGBA:
		*v++ = (currentVertex.color >> 16) & 0xff;
		*v++ = (currentVertex.color >> 8) & 0xff;
		*v++ = currentVertex.color & 0xff;
		*v++ = (currentVertex.color >> 24) & 0xff;
		break;

	default:
		Con_Error("DG_GetIntegerv: Unknown name=0x%x\n", name);
		return DGL_ERROR;
	}
	return DGL_OK;
}

//===========================================================================
// DG_SetInteger
//===========================================================================
int	DG_SetInteger(int name, int value)
{
	switch(name)
	{
	case DGL_WINDOW_HANDLE:
		hwnd = (HWND) value;
		break;

	case DGL_ACTIVE_TEXTURE:
		// Change the currently active texture unit.
		ActiveTexture(value);
		break;

	case DGL_GRAY_MIPMAP:
		// Set gray mipmap contrast.
		grayMipmapFactor = value/255.0f;
		break;

	case DGL_ENV_ALPHA:
		// Set constant alpha color.
		SetRS(D3DRS_TEXTUREFACTOR, 
			D3DCOLOR_ARGB( MAX_OF(0, MIN_OF(value, 255)), 0, 0, 0 ));
		break;

	case DGL_MODULATE_TEXTURE:
		StageIdentity();
		ActiveTexture(0);
		if(value == 0)
		{
			// No modulation: just replace with texture.
			SetTSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

			DISABLE_STAGE(1);
		}
		else if(value == 1)
		{
			// Normal texture modulation with primary color.
			SetTSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			SetTSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			SetTSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

			DISABLE_STAGE(1);
		}
		else if(value == 2 || value == 3)
		{	
			// Texture modulation and interpolation.
			SetTSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_CURRENT);
			
			SetTSS(1, D3DTSS_COLOROP, D3DTOP_BLENDFACTORALPHA);
			SetTSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			SetTSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
			SetTSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);

			if(value == 2) 
			{
				// Modulate with diffuse color.
				SetTSS(2, D3DTSS_COLOROP, D3DTOP_MODULATE);
				SetTSS(2, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
				SetTSS(2, D3DTSS_COLORARG2, D3DTA_CURRENT);
				SetTSS(2, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
				SetTSS(2, D3DTSS_ALPHAARG1, D3DTA_CURRENT);

				DISABLE_STAGE(3);
			}
			else 
			{
				// Don't modulate with diffuse.
				DISABLE_STAGE(2);
			}
		}
		else if(value == 4)
		{
			// Apply sector light, dynamic light and texture.
			// texAlpha * constRGB + 1 * prevRGB
			SetTSS(0, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
			SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);
			SetTSS(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
			SetTSS(0, D3DTSS_COLORARG0, D3DTA_DIFFUSE);

			SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_CURRENT);

			SetTSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
			SetTSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			SetTSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);

			SetTSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

			DISABLE_STAGE(2);
		}
		else if(value == 5 || value == 10)
		{
			// Sector light * texture + dynamic light.
			SetTSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			SetTSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

			SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

			SetTSS(1, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
			SetTSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE 
				| (value == 5? D3DTA_ALPHAREPLICATE : 0));
			SetTSS(1, D3DTSS_COLORARG2, D3DTA_TFACTOR);
			SetTSS(1, D3DTSS_COLORARG0, D3DTA_CURRENT);

			SetTSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);

			DISABLE_STAGE(2);
		}
		else if(value == 6)
		{
			// Simple dynlight addition (add to primary color).
			SetTSS(0, D3DTSS_COLOROP, D3DTOP_MULTIPLYADD);
			SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);
			SetTSS(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
			SetTSS(0, D3DTSS_COLORARG0, D3DTA_DIFFUSE);

			SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_CURRENT);

			DISABLE_STAGE(1);
		}
		else if(value == 7)
		{
			// Dynlight addition without primary color.
			SetTSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
			SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE);
			SetTSS(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);

			SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_CURRENT);

			DISABLE_STAGE(1);
		}
		else if(value == 8 || value == 9)
		{
			// Texture and Detail.
			if(value == 8)
			{
				SetTSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
				SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
				SetTSS(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
			}
			else 
			{
				// Ignore diffuse color.
				SetTSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
				SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			}
			SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
			SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			SetTSS(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
			
			SetTSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE2X);
			SetTSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			SetTSS(1, D3DTSS_COLORARG2, D3DTA_CURRENT);
			SetTSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);

			DISABLE_STAGE(2);
		}
		else if(value == 11)
		{
			// Tex0: texture
			// Tex1: shiny texture

			SetTSS(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			SetTSS(1, D3DTSS_COLOROP, D3DTOP_MODULATE);
			SetTSS(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
			SetTSS(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);

			SetTSS(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
			SetTSS(1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
			SetTSS(1, D3DTSS_ALPHAARG1, D3DTA_CURRENT);

			DISABLE_STAGE(2);
		}
		break;

	case DGL_CULL_FACE:
		SetRS(D3DRS_CULLMODE, value == DGL_CCW? D3DCULL_CCW : D3DCULL_CW);
		break;			

	default:
		Con_Error("DG_SetInteger: Unknown name=0x%x\n", name);
		return DGL_ERROR;
	}
	return DGL_OK;
}

//===========================================================================
// DG_GetString
//===========================================================================
char* DG_GetString(int name)
{
	switch(name)
	{
	case DGL_VERSION:
		return DRD3D_VERSION_FULL;

	default:
		Con_Error("DG_GetString: Unknown name=0x%x\n", name);
	}
	return NULL;
}

//===========================================================================
// DG_SetFloatv
//===========================================================================
int DG_SetFloatv(int name, float *values)
{
	float color[4];
	int i;

	switch(name)
	{
	case DGL_ENV_COLOR:
		for(i = 0; i < 4; i++) 
		{
			color[i] = values[i];
			CLAMP01(color[i]);
		}
		SetRS(D3DRS_TEXTUREFACTOR, 
			D3DCOLOR_COLORVALUE(color[0], color[1], color[2], color[3]));
		break;

	default:
		Con_Error("DG_SetFloatv: Unknown name=0x%x\n", name);
		return DGL_ERROR;
	}
	return DGL_OK;
}

//===========================================================================
// DG_Func
//===========================================================================
void DG_Func(int func, int param1, int param2)
{
	D3DBLEND d3dBlendMode[] =
	{
		D3DBLEND_ZERO,
		D3DBLEND_ONE,
		D3DBLEND_DESTCOLOR,
		D3DBLEND_INVDESTCOLOR,
		D3DBLEND_DESTALPHA,
		D3DBLEND_INVDESTALPHA,
		D3DBLEND_SRCCOLOR,
		D3DBLEND_INVSRCCOLOR,
		D3DBLEND_SRCALPHA,
		D3DBLEND_INVSRCALPHA,
		D3DBLEND_SRCALPHASAT
	};
	D3DCMPFUNC d3dCmpFunc[] =
	{
		D3DCMP_ALWAYS,
		D3DCMP_NEVER,
		D3DCMP_LESS,
		D3DCMP_EQUAL,
		D3DCMP_LESSEQUAL,
		D3DCMP_GREATER,
		D3DCMP_GREATEREQUAL,
		D3DCMP_NOTEQUAL
	};

	switch(func)
	{
	case DGL_BLENDING_OP:
		SetRS(D3DRS_BLENDOP, 
			  param1 == DGL_ADD? D3DBLENDOP_ADD
			: param1 == DGL_SUBTRACT? D3DBLENDOP_SUBTRACT
			: param1 == DGL_REVERSE_SUBTRACT? D3DBLENDOP_REVSUBTRACT
			: D3DBLENDOP_ADD);
		break;

	case DGL_BLENDING:
		if(param1 >= DGL_ZERO && param1 <= DGL_SRC_ALPHA_SATURATE &&
			param2 >= DGL_ZERO && param2 <= DGL_SRC_ALPHA_SATURATE)
		{
			SetRS(D3DRS_SRCBLEND, d3dBlendMode[param1 - DGL_ZERO]);
			SetRS(D3DRS_DESTBLEND, d3dBlendMode[param2 - DGL_ZERO]);
		}
		break;

	case DGL_DEPTH_TEST:
		if(param1 >= DGL_ALWAYS && param1 <= DGL_NOTEQUAL)
			SetRS(D3DRS_ZFUNC, d3dCmpFunc[param1 - DGL_ALWAYS]);
		break;

	case DGL_ALPHA_TEST:
		if(param1 >= DGL_ALWAYS && param1 <= DGL_NOTEQUAL)
		{
			SetRS(D3DRS_ALPHAFUNC, d3dCmpFunc[param1 - DGL_ALWAYS]);
			SetRS(D3DRS_ALPHAREF, param2);
		}
		break;

	default:
		Con_Error("DG_Func: Unknown func=0x%x\n", func);
	}
}

//===========================================================================
// DG_Fog
//===========================================================================
void DG_Fog(int pname, float param)
{
	int	iparam = (int) param;

	switch(pname)
	{
	case DGL_FOG_MODE:
		SetRS(D3DRS_FOGTABLEMODE, 
			param == DGL_LINEAR? D3DFOG_LINEAR 
			: param == DGL_EXP? D3DFOG_EXP : D3DFOG_EXP2);
		break;

	case DGL_FOG_DENSITY:
		SetRS(D3DRS_FOGDENSITY, *(DWORD*) &param);
		break;

	case DGL_FOG_START:
		SetRS(D3DRS_FOGSTART, *(DWORD*) &param);
		break;

	case DGL_FOG_END:
		SetRS(D3DRS_FOGEND, *(DWORD*) &param);
		break;

	case DGL_FOG_COLOR:
		if(iparam >= 0 && iparam < 256)
		{
			byte *col = GetPaletteColor(iparam);
			SetRS(D3DRS_FOGCOLOR, D3DCOLOR_XRGB(col[CR], col[CG], col[CB]));
		}
		break;

	default:
		Con_Error("DG_Fog: Unknown pname=0x%x\n", pname);
	}
}

//===========================================================================
// DG_Fogv
//===========================================================================
void DG_Fogv(int pname, void *data)
{
	float	param = *(float*) data;
	byte	*ubvparam = (byte*) data;

	switch(pname)
	{
	case DGL_FOG_COLOR:
		SetRS(D3DRS_FOGCOLOR, D3DCOLOR_RGBA(ubvparam[CR], ubvparam[CG], 
			ubvparam[CB], ubvparam[CA]));
		break;

	default:
		DG_Fog(pname, param);
		break;
	}
}
