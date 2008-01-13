/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2008 Daniel Swanson <danij@dengine.net>
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

/**
 * dgl_main.c: DGL Driver for OpenGL (Win32 specific)
 *
 * Init and shutdown, state management.
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

/**
 * \todo The *NIX/SDL and Windows/GDI OpenGL Routines really need to be
 * merged into a combined *NIX-Windows/SDL based system. We have far too
 * much duplication, obvious changes on the *NIX side, never propogated to
 * the Windows side. I'd use the *NIX/SDL files as the base for the new
 * combined OpenGL system - Yagian
 */

// HEADER FILES ------------------------------------------------------------

#include <stdlib.h>

#include "de_base.h"
#include "de_dgl.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

HGLRC   glContext;

dgl_state_t DGL_state;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean initedGL = false;
static boolean firstTimeInit = true;

// CODE --------------------------------------------------------------------

/**
 * Determine the desktop BPP.
 *
 * @return              BPP in use for the desktop.
 */
int DG_GetDesktopBPP(void)
{
    HWND        hDesktop = GetDesktopWindow();
    HDC         desktop_hdc = GetDC(hDesktop);
    int         deskbpp = GetDeviceCaps(desktop_hdc, PLANES) *
                            GetDeviceCaps(desktop_hdc, BITSPIXEL);
    ReleaseDC(hDesktop, desktop_hdc);
    return deskbpp;
}

/**
 * Change the display mode using the Win32 API, the closest available
 * refresh rate is selected.
 *
 * @param width         Requested horizontal resolution.
 * @param height        Requested vertical resolution.
 * @param bpp           Requested bits per pixel.
 *
 * @return              Non-zero= success.
 */
int DGL_ChangeVideoMode(int width, int height, int bpp)
{
    int         res, i;
    DEVMODE     current, testMode, newMode;

    DGL_state.screenBits = DG_GetDesktopBPP();

    // First get the current settings.
    memset(&current, 0, sizeof(current));
    current.dmSize = sizeof(DEVMODE);
    if(EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &current))
    {
        if(!bpp)
            bpp = current.dmBitsPerPel;
    }
    else if(!bpp)
    {   // A safe fallback.
        bpp = 16;
    }

    if(width == current.dmPelsWidth && height == current.dmPelsHeight &&
       bpp == current.dmBitsPerPel)
       return 1; // No need to change, so success!

    // Override refresh rate?
    if(ArgCheckWith("-refresh", 1))
        current.dmDisplayFrequency = strtol(ArgNext(), 0, 0);

    // Clear the structure.
    memset(&newMode, 0, sizeof(newMode));
    newMode.dmSize = sizeof(newMode);

    // Let's enumerate all possible modes to find the most suitable one.
    for(i = 0;; i++)
    {
        memset(&testMode, 0, sizeof(testMode));
        testMode.dmSize = sizeof(DEVMODE);
        if(!EnumDisplaySettings(NULL, i, &testMode))
            break;

        if(testMode.dmPelsWidth == (unsigned) width &&
           testMode.dmPelsHeight == (unsigned) height &&
           testMode.dmBitsPerPel == (unsigned) bpp)
        {
            // This looks promising. We'll take the one that best matches
            // the current refresh rate.
            if(abs(current.dmDisplayFrequency - testMode.dmDisplayFrequency) <
               abs(current.dmDisplayFrequency - newMode.dmDisplayFrequency))
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

    if((res = ChangeDisplaySettings(&newMode, 0)) != DISP_CHANGE_SUCCESSFUL)
    {
        Con_Message("DGL_ChangeVideoMode: Error %x.\n", res);
        return 0; // Failed, damn you.
    }

    // Update the screen size variables.
    DGL_state.screenWidth = width;
    DGL_state.screenHeight = height;
    if(bpp)
        DGL_state.screenBits = bpp;

    return 1; // Success.
}

/**
 * Set the currently active GL texture by name.
 *
 * @param texture       GL texture name to make active.
 */
void activeTexture(const GLenum texture)
{
    if(!glActiveTextureARB)
        return;
    glActiveTextureARB(texture);
}

static void checkExtensions(void)
{
    char       *token, *extbuf;

    if(!firstTimeInit)
        return;

    firstTimeInit = false;

    token = (char *) glGetString(GL_EXTENSIONS);
    extbuf = malloc(strlen(token) + 1);
    strcpy(extbuf, token);

    // Check the maximum texture size.
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &DGL_state.maxTexSize);

    DGL_InitExtensions();

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
        Con_Message("      ");  // Indent.
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
            if(token)
                Con_Message(" %-30.30s", token);
            Con_Message("\n");
        }
        token = strtok(NULL, " ");
    }
    Con_Message("  GLU Version: %s\n", gluGetString(GLU_VERSION));

    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &((GLint) DGL_state.maxTexUnits));
    Con_Message("  Found Texture units: %i\n", DGL_state.maxTexUnits);
