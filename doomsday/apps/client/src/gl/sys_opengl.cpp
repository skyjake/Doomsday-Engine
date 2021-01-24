/** @file sys_opengl.cpp
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2015 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "gl/sys_opengl.h"

#include <de/legacy/concurrency.h>
#include <de/glinfo.h>
#include <de/glstate.h>
#include <de/set.h>
#include <de/regexp.h>
#include "sys_system.h"
#include "gl/gl_main.h"

//#ifdef WIN32
//#   define GETPROC(Type, x)   x = de::function_cast<Type>(wglGetProcAddress(#x))
//#elif defined (DE_X11)
//#   include <GL/glx.h>
//#   undef None
//#   define GETPROC(Type, x)   x = de::function_cast<Type>(glXGetProcAddress((const GLubyte *)#x))
//#endif

using namespace de;

gl_state_t GL_state;

static dd_bool doneEarlyInit = false;
static dd_bool sysOpenGLInited = false;
static dd_bool firstTimeInit = true;

static void initialize(void)
{
    const de::GLInfo::Extensions &ext = de::GLInfo::extensions();

    if(CommandLine_Exists("-noanifilter"))
    {
        GL_state.features.texFilterAniso = false;
    }

    if (CommandLine_Exists("-texcomp") && !CommandLine_Exists("-notexcomp"))
    {
        GL_state.features.texCompression = true;
    }
#ifdef USE_TEXTURE_COMPRESSION_S3
    // Enabled by default if available.
    if (ext.EXT_texture_compression_s3tc)
    {
        GLint iVal;
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        LIBGUI_ASSERT_GL_OK();
        if (iVal == 0)// || glGetError() != GL_NO_ERROR)
            GL_state.features.texCompression = false;
    }
#else
    GL_state.features.texCompression = false;
#endif
}

#define TABBED(A, B)  _E(Ta) "  " _E(l) + String(A) + _E(.) " " _E(Tb) + String(B) + "\n"

String Sys_GLDescription()
{
    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    String str;

    str += _E(b) "OpenGL information:\n" _E(.);

    str += TABBED("Version:",  reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    str += TABBED("Renderer:", reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    str += TABBED("Vendor:",   reinterpret_cast<const char *>(glGetString(GL_VENDOR)));

    LIBGUI_ASSERT_GL_OK();

    str += _E(T`) "Capabilities:\n";

    GLint iVal;

#ifdef USE_TEXTURE_COMPRESSION_S3
    if (de::GLInfo::extensions().EXT_texture_compression_s3tc)
    {
        glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &iVal);
        LIBGUI_ASSERT_GL_OK();
        str += TABBED("Compressed texture formats:", String::asText(iVal));
    }
#endif

    str += TABBED("Use texture compression:", (GL_state.features.texCompression? "yes" : "no"));

    str += TABBED("Available texture units:", String::asText(de::GLInfo::limits().maxTexUnits));

    if (de::GLInfo::extensions().EXT_texture_filter_anisotropic)
    {
        str += TABBED("Maximum texture anisotropy:", String::asText(de::GLInfo::limits().maxTexFilterAniso));
    }
    else
    {
        str += _E(Ta) "  Variable texture anisotropy unavailable.";
    }

    str += TABBED("Maximum texture size:", String::asText(de::GLInfo::limits().maxTexSize));

    str += TABBED("Line width granularity:", String::asText(de::GLInfo::limits().smoothLineWidthGranularity));

    str += TABBED("Line width range:",
                  Stringf("%.2f...%.2f",
                                     de::GLInfo::limits().smoothLineWidth.start,
                                     de::GLInfo::limits().smoothLineWidth.end));

    return str.rightStrip();

#undef TABBED
}

static void printGLUInfo(void)
{
    LOG_GL_MSG("%s") << Sys_GLDescription();

    Sys_GLPrintExtensions();
}

dd_bool Sys_GLPreInit(void)
{
    if(novideo) return true;
    if(doneEarlyInit) return true; // Already been here??

    GL_state.features.texCompression = false;
    GL_state.features.texFilterAniso = true;
    GL_state.currentLineWidth = 1.5f;
    GL_state.currentPointSize = 1.5f;

    doneEarlyInit = true;
    return true;
}

dd_bool Sys_GLInitialize(void)
{
    if(novideo) return true;

    LOG_AS("Sys_GLInitialize");

    assert(doneEarlyInit);

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    assert(!Sys_GLCheckError());

    if(firstTimeInit)
    {
#if defined (DE_OPENGL)
        const GLubyte* versionStr = glGetString(GL_VERSION);
        double version = (versionStr? strtod((const char*) versionStr, NULL) : 0);
        if(version == 0)
        {
            LOG_GL_WARNING("Failed to determine OpenGL version; driver reports: %s")
                    << glGetString(GL_VERSION);
        }
        else if(version < 3.3)
        {
            if(!CommandLine_Exists("-noglcheck"))
            {
                Sys_CriticalMessagef("Your OpenGL is too old!\n"
                                     "  Driver version: %s\n"
                                     "  The minimum supported version is 3.3",
                                     glGetString(GL_VERSION));
                return false;
            }
            else
            {
                LOG_GL_WARNING("OpenGL may be too old (3.3+ required, "
                               "but driver reports %s)") << glGetString(GL_VERSION);
            }
        }
#endif

        initialize();
        printGLUInfo();

        firstTimeInit = false;
    }

    // GL system is now fully initialized.
    sysOpenGLInited = true;

    /**
     * We can now (re)configure GL state that is dependent upon extensions
     * which may or may not be present on the host system.
     */

    // Use nice quality for mipmaps please.
    //if(GL_state.features.genMipmap && de::GLInfo::extensions().SGIS_generate_mipmap)
    //glHint(GL_GENERATE_MIPMAP_HINT, GL_NICEST);

    assert(!Sys_GLCheckError());

    return true;
}

