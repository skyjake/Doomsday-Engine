
//**************************************************************************
//**
//** COMMON.C
//**
//** Target:		DGL Driver for OpenGL
//** Description:	Portable Init/State Routines
//**
//** Get OpenGL header files from:
//** http://oss.sgi.com/projects/ogl-sample/ 
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "drOpenGL.h"

#include <stdlib.h>

// MACROS ------------------------------------------------------------------

// A helpful macro that changes the origin of the screen
// coordinate system.
#define FLIP(y)	(screenHeight - (y+1))

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

float 	nearClip, farClip;
int		useFog;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// initState
//===========================================================================
void initState(void)
{
	GLfloat fogcol[4] = { .54f, .54f, .54f, 1 };

	nearClip = 5;
	farClip = 8000;	
	polyCounter = 0;

	usePalTex = DGL_FALSE;
	dumpTextures = DGL_FALSE;
	useCompr = DGL_FALSE;

	// Here we configure the OpenGL state and set projection matrix.
	glFrontFace(GL_CW);
	glDisable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDisable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
#ifndef DRMESA
	glEnable(GL_TEXTURE_2D);
#else
	glDisable(GL_TEXTURE_2D);
#endif

	// The projection matrix.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	// Initialize the modelview matrix.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	// Also clear the texture matrix.
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	// Alpha blending is a go!
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0);

	// Default state for the white fog is off.
	useFog = 0;
	glDisable(GL_FOG);
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogi(GL_FOG_END, 2100);	// This should be tweaked a bit.
	glFogfv(GL_FOG_COLOR, fogcol);

/*	if(!noArrays)
	{
		glEnableClientState(GL_VERTEX_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
		texCoordPtrEnabled = false;
	}*/

#if DRMESA
	glDisable(GL_DITHER);
	glDisable(GL_LIGHTING);
	glDisable(GL_LINE_SMOOTH);
	glDisable(GL_POINT_SMOOTH);
	glDisable(GL_POLYGON_SMOOTH);
	glShadeModel(GL_FLAT);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
#else
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
#endif

	// Prefer good quality in texture compression.
	glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
	
/*#ifdef RENDER_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#else
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif*/
}

//===========================================================================
// envAddColoredAlpha
//	Requires a texture environment mode that can add and multiply.
//	Nvidia's and ATI's appropriate extensions are supported, other 
//	cards will not be able to utilize multitextured lights.
//===========================================================================
void envAddColoredAlpha(int activate, GLenum addFactor)
{
	if(activate)
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, 
			extNvTexEnvComb? GL_COMBINE4_NV : GL_COMBINE);
		glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
		
		// Combine: texAlpha * constRGB + 1 * prevRGB
		if(extNvTexEnvComb)
		{
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, addFactor);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_ZERO);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_ONE_MINUS_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE3_RGB_NV, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND3_RGB_NV, GL_SRC_COLOR);
		}
		else if(extAtiTexEnvComb)
		{
			// MODULATE_ADD_ATI: Arg0 * Arg2 + Arg1
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE_ADD_ATI);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
		}		
		else
		{
			// This doesn't look right.
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
		}
	}
	else
	{
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}
}

//===========================================================================
// envModMultiTex
//	Setup the texture environment for single-pass multiplicative lighting.
//	The last texture unit is always used for the texture modulation.
//	TUs 1...n-1 are used for dynamic lights.
//===========================================================================
void envModMultiTex(int activate)
{
	// Setup TU 2: The modulated texture.
	activeTexture(GL_TEXTURE1);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	// Setup TU 1: The dynamic light.
	activeTexture(GL_TEXTURE0);
	envAddColoredAlpha(activate, GL_SRC_ALPHA);

	// This is a single-pass mode. The alpha should remain unmodified
	// during the light stage.
	if(activate)
	{
		// Replace: primAlpha
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
	}
}

