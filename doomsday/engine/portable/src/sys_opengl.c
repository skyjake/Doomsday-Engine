/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2009 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2009 Daniel Swanson <danij@dengine.net>
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
 * sys_opengl.c: OpenGL interface, low-level.
 *
 * Get OpenGL header files from:
 * http://oss.sgi.com/projects/ogl-sample/
 */

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"
#include "de_base.h"
#include "de_graphics.h"
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

gl_state_t GL_state;
gl_state_ext_t GL_state_ext;

#ifdef GL_EXT_framebuffer_object
PFNGLGENERATEMIPMAPEXTPROC glGenerateMipmapEXT;
#endif

#ifdef WIN32
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;

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

static boolean initedGL = false;
static boolean firstTimeInit = true;

#if WIN32
static PROC wglGetExtString;
#endif

// CODE --------------------------------------------------------------------

static void checkExtensions(void)
{
    Sys_InitGLExtensions();
#ifdef WIN32
    Sys_InitWGLExtensions();
#endif

    // Get the maximum texture size.
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*) &GL_state.maxTexSize);

    glGetIntegerv(GL_MAX_TEXTURE_UNITS, (GLint*) &GL_state.maxTexUnits);
#ifndef USE_MULTITEXTURE
    GL_state.maxTexUnits = 1;
#endif
    // But sir, we are simple people; two units is enough.
    if(GL_state.maxTexUnits > 2)
        GL_state.maxTexUnits = 2;

    if(GL_state_ext.aniso)
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint*) &GL_state.maxAniso);
    else
        GL_state.maxAniso = 1;

    // Decide whether vertex arrays should be done manually or with real
    // OpenGL calls.
    GL_InitArrays();

    GL_state.useAnisotropic =
        ((GL_state_ext.aniso && !ArgExists("-noanifilter"))? true : false);

    GL_state.allowCompression = true;
}

static void printGLUInfo(void)
{
    GLint iVal;
    GLfloat fVals[2];

    // Print some OpenGL information (console must be initialized by now).
    Con_Message("OpenGL information:\n");
    Con_Message("  Vendor: %s\n", glGetString(GL_VENDOR));
    Con_Message("  Renderer: %s\n", glGetString(GL_RENDERER));
    Con_Message("  Version: %s\n", glGetString(GL_VERSION));
    Con_Message("  GLU Version: %s\n", gluGetString(GLU_VERSION));

    if(GL_state_ext.s3TC)
    {
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        Con_Message("  Available Compressed Texture Formats: %i\n", iVal);
    }

    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &iVal);
    Con_Message("  Available Texture Units: %i\n", iVal);

    if(GL_state_ext.aniso)
    {
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iVal);
        Con_Message("  Maximum Texture Anisotropy: %i\n", iVal);
    }
    else
    {
        Con_Message("  Variable Texture Anisotropy Unavailable.\n");
    }

#if GL_EXT_texture_lod_bias
    { float lodBiasMax = 0;
    glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &lodBiasMax);
    Con_Message("  Maximum Texture LOD Bias: %3.1f\n", lodBiasMax); }
#endif

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iVal);
    Con_Message("  Maximum Texture Size: %i\n", iVal);

    glGetFloatv(GL_LINE_WIDTH_GRANULARITY, fVals);
    Con_Message("  Line Width Granularity: %3.1f\n", fVals[0]);

    glGetFloatv(GL_LINE_WIDTH_RANGE, fVals);
    Con_Message("  Line Width Range: %3.1f...%3.1f\n", fVals[0], fVals[1]);

    Sys_PrintGLExtensions();
}

