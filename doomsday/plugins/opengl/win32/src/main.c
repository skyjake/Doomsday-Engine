/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2005-2007 Daniel Swanson <danij@dengine.net>
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
 * main.c: DGL Driver for OpenGL
 *
 * Init and shutdown, state management.
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

/**\todo The *NIX/SDL and Windows/GDI OpenGL Routines really need to be
 * merged into a combined *NIX-Windows/SDL based system. We have far too
 * much duplication, obvious changes on the *NIX side, never propogated to
 * the Windows side. I'd use the *NIX/SDL files as the base for the new
 * combined OpenGL system - Yagian
 */

// HEADER FILES ------------------------------------------------------------

#include "drOpenGL.h"

#include <stdlib.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

HWND    windowHandle;
HGLRC   glContext;

// The State.
int     screenWidth, screenHeight;
int     palExtAvailable, sharedPalExtAvailable;
int     maxTexSize;
float   maxAniso = 1;
int     maxTexUnits;
int     useAnisotropic;
int     useVSync;
int     verbose;
boolean wireframeMode;
boolean allowCompression;
boolean noArrays;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstTimeInit = true;
static int screenBits, windowed;

// CODE --------------------------------------------------------------------

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
static int fullscreenMode(int width, int height, int bpp)
{
    int         res, i;
    DEVMODE     current, testMode, newMode;

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
        Con_Message("drOpenGL.setResolution: Error %x.\n", res);
        return 0; // Failed, damn you.
    }

    // Set the correct window style and size.
    SetWindowLong(windowHandle, GWL_STYLE,
                  WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    SetWindowPos(windowHandle, 0, 0, 0, width, height, SWP_NOZORDER);

    // Update the screen size variables.
    screenWidth = width;
    screenHeight = height;
    if(bpp)
        screenBits = bpp;

    return 1; // Success.
}

/**
 * Only adjusts the window style and size.
 *
 * @param width         Width of the window.
 * @param height        Height of the window.
 */
static void windowedMode(int width, int height)
{
    // We need to have a large enough client area.
    RECT        rect;
    int         xoff = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int         yoff = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    LONG        style;

    if(ArgCheck("-nocenter"))
        xoff = yoff = 0;
    if(ArgCheckWith("-xpos", 1))
        xoff = atoi(ArgNext());
    if(ArgCheckWith("-ypos", 1))
        yoff = atoi(ArgNext());

    rect.left = xoff;
    rect.top = yoff;
    rect.right = xoff + width;
    rect.bottom = yoff + height;

    // Set window style.
    style = GetWindowLong(windowHandle, GWL_STYLE) |
        WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE | WS_CAPTION |
        WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    SetWindowLong(windowHandle, GWL_STYLE, style);
    AdjustWindowRect(&rect, style, FALSE);
    SetWindowPos(windowHandle, 0, xoff, yoff, /*rect.left, rect.top, */
                 rect.right - rect.left, rect.bottom - rect.top,
                 SWP_NOZORDER);

    screenWidth = width;
    screenHeight = height;
}

/**
 * Attempt to create a context for GL rendering. 
 *
 * @return              <code>DGL_TRUE</code>= success.
 */