//===========================================================================
// DG_Clear
//===========================================================================
void DG_Clear(int bufferbits)
{
	GLbitfield mask = 0;

	if(bufferbits & DGL_COLOR_BUFFER_BIT) mask |= GL_COLOR_BUFFER_BIT;
	if(bufferbits & DGL_DEPTH_BUFFER_BIT) mask |= GL_DEPTH_BUFFER_BIT;
	glClear(mask);
}

//===========================================================================
// DG_Viewport
//===========================================================================
void DG_Viewport(int x, int y, int width, int height)
{
	glViewport(x, FLIP(y+height-1), width, height);
}

//===========================================================================
// DG_Scissor
//===========================================================================
void DG_Scissor(int x, int y, int width, int height)
{
	glScissor(x, FLIP(y+height-1), width, height);
}

//===========================================================================
// DG_GetIntegerv
//===========================================================================
int	DG_GetIntegerv(int name, int *v)
{
	int i;
	float color[4];

	switch(name)
	{
	case DGL_VERSION:
		*v = DGL_VERSION_NUM;
		break;

	case DGL_MAX_TEXTURE_SIZE:
		*v = maxTexSize;
		break;

	case DGL_MAX_TEXTURE_UNITS:
		*v = maxTexUnits;
		break;

	case DGL_MODULATE_ADD_COMBINE:
		*v = extNvTexEnvComb || extAtiTexEnvComb;
		break;

	case DGL_PALETTED_TEXTURES:
		*v = usePalTex;
		break;

	case DGL_PALETTED_GENMIPS:
		// We are unable to generate mipmaps for paletted textures.
		*v = DGL_FALSE;
		break;

	case DGL_SCISSOR_TEST:
		glGetIntegerv(GL_SCISSOR_TEST, v);
		break;

	case DGL_SCISSOR_BOX:
		glGetIntegerv(GL_SCISSOR_BOX, v);
		v[1] = FLIP(v[1]+v[3]-1);
		break;

	case DGL_FOG:
		*v = useFog;
		break;

	case DGL_R:
		glGetFloatv(GL_CURRENT_COLOR, color);
		*v = (int) (color[0] * 255);
		break;

	case DGL_G:
		glGetFloatv(GL_CURRENT_COLOR, color);
		*v = (int) (color[1] * 255);
		break;

	case DGL_B:
		glGetFloatv(GL_CURRENT_COLOR, color);
		*v = (int) (color[2] * 255);
		break;

	case DGL_A:
		glGetFloatv(GL_CURRENT_COLOR, color);
		*v = (int) (color[3] * 255);
		break;

	case DGL_RGBA:
		glGetFloatv(GL_CURRENT_COLOR, color);
		for(i = 0; i < 4; i++) v[i] = (int) (color[i] * 255);
		break;

	case DGL_POLY_COUNT:
		*v = polyCounter;
		polyCounter = 0;
		break;

	case DGL_TEXTURE_BINDING:
		glGetIntegerv(GL_TEXTURE_BINDING_2D, v);
		break;

	default:
		return DGL_ERROR;
	}
	return DGL_OK;
}

//===========================================================================
// DG_GetInteger
//===========================================================================
int DG_GetInteger(int name)
{
	int values[10];
	DG_GetIntegerv(name, values);
	return values[0];
}

