/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * gl_main.c: Graphics Subsystem
 */

// HEADER FILES ------------------------------------------------------------

#ifdef UNIX
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#endif

#include "de_base.h"
#include "de_console.h"
#include "de_system.h"
#include "de_graphics.h"
#include "de_refresh.h"
#include "de_render.h"
#include "de_misc.h"

#include "r_draw.h"

#if defined(WIN32) && defined(WIN32_GAMMA)
#  include <icm.h>
#  include <math.h>
#endif

// SDL's gamma settings seem more robust.
#if 0
#if defined(UNIX) && defined(HAVE_X11_EXTENSIONS_XF86VMODE_H)
#  define XFREE_GAMMA
#  include <X11/Xlib.h>
#  include <X11/extensions/xf86vmode.h>
#endif
#endif

#if !defined(WIN32_GAMMA) && !defined(XFREE_GAMMA)
#  define SDL_GAMMA
#  include <SDL.h>
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef unsigned short gramp_t[3 * 256];

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(Fog);
D_CMD(SetGamma);
D_CMD(SetRes);

void    GL_SetGamma(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int maxnumnodes;
extern boolean filloutlines;

#ifdef WIN32
extern HWND hWndMain;           // Handle to the main window.
#endif

// PUBLIC DATA DEFINITIONS -------------------------------------------------

int     UpdateState;

// The display mode and the default values.
// ScreenBits is ignored.
int     glScreenWidth = 640, glScreenHeight = 480, glScreenBits = 32;

// The default resolution (config file).
int     defResX = 640, defResY = 480, defBPP = 0;
int     glMaxTexSize;
int     numTexUnits;
boolean envModAdd;              // TexEnv: modulate and add is available.
int     ratioLimit = 0;         // Zero if none.
int     test3dfx = 0;
int     r_framecounter;         // Used only for statistics.
int     r_detail = true;        // Render detail textures (if available).

float   vid_gamma = 1.0f, vid_bright = 0, vid_contrast = 1.0f;
int     glFontFixed, glFontVariable[NUM_GLFS];

float   glNearClip, glFarClip;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initGLOk = false;
static boolean varFontInited = false;

static gramp_t original_gamma_ramp;
static boolean gamma_support = false;
static float oldgamma, oldcontrast, oldbright;
static int fogModeDefault = 0;

// CODE --------------------------------------------------------------------

void GL_Register(void)
{
    // Cvars
    C_VAR_INT("rend-dev-wireframe", &renderWireframe, 0, 0, 1);
    C_VAR_INT("rend-dev-framecount", &framecount,
              CVF_NO_ARCHIVE | CVF_PROTECTED, 0, 0);
    C_VAR_INT("rend-fog-default", &fogModeDefault, 0, 0, 2);
    // * Render-HUD
    C_VAR_FLOAT("rend-hud-offset-scale", &weaponOffsetScale, CVF_NO_MAX,
                0, 0);
    C_VAR_FLOAT("rend-hud-fov-shift", &weaponFOVShift, CVF_NO_MAX, 0, 1);
    // * Render-Mobj
    C_VAR_INT("rend-mobj-smooth-move", &r_use_srvo, 0, 0, 2);
    C_VAR_INT("rend-mobj-smooth-turn", &r_use_srvo_angle, 0, 0, 1);

    // * video
    C_VAR_INT("vid-res-x", &defResX, CVF_NO_MAX, 320, 0);
    C_VAR_INT("vid-res-y", &defResY, CVF_NO_MAX, 240, 0);
    C_VAR_FLOAT("vid-gamma", &vid_gamma, 0, 0.1f, 6);
    C_VAR_FLOAT("vid-contrast", &vid_contrast, 0, 0, 10);
    C_VAR_FLOAT("vid-bright", &vid_bright, 0, -2, 2);

    // Ccmds
    C_CMD("fog", Fog);
    C_CMD("setgamma", SetGamma);
    C_CMD("setres", SetRes);
    C_CMD("setvidramp", UpdateGammaRamp);

    GL_TexRegister();
}

boolean GL_IsInited(void)
{
    return initGLOk;
}

/**
 * This update stuff is really old-fashioned...
 */
void GL_Update(int flags)
{
    if(flags & DDUF_BORDER)
        BorderNeedRefresh = true;
    if(flags & DDUF_TOP)
        BorderTopRefresh = true;
    if(flags & DDUF_FULLVIEW)
        UpdateState |= I_FULLVIEW;
    if(flags & DDUF_STATBAR)
        UpdateState |= I_STATBAR;
    if(flags & DDUF_MESSAGES)
        UpdateState |= I_MESSAGES;
    if(flags & DDUF_FULLSCREEN)
        UpdateState |= I_FULLSCRN;

    if(flags & DDUF_UPDATE)
        GL_DoUpdate();
}

/**
 * Swaps buffers / blits the back buffer to the front.
 */
void GL_DoUpdate(void)
{
    // Check for color adjustment changes.
    if(oldgamma != vid_gamma || oldcontrast != vid_contrast ||
       oldbright != vid_bright)
        GL_SetGamma();

    if(UpdateState == I_NOUPDATE)
        return;

    // Blit screen to video.
    if(renderWireframe)
        gl.Enable(DGL_WIREFRAME_MODE);
    gl.Show();
    if(renderWireframe)
        gl.Disable(DGL_WIREFRAME_MODE);
    UpdateState = I_NOUPDATE;   // clear out all draw types

    // Increment frame counter.
    r_framecounter++;
}

/**
 * On Win32, use the gamma ramp functions in the Win32 API.  On Linux,
 * use the XFree86-VidMode extension.
 */
void GL_GetGammaRamp(unsigned short *ramp)
{
    if(ArgCheck("-noramp"))
    {
        gamma_support = false;
        return;
    }

#ifdef SDL_GAMMA
    gamma_support = true;
    if(SDL_GetGammaRamp(ramp, ramp + 256, ramp + 512) < 0)
    {
        gamma_support = false;
    }
#endif

#if defined(WIN32) && defined(WIN32_GAMMA)
    {
        HDC     hdc = GetDC(hWndMain);

        gamma_support = false;
        if(GetDeviceGammaRamp(hdc, (void*) ramp))
        {
            gamma_support = true;
        }
        ReleaseDC(hWndMain, hdc);
    }
#endif

#ifdef XFREE_GAMMA
    {
        Display *dpy = XOpenDisplay(NULL);
        int screen = DefaultScreen(dpy);
        int event = 0, error = 0;
        int rampSize = 0;

        Con_Message("GL_GetGammaRamp:\n");
        if(!dpy || !XF86VidModeQueryExtension(dpy, &event, &error))
        {
            Con_Message("  XFree86-VidModeExtension not available.\n");
            gamma_support = false;
            return;
        }
        VERBOSE(Con_Message("  XFree86-VidModeExtension: event# %i "
                            "error# %i\n", event, error));
        XF86VidModeGetGammaRampSize(dpy, screen, &rampSize);
        Con_Message("  Gamma ramp size: %i\n", rampSize);
        if(rampSize != 256)
        {
            Con_Message("  This implementation only understands ramp size "
                        "256.\n  Please complain to the developer.\n");
            gamma_support = false;
            XCloseDisplay(dpy);
            return;
        }

        // Get the current ramps.
        XF86VidModeGetGammaRamp(dpy, screen, rampSize, ramp, ramp + 256,
                                ramp + 512);
        XCloseDisplay(dpy);

        gamma_support = true;
    }
#endif
}

void GL_SetGammaRamp(unsigned short *ramp)
{
    if(!gamma_support)
        return;

#ifdef SDL_GAMMA
    SDL_SetGammaRamp(ramp, ramp + 256, ramp + 512);
#endif

#if defined(WIN32) && defined(WIN32_GAMMA)
    {
        HDC hdc = GetDC(hWndMain);
        SetDeviceGammaRamp(hdc, (void*) ramp);
        ReleaseDC(hWndMain, hdc);
    }
#endif

#ifdef XFREE_GAMMA
    {
        Display *dpy = XOpenDisplay(NULL);
        int screen = DefaultScreen(dpy);

        if(!dpy)
            return;

        // We assume that the gamme ramp size actually is 256.
        XF86VidModeSetGammaRamp(dpy, screen, 256, ramp, ramp + 256,
                                ramp + 512);

        XCloseDisplay(dpy);
    }
#endif
}

/**
 * Gamma      - non-linear factor (curvature; >1.0 multiplies)
 * Contrast   - steepness
 * Brightness - uniform offset
 */
void GL_MakeGammaRamp(unsigned short *ramp, float gamma, float contrast,
                      float bright)
{
    int     i;
    double  ideal[256];         // After processing clamped to unsigned short.
    double  norm;

    // Don't allow stupid values.
    if(contrast < 0.1f)
        contrast = 0.1f;
    if(bright > 0.8f)
        bright = 0.8f;
    if(bright < -0.8f)
        bright = -0.8f;

    // Init the ramp as a line with the steepness defined by contrast.
    for(i = 0; i < 256; i++)
        ideal[i] = i * contrast - (contrast - 1) * 127;

    // Apply the gamma curve.
    if(gamma != 1)
    {
        if(gamma <= 0.1f)
            gamma = 0.1f;
        norm = pow(255, 1 / gamma - 1); // Normalizing factor.
        for(i = 0; i < 256; i++)
            ideal[i] = pow(ideal[i], 1 / gamma) / norm;
    }

    // The last step is to add the brightness offset.
    for(i = 0; i < 256; i++)
        ideal[i] += bright * 128;

    // Clamp it and write the ramp table.
    for(i = 0; i < 256; i++)
    {
        ideal[i] *= 0x100;      // Byte => word
        if(ideal[i] < 0)
            ideal[i] = 0;
        if(ideal[i] > 0xffff)
            ideal[i] = 0xffff;
        ramp[i] = ramp[i + 256] = ramp[i + 512] = (unsigned short) ideal[i];
    }
}

/**
 * Updates the gamma ramp based on vid_gamma, vid_contrast and vid_bright.
 */
void GL_SetGamma(void)
{
    gramp_t myramp;

    oldgamma = vid_gamma;
    oldcontrast = vid_contrast;
    oldbright = vid_bright;

    GL_MakeGammaRamp(myramp, vid_gamma, vid_contrast, vid_bright);
    GL_SetGammaRamp(myramp);
}

const char* GL_ChooseFixedFont()
{
    if(glScreenHeight < 300)
        return "console11";
    if(glScreenHeight > 768)
        return "console18";
    return "console14";
}

const char* GL_ChooseVariableFont(glfontstyle_t style)
{
    const int SMALL_LIMIT = 500;
    const int MED_LIMIT = 800;

    switch(style)
    {
    default:
        return (glScreenHeight < SMALL_LIMIT ? "normal12" :
                glScreenHeight < MED_LIMIT ? "normal18" :
                "normal24");

    case GLFS_LIGHT:
        return (glScreenHeight < SMALL_LIMIT ? "normallight12" :
                glScreenHeight < MED_LIMIT ? "normallight18" :
                "normallight24");

    case GLFS_BOLD:
        return (glScreenHeight < SMALL_LIMIT ? "normalbold12" :
                glScreenHeight < MED_LIMIT ? "normalbold18" :
                "normalbold24");
    }
    return "normal18";
}

void GL_InitFont(void)
{
    FR_Init();
    FR_PrepareFont(GL_ChooseFixedFont());
    glFontFixed =
        glFontVariable[GLFS_NORMAL] =
        glFontVariable[GLFS_LIGHT] = FR_GetCurrent();

    Con_MaxLineLength();

    // Also keep the bold and light fonts loaded.
    FR_PrepareFont(GL_ChooseVariableFont(GLFS_BOLD));
    glFontVariable[GLFS_BOLD] = FR_GetCurrent();

    FR_PrepareFont(GL_ChooseVariableFont(GLFS_LIGHT));
    glFontVariable[GLFS_LIGHT] = FR_GetCurrent();

    FR_SetFont(glFontFixed);
}

void GL_ShutdownFont(void)
{
    FR_Shutdown();
    glFontFixed =
        glFontVariable[GLFS_NORMAL] =
        glFontVariable[GLFS_BOLD] =
        glFontVariable[GLFS_LIGHT] = 0;
}

void GL_InitVarFont(void)
{
    int     old_font;
    int     i;

    if(novideo || varFontInited)
        return;

    VERBOSE2(Con_Message("GL_InitVarFont.\n"));

    old_font = FR_GetCurrent();
    VERBOSE2(Con_Message("GL_InitVarFont: Old font = %i.\n", old_font));

    for(i = 0; i < NUM_GLFS; ++i)
    {
        // The bold font and light fonts are always loaded.
        if(i == GLFS_BOLD || i == GLFS_LIGHT) continue;

        FR_PrepareFont(GL_ChooseVariableFont(i));
        glFontVariable[i] = FR_GetCurrent();
        VERBOSE2(Con_Message("GL_InitVarFont: Variable font = %i.\n", glFontVariable[i]));
    }

    FR_SetFont(old_font);
    VERBOSE2(Con_Message("GL_InitVarFont: Restored old font %i.\n", old_font));

    varFontInited = true;
}

void GL_ShutdownVarFont(void)
{
    int i;

    if(novideo || !varFontInited)
        return;
    FR_SetFont(glFontFixed);
    for(i = 0; i < NUM_GLFS; ++i)
    {
        // Keep the bold and light fonts loaded.
        if(i == GLFS_BOLD || i == GLFS_LIGHT) continue;

        FR_DestroyFont(glFontVariable[i]);
        glFontVariable[i] = glFontFixed;
    }
    varFontInited = false;
}

/**
 * One-time initialization of DGL and the renderer.
 */
void GL_Init(void)
{
    if(initGLOk)
        return;                 // Already initialized.
    if(novideo)
        return;

    Con_Message("GL_Init: Initializing Doomsday Graphics Library.\n");

    // Get the original gamma ramp and check if ramps are supported.
    GL_GetGammaRamp(original_gamma_ramp);

    // By default, use the resolution defined in (default).cfg.
    glScreenWidth = defResX;
    glScreenHeight = defResY;
    glScreenBits = defBPP;

    // Check for command line options modifying the defaults.
    if(ArgCheckWith("-width", 1))
        glScreenWidth = atoi(ArgNext());
    if(ArgCheckWith("-height", 1))
        glScreenHeight = atoi(ArgNext());
    if(ArgCheckWith("-winsize", 2))
    {
        glScreenWidth = atoi(ArgNext());
        glScreenHeight = atoi(ArgNext());
    }
    if(ArgCheckWith("-bpp", 1))
        glScreenBits = atoi(ArgNext());

    gl.Init(glScreenWidth, glScreenHeight, glScreenBits, !ArgExists("-window"));

    // Initialize the renderer into a 2D state.
    GL_Init2DState();

    // Initialize font renderer.
    GL_InitFont();

    // Set the gamma in accordance with vid-gamma, vid-bright and
    // vid-contrast.
    GL_SetGamma();

    // Check the maximum texture size.
    gl.GetIntegerv(DGL_MAX_TEXTURE_SIZE, &glMaxTexSize);
    if(glMaxTexSize == 256)
    {
        Con_Message("  Using restricted texture w/h ratio (1:8).\n");
        ratioLimit = 8;
        if(glScreenBits == 32)
        {
            Con_Message("  Warning: Are you sure your video card accelerates"
                        " a 32 bit mode?\n");
        }
    }
    // Set a custom maximum size?
    if(ArgCheckWith("-maxtex", 1))
    {
        int     customSize = CeilPow2(strtol(ArgNext(), 0, 0));

        if(glMaxTexSize < customSize)
            customSize = glMaxTexSize;
        glMaxTexSize = customSize;
        Con_Message("  Using maximum texture size of %i x %i.\n", glMaxTexSize,
                    glMaxTexSize);
    }
    if(ArgCheck("-outlines"))
    {
        filloutlines = false;
        Con_Message("  Textures have outlines.\n");
    }

    // Does the graphics library support multitexturing?
    numTexUnits = gl.GetInteger(DGL_MAX_TEXTURE_UNITS);
    envModAdd = gl.GetInteger(DGL_MODULATE_ADD_COMBINE);
    if(numTexUnits > 1)
    {
        Con_Printf("  Multitexturing enabled (%s).\n",
                    envModAdd ? "full" : "partial");
    }
    else
    {
        // Can't use multitexturing...
        Con_Printf("  Multitexturing not available.\n");
    }

    initGLOk = true;
}

/**
 * Initializes the graphics library for refresh. Also called at update.
 * Loadmaps can be loaded after all definitions have been read.
 */
void GL_InitRefresh(boolean loadLightMaps, boolean loadFlares)
{
    GL_InitTextureManager();
    GL_LoadSystemTextures(loadLightMaps, loadFlares);
}

/**
 * Called once at final shutdown.
 */
void GL_ShutdownRefresh(void)
{
    GL_ShutdownTextureManager();
    GL_DestroySkinNames();
}

/**
 * Kills the graphics library for good.
 */
void GL_Shutdown(void)
{
    if(!initGLOk)
        return;                 // Not yet initialized fully.

    GL_ShutdownFont();
    Rend_ShutdownSky();
    Rend_Reset();
    GL_ShutdownRefresh();

    // Shutdown DGL.
    gl.Shutdown();

    // Restore original gamma.
    if(!ArgExists("-leaveramp"))
    {
        GL_SetGammaRamp(original_gamma_ramp);
    }

    initGLOk = false;
}

/**
 * Initializes the renderer to 2D state.
 */
void GL_Init2DState(void)
{
    // The variables.
    glNearClip = 5;
    glFarClip = 16500;

    // Here we configure the OpenGL state and set the projection matrix.
    gl.Disable(DGL_CULL_FACE);
    gl.Disable(DGL_DEPTH_TEST);
    gl.Enable(DGL_TEXTURING);

    // The projection matrix.
    gl.MatrixMode(DGL_PROJECTION);
    gl.LoadIdentity();
    gl.Ortho(0, 0, 320, 200, -1, 1);

    // Default state for the white fog is off.
    usingFog = false;
    gl.Disable(DGL_FOG);
    gl.Fog(DGL_FOG_MODE, (fogModeDefault == 0 ? DGL_LINEAR :
                          fogModeDefault == 1 ? DGL_EXP :
                          DGL_EXP2));
    gl.Fog(DGL_FOG_END, 2100);  // This should be tweaked a bit.
    fogColor[0] = fogColor[1] = fogColor[2] = 138;
    fogColor[3] = 255;
    gl.Fogv(DGL_FOG_COLOR, fogColor);

    /*glEnable(GL_POLYGON_SMOOTH);
       glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST); */
}

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

    viewpx = viewwindowx * glScreenWidth / 320, viewpy =
        viewwindowy * glScreenHeight / 200;
    // Set the viewport.
    if(viewheight != SCREENHEIGHT)
    {
        viewpw = viewwidth * glScreenWidth / 320;
        viewph = viewheight * glScreenHeight / 200 + 1;
        gl.Viewport(viewpx, viewpy, viewpw, viewph);
    }
    else
    {
        viewpw = glScreenWidth;
        viewph = glScreenHeight;
    }

    // The 3D projection matrix.
    GL_ProjectionMatrix();
}

