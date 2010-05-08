/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2010 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2010 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *\author Copyright © 2003 Grégory Smialek <texel@fr.fm>
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

/**
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
#include "de_ui.h"
#include "de_defs.h"

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

#ifdef TextOut
// Windows has its own TextOut.
#  undef TextOut
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

typedef unsigned short gramp_t[3 * 256];

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

D_CMD(Fog);
D_CMD(SetBPP);
D_CMD(SetRes);
D_CMD(ToggleFullscreen);

void    GL_SetGamma(void);

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

extern int maxnumnodes;
extern boolean fillOutlines;

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// The default resolution (config file).
int     defResX = 640, defResY = 480, defBPP = 32;
int     defFullscreen = true;
int     numTexUnits;
boolean envModAdd;              // TexEnv: modulate and add is available.
int     test3dfx = 0;
int     r_framecounter;         // Used only for statistics.
int     r_detail = true;        // Render detail textures (if available).

float   vid_gamma = 1.0f, vid_bright = 0, vid_contrast = 1.0f;
int     glFontFixed, glFontVariable[NUM_GLFS];

float   glNearClip;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initGLOk = false;
static boolean varFontInited = false;

static gramp_t original_gamma_ramp;
static boolean gamma_support = false;
static float oldgamma, oldcontrast, oldbright;
static int fogModeDefault = 0;

static viewport_t currentView;

// CODE --------------------------------------------------------------------

void GL_Register(void)
{
    // Cvars
    C_VAR_INT("rend-dev-wireframe", &renderWireframe, 0, 0, 1);
    C_VAR_INT("rend-fog-default", &fogModeDefault, 0, 0, 2);
    // * Render-HUD
    C_VAR_FLOAT("rend-hud-offset-scale", &weaponOffsetScale, CVF_NO_MAX,
                0, 0);
    C_VAR_FLOAT("rend-hud-fov-shift", &weaponFOVShift, CVF_NO_MAX, 0, 1);
    C_VAR_BYTE("rend-hud-nostretch", &weaponNoStretch, 0, 0, 1);

    // * Render-Mobj
    C_VAR_INT("rend-mobj-smooth-move", &useSRVO, 0, 0, 2);
    C_VAR_INT("rend-mobj-smooth-turn", &useSRVOAngle, 0, 0, 1);

    // * video
    C_VAR_INT("vid-res-x", &defResX, CVF_NO_MAX, 320, 0);
    C_VAR_INT("vid-res-y", &defResY, CVF_NO_MAX, 240, 0);
    C_VAR_INT("vid-bpp", &defBPP, 0, 16, 32);
    C_VAR_INT("vid-fullscreen", &defFullscreen, 0, 0, 1);
    C_VAR_FLOAT("vid-gamma", &vid_gamma, 0, 0.1f, 6);
    C_VAR_FLOAT("vid-contrast", &vid_contrast, 0, 0, 10);
    C_VAR_FLOAT("vid-bright", &vid_bright, 0, -2, 2);

    // Ccmds
    C_CMD_FLAGS("fog", NULL, Fog, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("setbpp", "i", SetBPP, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("setres", "ii", SetRes, CMDF_NO_DEDICATED);
    C_CMD_FLAGS("setvidramp", "", UpdateGammaRamp, CMDF_NO_DEDICATED);
    C_CMD("togglefullscreen", "", ToggleFullscreen);

    GL_TexRegister();
}

boolean GL_IsInited(void)
{
    return initGLOk;
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

    // Blit screen to video.
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    Sys_UpdateWindow(windowIDX);
    if(renderWireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // Increment frame counter.
    r_framecounter++;
}

/**
 * On Win32, use the gamma ramp functions in the Win32 API. On Linux, use
 * the XFree86-VidMode extension.
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
        HWND    hWnd = Sys_GetWindowHandle(windowIDX);
        HDC     hDC;

        if(!hWnd)
        {
            suspendMsgPump = true;
            MessageBox(HWND_DESKTOP,
                       "GL_GetGammaRamp: Main window not available.", NULL,
                       MB_ICONERROR | MB_OK);
            suspendMsgPump = false;
        }
        else
        {
            hDC = GetDC(hWnd);

            if(!hDC)
            {
                Con_Message("GL_GetGammaRamp: Failed getting device context.");
                gamma_support = false;
            }
            else
            {
                gamma_support = false;
                if(GetDeviceGammaRamp(hDC, (void*) ramp))
                {
                    gamma_support = true;
                }
                ReleaseDC(hWnd, hDC);
            }
        }
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
        HWND        hWnd = Sys_GetWindowHandle(windowIDX);

        if(!hWnd)
        {
            suspendMsgPump = true;
            MessageBox(HWND_DESKTOP,
                       "GL_SetGammaRamp: Main window not available.", NULL,
                       MB_ICONERROR | MB_OK);
            suspendMsgPump = false;
        }
        else
        {
            HDC hDC = GetDC(hWnd);

            if(!hDC)
            {
                Con_Message("GL_SetGammaRamp: Failed getting device context.");
                gamma_support = false;
            }
            else
            {
                SetDeviceGammaRamp(hDC, (void*) ramp);
                ReleaseDC(hWnd, hDC);
            }
        }
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
 * Calculate a gamma ramp and write the result to the location pointed to.
 *
 * PRE: 'ramp' must point to a ushort[256] area of memory.
 *
 * @param ramp          Ptr to the ramp table to write to.
 * @param gamma         Non-linear factor (curvature; >1.0 multiplies).
 * @param contrast      Steepness.
 * @param bright        Brightness, uniform offset.
 */
