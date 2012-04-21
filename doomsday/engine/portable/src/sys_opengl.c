/**\file sys_opengl.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2012 Daniel Swanson <danij@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "sys_opengl.h"

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

#ifdef WIN32
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = NULL;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;

PFNGLCLIENTACTIVETEXTUREPROC glClientActiveTexture = NULL;
PFNGLACTIVETEXTUREPROC glActiveTexture = NULL;

PFNGLMULTITEXCOORD2FPROC glMultiTexCoord2f = NULL;
PFNGLMULTITEXCOORD2FVPROC glMultiTexCoord2fv = NULL;

PFNGLBLENDEQUATIONEXTPROC glBlendEquationEXT = NULL;
PFNGLLOCKARRAYSEXTPROC glLockArraysEXT = NULL;
PFNGLUNLOCKARRAYSEXTPROC glUnlockArraysEXT = NULL;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static boolean doneEarlyInit = false;
static boolean inited = false;
static boolean firstTimeInit = true;

#if WIN32
static PROC wglGetExtString;
#endif

// CODE --------------------------------------------------------------------

static int query(const char* ext)
{
    int result = 0;
    assert(ext);

#if WIN32
    // Prefer the wgl-specific extensions.
    if(wglGetExtString)
    {
        const GLubyte* extensions =
            ((const GLubyte*(__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC());
        result = (Sys_GLQueryExtension(ext, extensions)? 1 : 0);
    }
#endif

    if(!result)
        result = (Sys_GLQueryExtension(ext, glGetString(GL_EXTENSIONS))? 1 : 0);
    return result;
}

static void initialize(void)
{
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*) &GL_state.maxTexSize);

#ifdef WIN32
    wglGetExtString = wglGetProcAddress("wglGetExtensionsStringARB");
#endif

    if(0 != (GL_state.extensions.lockArray = query("GL_EXT_compiled_vertex_array")))
    {
#ifdef WIN32
        GETPROC(glLockArraysEXT);
        GETPROC(glUnlockArraysEXT);
        if(NULL == glLockArraysEXT || NULL == glUnlockArraysEXT)
            GL_state.features.elementArrays = false;
#endif
    }
    if(ArgExists("-vtxar") && !ArgExists("-novtxar"))
        GL_state.features.elementArrays = true;

    if(0 != (GL_state.extensions.texFilterAniso = query("GL_EXT_texture_filter_anisotropic")))
    {
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint*) &GL_state.maxTexFilterAniso);
    }
    if(ArgExists("-noanifilter"))
        GL_state.features.texFilterAniso = false;

    GL_state.extensions.texNonPowTwo = query("GL_ARB_texture_non_power_of_two");
    if(!GL_state.extensions.texNonPowTwo || ArgExists("-notexnonpow2") || ArgExists("-notexnonpowtwo"))
        GL_state.features.texNonPowTwo = false;

    if(0 != (GL_state.extensions.blendSub = query("GL_EXT_blend_subtract")))
    {
#ifdef WIN32
        GETPROC(glBlendEquationEXT);
        if(NULL == glBlendEquationEXT)
            GL_state.features.blendSubtract = false;
#endif
    }
    else
    {
        GL_state.features.blendSubtract = false;
    }

    // ARB_texture_env_combine
    if(0 == (GL_state.extensions.texEnvComb = query("GL_ARB_texture_env_combine")))
    {
        // Try the older EXT_texture_env_combine (identical to ARB).
        GL_state.extensions.texEnvComb = query("GL_EXT_texture_env_combine");
    }

    GL_state.extensions.texEnvCombNV = query("GL_NV_texture_env_combine4");
    GL_state.extensions.texEnvCombATI = query("GL_ATI_texture_env_combine3");

#ifdef USE_TEXTURE_COMPRESSION_S3
    // Enabled by default if available.
    if(0 != (GL_state.extensions.texCompressionS3 = query("GL_EXT_texture_compression_s3tc")))
    {
        GLint iVal;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        if(iVal == 0 || glGetError() != GL_NO_ERROR)
            GL_state.features.texCompression = false;
    }
#else
    GL_state.features.texCompression = false;
#endif
    if(ArgExists("-notexcomp"))
        GL_state.features.texCompression = false;

#ifdef WIN32
    GETPROC(glActiveTexture);
    GETPROC(glClientActiveTexture);
    GETPROC(glMultiTexCoord2f);
    GETPROC(glMultiTexCoord2fv);
#endif
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, (GLint*) &GL_state.maxTexUnits);

    // Automatic mipmap generation.
    GL_state.extensions.genMipmapSGIS = query("GL_SGIS_generate_mipmap");
    if(0 == GL_state.extensions.genMipmapSGIS || ArgExists("-nosgm"))
        GL_state.features.genMipmap = false;

#ifdef WIN32
    if(0 != (GL_state.extensions.wglSwapIntervalEXT = query("WGL_EXT_swap_control")))
    {
        GETPROC(wglSwapIntervalEXT);
    }
    if(0 == GL_state.extensions.wglSwapIntervalEXT || NULL == wglSwapIntervalEXT)
        GL_state.features.vsync = false;
#else
    GL_state.features.vsync = true;
#endif
}

static void printGLUInfo(void)
{
    GLfloat fVals[2];
    GLint iVal;

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    Con_Message("OpenGL information:\n");
    Con_Message("  Vendor: %s\n", glGetString(GL_VENDOR));
    Con_Message("  Renderer: %s\n", glGetString(GL_RENDERER));
    Con_Message("  Version: %s\n", glGetString(GL_VERSION));

#ifdef USE_TEXTURE_COMPRESSION_S3
    if(GL_state.extensions.texCompressionS3)
    {
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        Con_Message("  Available Compressed Texture Formats: %i\n", iVal);
    }
#endif

    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &iVal);
    Con_Message("  Available Texture Units: %i\n", iVal);

    if(GL_state.extensions.texFilterAniso)
    {
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iVal);
        Con_Message("  Maximum Texture Anisotropy: %i\n", iVal);
    }
    else
    {
        Con_Message("  Variable Texture Anisotropy Unavailable.\n");
    }

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iVal);
    Con_Message("  Maximum Texture Size: %i\n", iVal);

    glGetFloatv(GL_LINE_WIDTH_GRANULARITY, fVals);
    Con_Message("  Line Width Granularity: %3.1f\n", fVals[0]);

    glGetFloatv(GL_LINE_WIDTH_RANGE, fVals);
    Con_Message("  Line Width Range: %3.1f...%3.1f\n", fVals[0], fVals[1]);

    Sys_GLPrintExtensions();
}

#if 0
#ifdef WIN32
static void testMultisampling(HDC hDC)
{
    int pixelFormat, valid;
    uint numFormats;
    float fAttributes[] = {0,0};
    int iAttributes[] = {
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
    valid = wglChoosePixelFormatARB(hDC, iAttributes, fAttributes, 1, &pixelFormat, &numFormats);

    if(valid && numFormats >= 1)
    {   // This will do nicely.
        GL_state.multisampleFormat = pixelFormat;
        GL_state.features.multisample = true;
    }
    else
    {   // Failed. Try a pixel format using two samples.
        iAttributes[19] = 2;
        valid = wglChoosePixelFormatARB(hDC, iAttributes, fAttributes, 1, &pixelFormat, &numFormats);
        if(valid && numFormats >= 1)
        {
            GL_state.multisampleFormat = pixelFormat;
            GL_state.features.multisample = true;
        }
    }
}

static void createDummyWindow(application_t* app)
{
    HWND                hWnd = NULL;
    HGLRC               hGLRC = NULL;
    boolean             ok = true;

    // Create the window.
    hWnd = CreateWindowEx(WS_EX_APPWINDOW, TEXT(MAINWCLASS), TEXT("dummy"),
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
                Sys_CriticalMessage("DD_CreateWindow: Failed setting pixel format.");
            }
        }

        // Create the OpenGL rendering context.
        if(ok)
        {
            if(!(hGLRC = wglCreateContext(hDC)))
            {
                Sys_CriticalMessage("DD_CreateWindow: Failed creating render context.");
                ok = false;
            }
            // Make the context current.
            else if(!wglMakeCurrent(hDC, hGLRC))
            {
                Sys_CriticalMessage("DD_CreateWindow: Failed making render context current.");
                ok = false;
            }
        }

        if(ok)
        {
            PROC wglGetExtensionsStringARB;
            const GLubyte* exts;

            GETPROC(wglGetExtensionsStringARB);
            exts = ((const GLubyte*(__stdcall*)(HDC))wglGetExtensionsStringARB)(hDC);

            GL_state.extensions.wglMultisampleARB = Sys_GLQueryExtension("WGL_ARB_multisample", exts);
            if(GL_state.extensions.wglMultisampleARB)
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
#endif // 0

boolean Sys_GLPreInit(void)
{
    if(novideo) return true;
    if(doneEarlyInit) return true; // Already been here??

    memset(&GL_state.extensions, 0, sizeof(GL_state.extensions));
    // Init assuming ideal configuration.
    GL_state.forceFinishBeforeSwap = ArgExists("-glfinish");
    GL_state.maxTexFilterAniso = 4;
    GL_state.maxTexSize = 4096;
    GL_state.maxTexUnits = 1;
    GL_state.multisampleFormat = 0; // No valid default can be assumed at this time.

    GL_state.features.blendSubtract = true;
    GL_state.features.genMipmap = true;
    GL_state.features.multisample = false; // We'll test for availability...
    GL_state.features.texCompression = true;
    GL_state.features.texFilterAniso = true;
    GL_state.features.texNonPowTwo = true;
    GL_state.features.vsync = true;
    GL_state.features.elementArrays = false;

    GL_state.currentLineWidth = 1.5f;
    GL_state.currentPointSize = 1.5f;
    GL_state.currentUseFog = false;

#if 0 // WIN32
    // We prefer to use  multisampling if available so create a dummy window
    // and see what pixel formats are present.
    createDummyWindow(&app);

    // User disabled?
    if(ArgCheck("-noaa"))
        GL_state.features.multisample = false;
#endif

    doneEarlyInit = true;
    return true;
}

boolean Sys_GLInitialize(void)
{
    if(novideo) return true;

    assert(doneEarlyInit);

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    assert(!Sys_GLCheckError());

    if(firstTimeInit)
    {
        double version = strtod((const char*) glGetString(GL_VERSION), NULL);
        if(version == 0)
        {
            Con_Message("Sys_GLInitialize: Failed to determine OpenGL version.\n");
            Con_Message("  OpenGL version: %s\n", glGetString(GL_VERSION));
        }
        else if(version < 1.4)
        {
            if(!ArgExists("-noglcheck"))
            {
                Sys_CriticalMessagef("OpenGL implementation is too old!\n"
                                     "  Driver version: %s\n"
                                     "  The minimum supported version is 1.4",
                                     glGetString(GL_VERSION));
                return false;
            }
            else
            {
                Con_Message("Warning: Sys_GLInitialize: OpenGL implementation may be too old (1.4+ required).\n");
                Con_Message("  OpenGL version: %s\n", glGetString(GL_VERSION));
            }
        }

        initialize();
        printGLUInfo();

        firstTimeInit = false;
    }

    // GL system is now fully initialized.
    inited = true;

    /**
     * We can now (re)configure GL state that is dependent upon extensions
     * which may or may not be present on the host system.
     */

    // Use nice quality for mipmaps please.
    if(GL_state.features.genMipmap && GL_state.extensions.genMipmapSGIS)
        glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);

    assert(!Sys_GLCheckError());

    return true;
}

