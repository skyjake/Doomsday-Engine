
//**************************************************************************
//**
//** MAIN.C (Unix/SDL)
//**
//** Target:		DGL Driver for OpenGL
//** Description:	Init and shutdown, state management (using SDL)
//**
//** Get OpenGL header files from:
//** http://oss.sgi.com/projects/ogl-sample/ 
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "drOpenGL.h"

#include <stdlib.h>

/*
//#define DEBUGMODE
#ifdef DEBUGMODE
#	define DEBUG
#	include <assert.h>
#	include <dprintf.h>
#endif*/

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

void initState(void);

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

boolean			firstTimeInit = true;

// The State.
int				screenWidth, screenHeight, screenBits, windowed;
int				palExtAvailable, sharedPalExtAvailable;
boolean			texCoordPtrEnabled;
int				maxTexSize;
float			maxAniso = 1;
int				maxTexUnits;
int				useAnisotropic;
int				verbose;
boolean			wireframeMode;
boolean			allowCompression;
boolean			noArrays;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

#if 0
//===========================================================================
// fullscreenMode
//	Change the display mode using the Win32 API.
//	The closest available refresh rate is selected.
//===========================================================================
int fullscreenMode(int width, int height, int bpp)
{
	DEVMODE	current, testMode, newMode;
	int		res, i;

	// First get the current settings.
	memset(&current, 0, sizeof(current));
	current.dmSize = sizeof(DEVMODE);
	if(EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &current))
	{
		if(!bpp) bpp = current.dmBitsPerPel;
	}	
	else if(!bpp)
	{
		// A safe fallback.
		bpp = 16;
	}

	// Override refresh rate?
	if(ArgCheckWith("-refresh", 1))
		current.dmDisplayFrequency = strtol(ArgNext(), 0, 0);

	// Clear the structure.
	memset(&newMode, 0, sizeof(newMode));	
	newMode.dmSize = sizeof(newMode);

	// Let's enumerate all possible modes to find the most suitable one.
	for(i = 0; ; i++)
	{
		memset(&testMode, 0, sizeof(testMode));
		testMode.dmSize = sizeof(DEVMODE);
		if(!EnumDisplaySettings(NULL, i, &testMode)) break;

		if(testMode.dmPelsWidth == (unsigned) width 
			&& testMode.dmPelsHeight == (unsigned) height
			&& testMode.dmBitsPerPel == (unsigned) bpp)
		{
			// This looks promising. We'll take the one that best matches
			// the current refresh rate.
			if(abs(current.dmDisplayFrequency - testMode.dmDisplayFrequency)
				< abs(current.dmDisplayFrequency - newMode.dmDisplayFrequency))
			{
				memcpy(&newMode, &testMode, sizeof(DEVMODE));
			}
		}
	}

	if(!newMode.dmPelsWidth)
	{
		// A perfect match was not found. Let's try something.
		newMode.dmPelsWidth = width;
		newMode.dmPelsHeight = height;
		newMode.dmBitsPerPel = bpp;
		newMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
	}

	if((res=ChangeDisplaySettings(&newMode, 0)) != DISP_CHANGE_SUCCESSFUL)
	{
		Con_Message("drOpenGL.setResolution: Error %x.\n", res);
		return 0; // Failed, damn you.
	}

	// Set the correct window style and size.
	SetWindowLong(windowHandle, GWL_STYLE, WS_POPUP | WS_VISIBLE 
		| WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	SetWindowPos(windowHandle, 0, 0, 0, width, height, SWP_NOZORDER);

	// Update the screen size variables.
	screenWidth = width;
	screenHeight = height;
	if(bpp) screenBits = bpp;

	// Done!
	return 1;
}

//===========================================================================
// windowedMode
//	Only adjusts the window style and size.
//===========================================================================
void windowedMode(int width, int height)
{
	// We need to have a large enough client area.
	RECT rect;
	int xoff = (GetSystemMetrics(SM_CXSCREEN) - width) / 2, 
		yoff = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	LONG style;
	
	if(ArgCheck("-nocenter")) xoff = yoff = 0;
	if(ArgCheckWith("-xpos", 1)) xoff = atoi(ArgNext());
	if(ArgCheckWith("-ypos", 1)) yoff = atoi(ArgNext());
	
	rect.left = xoff;
	rect.top = yoff;
	rect.right = xoff + width;
	rect.bottom = yoff + height;

	// Set window style.
	style = GetWindowLong(windowHandle, GWL_STYLE) 
		| WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE | WS_CAPTION 
		| WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	SetWindowLong(windowHandle, GWL_STYLE, style);
	AdjustWindowRect(&rect, style, FALSE);
	SetWindowPos(windowHandle, 0, xoff, yoff, /*rect.left, rect.top,*/ 
		rect.right-rect.left, rect.bottom-rect.top, SWP_NOZORDER);

	screenWidth = width;
	screenHeight = height;
}
#endif

//===========================================================================
// initOpenGL
//===========================================================================
int initOpenGL(void)
{
	int flags = SDL_OPENGL /*| SDL_FULLSCREEN*/;

	// Attempt to set the video mode.
    if(!SDL_SetVideoMode(screenWidth, screenHeight, screenBits, flags))
	{
        // This could happen for a variety of reasons, including
        // DISPLAY not being set, the specified resolution not being
        // available, etc.
		Con_Message("SDL Error: %s\n", SDL_GetError());
		return DGL_FALSE;
    }

	// Setup the GL state like we want it.
	initState();
	return DGL_TRUE;
}

