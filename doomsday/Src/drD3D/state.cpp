
//**************************************************************************
//**
//** STATE.CPP
//**
//** Target:		DGL Driver for Direct3D 8.1
//** Description:	Controlling of the Direct3D rendering state
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "drD3D.h"

// MACROS ------------------------------------------------------------------

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

	for(i = 0; i < caps.MaxTextureBlendStages; i++)
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

	case DGL_DETAIL_TEXTURE_MODE:
		SetRS(D3DRS_TEXTUREFACTOR, 0xFF808080);

		// S = (Arg0) * Arg1 + (1-Arg0) * Arg2.
		SetTSS(0, D3DTSS_COLOROP, D3DTOP_LERP); // Lerp?! 
		SetTSS(0, D3DTSS_COLORARG0, D3DTA_CURRENT);
		SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		SetTSS(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
				
		// With this blending (0.5, 0.5, 0.5) means 'no change'.
		SetRS(D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR);
		SetRS(D3DRS_DESTBLEND, D3DBLEND_SRCCOLOR);
		break;

	default:
		// What was that all about?
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

	case DGL_DETAIL_TEXTURE_MODE:
		SetTSS(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		SetTSS(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		SetTSS(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
		SetRS(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		SetRS(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		break;

	default:
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

	case DGL_PALETTED_TEXTURES:
		*v = DGL_TRUE;
		break;

	case DGL_PALETTED_GENMIPS:
		// We are able to generate mipmaps for paletted textures.
		*v = DGL_TRUE;
		break;

	case DGL_POLY_COUNT:
		*v = triCounter;
		triCounter = 0;
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

	default:
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
	}
	return NULL;
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