#ifndef USE_MULTITEXTURE
    DGL_state.maxTexUnits = 1;
#endif
    // But sir, we are simple people; two units is enough.
    if(DGL_state.maxTexUnits > 2)
        DGL_state.maxTexUnits = 2;
    Con_Message("  Utilised Texture units: %i\n", DGL_state.maxTexUnits);

    Con_Message("  Maximum texture size: %i\n", DGL_state.maxTexSize);
    if(DGL_state_ext.extAniso)
    {
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &DGL_state.maxAniso);
        Con_Message("  Maximum anisotropy: %i\n", DGL_state.maxAniso);
    }
    free(extbuf);

    // Decide whether vertex arrays should be done manually or with real
    // OpenGL calls.
    InitArrays();

    DGL_state_texture.dumpTextures =
        (ArgCheck("-dumptextures")? true : false);

    if(DGL_state_texture.dumpTextures)
        Con_Message("  Dumping textures (mipmap level zero).\n");

    DGL_state.useAnisotropic =
        ((DGL_state_ext.extAniso && !ArgExists("-noanifilter"))? true : false);

    if(DGL_state.useAnisotropic)
        Con_Message("  Using anisotropic texture filtering.\n");

    DGL_state.forceFinishBeforeSwap =
        (ArgExists("-glfinish")? true : false);

    if(DGL_state.forceFinishBeforeSwap)
        Con_Message("  glFinish() forced before swapping buffers.\n");
}

/**
 * Attempt to acquire a device context for OGL rendering and then init.
 *
 * @param width         Width of the OGL window.
 * @param height        Height of the OGL window.
 * @param bpp           0= the current display color depth is used.
 * @param windowed      @c true = windowed mode ELSE fullscreen.
 * @param data          Ptr to system-specific data, e.g a window handle or
 *                      similar.
 *
 * @return              @c true iff successful.
 */
boolean DGL_CreateContext(int width, int height, int bpp, boolean windowed,
                          void *data)
{
    HWND        hWnd = (HWND) data;
    HDC         hdc;
    boolean     fullscreen = !windowed;
    boolean     ok = true;

    Con_Message("DGL_CreateContext: OpenGL.\n");

    hdc = GetDC(hWnd);
    if(!hdc)
    {
        Sys_CriticalMessage("DGL_CreateContext: Failed acquiring device.");
        ok = false;
    }

    DGL_state.screenWidth = width;
    DGL_state.screenHeight = height;
    DGL_state.screenBits = bpp;
    DGL_state.windowed = !fullscreen;

    DGL_state.allowCompression = true;

    // Create the OpenGL rendering context.
    if(!(glContext = wglCreateContext(hdc)))
    {
        Sys_CriticalMessage("DGL_CreateContext: Creation of rendering context "
                            "failed.");
        ok = false;
    }
    else if(!wglMakeCurrent(hdc, glContext)) // Make the context current.
    {
        Sys_CriticalMessage("DGL_CreateContext: Couldn't make the rendering "
                            "context current.");
        ok = false;
    }

    if(hdc)
        ReleaseDC(hWnd, hdc);

    if(ok)
    {
        checkExtensions();

        // We can get on with initializing the OGL state.
        initState();
    }

    return ok;
}

void DGL_DestroyContext(void)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(glContext);
}

/**
 * Initializes DGL.
 */
boolean DGL_Init(void)
{
    if(isDedicated)
        return true;

    if(initedGL)
        return true; // Already inited.

    initedGL = true;
    return true;
}

/**
 * Releases the OGL context and restores any changed environment settings.
 */
void DGL_Shutdown(void)
{
    if(!initedGL)
        return;

    // Delete the rendering context.
    if(glContext)
        DGL_DestroyContext();

    // Go back to normal display settings.
    ChangeDisplaySettings(0, 0);
}

/**
 * Make the content of the framebuffer visible.
 */
void DGL_Show(void)
{
    HWND    hWnd = (HWND) DD_GetVariable(DD_WINDOW_HANDLE);
    HDC     hdc = GetDC(hWnd);

    if(DGL_state.forceFinishBeforeSwap)
    {
        glFinish();
    }

    // Swap buffers.
    glFlush();
    SwapBuffers(hdc);
    ReleaseDC(hWnd, hdc);
}