#ifdef WIN32
static void testMultisampling(HDC hDC)
{
    int             pixelFormat;
    int             valid;
    uint            numFormats;
    float           fAttributes[] = {0,0};

    int iAttributes[] =
    {
        WGL_DRAW_TO_WINDOW_ARB,GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,GL_TRUE,
        WGL_ACCELERATION_ARB,WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB,24,
        WGL_ALPHA_BITS_ARB,8,
        WGL_DEPTH_BITS_ARB,16,
        WGL_STENCIL_BITS_ARB,8,
        WGL_DOUBLE_BUFFER_ARB,GL_TRUE,
        WGL_SAMPLE_BUFFERS_ARB,GL_TRUE,
        WGL_SAMPLES_ARB,4,
        0,0
    };

    // First, see if we can get a pixel format using four samples.
    valid = wglChoosePixelFormatARB(hDC, iAttributes, fAttributes, 1,
                                    &pixelFormat, &numFormats);

    if(valid && numFormats >= 1)
    {   // This will do nicely.
        GL_state_ext.wglMultisampleARB = 1;
        GL_state.multisampleFormat = pixelFormat;
    }
    else
    {   // Failed. Try a pixel format using two samples.
        iAttributes[19] = 2;
        valid = wglChoosePixelFormatARB(hDC, iAttributes, fAttributes, 1,
                                        &pixelFormat, &numFormats);
        if(valid && numFormats >= 1)
        {
            GL_state_ext.wglMultisampleARB = 1;
            GL_state.multisampleFormat = pixelFormat;
        }
    }
}

static void createDummyWindow(application_t* app)
{
    HWND                hWnd = NULL;
    HGLRC               hGLRC = NULL;
    boolean             ok = true;

    // Create the window.
    hWnd = CreateWindowEx(WS_EX_APPWINDOW, MAINWCLASS, "dummy",
                          (WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS),
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          CW_USEDEFAULT, CW_USEDEFAULT,
                          NULL, NULL,
                          app->hInstance, NULL);
    if(hWnd)
    {   // Initialize.
        PIXELFORMATDESCRIPTOR pfd;
        int         pixForm = 0;
        HDC         hDC = NULL;

        // Setup the pixel format descriptor.
        ZeroMemory(&pfd, sizeof(pfd));
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.iLayerType = PFD_MAIN_PLANE;
#ifndef DRMESA
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 16;
        pfd.cStencilBits = 8;
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
        pfd.cStencilBits = 2;
#endif

        if(ok)
        {
            // Acquire a device context handle.
            hDC = GetDC(hWnd);
            if(!hDC)
            {
                Sys_CriticalMessage("DD_CreateWindow: Failed acquiring device context handle.");
                hDC = NULL;
                ok = false;
            }
        }

        if(ok)
        {   // Request a matching (or similar) pixel format.
            pixForm = ChoosePixelFormat(hDC, &pfd);
            if(!pixForm)
            {
                Sys_CriticalMessage("DD_CreateWindow: Choosing of pixel format failed.");
                pixForm = -1;
                ok = false;
            }
        }

        if(ok)
        {   // Make sure that the driver is hardware-accelerated.
            DescribePixelFormat(hDC, pixForm, sizeof(pfd), &pfd);
            if((pfd.dwFlags & PFD_GENERIC_FORMAT) && !ArgCheck("-allowsoftware"))
            {
                Sys_CriticalMessage("DD_CreateWindow: GL driver not accelerated!\n"
                                    "Use the -allowsoftware option to bypass this.");
                ok = false;
            }
        }

        if(ok)
        {   // Set the pixel format for the device context. Can only be done once
            // (unless we release the context and acquire another).
            if(!SetPixelFormat(hDC, pixForm, &pfd))
            {
                Sys_CriticalMessage("DD_CreateWindow: Warning, setting of pixel "
                                    "format failed.");
            }
        }

        // Create the OpenGL rendering context.
        if(ok)
        {
            if(!(hGLRC = wglCreateContext(hDC)))
            {
                Sys_CriticalMessage("createContext: Creation of rendering context "
                                    "failed.");
                ok = false;
            }
            // Make the context current.
            else if(!wglMakeCurrent(hDC, hGLRC))
            {
                Sys_CriticalMessage("createContext: Couldn't make the rendering "
                                    "context current.");
                ok = false;
            }
        }

        if(ok)
        {
            PROC            getExtString = wglGetProcAddress("wglGetExtensionsStringARB");
            const GLubyte* extensions =
            ((const GLubyte*(__stdcall*)(HDC))getExtString)(hDC);

            if(Sys_QueryGLExtension("WGL_ARB_multisample", extensions))
            {
                GETPROC(wglChoosePixelFormatARB);
                if(wglChoosePixelFormatARB)
                    testMultisampling(hDC);
            }
        }

        // We've now finished with the device context.
        if(hDC)
            ReleaseDC(hWnd, hDC);
    }

    // Delete the window's rendering context if one has been acquired.
    if(hGLRC)
    {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hGLRC);
    }

    // Destroy the window and release the handle.
    if(hWnd)
        DestroyWindow(hWnd);
}
#endif