void Sys_GLShutdown(void)
{
    if(!sysOpenGLInited) return;
    // No cleanup.
    sysOpenGLInited = false;
}

void Sys_GLConfigureDefaultState(void)
{
    LIBGUI_ASSERT_GL_OK();

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

    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT_GL_CONTEXT_ACTIVE();

    glFrontFace(GL_CW);
    LIBGUI_ASSERT_GL_OK();

    DGL_CullFace(DGL_NONE);
    DGL_Disable(DGL_DEPTH_TEST);
    DGL_DepthFunc(DGL_LESS);

    DGL_Disable(DGL_TEXTURE_2D);

#if defined (DE_OPENGL)
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    LIBGUI_ASSERT_GL_OK();
#endif

    // The projection matrix.
    DGL_MatrixMode(DGL_PROJECTION);
    DGL_LoadIdentity();

    // Initialize the modelview matrix.
    DGL_MatrixMode(DGL_MODELVIEW);
    DGL_LoadIdentity();

    // Also clear the texture matrix.
    DGL_MatrixMode(DGL_TEXTURE);
    DGL_LoadIdentity();

//    de::GLInfo::setLineWidth(GL_state.currentLineWidth);

#if defined (DE_OPENGL)
    // Setup for antialiased lines/points.
    glEnable(GL_LINE_SMOOTH);
    LIBGUI_ASSERT_GL_OK();
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    LIBGUI_ASSERT_GL_OK();

    glPointSize(GL_state.currentPointSize);
    LIBGUI_ASSERT_GL_OK();

    // Prefer good quality in texture compression.
    glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
    LIBGUI_ASSERT_GL_OK();
#endif

    // Default state for the white fog is off.
    DGL_Disable(DGL_FOG);
    DGL_Fogi(DGL_FOG_MODE, DGL_LINEAR);
    DGL_Fogi(DGL_FOG_END, 2100); // This should be tweaked a bit.
    DGL_Fogfv(DGL_FOG_COLOR, fogcol);

    LIBGUI_ASSERT_GL_OK();

    // Configure the default GLState (bottom of the stack).
    DGL_BlendFunc(DGL_SRC_ALPHA, DGL_ONE_MINUS_SRC_ALPHA);
}

static String omitGLPrefix(const String &str)
{
    if (str.beginsWith("GL_")) return str.substr(de::BytePos(3));
    return str;
}

static void printExtensions(StringList extensions)
{
    extensions.sort();

    // Find all the prefixes.
    de::Set<String> prefixes;
    for (String ext : extensions)
    {
        ext = omitGLPrefix(ext);
        auto pos = ext.indexOf("_");
        if (pos > 0)
        {
            prefixes.insert(ext.left(pos));
        }
    }

    auto sortedPrefixes = de::compose<StringList>(prefixes.begin(), prefixes.end());
    sortedPrefixes.sort();
    for (const String &prefix : sortedPrefixes)
    {
        String str;

        str += "    " + prefix + " extensions:\n        " _E(>) _E(2);

        bool first = true;
        for (String ext : extensions)
        {
            ext = omitGLPrefix(ext);
            if (ext.beginsWith(prefix + "_"))
            {
                ext.remove(de::BytePos(0), prefix.size() + 1);
                if (!first) str += ", ";
                str += ext;
                first = false;
            }
        }

        LOG_GL_MSG("%s") << str;
    }
}

void Sys_GLPrintExtensions(void)
{
    using namespace de;

    LOG_GL_MSG(_E(b) "OpenGL Extensions:");

    int count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    StringList allExts;
    for (int i = 0; i < count; ++i)
    {
        allExts << reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, i));
    }
    printExtensions(allExts);

    /*
#if WIN32
    // List the WGL extensions too.
    if(wglGetExtensionsStringARB)
    {
        LOG_GL_MSG("  Extensions (WGL):");
        printExtensions(QString((const char *) ((const GLubyte *(__stdcall *)(HDC))wglGetExtensionsStringARB)(wglGetCurrentDC())).split(" ", QString::SkipEmptyParts));
    }
#endif

#ifdef Q_WS_X11
    // List GLX extensions.
    LOG_GL_MSG("  Extensions (GLX):");
    printExtensions(QString(getGLXExtensionsString()).split(" ", QString::SkipEmptyParts));
#endif
    */
}

dd_bool Sys_GLCheckErrorArgs(const char *file, int line)
{
    if (novideo) return false;
#ifdef DE_DEBUG
    de::GLInfo::checkError(file, line);
#endif
    return false;
}