static int initOpenGL(void)
{
    HDC         hdc = GetDC(windowHandle);
    int         state = DGL_FALSE;

    // Create the OpenGL rendering context.
    if(!(glContext = wglCreateContext(hdc)))
    {
        int     res = GetLastError();
        Con_Message("drOpenGL.initOpenGL: Creation of rendering context "
                    "failed. Error %d.\n", res);
    }
    else if(!wglMakeCurrent(hdc, glContext)) // Make the context current.
    {
        Con_Message("drOpenGL.initOpenGL: Couldn't make the rendering "
                    "context current.\n");
    }
    else // Success!
    {
        state = DGL_TRUE;
    }

    ReleaseDC(windowHandle, hdc);

    // If successful we can get on with initializing the OGL state.
    if(state)
        initState();

    return state;
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
 * Called after the plugin has been loaded.
 */
int DG_Init(void)
{
    // Nothing to do.
    return DGL_TRUE;
}

/**
 * Attempt to acquire a device context for OGL rendering and then init. 
 *
 * @param width         Width of the OGL window.
 * @param height        Height of the OGL window.
 * @param bpp           0= the current display color depth is used.
 * @param mode          Either DGL_MODE_WINDOW or DGL_MODE_FULLSCREEN.
 *
 * @return              <code>DGL_OK</code>= success.
 */
int DG_CreateContext(int width, int height, int bpp, int mode)
{
    HDC         hDC;
    PIXELFORMATDESCRIPTOR pfd;
    boolean     fullscreen = (mode == DGL_MODE_FULLSCREEN);
    char       *token, *extbuf;
    int         res, pixForm;
    int         deskbpp;
    boolean     ok = DGL_OK;

    Con_Message("DG_Init: OpenGL.\n");

    deskbpp = DG_GetDesktopBPP();

    // Setup the pixel format descriptor.
    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.iLayerType = PFD_MAIN_PLANE;
#ifndef DRMESA
    pfd.dwFlags =
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 32;
#else /* Double Buffer, no alpha */
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL |
        PFD_GENERIC_FORMAT | PFD_DOUBLEBUFFER | PFD_SWAP_COPY;
    pfd.cColorBits = 24;
    pfd.cRedBits = 8;
    pfd.cGreenBits = 8;
    pfd.cGreenShift = 8;
    pfd.cBlueBits = 8;
    pfd.cBlueShift = 16;
    pfd.cDepthBits = 16;
    pfd.cStencilBits = 8;
#endif

    if(ok)
    {
        // Acquire a device context handle.
        hDC = GetDC(windowHandle);
        if(!hDC)
        {
            Sys_CriticalMessage("drOpenGL.Init: Failed acquiring device context handle.");
            hDC = NULL;
            ok = DGL_FALSE;
        }
        else // Initialize.
        {
            // Nothing to do.
        }
    }

    if(ok)
    {   // Request a matching (or similar) pixel format.
        pixForm = ChoosePixelFormat(hDC, &pfd);
        if(!pixForm)
        {
            Sys_CriticalMessage("drOpenGL.Init: Choosing of pixel format failed.");
            pixForm = -1;
            ok = DGL_FALSE;
        }
    }

    if(ok)
    {   // Make sure that the driver is hardware-accelerated.
        DescribePixelFormat(hDC, pixForm, sizeof(pfd), &pfd);
        if((pfd.dwFlags & PFD_GENERIC_FORMAT) && !ArgCheck("-allowsoftware"))
        {
            Sys_CriticalMessage("drOpenGL.Init: OpenGL driver not accelerated!\n"
                                "Use the -allowsoftware option to bypass this.");
            ok = DGL_FALSE;
        }
    }

    if(ok)
    {   // Set the pixel format for the device context. Can only be done once
        // (unless we release the context and acquire another).
        if(!SetPixelFormat(hDC, pixForm, &pfd))
        {
           res = GetLastError();
           Con_Printf("Warning: Setting of pixel format failed. Error %d.\n",
                      res);
        }
    }

    // We've now finished with the device context.
    if(hDC)
        ReleaseDC(windowHandle, hDC);

    // Are we in range here?
    if(!fullscreen)
    {
        if(width > GetSystemMetrics(SM_CXSCREEN))
            width = GetSystemMetrics(SM_CXSCREEN);

        if(height > GetSystemMetrics(SM_CYSCREEN))
            height = GetSystemMetrics(SM_CYSCREEN);
    }

    screenWidth = width;
    screenHeight = height;
    screenBits = deskbpp;
    windowed = !fullscreen;

    allowCompression = true;
    verbose = ArgExists("-verbose");

    if(fullscreen)
    {
        if(!fullscreenMode(screenWidth, screenHeight, bpp))
        {
            Sys_CriticalMessage("drOpenGL.Init: Resolution change failed (%d x %d).\n",
                                screenWidth, screenHeight);
            ok = DGL_FALSE;
        }
    }
    else
    {
        windowedMode(screenWidth, screenHeight);
    }

    if(ok && !initOpenGL())
    {
        Sys_CriticalMessage("drOpenGL.Init: OpenGL init failed.");
        ok = DGL_FALSE;
    }

    if(ok)
    {
        // Clear the buffers.
        DG_Clear(DGL_COLOR_BUFFER_BIT | DGL_DEPTH_BUFFER_BIT);

        token = (char *) glGetString(GL_EXTENSIONS);
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

            glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTexUnits);
            Con_Message("  Found Texture units: %i\n", maxTexUnits);
#ifndef USE_MULTITEXTURE
            maxTexUnits = 1;
#endif
            // But sir, we are simple people; two units is enough.
            if(maxTexUnits > 2)
                maxTexUnits = 2;
            Con_Message("  Utilised Texture units: %i\n", maxTexUnits);

            Con_Message("  Maximum texture size: %i\n", maxTexSize);
            if(extAniso)
            {
                glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
                Con_Message("  Maximum anisotropy: %g\n", maxAniso);
            }
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
    }

    return ok;
}

/**
 * Releases the OGL context and restores any changed environment settings.
 */
void DG_Shutdown(void)
{
    // Delete the rendering context.
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(glContext);

    // Go back to normal display settings.
    ChangeDisplaySettings(0, 0);
}

/**
 * Make the content of the framebuffer visible.
 */
void DG_Show(void)
{
    HDC     hdc = GetDC(windowHandle);

    // Swap buffers.
    glFlush();
    SwapBuffers(hdc);
    ReleaseDC(windowHandle, hdc);

    if(wireframeMode)
    {
        // When rendering is wireframe mode, we must clear the screen
        // before rendering a frame.
        DG_Clear(DGL_COLOR_BUFFER_BIT);
    }
}