void GL_Restore2DState(int step)
{
    switch (step)
    {
    case 1:
        // After Restore Step 1 normal player sprites are rendered.
        gl.MatrixMode(DGL_PROJECTION);
        gl.LoadIdentity();
        gl.Ortho(0, 0, 320, (320 * viewheight) / viewwidth, -1, 1);
        gl.MatrixMode(DGL_MODELVIEW);
        gl.LoadIdentity();

        // Depth testing must be disabled so that psprite 1 will be drawn
        // on top of psprite 0 (Doom plasma rifle fire).
        gl.Disable(DGL_DEPTH_TEST);
        break;

    case 2:
        // After Restore Step 2 nothing special happens.
        gl.Viewport(0, 0, glScreenWidth, glScreenHeight);
        break;

    case 3:
        // After Restore Step 3 we're back in 2D rendering mode.
        gl.MatrixMode(DGL_PROJECTION);
        gl.PopMatrix();
        gl.MatrixMode(DGL_MODELVIEW);
        gl.PopMatrix();
        gl.Disable(DGL_CULL_FACE);
        gl.Disable(DGL_DEPTH_TEST);
        break;
    }
}

void GL_ProjectionMatrix(void)
{
    // We're assuming pixels are squares.
    float   aspect = viewpw / (float) viewph;

    gl.MatrixMode(DGL_PROJECTION);
    gl.LoadIdentity();
    gl.Perspective(yfov = fieldOfView / aspect, aspect, glNearClip, glFarClip);

    // We'd like to have a left-handed coordinate system.
    gl.Scalef(1, 1, -1);
}

