
//**************************************************************************
//**
//** GL_MAIN.C
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_misc.h"

#include "r_draw.h"

#if WIN32_GAMMA
#	include <icm.h>
#	include <math.h>
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef unsigned short gramp_t[3 * 256];

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

void GL_SetGamma(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int		maxnumnodes;
extern boolean	filloutlines;
extern HWND		hWndMain;		// Handle to the main window.

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int			UpdateState;

// The display mode and the default values.
// ScreenBits is ignored.
int			screenWidth = 640, screenHeight = 480, screenBits = 32;

// The default resolution (config file).
int			defResX = 640, defResY = 480, defBPP = 0;	
int			maxTexSize;
int			ratioLimit = 0;		// Zero if none.
int			test3dfx = 0;
int			r_framecounter;		// Used only for statistics.
int			r_detail = true;	// Render detail textures (if available).

float		vid_gamma = 1.0f, vid_bright = 0, vid_contrast = 1.0f;
int			glFontFixed, glFontVariable;

float		nearClip, farClip;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initOk = false;

static gramp_t original_gamma_ramp;
static boolean gamma_support = false;
static float oldgamma, oldcontrast, oldbright;

// CODE --------------------------------------------------------------------

//===========================================================================
// GL_IsInited
//===========================================================================
boolean GL_IsInited(void)
{
	return initOk;
}

//===========================================================================
// GL_Update
//	This update stuff is really old-fashioned...
//===========================================================================
void GL_Update(int flags)
{
	if(flags & DDUF_BORDER) BorderNeedRefresh = true;
	if(flags & DDUF_TOP) BorderTopRefresh = true;
	if(flags & DDUF_FULLVIEW) UpdateState |= I_FULLVIEW;
	if(flags & DDUF_STATBAR) UpdateState |= I_STATBAR;
	if(flags & DDUF_MESSAGES) UpdateState |= I_MESSAGES;
	if(flags & DDUF_FULLSCREEN) UpdateState |= I_FULLSCRN;

	if(flags & DDUF_UPDATE) GL_DoUpdate();
}

//==========================================================================
// GL_DoUpdate
//	Swaps buffers / blits the back buffer to the front.
//==========================================================================
void GL_DoUpdate(void)
{
	// Check for color adjustment changes.
	if(oldgamma != vid_gamma 
		|| oldcontrast != vid_contrast
		|| oldbright != vid_bright)	GL_SetGamma();

	if(UpdateState == I_NOUPDATE) return;

	// Blit screen to video.
	gl.Show();
	UpdateState = I_NOUPDATE; // clear out all draw types

	// Increment frame counter.
	r_framecounter++;
}

//===========================================================================
// GL_GetGammaRamp
//===========================================================================
void GL_GetGammaRamp(void *ramp)
{
#if WIN32_GAMMA
	HDC hdc = GetDC(hWndMain);
#endif

	if(ArgCheck("-noramp"))
	{
		gamma_support = false;
		return;
	}
#if WIN32_GAMMA
	gamma_support = false;
	if(GetDeviceGammaRamp(hdc, ramp)) gamma_support = true;
	ReleaseDC(hWndMain, hdc);
#endif
}

//===========================================================================
// GL_SetGammaRamp
//===========================================================================
void GL_SetGammaRamp(void *ramp)
{
#if WIN32_GAMMA
	HDC hdc;

	if(!gamma_support) return;
	hdc = GetDC(hWndMain);
	SetDeviceGammaRamp(hdc, ramp);
	ReleaseDC(hWndMain, hdc);
#endif
}

//===========================================================================
// GL_MakeGammaRamp
//	Gamma      - non-linear factor (curvature; >1.0 multiplies)
//	Contrast   - steepness
//	Brightness - uniform offset 
//===========================================================================
void GL_MakeGammaRamp(unsigned short *ramp, float gamma, float contrast, 
					 float bright)
{
	int i;
	double ideal[256]; // After processing clamped to unsigned short.
	double norm;

	// Init the ramp as a line with the steepness defined by contrast.
	for(i = 0; i < 256; i++)
		ideal[i] = i*contrast - (contrast-1)*127;

	// Apply the gamma curve.
	if(gamma != 1)
	{
		if(gamma <= 0.1f) gamma = 0.1f;
		norm = pow(255, 1/gamma - 1); // Normalizing factor.
		for(i = 0; i < 256; i++)
			ideal[i] = pow(ideal[i], 1/gamma) / norm;
	}

	// The last step is to add the brightness offset.
	for(i = 0; i < 256; i++) ideal[i] += bright * 128;

	// Clamp it and write the ramp table.
	for(i = 0; i < 256; i++)
	{
		ideal[i] *= 0x100; // Byte => word
		if(ideal[i] < 0) ideal[i] = 0;
		if(ideal[i] > 0xffff) ideal[i] = 0xffff;
		ramp[i] = ramp[i + 256] = ramp[i + 512] = (unsigned short) ideal[i];
	}
}

//===========================================================================
// GL_SetGamma
//	Updates the gamma ramp based on vid_gamma, vid_contrast and vid_bright.
//===========================================================================
void GL_SetGamma(void)
{
	gramp_t myramp;

	oldgamma = vid_gamma;
	oldcontrast = vid_contrast;
	oldbright = vid_bright;

	GL_MakeGammaRamp(myramp, vid_gamma, vid_contrast, vid_bright);
	GL_SetGammaRamp(myramp);
}

//===========================================================================
// GL_InitFont
//===========================================================================
void GL_InitFont(void)
{
	FR_Init();
	FR_PrepareFont("Fixed");
	glFontFixed = glFontVariable = FR_GetCurrent();
	Con_MaxLineLength();
	Con_Execute("font default", true);	// Set the console font.
}

//===========================================================================
// GL_ShutdownFont
//===========================================================================
void GL_ShutdownFont(void)
{
	FR_Shutdown();
	glFontFixed = glFontVariable = 0;
}

//===========================================================================
// GL_InitVarFont
//===========================================================================
void GL_InitVarFont(void)
{
	int old_font;

	if(novideo) return;
	old_font = FR_GetCurrent();
	FR_PrepareFont(screenHeight < 300? "Small7"
		: screenHeight < 400? "Small8" 
		: screenHeight < 480? "Small10" 
		: screenHeight < 600? "System"
		: screenHeight < 800? "System12"
		: "Large");
	glFontVariable = FR_GetCurrent();
	FR_SetFont(old_font);
}

//===========================================================================
// GL_ShutdownVarFont
//===========================================================================
void GL_ShutdownVarFont(void)
{
	if(novideo) return;
	FR_DestroyFont(glFontVariable);
	FR_SetFont(glFontFixed);
	glFontVariable = glFontFixed;
}

//===========================================================================
// GL_Init
//	One-time initialization of DGL and the renderer.
//===========================================================================
void GL_Init(void)
{
	//boolean fullScreenStartup = ArgExists("-fullstart")
		/*&& !ArgExists("-window")*/;

	if(initOk) return;	// Already initialized.
	if(novideo) return;

	Con_Message("GL_Init: Initializing Doomsday Graphics Library.\n");

	// Get the original gamma ramp and check if ramps are supported.
	GL_GetGammaRamp(original_gamma_ramp);

	// By default, use the resolution defined in (default).cfg.
	/*screenWidth = defResX;
	screenHeight = defResY;
	screenBits = defBPP;*/

	//if(fullScreenStartup)
	//{
		screenWidth = defResX;
		screenHeight = defResY;
		screenBits = defBPP;
	//}
/*	else
	{
		// Startup screen size. DGL will modify if doesn't fit 
		// on the screen.
		screenWidth = 600;
		screenHeight = 650;
		screenBits = 0;
	}*/

	// Check for command line options modifying the defaults.
	if(ArgCheckWith("-width", 1)) screenWidth = atoi(ArgNext());
	if(ArgCheckWith("-height", 1)) screenHeight = atoi(ArgNext());
	if(ArgCheckWith("-winsize", 2))
	{
		screenWidth = atoi(ArgNext());
		screenHeight = atoi(ArgNext());
	}
	if(ArgCheckWith("-bpp", 1)) screenBits = atoi(ArgNext());

	gl.Init(screenWidth, screenHeight, screenBits, 
		/*fullScreenStartup && */ !ArgExists("-window"));

	// Check the maximum texture size.
	gl.GetIntegerv(DGL_MAX_TEXTURE_SIZE, &maxTexSize);
	if(maxTexSize == 256) 
	{
		Con_Message("  Using restricted texture w/h ratio (1:8).\n");
		ratioLimit = 8;
		if(screenBits == 32)
			Con_Message("  Warning: Are you sure your video card accelerates a 32 bit mode?\n");
	}
	if(ArgCheck("-outlines"))
	{
		filloutlines = false;
		Con_Message( "  Textures have outlines.\n");
	}

	// Initialize the renderer into a 2D state.
	GL_Init2DState();
	
	// Initialize font renderer.
	GL_InitFont();

	// Set the gamma in accordance with vid-gamma, vid-bright and 
	// vid-contrast.
	GL_SetGamma();

	initOk = true;
}

//===========================================================================
// GL_RuntimeMode
//	Startup is over, so go to runtime resolution / window size.
//===========================================================================
/*void GL_RuntimeMode(void)
{
	int w, h;

	if(isDedicated || novideo) return;

	w = defResX;
	h = defResY;

	// Check for command line options modifying the defaults.
	if(ArgCheckWith("-width", 1)) w = atoi(ArgNext());
	if(ArgCheckWith("-height", 1)) h = atoi(ArgNext());
	if(ArgCheckWith("-winsize", 2))
	{
		w = atoi(ArgNext());
		h = atoi(ArgNext());
	}

	GL_ChangeResolution(w, h, screenBits);
}*/

//===========================================================================
// GL_InitRefresh
//	Initializes the graphics library for refresh. Also called at update.
//===========================================================================
void GL_InitRefresh(void)
{
	GL_InitTextureManager();
	GL_LoadSystemTextures();
}

//===========================================================================
// GL_ShutdownRefresh
//	Called once at final shutdown.
//===========================================================================
void GL_ShutdownRefresh(void)
{
	GL_ShutdownTextureManager();
	GL_DestroySkinNames();
}

//===========================================================================
// GL_Shutdown
//	Kills the graphics library for good.
//===========================================================================
void GL_Shutdown(void)
{
	if(!initOk) return;	// Not yet initialized fully.

	GL_ShutdownFont();
	Rend_ShutdownSky();
	Rend_Reset();
	GL_ShutdownRefresh();

	// Shutdown DGL.
	gl.Shutdown();

	// Restore original gamma.
	GL_SetGammaRamp(original_gamma_ramp);

	initOk = false;
}

//===========================================================================
// GL_Init2DState
//	Initializes the renderer to 2D state. 
//===========================================================================
void GL_Init2DState(void)	
{
	byte fogcol[4] = { 138, 138, 138, 1 };

	// The variables.
	nearClip = 5;
	farClip = 16500;

	// Here we configure the OpenGL state and set the projection matrix.
	gl.Disable(DGL_CULL_FACE);
	gl.Disable(DGL_DEPTH_TEST);
	gl.Enable(DGL_TEXTURING);

	// The projection matrix.
	gl.MatrixMode(DGL_PROJECTION);
	gl.LoadIdentity();
	gl.Ortho(0, 0, 320, 200, -1, 1);

	// Default state for the white fog is off.
	whitefog = false;
	gl.Disable(DGL_FOG);
	gl.Fog(DGL_FOG_MODE, DGL_LINEAR);
	gl.Fog(DGL_FOG_END, 2100);	// This should be tweaked a bit.
	gl.Fogv(DGL_FOG_COLOR, fogcol);

	/*glEnable(GL_POLYGON_SMOOTH);
	glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);*/
}

//===========================================================================
// GL_SwitchTo3DState
//===========================================================================
void GL_SwitchTo3DState(boolean push_state)
{
	if(push_state)
	{
		// Push the 2D matrices on the stack.
		gl.MatrixMode(DGL_PROJECTION);
		gl.PushMatrix();
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PushMatrix();
	}

	gl.Enable(DGL_CULL_FACE);
	gl.Enable(DGL_DEPTH_TEST);

	viewpx = viewwindowx * screenWidth/320,
	viewpy = viewwindowy * screenHeight/200;
	// Set the viewport.
	if(viewheight != SCREENHEIGHT)
	{
		viewpw = viewwidth * screenWidth/320;
		viewph = viewheight * screenHeight/200 + 1;
		gl.Viewport(viewpx, viewpy, viewpw, viewph);
	}
	else
	{
		viewpw = screenWidth;
		viewph = screenHeight;
	}

	// The 3D projection matrix.
	GL_ProjectionMatrix();
}

//===========================================================================
// GL_Restore2DState
//===========================================================================
void GL_Restore2DState(int step)
{
	switch(step)
	{
	case 1:
		gl.MatrixMode(DGL_PROJECTION);
		gl.LoadIdentity();
		gl.Ortho(0, 0, 320, (320*viewheight)/viewwidth, -1, 1);
		gl.MatrixMode(DGL_MODELVIEW);
		gl.LoadIdentity();
		break;

	case 2:
		gl.Viewport(0, 0, screenWidth, screenHeight);
		break;

	case 3:
		gl.MatrixMode(DGL_PROJECTION);
		gl.PopMatrix();
		gl.MatrixMode(DGL_MODELVIEW);
		gl.PopMatrix();
		gl.Disable(DGL_CULL_FACE);
		gl.Disable(DGL_DEPTH_TEST);
		break;
	}
}

//===========================================================================
// GL_ProjectionMatrix
//===========================================================================
void GL_ProjectionMatrix(void)
{
	// We're assuming pixels are squares.
	float aspect = viewpw/(float)viewph;

	gl.MatrixMode(DGL_PROJECTION);
	gl.LoadIdentity();
	gl.Perspective(yfov = fieldOfView/aspect, aspect, nearClip, farClip);
	
	// We'd like to have a left-handed coordinate system.
	gl.Scalef(1, 1, -1);
}

//===========================================================================
// GL_UseFog
//===========================================================================
void GL_UseFog(int yes)
{
	if(!whitefog && yes)
	{
		// Fog is turned on.
		whitefog = true;
		gl.Enable(DGL_FOG);
	}
	else if(whitefog && !yes)
	{
		// Fog must be turned off.
		whitefog = false;
		gl.Disable(DGL_FOG);
	}
	// Otherwise we won't do a thing.
}

//===========================================================================
// GL_ChangeResolution
//	Changes the resolution to the specified one. 
//	The change is carried out by SHUTTING DOWN the rendering DLL and then
//	restarting it. All textures will be lost in the process.
//	Restarting the renderer is the compatible way to do the change.
//===========================================================================
int GL_ChangeResolution(int w, int h, int bits)
{
/*	int res = gl.ChangeMode(w, h, 0, !nofullscreen);

	if(res == DGL_UNSUPPORTED)
	{
		Con_Message("I_ChangeResolution: Not supported by DGL driver.\n");
		return false;
	}
	if(res == DGL_OK)
	{
		screenWidth = w;
		screenHeight = h;
		// We can't be in a 3D mode right now, so we can
		// adjust the viewport right away.
		gl.Viewport(0, 0, screenWidth, screenHeight);
		Con_MaxLineLength();
		return true;
	}*/

	char oldFontName[256];
	boolean hadFog = whitefog;

	if(novideo) return false;
	if(screenWidth == w && screenHeight == h 
		&& (!bits || screenBits == bits)) return true;		

	// Remember the name of the font.
	strcpy(oldFontName, FR_GetFont(FR_GetCurrent())->name);

	// Delete all textures.
	GL_ShutdownTextureManager();
	GL_ShutdownFont();
	gx.UpdateState(DD_RENDER_RESTART_PRE);

	screenWidth = w;
	screenHeight = h;
	screenBits = bits;

	// Shutdown and re-initialize DGL.
	gl.Shutdown();
	gl.Init(w, h, bits, !nofullscreen);

	// Re-initialize.
	GL_InitFont();
	GL_Init2DState();
	GL_InitRefresh();
	R_SetupFog();				// Restore map's fog settings.
	// Make sure the fog is enabled, if necessary.
	if(hadFog) GL_UseFog(true);
	gx.UpdateState(DD_RENDER_RESTART_POST);

	// Restore the old font.
	Con_Executef(true, "font name %s", oldFontName);

	Con_Message("Display mode: %i x %i", screenWidth, screenHeight);
	if(screenBits) Con_Message( " x %i", screenBits);
	Con_Message(".\n");
	return true;	
}

//===========================================================================
// GL_GrabScreen
//	Copies the current contents of the frame buffer and returns a pointer
//	to data containing 24-bit RGB triplets. The caller must free the 
//	returned buffer using free()!
//===========================================================================
unsigned char *GL_GrabScreen(void)
{
	unsigned char *buffer = malloc(screenWidth*screenHeight*3);

	gl.Grab(0, 0, screenWidth, screenHeight, DGL_RGB, buffer);
	return buffer;
}

//==========================================================================
// CCmdSetRes
//	Change graphics mode resolution.
//==========================================================================
int CCmdSetRes(int argc, char **argv)
{
	if(isDedicated)
	{
		Con_Printf("Impossible in dedicated mode.\n");
		return false;
	}
	if(argc < 3)
	{
		Con_Printf("Usage: %s (width) (height)\n", argv[0]);
		Con_Printf("Changes display mode resolution.\n");
		return true;
	}
	return GL_ChangeResolution(atoi(argv[1]), atoi(argv[2]), 0);
}

//===========================================================================
// CCmdUpdateGammaRamp
//===========================================================================
int CCmdUpdateGammaRamp(int argc, char **argv)
{
	GL_SetGamma();
	Con_Printf("Gamma ramp set.\n");
	return true;
}
