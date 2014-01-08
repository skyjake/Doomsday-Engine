/**\file sys_opengl.cpp
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <yagisan@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_misc.h"

#include "gl/sys_opengl.h"

#include <de/libdeng2.h>
#include <de/GLInfo>
#include <de/GLState>
#include <QSet>
#include <QStringList>

#ifdef WIN32
#   define GETPROC(Type, x)   x = de::function_cast<Type>(wglGetProcAddress(#x))
#elif defined(UNIX) && !defined(MACOSX)
#   include <GL/glx.h>
#   define GETPROC(Type, x)   x = de::function_cast<Type>(glXGetProcAddress((GLubyte const *)#x))
#endif

gl_state_t GL_state;

#ifdef WIN32
PFNWGLSWAPINTERVALEXTPROC      wglSwapIntervalEXT = NULL;
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = NULL;
#endif

#ifdef LIBGUI_USE_GLENTRYPOINTS
PFNGLBLENDEQUATIONEXTPROC      glBlendEquationEXT = NULL;
PFNGLLOCKARRAYSEXTPROC         glLockArraysEXT = NULL;
PFNGLUNLOCKARRAYSEXTPROC       glUnlockArraysEXT = NULL;
#endif

static boolean doneEarlyInit = false;
static boolean inited = false;
static boolean firstTimeInit = true;

static void initialize(void)
{
    de::GLInfo::Extensions const &ext = de::GLInfo::extensions();

    if(CommandLine_Exists("-noanifilter"))
    {
        GL_state.features.texFilterAniso = false;
    }

    if(!ext.ARB_texture_non_power_of_two ||
       CommandLine_Exists("-notexnonpow2") ||
       CommandLine_Exists("-notexnonpowtwo"))
    {
        GL_state.features.texNonPowTwo = false;
    }

    if(ext.EXT_blend_subtract)
    {
#ifdef LIBGUI_USE_GLENTRYPOINTS
        GETPROC(PFNGLBLENDEQUATIONEXTPROC, glBlendEquationEXT);
        if(!glBlendEquationEXT)
            GL_state.features.blendSubtract = false;
#endif
    }
    else
    {
        GL_state.features.blendSubtract = false;
    }

#ifdef USE_TEXTURE_COMPRESSION_S3
    // Enabled by default if available.
    if(ext.EXT_texture_compression_s3tc)
    {
        GLint iVal;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        if(iVal == 0 || glGetError() != GL_NO_ERROR)
            GL_state.features.texCompression = false;
    }
#else
    GL_state.features.texCompression = false;
#endif
    if(CommandLine_Exists("-notexcomp"))
    {
        GL_state.features.texCompression = false;
    }

    // Automatic mipmap generation.
    if(!ext.SGIS_generate_mipmap || CommandLine_Exists("-nosgm"))
    {
        GL_state.features.genMipmap = false;
    }

#ifdef WIN32
    if(ext.Windows_EXT_swap_control)
    {
        GETPROC(PFNWGLSWAPINTERVALEXTPROC, wglSwapIntervalEXT);
    }
    if(!ext.Windows_EXT_swap_control || !wglSwapIntervalEXT)
        GL_state.features.vsync = false;
#else
    GL_state.features.vsync = true;
#endif
}

#define TABBED(A, B)  _E(Ta) "  " A " " _E(Tb) << B << "\n"

de::String Sys_GLDescription()
{
    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    de::String str;
    QTextStream os(&str);

    os << _E(b) "OpenGL information:\n" << _E(.);

    os << TABBED("Version:",  (char const *) glGetString(GL_VERSION));
    os << TABBED("Renderer:", (char const *) glGetString(GL_RENDERER));
    os << TABBED("Vendor:",   (char const *) glGetString(GL_VENDOR));

    os << _E(T`) "Capabilities:\n";

    GLint iVal;

#ifdef USE_TEXTURE_COMPRESSION_S3
    if(de::GLInfo::extensions().EXT_texture_compression_s3tc)
    {
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        os << TABBED("Compressed texture formats:", iVal);
    }
#endif

    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &iVal);
    os << TABBED("Available texture units:", iVal);

    if(de::GLInfo::extensions().EXT_texture_filter_anisotropic)
    {
        glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iVal);
        os << TABBED("Maximum texture anisotropy:", iVal);
    }
    else
    {
        os << _E(Ta) "  Variable texture anisotropy unavailable.";
    }

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &iVal);
    os << TABBED("Maximum texture size:", iVal);

    GLfloat fVals[2];
    glGetFloatv(GL_LINE_WIDTH_GRANULARITY, fVals);
    os << TABBED("Line width granularity:", fVals[0]);

    glGetFloatv(GL_LINE_WIDTH_RANGE, fVals);
    os << TABBED("Line width range:", fVals[0] << "..." << fVals[1]);

    return str.rightStrip();

#undef TABBED
}

static void printGLUInfo(void)
{
    LOG_GL_MSG("%s") << Sys_GLDescription();

    Sys_GLPrintExtensions();
}

boolean Sys_GLPreInit(void)
{
    if(novideo) return true;
    if(doneEarlyInit) return true; // Already been here??

    // Init assuming ideal configuration.
    GL_state.multisampleFormat = 0; // No valid default can be assumed at this time.

    GL_state.features.blendSubtract = true;
    GL_state.features.genMipmap = true;
    GL_state.features.multisample = false; // We'll test for availability...
    GL_state.features.texCompression = true;
    GL_state.features.texFilterAniso = true;
    GL_state.features.texNonPowTwo = true;
    GL_state.features.vsync = true;

    GL_state.currentLineWidth = 1.5f;
    GL_state.currentPointSize = 1.5f;
    GL_state.currentUseFog = false;

    doneEarlyInit = true;
    return true;
}

boolean Sys_GLInitialize(void)
{
    if(novideo) return true;

    assert(doneEarlyInit);

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    assert(!Sys_GLCheckError());

    if(firstTimeInit)
    {
        const GLubyte* versionStr = glGetString(GL_VERSION);
        double version = (versionStr? strtod((const char*) versionStr, NULL) : 0);
        if(version == 0)
        {
            Con_Message("Sys_GLInitialize: Failed to determine OpenGL version.");
            Con_Message("  OpenGL version: %s", glGetString(GL_VERSION));
        }
        else if(version < 2.0)
        {
            if(!CommandLine_Exists("-noglcheck"))
            {
                Sys_CriticalMessagef("OpenGL implementation is too old!\n"
                                     "  Driver version: %s\n"
                                     "  The minimum supported version is 2.0",
                                     glGetString(GL_VERSION));
                return false;
            }
            else
            {
                Con_Message("Warning: Sys_GLInitialize: OpenGL implementation may be too old (2.0+ required).");
                Con_Message("  OpenGL version: %s", glGetString(GL_VERSION));
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
    if(GL_state.features.genMipmap && de::GLInfo::extensions().SGIS_generate_mipmap)
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

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

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

    // Configure the default GLState (bottom of the stack).
    de::GLState::current().setBlendFunc(de::gl::SrcAlpha, de::gl::OneMinusSrcAlpha);
}

static de::String omitGLPrefix(de::String str)
{
    if(str.startsWith("GL_")) return str.substr(3);
    return str;
}

static void printExtensions(QStringList extensions)
{
    // Find all the prefixes.
    QSet<QString> prefixes;
    foreach(QString ext, extensions)
    {
        ext = omitGLPrefix(ext);
        int pos = ext.indexOf("_");
        if(pos > 0)
        {
            prefixes.insert(ext.left(pos));
        }
    }

    QStringList sortedPrefixes = prefixes.toList();
    qSort(sortedPrefixes);
    foreach(QString prefix, sortedPrefixes)
    {
        de::String str;
        QTextStream os(&str);

        os << "    " << prefix << " extensions:\n        " _E(>) _E(2);

        bool first = true;
        foreach(QString ext, extensions)
        {
            ext = omitGLPrefix(ext);
            if(ext.startsWith(prefix + "_"))
            {
                ext.remove(0, prefix.size() + 1);
                if(!first) os << ", ";
                os << ext;
                first = false;
            }
        }

        LOG_GL_MSG("%s") << str;
    }
}

void Sys_GLPrintExtensions(void)
{
    LOG_GL_MSG(_E(b) "OpenGL Extensions:");
    printExtensions(QString((char const *) glGetString(GL_EXTENSIONS)).split(" ", QString::SkipEmptyParts));

#if WIN32
    // List the WGL extensions too.
    if(wglGetExtensionsStringARB)
    {
        Con_Message("  Extensions (WGL):");
        printExtensions(QString((char const *) ((GLubyte const *(__stdcall *)(HDC))wglGetExtensionsStringARB)(wglGetCurrentDC())).split(" ", QString::SkipEmptyParts));
    }
#endif
}

boolean Sys_GLCheckError()
{
#ifdef DENG_DEBUG
    if(!novideo)
    {
        GLenum error = glGetError();
        if(error != GL_NO_ERROR)
            Con_Message("OpenGL error: 0x%x", error);
    }
#endif
    return false;
}
