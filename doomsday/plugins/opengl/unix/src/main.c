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

//**************************************************************************
//**
//** MAIN.C (Unix/SDL)
//**
//** Target:        DGL Driver for OpenGL
//** Description:   Init and shutdown, state management (using SDL)
//**
//** Get OpenGL header files from:
//** http://oss.sgi.com/projects/ogl-sample/
//**
//**************************************************************************

/** \todo The *NIX/SDL and Windows/GDI OpenGL Routines really need to be merged
* into a combined *NIX-Windows/SDL based system. We far far too much duplication,
* it its obvious changes on the *NIX side, never propogated to the Windows side.
* I'd use the *NIX/SDL files as the base for the new combined OpenGL system - Yagisan
* \todo We need to fully utilise graphics cards - suggest a few serperate rendering
* paths, software (as a fallback when feature is unavialable), OpenGL fixed function
* (Default older hardware), OpenGL Shaders (Default newer hardware). These graphics
* cards, even the OpenGL fixed function ones (~1995) can instance many of our models
* in hardware, giving a tremndous boost in speed, and allowing us to make deng the
* undisputed eye-candy engine. - Yagisan
*/

// HEADER FILES ------------------------------------------------------------

#include "dropengl.h"

#include <stdlib.h>

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

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
boolean forceFinishBeforeSwap = DGL_FALSE; 

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean firstTimeInit = true;
static int screenBits, windowed;

// CODE --------------------------------------------------------------------

int DG_ChangeVideoMode(int width, int height, int bpp)
{
    int     flags = SDL_OPENGL;

    if(!windowed)
        flags |= SDL_FULLSCREEN;

    if(!SDL_SetVideoMode(width, height, bpp, flags))
    {
        // This could happen for a variety of reasons, including
        // DISPLAY not being set, the specified resolution not being
        // available, etc.
        Con_Message("SDL Error: %s\n", SDL_GetError());
        return DGL_FALSE;
    }

    return DGL_TRUE;
}

/**
 * Attempt to create a context for GL rendering. 
 *
 * @return              Non-zero= success.
 */
int initOpenGL(void)
{
    // Attempt to set the video mode.
    if(!DG_ChangeVideoMode(screenWidth, screenHeight, screenBits))
        return DGL_FALSE;

    // Setup the GL state like we want it.
    initState();
    return DGL_TRUE;
}

/**
 * Set the currently active GL texture by name.
 *
 * @param texture       GL texture name to make active.
 */
void activeTexture(const GLenum texture)
{
#ifdef USE_MULTITEXTURE
    glActiveTextureARB(texture);
#endif
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
    boolean fullscreen = (mode == DGL_MODE_FULLSCREEN);
    char   *token, *extbuf;
    const SDL_VideoInfo *info = NULL;

    Con_Message("DG_Init: OpenGL.\n");

    info = SDL_GetVideoInfo();
    screenWidth = width;
    screenHeight = height;
    screenBits = info->vfmt->BitsPerPixel;
    windowed = !fullscreen;

    allowCompression = true;
    verbose = ArgExists("-verbose");

    // Set GL attributes.  We want at least 5 bits per color and a 16
    // bit depth buffer.  Plus double buffering, of course.
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    if(!initOpenGL())
    {
        Con_Error("drOpenGL.Init: OpenGL init failed.\n");
    }

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
            Con_Message("  ");  // Indent.
            if(verbose)
            {
                // Show full names.
                Con_Message("%s\n", token);
            }
            else
            {
                // Two on one line, clamp to 33 characters.
                Con_Message("%-33.33s", token);
                token = strtok(NULL, " ");
                if(token)
                    Con_Message(" %-33.33s", token);
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

    if(ArgExists("-glfinish"))
    {
        forceFinishBeforeSwap = DGL_TRUE;
        Con_Message("  glFinish() forced before swapping buffers.\n");
    }
    return DGL_OK;
}

void DG_DestroyContext(void)
{
    // Nothing required??
}

void DG_Shutdown(void)
{
    // No special shutdown procedures required.
}

/**
 * Make the content of the framebuffer visible.
 */
void DG_Show(void)
{
    if(forceFinishBeforeSwap)
    {
        glFinish();
    }
    
    // Swap buffers.
    SDL_GL_SwapBuffers();

    if(wireframeMode)
    {
        // When rendering is wireframe mode, we must clear the screen
        // before rendering a frame.
        DG_Clear(DGL_COLOR_BUFFER_BIT);
    }
}