void Sys_GLShutdown(void)
{
    if(!inited) return;
    // No cleanup.
    inited = false;
}

void Sys_GLConfigureDefaultState(void)
{
    GLfloat fogcol[4] = { .54f, .54f, .54f, 1 };

    /**
     * @note Only core OpenGL features can be configured at this time
     *       because we have not yet queried for available extensions,
     *       or configured our prefered feature default state.
     *
     *       This means that GL_state.extensions and GL_state.features
     *       cannot be accessed here during initial startup.
     */
    assert(doneEarlyInit);

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    glFrontFace(GL_CW);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    glDisable(GL_TEXTURE_1D);
    glDisable(GL_TEXTURE_2D);
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
    glLineWidth(GL_state.currentLineWidth);

    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    glPointSize(GL_state.currentPointSize);

    glShadeModel(GL_SMOOTH);
#endif

    // Alpha blending is a go!
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0);

    // Default state for the white fog is off.
    glDisable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogi(GL_FOG_END, 2100); // This should be tweaked a bit.
    glFogfv(GL_FOG_COLOR, fogcol);

#if DRMESA
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
#else
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
#endif

    // Prefer good quality in texture compression.
    glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);

    // Clear the buffers.
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

/**
 * This routine is based on the method used by David Blythe and Tom McReynolds
 * in the book "Advanced Graphics Programming Using OpenGL" ISBN: 1-55860-659-9.
 */
