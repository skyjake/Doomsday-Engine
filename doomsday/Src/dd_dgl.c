
//**************************************************************************
//**
//** DD_DGL.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "de_base.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// Optional function. Doesn't need to be exported.
#define Opt(fname)	gl.fname = (void*) GetProcAddress(hInstDGL, "DG_"#fname)

// Required function. If not exported, the rendering DLL can't be used.
#define Req(fname)	if(( Opt(fname) ) == NULL) return false

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

dgldriver_t		gl;				// Engine's internal function table.

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HINSTANCE hInstDGL;		// Instance handle to the rendering DLL.

// CODE --------------------------------------------------------------------

/*
 * DD_InitDGLDriver
 *	Returns true if no required functions are missing.
 *	All exported DGL functions should have the DG_ prefix (Driver/Graphics).
 */
int DD_InitDGLDriver(void)
{
	Req(Init);
	Req(Shutdown);

	// Viewport.
	Req(Clear);
	Req(Show);
	Req(Viewport);
	Req(Scissor);

	// State.
	Req(GetInteger);
	Req(GetIntegerv);
	Req(SetInteger);
	Opt(SetFloatv);
	Req(GetString);
	Req(Enable);
	Req(Disable);
	Req(EnableArrays);
	Req(DisableArrays);
	Req(Arrays);
	Req(UnlockArrays);
	Req(Func);
	//Req(ZBias);	

	// Textures.
	Req(NewTexture);
	Req(DeleteTextures);
	Req(TexImage);
	Req(TexParameter);
	Req(GetTexParameterv);
	Req(Palette);	
	Req(Bind);

	// Matrix operations.
	Req(MatrixMode);
	Req(PushMatrix);
	Req(PopMatrix);
	Req(LoadIdentity);
	Req(Translatef);
	Req(Rotatef);
	Req(Scalef);
	Req(Ortho);
	Req(Perspective);

	// Colors.
	Req(Color3ub);
	Req(Color3ubv);
	Req(Color4ub);
	Req(Color4ubv);
	Req(Color3f);
	Req(Color3fv);
	Req(Color4f);
	Req(Color4fv);

	// Drawing.
	Req(Begin);
	Req(End);
	Req(Vertex2f);
	Req(Vertex2fv);
	Req(Vertex3f);
	Req(Vertex3fv);
	Req(TexCoord2f);
	Req(TexCoord2fv);
	Opt(MultiTexCoord2f);
	Opt(MultiTexCoord2fv);
	Req(Vertices2ftv);
	Req(Vertices3ftv);
	Req(Vertices3fctv);
	Req(Arrays);
	Req(UnlockArrays);
	Req(ArrayElement);
	Req(DrawElements);

	// Miscellaneous.
	Req(Grab);
	Req(Fog);
	Req(Fogv);
	Req(Project);
	Req(ReadPixels); 

	// All was OK.
	return true;
}

/*
 * DD_InitDGL
 *	Load the rendering DLL and setup the driver struct. The rendering DLL
 *	could be changed at runtime (but such an operation is currently never 
 *	done). Returns true if successful.
 */
int DD_InitDGL(void)
{
	char *dllName = "drOpenGL.dll";	// The default renderer.

	// See if a specific renderer DLL is specified.
	if(ArgCheckWith("-gl", 1)) dllName = ArgNext();

	// Load the DLL.
	hInstDGL = LoadLibrary(dllName);
	if(!hInstDGL)
	{
		ErrorBox(true, "DD_InitDGL: Loading of %s failed (error %i).\n", 
			dllName, GetLastError());
		return false;
	}

	// Prepare the driver struct.
	if(!DD_InitDGLDriver())
	{
		ErrorBox(true, "DD_InitDGL: Rendering DLL %s is incompatible.\n",
			dllName);
		return false;
	}

	// Check the version of the DLL.
	if(gl.GetInteger(DGL_VERSION) < DGL_VERSION_NUM)
	{
		ErrorBox(true, "DD_InitDGL: Version %i renderer found. "
			"Version %i is required.\n", gl.GetInteger(DGL_VERSION), 
			DGL_VERSION_NUM);
		return false;
	}
	return true;
}

/*
 * DD_ShutdownDGL
 *	Free the rendering DLL. You should shut it down first.
 */
void DD_ShutdownDGL(void)
{
	FreeLibrary(hInstDGL);
	hInstDGL = NULL;
}

/*
 * DD_GetDGLProcAddress
 *	Used by other modules (the Game) to get the addresses of the
 *	DGL routines.
 */
void *DD_GetDGLProcAddress(const char *name)
{
	return GetProcAddress(hInstDGL, name);
}