//===========================================================================
// DG_SetInteger
//===========================================================================
int	DG_SetInteger(int name, int value)
{
	float color[4];

	switch(name)
	{
#ifdef WIN32
	case DGL_WINDOW_HANDLE:
		windowHandle = (HWND) value;
		break;
#endif

	case DGL_ACTIVE_TEXTURE:
		activeTexture(GL_TEXTURE0 + value);
		break;

	case DGL_MODULATE_TEXTURE:
		if(value == 0)
		{
			// No modulation: just replace with texture.
			activeTexture(GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		}
		else if(value == 1)
		{
			// Normal texture modulation with primary color.
			activeTexture(GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		else if(value == 2 || value == 3)
		{	
			// Texture modulation and interpolation.
			activeTexture(GL_TEXTURE1);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
			if(value == 2) // Used with surfaces that have a color.
			{
				// TU 2: Modulate previous with primary color.
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PRIMARY_COLOR);
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);

			}
			else // Mode 3: Used with surfaces with no primary color.
			{
				// TU 2: Pass through.
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			}
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);	
			
			// TU 1: Interpolate between texture 1 and 2, using the constant 
			// alpha as the factor.
			activeTexture(GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_INTERPOLATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE1);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB, GL_CONSTANT);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
			
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
		}
		else if(value == 4)
		{
			// Apply sector light, dynamic light and texture.
			envModMultiTex(true);
		}
		else if(value == 5 || value == 10)
		{
			// Sector light * texture + dynamic light.
			activeTexture(GL_TEXTURE1);
			envAddColoredAlpha(true, 
				value == 5? GL_SRC_ALPHA : GL_SRC_COLOR);
			
			// Alpha remains unchanged.
			if(extNvTexEnvComb)
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_ADD);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_ZERO);
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PREVIOUS);
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA, GL_ZERO);
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_ALPHA, GL_SRC_ALPHA);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE3_ALPHA_NV, GL_ZERO);
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND3_ALPHA_NV, GL_SRC_ALPHA);
			}
			else
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
				glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
				glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
			}

			activeTexture(GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
		else if(value == 6)
		{
			// Simple dynlight addition (add to primary color).
			activeTexture(GL_TEXTURE0);
			envAddColoredAlpha(true, GL_SRC_ALPHA);
		}
		else if(value == 7)
		{
			// Dynlight addition without primary color.
			activeTexture(GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_CONSTANT);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);	
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
		}
		else if(value == 8 || value == 9)
		{
			// Texture and Detail.
			activeTexture(GL_TEXTURE1);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 2);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
			
			activeTexture(GL_TEXTURE0);
			if(value == 8)
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			}
			else // Mode 9: Ignore primary color.
			{
				glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			}
		}
		else if(value == 11)
		{
			// Normal modulation, alpha of 2nd stage.
			// Tex0: texture
			// Tex1: shiny texture
			activeTexture(GL_TEXTURE1);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);

			activeTexture(GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
			glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_TEXTURE1);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_TEXTURE0);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
		}
		break;

	case DGL_ENV_ALPHA:
		color[0] = color[1] = color[2] = 0;
		color[3] = value/256.0f;
		glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);
		break;

	case DGL_GRAY_MIPMAP:
		grayMipmapFactor = value/255.0f;
		break;

	case DGL_CULL_FACE:
		glFrontFace(value == DGL_CCW? GL_CW : GL_CCW);
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
		return DROGL_VERSION_FULL;
	}
	return NULL;
}

//===========================================================================
// DG_SetFloatv
//===========================================================================
int DG_SetFloatv(int name, float *values)
{
	switch(name)
	{
	case DGL_ENV_COLOR:
		glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, values);
		break;

	default:
		return DGL_ERROR;
	}
	return DGL_OK;
}
	