boolean Sys_GLQueryExtension(const char* name, const GLubyte* extensions)
{
    const GLubyte* start;
    GLubyte* c, *terminator;

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
        c = (GLubyte*) strstr((const char*) start, name);
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

static void printExtensions(const GLubyte* extensions)
{
    char* token, *extbuf;
    size_t len;

    if(!extensions || !extensions[0])
        return;
    len = strlen((const char*) extensions);

    if(0 == (extbuf = malloc(len+1)))
        Con_Error("printExtensions: Failed on allocation of %lu bytes for print buffer.",
                  (unsigned long) (len+1));

    strcpy(extbuf, (const char*) extensions);
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
    free(extbuf);
}

void Sys_GLPrintExtensions(void)
{
    Con_Message("  Extensions:\n");
    printExtensions(glGetString(GL_EXTENSIONS));
#if WIN32
    // List the WGL extensions too.
    if(wglGetExtString)
    {
        Con_Message("  Extensions (WGL):\n");
        printExtensions(((const GLubyte*(__stdcall*)(HDC))wglGetExtString)(wglGetCurrentDC()));
    }
#endif
}

boolean Sys_GLCheckError(void)
{
    if(novideo) return false;
#ifdef _DEBUG
    {
        GLenum error = glGetError();
        if(error != GL_NO_ERROR)
            Con_Message("OpenGL error: 0x%x\n", error);
    }
#endif
    return false;
}