void GL_UseFog(int yes)
{
    if(isDedicated)
    {
        // The dedicated server does not care about fog.
        return;
    }

    if(!usingFog && yes)
    {
        // Fog is turned on.
        usingFog = true;
        gl.Enable(DGL_FOG);
    }
    else if(usingFog && !yes)
    {
        // Fog must be turned off.
        usingFog = false;
        gl.Disable(DGL_FOG);
    }
    // Otherwise we won't do a thing.
}

/**
 * This needs to be called twice: first shutdown, then restore.
 * GL is reset back to the state it was right after initialization.
 * Lightmaps and flares can be loaded after defs have been loaded
 * (during restore).
 */
void GL_TotalReset(boolean doShutdown, boolean loadLightMaps,
                   boolean loadFlares)
{
    static char oldFontName[256];
    static boolean hadFog;
    static boolean wasStartup;

    if(isDedicated)
        return;

    if(doShutdown)
    {
        hadFog = usingFog;
        wasStartup = startupScreen;

        // Remember the name of the font.
        if(wasStartup)
            Con_StartupDone();
        else
            strcpy(oldFontName, FR_GetFont(FR_GetCurrent())->name);

        // Delete all textures.
        GL_ShutdownTextureManager();
        GL_ShutdownFont();
    }
    else
    {
        // Getting back up and running.
        GL_InitFont();

        // Go back to startup mode, if that's where we were.
        if(wasStartup)
        {
            Con_StartupInit();
        }
        else
        {
            // Restore the old font.
            //Con_Executef(CMDS_DDAY, true, "font name %s", oldFontName, false);
            GL_Init2DState();
        }
        GL_InitRefresh(loadLightMaps, loadFlares);

        // Restore map's fog settings.
        R_SetupFog();

        // Make sure the fog is enabled, if necessary.
        if(hadFog)
            GL_UseFog(true);
    }
}

