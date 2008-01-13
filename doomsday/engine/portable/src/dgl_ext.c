/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2007 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2008 Daniel Swanson <danij@dengine.net>
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
 * dgl_ext.c: OpenGL Extensions
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_dgl.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

#ifdef WIN32
#   define GETPROC(x)   x = (void*)wglGetProcAddress(#x)
#elif defined(UNIX)
#   define GETPROC(x)   x = SDL_GL_GetProcAddress(#x)
#endif

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

dgl_state_ext_t DGL_state_ext;

#ifdef WIN32
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

PFNGLCLIENTACTIVETEXTUREPROC glClientActiveTextureARB;
PFNGLACTIVETEXTUREARBPROC glActiveTextureARB;
PFNGLMULTITEXCOORD2FARBPROC glMultiTexCoord2fARB;
PFNGLMULTITEXCOORD2FVARBPROC glMultiTexCoord2fvARB;
PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT;
PFNGLLOCKARRAYSEXTPROC glLockArraysEXT;
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT;
PFNGLCOLORTABLEEXTPROC glColorTableEXT;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

#if WIN32
static PROC wglGetExtString;
#endif

// CODE --------------------------------------------------------------------

/**
 * @return          Non-zero iff the extension string is found. This
 *                  function is based on the method used by David Blythe
 *                  and Tom McReynolds in the book "Advanced Graphics
 *                  Programming Using OpenGL" ISBN: 1-55860-659-9.
 */
static boolean queryExtension(const char *name, const GLubyte  *extensions)
{
    const GLubyte  *start;
    GLubyte        *c, *terminator;

    // Extension names should not have spaces.
    c = (GLubyte *) strchr(name, ' ');
    if(c || *name == '\0')
        return false;

    if(!extensions)
        return false;

    // It takes a bit of care to be fool-proof about parsing the
    // OpenGL extensions string. Don't be fooled by sub-strings, etc.
    start = extensions;
    for(;;)
    {
        c = (GLubyte *) strstr((const char *) start, name);
        if(!c)
            break;

        terminator = c + strlen(name);
        if(c == start || *(c - 1) == ' ')
            if(*terminator == ' ' || *terminator == '\0')
            {
                return true;
            }
        start = terminator;
    }

    return false;
}

static int query(const char *ext, int *var)
{
    int         result = 0;

    if(ext)
    {
#if WIN32
        // Preference the wgl-specific extensions.
        if(wglGetExtString)
        {
            const GLubyte* extensions =
            ((const GLubyte*(__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC());

            result = (queryExtension(ext, extensions)? 1 : 0);
        }
#endif
        if(!result)
            result = (queryExtension(ext, glGetString(GL_EXTENSIONS))? 1 : 0);

        if(var)
            *var = result;
    }

    return result;
}

/**
 * Pre: A rendering context must be aquired and made current before this is
 * called.
 */
void DGL_InitExtensions(void)
{
    GLint           iVal;

#ifdef WIN32
    wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");
#endif

    if(query("GL_EXT_compiled_vertex_array", &DGL_state_ext.lockArray))
    {
#ifdef WIN32
        GETPROC(glLockArraysEXT);
        GETPROC(glUnlockArraysEXT);
#endif
    }

    query("GL_EXT_paletted_texture", &DGL_state.palExtAvailable);
    query("GL_EXT_shared_texture_palette", &DGL_state.sharedPalExtAvailable);
    query("GL_EXT_texture_filter_anisotropic", &DGL_state_ext.aniso);

#ifdef WIN32
    if(query("WGL_EXT_swap_control", &DGL_state_ext.wglSwapIntervalEXT))
    {
        GETPROC(wglSwapIntervalEXT);
    }
#endif

    // EXT_blend_subtract
    if(query("GL_EXT_blend_subtract", &DGL_state_ext.blendSub))
    {
#ifdef WIN32
        GETPROC(glBlendEquationEXT);
#endif
    }

    // ARB_texture_env_combine
    if(!query("GL_ARB_texture_env_combine", &DGL_state_ext.texEnvComb))
    {
        // Try the older EXT_texture_env_combine (identical to ARB).
        query("GL_EXT_texture_env_combine", &DGL_state_ext.texEnvComb);
    }

    // NV_texture_env_combine4
    query("GL_NV_texture_env_combine4", &DGL_state_ext.nvTexEnvComb);

    // ATI_texture_env_combine3
    query("GL_ATI_texture_env_combine3", &DGL_state_ext.atiTexEnvComb);

    // Texture compression.
    query("GL_EXT_texture_compression_s3tc", &DGL_state_ext.s3TC);
    // On by default if we have it.
    glGetError();
    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
    if(iVal && glGetError() == GL_NO_ERROR)
        DGL_state_texture.useCompr = true;

    if(ArgExists("-notexcomp"))
        DGL_state_texture.useCompr = false;

#ifdef USE_MULTITEXTURE
    // ARB_multitexture
    if(query("GL_ARB_multitexture", &DGL_state_ext.multiTex))
    {
#  ifdef WIN32
        // Get the function pointers.
        GETPROC(glClientActiveTextureARB);
        GETPROC(glActiveTextureARB);
        GETPROC(glMultiTexCoord2fARB);
        GETPROC(glMultiTexCoord2fvARB);
#  endif
    }
#endif

    // Automatic mipmap generation.
    if(!ArgExists("-nosgm") && query("GL_SGIS_generate_mipmap", &DGL_state_ext.genMip))
    {
        // Use nice quality, please.
        glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
    }
}

/**
 * Show the list of GL extensions.
 */
void printExtensions(const GLubyte* extensions)
{
    char           *token, *extbuf;

    extbuf = malloc(strlen((const char*) extensions) + 1);
    strcpy(extbuf, (const char *) extensions);

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
            if(token)
                Con_Message(" %-30.30s", token);
            Con_Message("\n");
        }
        token = strtok(NULL, " ");
    }
    free(extbuf);
}

void DGL_PrintExtensions(void)
{
    Con_Message("  Extensions:\n");
    printExtensions(glGetString(GL_EXTENSIONS));

#if WIN32
    // List the WGL extensions too.
    if(wglGetExtString)
    {
        Con_Message("  Extensions (WGL):\n");

        printExtensions(
        ((const GLubyte*(__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC()));
    }
#endif
}