void GL_MakeGammaRamp(unsigned short *ramp, float gamma, float contrast,
                      float bright)
{
    int         i;
    double      ideal[256]; // After processing clamped to unsigned short.
    double      norm;

    // Don't allow stupid values.
    if(contrast < 0.1f)
        contrast = 0.1f;
    if(bright > 0.8f)
        bright = 0.8f;
    if(bright < -0.8f)
        bright = -0.8f;

    // Init the ramp as a line with the steepness defined by contrast.
    for(i = 0; i < 256; ++i)
        ideal[i] = i * contrast - (contrast - 1) * 127;

    // Apply the gamma curve.
    if(gamma != 1)
    {
        if(gamma <= 0.1f)
            gamma = 0.1f;
        norm = pow(255, 1 / gamma - 1); // Normalizing factor.
        for(i = 0; i < 256; ++i)
            ideal[i] = pow(ideal[i], 1 / gamma) / norm;
    }

    // The last step is to add the brightness offset.
    for(i = 0; i < 256; ++i)
        ideal[i] += bright * 128;

    // Clamp it and write the ramp table.
    for(i = 0; i < 256; ++i)
    {
        ideal[i] *= 0x100; // Byte => word
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
    gramp_t     myramp;

    oldgamma = vid_gamma;
    oldcontrast = vid_contrast;
    oldbright = vid_bright;

    GL_MakeGammaRamp(myramp, vid_gamma, vid_contrast, vid_bright);
    GL_SetGammaRamp(myramp);
}

const char* GL_ChooseFixedFont(void)
{
    if(theWindow->width < 300)
        return "console11";
    if(theWindow->width > 768)
        return "console18";
    return "console14";
}

const char* GL_ChooseVariableFont(glfontstyle_t style, int resX, int resY)
{
    const int SMALL_LIMIT = 500;
    const int MED_LIMIT = 800;

    switch(style)
    {
    default:
        return (resY < SMALL_LIMIT ? "normal12" :
                resY < MED_LIMIT ? "normal18" :
                "normal24");

    case GLFS_LIGHT:
        return (resY < SMALL_LIMIT ? "normallight12" :
                resY < MED_LIMIT ? "normallight18" :
                "normallight24");

    case GLFS_BOLD:
        return (resY < SMALL_LIMIT ? "normalbold12" :
                resY < MED_LIMIT ? "normalbold18" :
                "normalbold24");
    }
}

void GL_InitFont(void)
{
    FR_Init();
    FR_PrepareFont(GL_ChooseFixedFont());
    glFontFixed = FR_GetCurrent();

    Con_SetMaxLineLength();

    FR_PrepareFont(GL_ChooseVariableFont(GLFS_NORMAL, theWindow->width, theWindow->height));
    glFontVariable[GLFS_NORMAL] = FR_GetCurrent();

    FR_PrepareFont(GL_ChooseVariableFont(GLFS_BOLD, theWindow->width, theWindow->height));
    glFontVariable[GLFS_BOLD] = FR_GetCurrent();

    FR_PrepareFont(GL_ChooseVariableFont(GLFS_LIGHT, theWindow->width, theWindow->height));
    glFontVariable[GLFS_LIGHT] = FR_GetCurrent();

    FR_SetFont(glFontFixed);

    Cfont.flags = DDFONT_WHITE;
    Cfont.height = FR_SingleLineHeight("Con");
    Cfont.sizeX = 1;
    Cfont.sizeY = 1;
    Cfont.drawText = FR_ShadowTextOut;
    Cfont.getWidth = FR_TextWidth;
    Cfont.filterText = NULL;
}

void GL_ShutdownFont(void)
{
    FR_Shutdown();
    glFontFixed =
        glFontVariable[GLFS_NORMAL] =
        glFontVariable[GLFS_BOLD] =
        glFontVariable[GLFS_LIGHT] = 0;
}

#if 0
void GL_InitVarFont(void)
{
    int         oldFont;
    //int         i;

    if(novideo) //|| varFontInited)
        return;

    /*
    VERBOSE2(Con_Message("GL_InitVarFont.\n"));

    oldFont = FR_GetCurrent();
    VERBOSE2(Con_Message("GL_InitVarFont: Old font = %i.\n", oldFont));

    for(i = 0; i < NUM_GLFS; ++i)
    {
        // The bold font and light fonts are always loaded.
        if(i == GLFS_BOLD || i == GLFS_LIGHT)
            continue;

        FR_PrepareFont(GL_ChooseVariableFont(i, theWindow->width, theWindow->height));
        glFontVariable[i] = FR_GetCurrent();
        VERBOSE2(Con_Message("GL_InitVarFont: Variable font = %i.\n",
                             glFontVariable[i]));
    }

    FR_SetFont(oldFont);
    VERBOSE2(Con_Message("GL_InitVarFont: Restored old font %i.\n", oldFont));
     */
    //varFontInited = true;
}

void GL_ShutdownVarFont(void)
{
//    int         i;

    if(novideo) // || !varFontInited)
        return;

    //FR_SetFont(glFontFixed);
    /*
    for(i = 0; i < NUM_GLFS; ++i)
    {
        // Keep the bold and light fonts loaded.
        if(i == GLFS_BOLD || i == GLFS_LIGHT)
            continue;

        FR_DestroyFont(glFontVariable[i]);
        glFontVariable[i] = glFontFixed;
    }
     */
    //varFontInited = false;
}
#endif

/**
 * One-time initialization of DGL and the renderer. This is done very early
 * on during engine startup, and is supposed to be fast. All subsystems
 * cannot yet be initialized, such as fonts or texture management, so any
 * rendering occuring before GL_Init() must be done with manually prepared
 * textures.
 */
boolean GL_EarlyInit(void)
{
    if(initGLOk)
        return true; // Already initialized.

    if(novideo)
        return true;

    Con_Message("GL_Init: Initializing Doomsday Graphics Library.\n");

    // Get the original gamma ramp and check if ramps are supported.
    GL_GetGammaRamp(original_gamma_ramp);

    GL_InitDeferred();

    // Check the maximum texture size.
    if(GL_state.maxTexSize == 256)
    {
        int             bpp;

        Con_Message("  Using restricted texture w/h ratio (1:8).\n");
        ratioLimit = 8;
        Sys_GetWindowBPP(windowIDX, &bpp);
        if(bpp == 32)
        {
            Con_Message("  Warning: Are you sure your video card accelerates"
                        " a 32 bit mode?\n");
        }
    }
    // Set a custom maximum size?
    if(ArgCheckWith("-maxtex", 1))
    {
        int     customSize = M_CeilPow2(strtol(ArgNext(), 0, 0));

        if(GL_state.maxTexSize < customSize)
            customSize = GL_state.maxTexSize;
        GL_state.maxTexSize = customSize;
        Con_Message("  Using maximum texture size of %i x %i.\n",
                    GL_state.maxTexSize, GL_state.maxTexSize);
    }
    if(ArgCheck("-outlines"))
    {
        fillOutlines = false;
        Con_Message("  Textures have outlines.\n");
    }

    // Does the graphics library support multitexturing?
    numTexUnits = GL_state.maxTexUnits;
    envModAdd = (DGL_GetInteger(DGL_MODULATE_ADD_COMBINE)? true : false);
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

    // Initialize the renderer into a 2D state.
    GL_Init2DState();

    // Allow font rendering.
    FR_Init();

    // Render a few black frames before we continue. This will help to
    // stableize things before we begin drawing for real and to avoid any
    // unwanted video artefacts.
    {
    int i = 0;
    while(i++ < 3)
    {
        glClear(GL_COLOR_BUFFER_BIT);
        GL_DoUpdate();
    }
    }

    initGLOk = true;
    return true;
}

/**
 * Finishes GL initialization. This can be called once the virtual file
 * system has been fully loaded up, and the console variables have been
 * read from the config file.
 */
void GL_Init(void)
{
    if(!initGLOk)
    {
        Con_Error("GL_Init: GL_EarlyInit has not been done yet.\n");
    }

    // Initialize font renderer.
    GL_InitFont();

    // Initialize palette management.
    GL_InitPalettedTexture();

    // Set the gamma in accordance with vid-gamma, vid-bright and
    // vid-contrast.
    GL_SetGamma();

    // Initialize one viewport.
    R_SetViewGrid(1, 1);
}

/**
 * Initializes the graphics library for refresh. Also called at update.
 */
void GL_InitRefresh(void)
{
    GL_InitTextureManager();
}

/**
 * Called once at final shutdown.
 */
void GL_ShutdownRefresh(void)
{
    GL_ShutdownTextureManager();
    R_DestroySkins();
    R_DestroyDetailTextures();
    R_DestroyLightMaps();
    R_DestroyFlareTextures();
    R_DestroyShinyTextures();
    R_DestroyMaskTextures();
}

/**
 * Kills the graphics library for good.
 */
void GL_Shutdown(void)
{
    if(!initGLOk)
        return; // Not yet initialized fully.

    GL_ShutdownDeferred();
    GL_ShutdownFont();
    Rend_ShutdownSky();
    Rend_Reset();
    GL_ShutdownRefresh();

    // Shutdown OpenGL.
    Sys_ShutdownGL();

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
    glNearClip = 0.05f;

    // Here we configure the OpenGL state and set the projection matrix.
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    // The projection matrix.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 320, 200, 0, -1, 1);

    // Default state for the white fog is off.
    usingFog = false;
    glDisable(GL_FOG);
    glFogi(GL_FOG_MODE, (fogModeDefault == 0 ? GL_LINEAR :
                          fogModeDefault == 1 ? GL_EXP :
                          GL_EXP2));
    glFogf(GL_FOG_START, DEFAULT_FOG_START);
    glFogf(GL_FOG_END, DEFAULT_FOG_END);
    glFogf(GL_FOG_DENSITY, DEFAULT_FOG_DENSITY);
    fogColor[0] = DEFAULT_FOG_COLOR_RED;
    fogColor[1] = DEFAULT_FOG_COLOR_GREEN;
    fogColor[2] = DEFAULT_FOG_COLOR_BLUE;
    fogColor[3] = 1;
    glFogfv(GL_FOG_COLOR, fogColor);
}

void GL_SwitchTo3DState(boolean push_state, viewport_t* port)
{
    if(push_state)
    {
        // Push the 2D matrices on the stack.
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    memcpy(&currentView, port, sizeof(currentView));

    viewpx = port->x + MIN_OF(viewwindowx, port->width);
    viewpy = port->y + MIN_OF(viewwindowy, port->height);
    viewpw = MIN_OF(port->width, viewwidth);
    viewph = MIN_OF(port->height, viewheight);
    glViewport(viewpx, FLIP(viewpy + viewph - 1), viewpw, viewph);

    // The 3D projection matrix.
    GL_ProjectionMatrix();
}

static boolean __inline pickScalingStrategy(int viewportWidth, int viewportHeight)
{
    float a = (float)viewportWidth/viewportHeight;
    float b = (float)SCREENWIDTH/SCREENHEIGHT;

    if(INRANGE_OF(a, b, .001f))
        return true; // The same, so stretch.
    if(weaponNoStretch || !INRANGE_OF(a, b, .38f))
        return false; // No stretch; translate and scale to fit.
    // Otherwise stretch.
    return true;
}

void GL_Restore2DState(int step, viewport_t* port)
{
    switch(step)
    {
    case 1: // After Restore Step 1 normal player sprites are rendered.
        {
        int height = (float)(port->width * viewheight / viewwidth) / port->height * SCREENHEIGHT;

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        if(pickScalingStrategy(port->width, port->height))
        {
            /**
             * Use an orthographic projection in a fixed 320x200 space
             * with the height scaled to the viewport height.
             */
            glOrtho(0, SCREENWIDTH, height, 0, -1, 1);
        }
        else
        {
            /**
             * Use an orthographic projection in native screenspace. Then
             * translate and scale the projection to produce an aspect
             * corrected coordinate space at 4:3, aligned vertically to
             * the bottom and centered horizontally in the window.
             */
            glOrtho(0, port->width, port->height, 0, -1, 1);
            glTranslatef(port->width/2, port->height, 0);

            if(port->width >= port->height)
                glScalef((float)port->height/SCREENHEIGHT, (float)port->height/SCREENHEIGHT, 1);
            else
                glScalef((float)port->width/SCREENWIDTH, (float)port->width/SCREENWIDTH, 1);

            // Special case: viewport height is greater than width.
            // Apply an additional scaling factor to prevent player sprites looking too small.
            if(port->height > port->width)
            {
                float extraScale = (((float)port->height*2)/port->width) / 2;
                glScalef(extraScale, extraScale, 1);
            }

            glTranslatef(-(SCREENWIDTH/2), -SCREENHEIGHT, 0);
            glScalef(1, (float)SCREENHEIGHT/height, 1);
        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        // Depth testing must be disabled so that psprite 1 will be drawn
        // on top of psprite 0 (Doom plasma rifle fire).
        glDisable(GL_DEPTH_TEST);
        break;
        }
    case 2: // After Restore Step 2 we're back in 2D rendering mode.
        glViewport(currentView.x, FLIP(currentView.y + currentView.height - 1),
                   currentView.width, currentView.height);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        break;

    default:
        Con_Error("GL_Restore2DState: Invalid value, step = %i.", step);
        break;
    }
}

/**
 * Like gluPerspective but with a far clip plane at infinity.
 * Borrowed from texel3d by Grégory Smialek.
 */
void GL_InfinitePerspective(GLdouble fovy, GLdouble aspect, GLdouble znear)
{
#define OFFSET (1 - 1.0 / (1 << 23));

    GLdouble tangent, left, right, bottom, top;
    GLdouble m[16];

    tangent = tan(DEG2RAD(fovy/2.0));
    top = tangent * znear;
    bottom = -top;
    left = bottom * aspect;
    right = top * aspect;

    m[ 0] = (2 * znear) / (right - left);
    m[ 4] = 0;
    m[ 8] = (right + left) / (right - left);
    m[12] = 0;

    m[ 1] = 0;
    m[ 5] = (2 * znear) / (top - bottom);
    m[ 9] = (top + bottom) / (top - bottom);
    m[13] = 0;

    m[ 2] = 0;
    m[ 6] = 0;
    m[10] = -1 * OFFSET;
    m[14] = -2 * znear * OFFSET;

    m[ 3] = 0;
    m[ 7] = 0;
    m[11] = -1;
    m[15] = 0;

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMultMatrixd(m);

#undef OFFSET
}

void GL_ProjectionMatrix(void)
{
    // We're assuming pixels are squares.
    float aspect = viewpw / (float) viewph;

    yfov = 2 * RAD2DEG(atan(tan(DEG2RAD(fieldOfView) / 2) / aspect));
    GL_InfinitePerspective(yfov, aspect, glNearClip);

    // We'd like to have a left-handed coordinate system.
    glScalef(1, 1, -1);
}

void GL_UseFog(int yes)
{
    usingFog = yes;
}

/**
 * GL is reset back to the state it was right after initialization.
 * Use GL_TotalRestore to bring back online.
 */
void GL_TotalReset(void)
{
    if(isDedicated)
        return;

    //GL_ShutdownVarFont();

    // Update the secondary title and the game status.
    Con_InitUI();

    // Delete all textures.
    GL_ResetTextureManager();
    GL_ShutdownFont();
    GL_ReleaseReservedNames();

#if _DEBUG
    Z_CheckHeap();
#endif
}

/**
 * Called after a GL_TotalReset to bring GL back online.
 */
void GL_TotalRestore(void)
{
    if(isDedicated)
        return;

    // Getting back up and running.
    GL_ReserveNames();
    GL_InitFont();
    //GL_InitVarFont();
    GL_Init2DState();
    GL_InitPalettedTexture();

    {
    gamemap_t*          map = P_GetCurrentMap();
    ded_mapinfo_t*      mapInfo = Def_GetMapInfo(P_GetMapID(map));

    // Restore map's fog settings.
    if(!mapInfo || !(mapInfo->flags & MIF_FOG))
        R_SetupFogDefaults();
    else
        R_SetupFog(mapInfo->fogStart, mapInfo->fogEnd,
                   mapInfo->fogDensity, mapInfo->fogColor);
    }

#if _DEBUG
    Z_CheckHeap();
#endif
}

/**
 * Copies the current contents of the frame buffer and returns a pointer
 * to data containing 24-bit RGB triplets. The caller must free the
 * returned buffer using M_Free()!
 */
unsigned char* GL_GrabScreen(void)
{
    unsigned char*      buffer = 0;

    buffer = M_Malloc(theWindow->width * theWindow->height * 3);
    GL_Grab(0, 0, theWindow->width, theWindow->height, DGL_RGB, buffer);
    return buffer;
}

/**
 * Set the GL blending mode.
 */
void GL_BlendMode(blendmode_t mode)
{
    switch(mode)
    {
    case BM_ZEROALPHA:
        GL_BlendOp(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ZERO);
        break;

    case BM_ADD:
        GL_BlendOp(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;

    case BM_DARK:
        GL_BlendOp(GL_FUNC_ADD);
        glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
        break;

    case BM_SUBTRACT:
        GL_BlendOp(GL_FUNC_SUBTRACT);
        glBlendFunc(GL_ONE, GL_SRC_ALPHA);
        break;

    case BM_ALPHA_SUBTRACT:
        GL_BlendOp(GL_FUNC_SUBTRACT);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;

    case BM_REVERSE_SUBTRACT:
        GL_BlendOp(GL_FUNC_REVERSE_SUBTRACT);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;

    case BM_MUL:
        GL_BlendOp(GL_FUNC_ADD);
        glBlendFunc(GL_ZERO, GL_SRC_COLOR);
        break;

    case BM_INVERSE:
        GL_BlendOp(GL_FUNC_ADD);
        glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR);
        break;

    case BM_INVERSE_MUL:
        GL_BlendOp(GL_FUNC_ADD);
        glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
        break;

    default:
        GL_BlendOp(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    }
}

void GL_LowRes(void)
{
    // Set everything as low as they go.
    filterSprites = 0;
    linearRaw = 0;
    texMagMode = 0;

    // And do a texreset so everything is updated.
    GL_SetTextureParams(GL_NEAREST, true, true);
    GL_TexReset();
}

/**
 * Change graphics mode resolution.
 */
D_CMD(SetRes)
{
    int                 width = atoi(argv[1]), height = atoi(argv[2]);

    return Sys_SetWindow(windowIDX, 0, 0, width, height, 0, 0,
                         DDSW_NOVISIBLE|DDSW_NOCENTER|DDSW_NOFULLSCREEN|
                         DDSW_NOBPP);
}

D_CMD(ToggleFullscreen)
{
    boolean             fullscreen;

    if(!Sys_GetWindowFullscreen(windowIDX, &fullscreen))
        Con_Message("CCmd 'ToggleFullscreen': Failed acquiring window "
                    "fullscreen");
    else
        Sys_SetWindow(windowIDX, 0, 0, 0, 0, 0,
                      (!fullscreen? DDWF_FULLSCREEN : 0),
                      DDSW_NOCENTER|DDSW_NOSIZE|DDSW_NOBPP|DDSW_NOVISIBLE);
    return true;
}

D_CMD(UpdateGammaRamp)
{
    GL_SetGamma();
    Con_Printf("Gamma ramp set.\n");
    return true;
}

D_CMD(SetBPP)
{
    int                 bpp = atoi(argv[1]);

    if(bpp != 16 && bpp != 32)
    {
        bpp = 32;
        Con_Printf("%d not valid for bits per pixel, setting to 32.\n", bpp);
    }

    return Sys_SetWindow(windowIDX, 0, 0, 0, 0, bpp, 0,
                        DDSW_NOCENTER|DDSW_NOSIZE|DDSW_NOFULLSCREEN|
                        DDSW_NOVISIBLE);
}

D_CMD(Fog)
{
    int                 i;

    if(argc == 1)
    {
        Con_Printf("Usage: %s (cmd) (args)\n", argv[0]);
        Con_Printf("Commands: on, off, mode, color, start, end, density.\n");
        Con_Printf("Modes: linear, exp, exp2.\n");
        //Con_Printf( "Hints: fastest, nicest, dontcare.\n");
        Con_Printf("Color is given as RGB (0-255).\n");
        Con_Printf
            ("Start and end are for linear fog, density for exponential.\n");
        return true;
    }
    if(!stricmp(argv[1], "on"))
    {
        GL_UseFog(true);
        Con_Printf("Fog is now active.\n");
    }
    else if(!stricmp(argv[1], "off"))
    {
        GL_UseFog(false);
        Con_Printf("Fog is now disabled.\n");
    }
    else if(!stricmp(argv[1], "mode") && argc == 3)
    {
        if(!stricmp(argv[2], "linear"))
        {
            glFogi(GL_FOG_MODE, GL_LINEAR);
            Con_Printf("Fog mode set to linear.\n");
        }
        else if(!stricmp(argv[2], "exp"))
        {
            glFogi(GL_FOG_MODE, GL_EXP);
            Con_Printf("Fog mode set to exp.\n");
        }
        else if(!stricmp(argv[2], "exp2"))
        {
            glFogi(GL_FOG_MODE, GL_EXP2);
            Con_Printf("Fog mode set to exp2.\n");
        }
        else
            return false;
    }
    else if(!stricmp(argv[1], "color") && argc == 5)
    {
        for(i = 0; i < 3; i++)
            fogColor[i] = strtol(argv[2 + i], NULL, 0) / 255.0f;
        fogColor[3] = 1;
        glFogfv(GL_FOG_COLOR, fogColor);
        Con_Printf("Fog color set.\n");
    }
    else if(!stricmp(argv[1], "start") && argc == 3)
    {
        glFogf(GL_FOG_START, (GLfloat) strtod(argv[2], NULL));
        Con_Printf("Fog start distance set.\n");
    }
    else if(!stricmp(argv[1], "end") && argc == 3)
    {
        glFogf(GL_FOG_END, (GLfloat) strtod(argv[2], NULL));
        Con_Printf("Fog end distance set.\n");
    }
    else if(!stricmp(argv[1], "density") && argc == 3)
    {
        glFogf(GL_FOG_DENSITY, (GLfloat) strtod(argv[2], NULL));
        Con_Printf("Fog density set.\n");
    }
    else
        return false;
    // Exit with a success.
    return true;
}