/**
 * Changes the resolution to the specified one.
 * The change is carried out by SHUTTING DOWN the rendering DLL and then
 * restarting it. All textures will be lost in the process.
 * Restarting the renderer is the compatible way to do the change.
 */
int GL_ChangeResolution(int w, int h, int bits)
{
    if(novideo)
        return false;
    if(glScreenWidth == w && glScreenHeight == h && (!bits || glScreenBits == bits))
        return true;

    // Shut everything down, but remember our settings.
    GL_TotalReset(true, 0, 0);
    gx.UpdateState(DD_RENDER_RESTART_PRE);

    glScreenWidth = w;
    glScreenHeight = h;
    glScreenBits = bits;

    // Shutdown and re-initialize DGL.
    gl.Shutdown();
    gl.Init(w, h, bits, !nofullscreen);

    // Re-initialize.
    GL_TotalReset(false, true, true);
    gx.UpdateState(DD_RENDER_RESTART_POST);

    Con_Message("Display mode: %i x %i", glScreenWidth, glScreenHeight);
    if(glScreenBits)
        Con_Message(" x %i", glScreenBits);
    Con_Message(".\n");
    return true;
}

/**
 * Copies the current contents of the frame buffer and returns a pointer
 * to data containing 24-bit RGB triplets. The caller must free the
 * returned buffer using free()!
 */
