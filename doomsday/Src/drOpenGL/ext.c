
//**************************************************************************
//**
//** EXT.C
//**
//** Target:		DGL Driver for OpenGL
//** Description:	Extensions
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "drOpenGL.h"

// MACROS ------------------------------------------------------------------

#define GETPROC(x)	x = (void*)wglGetProcAddress(#x)

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int		extMultiTex = DGL_FALSE;
int		extTexEnvComb = DGL_FALSE;
int		extAniso = DGL_FALSE;

PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = 0;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB = 0;
PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// queryExtension
//	Returns non-zero iff the extension string is found.
//===========================================================================
int queryExtension(const char *name)
{
    return strstr(glGetString(GL_EXTENSIONS), name) != 0;
}

//===========================================================================
// query
//===========================================================================
int query(const char *ext, int *var)
{
	if((*var = queryExtension(ext)) != DGL_FALSE) 
	{
		if(verbose) Con_Message("OpenGL extension: %s\n", ext);
		return true;
	}
	return false;
}

//===========================================================================
// initExtensions
//===========================================================================
void initExtensions(void)
{
	int i;

	query("GL_EXT_paletted_texture", &palExtAvailable);
	query("GL_EXT_shared_texture_palette", &sharedPalExtAvailable);
	query("GL_EXT_texture_filter_anisotropic", &extAniso);

	// ARB_texture_env_combine
	if(!query("GL_ARB_texture_env_combine", &extTexEnvComb))
	{
		// Try the older EXT_texture_env_combine (identical to ARB).
		query("GL_EXT_texture_env_combine", &extTexEnvComb);
	}

	// Texture compression.
	useCompr = DGL_FALSE;
	if(ArgCheck("-texcomp"))
	{
		glGetError();
		glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &i);
		if(i && glGetError() == GL_NO_ERROR) 
		{
			useCompr = DGL_TRUE;
			Con_Message("OpenGL: Texture compression (%i formats).\n", i);
		}
	}

#if 0 // Not needed at the moment.

	/* ARB_multitexture */
	extMultiTex = queryExtension("GL_ARB_multitexture");
	if(extMultiTex)
	{
		// Get the function pointers.
		GETPROC(glActiveTextureARB);
		GETPROC(glMultiTexCoord2fARB);
		GETPROC(glMultiTexCoord2fvARB);
		// Check that we have enough texturing units.
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &i);
		Con_Message("drOpenGL.initExtensions: ARB_multitexture (%i)\n", i);
		if(i < 2) 
		{
			Con_Message("--(!)-- 2 texture units required!\n");
			extMultiTex = DGL_FALSE;
		}
	}
#endif
}