boolean Sys_PreInitGL(void)
{
    if(isDedicated)
        return true;

    if(initedGL)
        return true; // Already inited.

    memset(&GL_state, 0, sizeof(GL_state));
    memset(&GL_state_ext, 0, sizeof(GL_state_ext));
    memset(&GL_state_texture, 0, sizeof(GL_state_texture));

#ifdef WIN32
    // We want to be able to use multisampling if available so lets create a
    // dummy window and see what pixel formats we have.
    createDummyWindow(&app);
#endif

    GL_state_texture.dumpTextures =
        (ArgCheck("-dumptextures")? true : false);

    if(GL_state_texture.dumpTextures)
        Con_Message("  Dumping textures (mipmap level zero).\n");

    GL_state.forceFinishBeforeSwap =
        (ArgExists("-glfinish")? true : false);

    if(GL_state.forceFinishBeforeSwap)
        Con_Message("  glFinish() forced before swapping buffers.\n");

    initedGL = true;
    return true;
}

/**
 * Initializes OpenGL.
 */
boolean Sys_InitGL(void)
{
    if(isDedicated)
        return true;

    if(!initedGL)
        return false;

    if(firstTimeInit)
    {
        checkExtensions();
        printGLUInfo();

        firstTimeInit = false;
    }

    return true;
}

void Sys_ShutdownGL(void)
{
    if(!initedGL)
        return;

    initedGL = false;
}

void Sys_InitGLState(void)
{
    GLfloat fogcol[4] = { .54f, .54f, .54f, 1 };

    GL_state.nearClip = 5;
    GL_state.farClip = 8000;
    polyCounter = 0;

    GL_state_texture.usePalTex = false;
    GL_state_texture.dumpTextures = false;
    GL_state_texture.useCompr = false;

    { double version = strtod((const char*) glGetString(GL_VERSION), 0);
    // If older than 1.4, disable use of cube maps.
    GL_state_texture.haveCubeMap = !(version < 1.4); }

    // Here we configure the OpenGL state and set projection matrix.
    glFrontFace(GL_CW);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glDisable(GL_TEXTURE_1D);
    glDisable(GL_TEXTURE_2D);
    if(GL_state_texture.haveCubeMap)
        glDisable(GL_TEXTURE_CUBE_MAP);

    // The projection matrix.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Initialize the modelview matrix.
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Also clear the texture matrix.
    glMatrixMode(GL_TEXTURE);
    glLoadIdentity();

#if DRMESA
    glDisable(GL_DITHER);
    glDisable(GL_LIGHTING);
    glDisable(GL_LINE_SMOOTH);
    glDisable(GL_POINT_SMOOTH);
    glDisable(GL_POLYGON_SMOOTH);
    glShadeModel(GL_FLAT);
#else
    // Setup for antialiased lines/points.
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    GL_state.currentLineWidth = 1.5f;
    glLineWidth(GL_state.currentLineWidth);

    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    GL_state.currentPointSize = 1.5f;
    glPointSize(GL_state.currentPointSize);

    glShadeModel(GL_SMOOTH);
#endif

    // Alpha blending is a go!
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);

    // Default state for the white fog is off.
    GL_state.useFog = 0;
    glDisable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogi(GL_FOG_END, 2100);   // This should be tweaked a bit.
    glFogfv(GL_FOG_COLOR, fogcol);

#ifdef WIN32
    // Default state for vsync is on.
    GL_state.useVSync = true;
    if(wglSwapIntervalEXT != NULL)
    {
        wglSwapIntervalEXT(1);
    }
#else
    GL_state.useVSync = false;
#endif

#if DRMESA
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
#else
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
#endif

    // Prefer good quality in texture compression.
    glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);

    // Clear the buffers.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/**
 * @return          Non-zero iff the extension string is found. This
 *                  function is based on the method used by David Blythe
 *                  and Tom McReynolds in the book "Advanced Graphics
 *                  Programming Using OpenGL" ISBN: 1-55860-659-9.
 */
