
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

int extMultiTex;
int	extTexEnvComb;
int	extNvTexEnvComb;
int	extAtiTexEnvComb;
int	extAniso;
int	extLockArray;
int extGenMip;

PFNGLCLIENTACTIVETEXTUREPROC	glClientActiveTextureARB = 0;
PFNGLACTIVETEXTUREARBPROC		glActiveTextureARB = 0;
PFNGLMULTITEXCOORD2FARBPROC		glMultiTexCoord2fARB = 0;
PFNGLMULTITEXCOORD2FVARBPROC	glMultiTexCoord2fvARB = 0;

PFNGLLOCKARRAYSEXTPROC			glLockArraysEXT = 0;
PFNGLUNLOCKARRAYSEXTPROC		glUnlockArraysEXT = 0;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// queryExtension
//	Returns non-zero iff the extension string is found.
//  This function is based on Mark J. Kilgard's tutorials about OpenGL 
//	extensions.
//===========================================================================
int queryExtension(const char *name)
{
	const GLubyte *extensions = glGetString(GL_EXTENSIONS);
	const GLubyte *start;
	GLubyte *where, *terminator;

	if(!extensions) return false;

	// Extension names should not have spaces. 
	where = (GLubyte*) strchr(name, ' ');
	if(where || *name == '\0') return false;

	// It takes a bit of care to be fool-proof about parsing the
	// OpenGL extensions string. Don't be fooled by sub-strings, etc.
	start = extensions;

	for(;;) 
	{
		where = (GLubyte*) strstr((const char*) start, name);
		if(!where) break;
		terminator = where + strlen(name);
		if(where == start || *(where - 1) == ' ')
			if(*terminator == ' ' || *terminator == '\0')
			{
				return true;
			}
		start = terminator;
	}
	return false;
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

	if(query("GL_EXT_compiled_vertex_array", &extLockArray))
	{
		GETPROC( glLockArraysEXT );
		GETPROC( glUnlockArraysEXT );
	}

	query("GL_EXT_paletted_texture", &palExtAvailable);
	query("GL_EXT_shared_texture_palette", &sharedPalExtAvailable);
	query("GL_EXT_texture_filter_anisotropic", &extAniso);

	// ARB_texture_env_combine
	if(!query("GL_ARB_texture_env_combine", &extTexEnvComb))
	{
		// Try the older EXT_texture_env_combine (identical to ARB).
		query("GL_EXT_texture_env_combine", &extTexEnvComb);
	}

	// NV_texture_env_combine4
	query("GL_NV_texture_env_combine4", &extNvTexEnvComb);

	// ATI_texture_env_combine3
	query("GL_ATI_texture_env_combine3", &extAtiTexEnvComb);

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

#ifdef USE_MULTITEXTURE
	// ARB_multitexture 
	if(query("GL_ARB_multitexture", &extMultiTex))
	{
		// Get the function pointers.
		GETPROC( glClientActiveTextureARB );
		GETPROC( glActiveTextureARB );
		GETPROC( glMultiTexCoord2fARB );
		GETPROC( glMultiTexCoord2fvARB );
	}
#endif

	// Automatic mipmap generation.
	if(query("GL_SGIS_generate_mipmap", &extGenMip))
	{
		// Use nice quality, please.
		glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
	}
}