//===========================================================================
// DG_Enable
//===========================================================================
int DG_Enable(int cap)
{
	switch(cap)
	{
	case DGL_TEXTURING:
#ifndef DRMESA
		glEnable(GL_TEXTURE_2D);
#endif
		break;

	case DGL_TEXTURE_COMPRESSION:
		allowCompression = DGL_TRUE;
		break;

	case DGL_BLENDING:
		glEnable(GL_BLEND);
		break;

	case DGL_FOG:
		glEnable(GL_FOG);
		useFog = DGL_TRUE;
		break;

	case DGL_DEPTH_TEST:
		glEnable(GL_DEPTH_TEST);
		break;

	case DGL_ALPHA_TEST:
		glEnable(GL_ALPHA_TEST);
		break;

	case DGL_CULL_FACE:
		glEnable(GL_CULL_FACE);
		break;

	case DGL_SCISSOR_TEST:
		glEnable(GL_SCISSOR_TEST);
		break;

	case DGL_COLOR_WRITE:
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		break;

	case DGL_DEPTH_WRITE:
		glDepthMask(GL_TRUE);
		break;

	case DGL_PALETTED_TEXTURES:
		enablePalTexExt(DGL_TRUE);
		break;

	case DGL_TEXTURE0:
	case DGL_TEXTURE1:
	case DGL_TEXTURE2:
	case DGL_TEXTURE3:
	case DGL_TEXTURE4:
	case DGL_TEXTURE5:
	case DGL_TEXTURE6:
	case DGL_TEXTURE7:
		activeTexture(GL_TEXTURE0 + cap - DGL_TEXTURE0);
		glEnable(GL_TEXTURE_2D);
		break;

	case DGL_WIREFRAME_MODE:
		wireframeMode = true;
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		break;

	default:
		return DGL_FALSE;
	}
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
		glDisable(GL_TEXTURE_2D);
		break;

	case DGL_TEXTURE_COMPRESSION:
		allowCompression = DGL_FALSE;
		break;

	case DGL_BLENDING:
		glDisable(GL_BLEND);
		break;

	case DGL_FOG:
		glDisable(GL_FOG);
		useFog = DGL_FALSE;
		break;

	case DGL_DEPTH_TEST:
		glDisable(GL_DEPTH_TEST);
		break;

	case DGL_ALPHA_TEST:
		glDisable(GL_ALPHA_TEST);
		break;

	case DGL_CULL_FACE:
		glDisable(GL_CULL_FACE);
		break;

	case DGL_SCISSOR_TEST:
		glDisable(GL_SCISSOR_TEST);
		break;

	case DGL_COLOR_WRITE:
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		break;

	case DGL_DEPTH_WRITE:
		glDepthMask(GL_FALSE);
		break;

	case DGL_PALETTED_TEXTURES:
		enablePalTexExt(DGL_FALSE);
		break;

	case DGL_TEXTURE0:
	case DGL_TEXTURE1:
	case DGL_TEXTURE2:
	case DGL_TEXTURE3:
	case DGL_TEXTURE4:
	case DGL_TEXTURE5:
	case DGL_TEXTURE6:
	case DGL_TEXTURE7:
		activeTexture(GL_TEXTURE0 + cap - DGL_TEXTURE0);
		glDisable(GL_TEXTURE_2D);

		// Implicit disabling of texcoord array.
		if(noArrays)
		{
			DG_DisableArrays(0, 0, 1 << (cap - DGL_TEXTURE0));
		}
		break;

	case DGL_WIREFRAME_MODE:
		wireframeMode = false;
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		break;
	}
}