//===========================================================================
// activeTexture
//===========================================================================
void activeTexture(const GLenum texture)
{
#ifdef USE_MULTITEXTURE
	glActiveTextureARB(texture);
#endif
}

//===========================================================================
// DG_Init
//	'mode' is either DGL_MODE_WINDOW or DGL_MODE_FULLSCREEN. If 'bpp' is
//	zero, the current display color depth is used.
//===========================================================================
int DG_Init(int width, int height, int bpp, int mode)
{
	boolean fullscreen = (mode == DGL_MODE_FULLSCREEN);
	char *token, *extbuf;
	const SDL_VideoInfo *info = NULL;

	Con_Message("DG_Init: OpenGL.\n");

	// Are we in range here?
/*	if(!fullscreen)
	{
		if(width > GetSystemMetrics(SM_CXSCREEN))
			width = GetSystemMetrics(SM_CXSCREEN);
	
		if(height >	GetSystemMetrics(SM_CYSCREEN))
			height = GetSystemMetrics(SM_CYSCREEN);
	}
*/
	info = SDL_GetVideoInfo();
	screenBits   = info->vfmt->BitsPerPixel;
	screenWidth  = width;
	screenHeight = height;
	windowed     = !fullscreen;
	
	allowCompression = true;
	verbose = ArgExists("-verbose");

	// Set GL attributes.  We want at least 5 bits per color and a 16
	// bit depth buffer.  Plus double buffering, of course.
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	
	/*
	if(fullscreen)
	{
		if(!fullscreenMode(screenWidth, screenHeight, bpp))		
		{
			Con_Error("drOpenGL.Init: Resolution change failed (%d x %d).\n",
				screenWidth, screenHeight);
		}
	}
	else
	{
		windowedMode(screenWidth, screenHeight);
	}
	*/

	

	if(!initOpenGL())
	{
		Con_Error("drOpenGL.Init: OpenGL init failed.\n");
	}

	// Clear the buffers.
	DG_Clear(DGL_COLOR_BUFFER_BIT | DGL_DEPTH_BUFFER_BIT);

	token = (char*) glGetString(GL_EXTENSIONS);
	extbuf = malloc(strlen(token) + 1);
	strcpy(extbuf, token);	

	// Check the maximum texture size.
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);

	initExtensions();
	
	if(firstTimeInit)
	{
		firstTimeInit = DGL_FALSE;
		// Print some OpenGL information (console must be initialized by now).
		Con_Message("OpenGL information:\n");
		Con_Message("  Vendor: %s\n", glGetString(GL_VENDOR));
		Con_Message("  Renderer: %s\n", glGetString(GL_RENDERER));
		Con_Message("  Version: %s\n", glGetString(GL_VERSION));
		Con_Message("  Extensions:\n");

		// Show the list of GL extensions.
		token = strtok(extbuf, " ");
		while(token)
		{
			Con_Message("      "); // Indent.
			if(verbose)
			{
				// Show full names.
				Con_Message("%s\n", token);
			}
			else
			{
				// Two on one line, clamp to 30 characters.
				Con_Message("%-30.30s", token);
				token = strtok(NULL, " ");
				if(token) Con_Message(" %-30.30s", token);
				Con_Message("\n");
			}
			token = strtok(NULL, " ");
		}
		Con_Message("  GLU Version: %s\n", gluGetString(GLU_VERSION));	

		glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTexUnits);
#ifndef USE_MULTITEXTURE
		maxTexUnits = 1;
#endif
		// But sir, we are simple people; two units is enough.
		if(maxTexUnits > 2) maxTexUnits = 2;
		Con_Message("  Texture units used: %i\n", maxTexUnits);

		Con_Message("  Maximum texture size: %i\n", maxTexSize);
		if(extAniso)
		{
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
			Con_Message("  Maximum anisotropy: %g\n", maxAniso);
		}
		//if(!noArrays) Con_Message("  Using vertex arrays.\n");
	}
	free(extbuf);

	// Decide whether vertex arrays should be done manually or with real
	// OpenGL calls.
	InitArrays();

	if(ArgCheck("-dumptextures")) 
	{
		dumpTextures = DGL_TRUE;
		Con_Message("  Dumping textures (mipmap level zero).\n");
	}
	if(extAniso && ArgExists("-anifilter"))
	{
		useAnisotropic = DGL_TRUE;
		Con_Message("  Using anisotropic texture filtering.\n");
	}
	return DGL_OK;
}

//===========================================================================
// DG_Shutdown
//===========================================================================
void DG_Shutdown(void)
{
	// No special shutdown procedures required.
}

//===========================================================================
// DG_Show
//===========================================================================
void DG_Show(void)
{
	// Swap buffers.
	SDL_GL_SwapBuffers();
	
	if(wireframeMode)
	{
		// When rendering is wireframe mode, we must clear the screen
		// before rendering a frame.
		DG_Clear(DGL_COLOR_BUFFER_BIT);
	}
}