unsigned char *GL_GrabScreen(void)
{
    unsigned char *buffer = malloc(glScreenWidth * glScreenHeight * 3);

    gl.Grab(0, 0, glScreenWidth, glScreenHeight, DGL_RGB, buffer);
    return buffer;
}

/**
 * Set the GL blending mode.
 */
void GL_BlendMode(blendmode_t mode)
{
    switch (mode)
    {
    case BM_ZEROALPHA:
        gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
        gl.Func(DGL_BLENDING, DGL_ONE, DGL_ZERO);
        break;

    case BM_ADD:
        gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
        break;

    case BM_DARK:
        gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
        gl.Func(DGL_BLENDING, DGL_DST_COLOR, DGL_ONE_MINUS_SRC_ALPHA);
        break;

    case BM_SUBTRACT:
        gl.Func(DGL_BLENDING_OP, DGL_SUBTRACT, 0);
        gl.Func(DGL_BLENDING, DGL_ONE, DGL_SRC_ALPHA);
        break;

    case BM_ALPHA_SUBTRACT:
        gl.Func(DGL_BLENDING_OP, DGL_SUBTRACT, 0);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
        break;

    case BM_REVERSE_SUBTRACT:
        gl.Func(DGL_BLENDING_OP, DGL_REVERSE_SUBTRACT, 0);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE);
        break;

    case BM_MUL:
        gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
        gl.Func(DGL_BLENDING, DGL_ZERO, DGL_SRC_COLOR);
        break;

    case BM_INVERSE_MUL:
        gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
        gl.Func(DGL_BLENDING, DGL_ZERO, DGL_ONE_MINUS_SRC_COLOR);
        break;

    default:
        gl.Func(DGL_BLENDING_OP, DGL_ADD, 0);
        gl.Func(DGL_BLENDING, DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
        break;
    }
}

/**
 * Change graphics mode resolution.
 */
D_CMD(SetRes)
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

D_CMD(UpdateGammaRamp)
{
    if(isDedicated)
    {
        Con_Printf("Impossible in dedicated mode.\n");
        return false;
    }

    GL_SetGamma();
    Con_Printf("Gamma ramp set.\n");
    return true;
}

D_CMD(SetGamma)
{
    int     newlevel;

    if(isDedicated)
    {
        Con_Printf("Impossible in dedicated mode.\n");
        return false;
    }

    if(argc != 2)
    {
        Con_Printf("Usage: %s (0-4)\n", argv[0]);
        return true;
    }
    newlevel = strtol(argv[1], NULL, 0);
    // Clamp it to the min and max.
    if(newlevel < 0)
        newlevel = 0;
    if(newlevel > 4)
        newlevel = 4;
    // Only reload textures if it's necessary.
    if(newlevel != usegamma)
    {
        usegamma = newlevel;
        GL_UpdateGamma();
        Con_Printf("Gamma correction set to level %d.\n", usegamma);
    }
    else
        Con_Printf("Gamma correction already set to %d.\n", usegamma);
    return true;
}