boolean Sys_QueryGLExtension(const char* name, const GLubyte* extensions)
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

            result = (Sys_QueryGLExtension(ext, extensions)? 1 : 0);
        }
#endif
        if(!result)
            result = (Sys_QueryGLExtension(ext, glGetString(GL_EXTENSIONS))? 1 : 0);

        if(var)
            *var = result;
    }

    return result;
}

#ifdef WIN32
void Sys_InitWGLExtensions(void)
{
    wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");

    if(query("WGL_EXT_swap_control", &GL_state_ext.wglSwapIntervalEXT))
    {
        GETPROC(wglSwapIntervalEXT);
    }

    if(query("WGL_ARB_multisample", NULL))
    {
        GETPROC(wglChoosePixelFormatARB);
        if(wglChoosePixelFormatARB)
        {
            GL_state_ext.wglMultisampleARB = 1;
        }
    }
}
#endif

/**
 * Pre: A rendering context must be aquired and made current before this is
 * called.
 */
void Sys_InitGLExtensions(void)
{
    GLint           iVal;

    if(query("GL_EXT_compiled_vertex_array", &GL_state_ext.lockArray))
    {
#ifdef WIN32
        GETPROC(glLockArraysEXT);
        GETPROC(glUnlockArraysEXT);
#endif
    }

    query("GL_EXT_paletted_texture", &GL_state.palExtAvailable);
    query("GL_EXT_texture_filter_anisotropic", &GL_state_ext.aniso);
    if(!ArgExists("-notexnonpow2"))
       query("GL_ARB_texture_non_power_of_two", &GL_state.textureNonPow2);

    // EXT_blend_subtract
    if(query("GL_EXT_blend_subtract", &GL_state_ext.blendSub))
    {
#ifdef WIN32
        GETPROC(glBlendEquationEXT);
#endif
    }

    // ARB_texture_env_combine
    if(!query("GL_ARB_texture_env_combine", &GL_state_ext.texEnvComb))
    {
        // Try the older EXT_texture_env_combine (identical to ARB).
        query("GL_EXT_texture_env_combine", &GL_state_ext.texEnvComb);
    }

    query("GL_NV_texture_env_combine4", &GL_state_ext.nvTexEnvComb);

    query("GL_ATI_texture_env_combine3", &GL_state_ext.atiTexEnvComb);

    query("GL_EXT_texture_compression_s3tc", &GL_state_ext.s3TC);

    // On by default if we have it.
    glGetError();
    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
    if(iVal && glGetError() == GL_NO_ERROR)
        GL_state_texture.useCompr = true;

    if(ArgExists("-notexcomp"))
        GL_state_texture.useCompr = false;

#ifdef USE_MULTITEXTURE
    // ARB_multitexture
    if(query("GL_ARB_multitexture", &GL_state_ext.multiTex))
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
    if(!ArgExists("-nosgm") && query("GL_SGIS_generate_mipmap", &GL_state_ext.genMip))
    {
        // Use nice quality, please.
        glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
    }

    if(query("GL_EXT_framebuffer_object", &GL_state_ext.framebufferObject))
    {
        GETPROC(glGenerateMipmapEXT);
    }

    query("GL_SGIS_generate_mipmap", &GL_state_ext.genMip);

#if GL_EXT_texture_lod_bias
    query("GL_EXT_texture_lod_bias", &GL_state_ext.texEnvLODBias);
#endif
}

/**
 * Show the list of GL extensions.
 */
static void printExtensions(const GLubyte* extensions)
{
    char           *token, *extbuf;

    extbuf = M_Malloc(strlen((const char*) extensions) + 1);
    strcpy(extbuf, (const char *) extensions);

    token = strtok(extbuf, " ");
    while(token)
    {
        Con_Message("    "); // Indent.
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
    M_Free(extbuf);
}

void Sys_PrintGLExtensions(void)
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

void Sys_CheckGLError(void)
{
#ifdef _DEBUG
    GLenum  error;

    if((error = glGetError()) != GL_NO_ERROR)
        Con_Error("OpenGL error: %i\n", error);
#endif
}