//===========================================================================
// DG_Func
//===========================================================================
void DG_Func(int func, int param1, int param2)
{
	switch(func)
	{
	case DGL_BLENDING:
		glBlendFunc(param1==DGL_ZERO?			GL_ZERO
			: param1==DGL_ONE?					GL_ONE
			: param1==DGL_DST_COLOR?			GL_DST_COLOR
			: param1==DGL_ONE_MINUS_DST_COLOR?	GL_ONE_MINUS_DST_COLOR
			: param1==DGL_SRC_ALPHA?			GL_SRC_ALPHA
			: param1==DGL_ONE_MINUS_SRC_ALPHA?	GL_ONE_MINUS_SRC_ALPHA
			: param1==DGL_DST_ALPHA?			GL_DST_ALPHA
			: param1==DGL_ONE_MINUS_DST_ALPHA?	GL_ONE_MINUS_DST_ALPHA
			: param1==DGL_SRC_ALPHA_SATURATE?	GL_SRC_ALPHA_SATURATE
			: GL_ZERO,

			param2==DGL_ZERO?					GL_ZERO
			: param2==DGL_ONE?					GL_ONE
			: param2==DGL_SRC_COLOR?			GL_SRC_COLOR
			: param2==DGL_ONE_MINUS_SRC_COLOR?	GL_ONE_MINUS_SRC_COLOR
			: param2==DGL_SRC_ALPHA?			GL_SRC_ALPHA
			: param2==DGL_ONE_MINUS_SRC_ALPHA?	GL_ONE_MINUS_SRC_ALPHA
			: param2==DGL_DST_ALPHA?			GL_DST_ALPHA
			: param2==DGL_ONE_MINUS_DST_ALPHA?	GL_ONE_MINUS_DST_ALPHA
			: GL_ZERO);
		break;

	case DGL_BLENDING_OP:
		if(!glBlendEquationEXT) break;
		glBlendEquationEXT(param1==DGL_SUBTRACT?	GL_FUNC_SUBTRACT
			: param1==DGL_REVERSE_SUBTRACT?			GL_FUNC_REVERSE_SUBTRACT
			: GL_FUNC_ADD);
		break;

	case DGL_DEPTH_TEST:
		glDepthFunc(param1==DGL_NEVER?	GL_NEVER
			: param1==DGL_LESS?			GL_LESS
			: param1==DGL_EQUAL?		GL_EQUAL
			: param1==DGL_LEQUAL?		GL_LEQUAL
			: param1==DGL_GREATER?		GL_GREATER
			: param1==DGL_NOTEQUAL?		GL_NOTEQUAL
			: param1==DGL_GEQUAL?		GL_GEQUAL
			: GL_ALWAYS);
		break;

	case DGL_ALPHA_TEST:
		glAlphaFunc(param1==DGL_NEVER?	GL_NEVER
			: param1==DGL_LESS?			GL_LESS
			: param1==DGL_EQUAL?		GL_EQUAL
			: param1==DGL_LEQUAL?		GL_LEQUAL
			: param1==DGL_GREATER?		GL_GREATER
			: param1==DGL_NOTEQUAL?		GL_NOTEQUAL
			: param1==DGL_GEQUAL?		GL_GEQUAL
			: GL_ALWAYS,
			param2 / 255.0f);
		break;
	}
}

//===========================================================================
// DG_MatrixMode
//===========================================================================
void DG_MatrixMode(int mode)
{
	glMatrixMode(mode==DGL_PROJECTION? GL_PROJECTION
		: mode==DGL_TEXTURE? GL_TEXTURE
		: GL_MODELVIEW);
}

//===========================================================================
// DG_PushMatrix
//===========================================================================
void DG_PushMatrix(void)
{
	glPushMatrix();
}

//===========================================================================
// DG_PopMatrix
//===========================================================================
void DG_PopMatrix(void)
{
	glPopMatrix();
}

//===========================================================================
// DG_LoadIdentity
//===========================================================================
void DG_LoadIdentity(void)
{
	glLoadIdentity();
}

//===========================================================================
// DG_Translatef
//===========================================================================
void DG_Translatef(float x, float y, float z)
{
	glTranslatef(x, y, z);
}

//===========================================================================
// DG_Rotatef
//===========================================================================
void DG_Rotatef(float angle, float x, float y, float z)
{
	glRotatef(angle, x, y, z);
}

//===========================================================================
// DG_Scalef
//===========================================================================
void DG_Scalef(float x, float y, float z)
{
	glScalef(x, y, z);
}

//===========================================================================
// DG_Ortho
//===========================================================================
void DG_Ortho(float left, float top, float right, float bottom, float znear, float zfar)
{
	glOrtho(left, right, bottom, top, znear, zfar);
}

//===========================================================================
// DG_Perspective
//===========================================================================
void DG_Perspective(float fovy, float aspect, float zNear, float zFar)
{
	gluPerspective(fovy, aspect, zNear, zFar);
}

//===========================================================================
// DG_Grab
//===========================================================================
int DG_Grab(int x, int y, int width, int height, int format, void *buffer)
{
	if(format != DGL_RGB) return DGL_UNSUPPORTED;
	// y+height-1 is the bottom edge of the rectangle. It's
	// flipped to change the origin.
	glReadPixels(x, FLIP(y+height-1), width, height, GL_RGB,
		GL_UNSIGNED_BYTE, buffer);
	return DGL_OK;
}

//===========================================================================
// DG_Fog
//===========================================================================
void DG_Fog(int pname, float param)
{
	int		iparam = (int) param;

	switch(pname)
	{
	case DGL_FOG_MODE:
		glFogi(GL_FOG_MODE, param==DGL_LINEAR? GL_LINEAR
			: param==DGL_EXP? GL_EXP
			: GL_EXP2);
		break;

	case DGL_FOG_DENSITY:
		glFogf(GL_FOG_DENSITY, param);
		break;

	case DGL_FOG_START:
		glFogf(GL_FOG_START, param);
		break;

	case DGL_FOG_END:
		glFogf(GL_FOG_END, param);
		break;

	case DGL_FOG_COLOR:
		if(iparam >= 0 && iparam < 256)
		{
			float col[4];
			int i;
			for(i=0; i<4; i++)
				col[i] = palette[iparam].color[i] / 255.0f;
			glFogfv(GL_FOG_COLOR, col);
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
	float	col[4];
	int		i;

	switch(pname)
	{
	case DGL_FOG_COLOR:
		for(i=0; i<4; i++)
			col[i] = ubvparam[i] / 255.0f;
		glFogfv(GL_FOG_COLOR, col);
		break;

	default:
		DG_Fog(pname, param);
		break;
	}
}

//===========================================================================
// DG_Project
//	Clipping is performed. 
//===========================================================================
int DG_Project(int num, gl_fc3vertex_t *inVertices, gl_fc3vertex_t *outVertices)
{
	GLdouble	modelMatrix[16], projMatrix[16];
	GLint		viewport[4];
	GLdouble	x, y, z;
	int			i, numOut;
	gl_fc3vertex_t *in = inVertices, *out = outVertices;

	if(num == 0) return 0;

	// Get the data we'll need in the operation.
	glGetDoublev(GL_MODELVIEW_MATRIX, modelMatrix);
	glGetDoublev(GL_PROJECTION_MATRIX, projMatrix);
	glGetIntegerv(GL_VIEWPORT, viewport);
	for(i = numOut = 0; i < num; i++, in++)
	{
		if(gluProject(in->pos[VX], in->pos[VY], in->pos[VZ],
			modelMatrix, projMatrix, viewport,
			&x, &y, &z) == GL_TRUE)
		{
			// A success: add to the out vertices.
			out->pos[VX] = (float) x;
			out->pos[VY] = (float) FLIP(y);
			out->pos[VZ] = (float) z;
			// Check that it's truly visible.
			if(out->pos[VX] < 0 || out->pos[VY] < 0 
				|| out->pos[VX] >= screenWidth 
				|| out->pos[VY] >= screenHeight) 
				continue;
			memcpy(out->color, in->color, sizeof(in->color));
			numOut++;
			out++;
		}
	}
	return numOut;
}

//===========================================================================
// DG_ReadPixels
//	NOTE: This function will not be needed any more when the halos are
//	rendered using the new method.
//===========================================================================
int DG_ReadPixels(int *inData, int format, void *pixels)
{
	int		type = inData[0], num, *coords, i;
	float	*fv = pixels;
	
	if(format != DGL_DEPTH_COMPONENT) return DGL_UNSUPPORTED;

	// Check the type.
	switch(type)
	{
	case DGL_SINGLE_PIXELS:
		num = inData[1];
		coords = inData + 2;
		for(i=0; i<num; i++, coords+=2)
		{
			glReadPixels(coords[0], FLIP(coords[1]), 1, 1,
				GL_DEPTH_COMPONENT, GL_FLOAT, fv+i);
		}
		break;
	
	case DGL_BLOCK:
		coords = inData + 1;
		glReadPixels(coords[0], FLIP(coords[1]+coords[3]-1), coords[2], coords[3],
			GL_DEPTH_COMPONENT, GL_FLOAT, pixels);
		break;

	default:
		return DGL_UNSUPPORTED;
	}
	return DGL_OK;
}

