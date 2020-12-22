/** @file glinfo.cpp  OpenGL information.
 *
 * @authors Copyright (c) 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2007-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/glinfo.h"
#include "de/glwindow.h"
#include "de/opengl.h"

#include <cstring>
#include <de/string.h>
#include <de/log.h>
#include <de/math.h>
#include <de/c_wrapper.h>

#if (DE_OPENGL == 330)
#  include <glbinding/gl33ext/enum.h>
#  include <glbinding/Binding.h>
#endif

#if defined(DE_DEBUG)
//#  define DE_ENABLE_OPENGL_DEBUG_LOGGER
//#  include <QOpenGLDebugLogger>
#endif

//#if defined (MACOSX)
//#  include <OpenGL/OpenGL.h>
//#endif

//#if defined (DE_X11)
//#  include <QX11Info>
//#  include <GL/glx.h>
//#  include <GL/glxext.h>
//#  undef None
//#  undef Always
//#  undef Status
//#endif

#if defined (WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>
#endif

namespace de {

static GLInfo info;

#if defined(DE_ENABLE_OPENGL_DEBUG_LOGGER)
#ifndef WIN32
#  define APIENTRY
#endif
void APIENTRY debugMessageCallback(gl::GLenum source,
                                   gl::GLenum type,
                                   gl::GLuint id,
                                   gl::GLenum severity,
                                   gl::GLsizei length,
                                   const gl::GLchar *message,
                                   const void *userParam)
{
    using namespace gl;

    DE_UNUSED(source, userParam);
    
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        // Too verbose.
        return;
    }

    const char *mType = "--";
    const char *mSeverity = "--";

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
        mType = "ERROR";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        mType = "Deprecated";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        mType = "Undefined";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        mType = "Portability";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        mType = "Performance";
        break;
    case GL_DEBUG_TYPE_OTHER:
        mType = "Other";
        break;
    case GL_DEBUG_TYPE_MARKER:
        mType = "Marker";
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        mType = "Group Push";
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        mType = "Group Pop";
        break;
    default:
        break;
    }

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        mType = " HIGH ";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        mType = "MEDIUM";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        mType = " low  ";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        mType = " note ";
        break;
    default:
        break;
    }

    const String msg(message, length);

    debug("[OpenGL] %04x %s (%s): %s", id, mType, mSeverity, msg.c_str());
}
#endif
    
DE_PIMPL_NOREF(GLInfo) //, public QOpenGLFunctions_Doomsday
{
    bool inited = false;
    Extensions ext;
    Limits lim;

    //std::unique_ptr<QOpenGLExtension_ARB_draw_instanced>          ARB_draw_instanced;
    //std::unique_ptr<QOpenGLExtension_ARB_instanced_arrays>        ARB_instanced_arrays;
    //std::unique_ptr<QOpenGLExtension_EXT_framebuffer_blit>        EXT_framebuffer_blit;
    //std::unique_ptr<QOpenGLExtension_EXT_framebuffer_multisample> EXT_framebuffer_multisample;
    //std::unique_ptr<QOpenGLExtension_EXT_framebuffer_object>      EXT_framebuffer_object;
//#if defined (DE_OPENGL)
//    std::unique_ptr<QOpenGLExtension_NV_framebuffer_multisample_coverage> NV_framebuffer_multisample_coverage;
//#endif

// #ifdef WIN32
//     BOOL (APIENTRY *wglSwapIntervalEXT)(int interval) = nullptr;
// #endif

// #ifdef DE_X11
//     PFNGLXSWAPINTERVALEXTPROC  glXSwapIntervalEXT  = nullptr;
//     PFNGLXSWAPINTERVALSGIPROC  glXSwapIntervalSGI  = nullptr;
//     PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = nullptr;
// #endif

#ifdef DE_ENABLE_OPENGL_DEBUG_LOGGER
//    QOpenGLDebugLogger *logger = nullptr;
#endif

    Impl()
    {
        zap(ext);
    }

    /**
     * This routine is based on the method used by David Blythe and Tom
     * McReynolds in the book "Advanced Graphics Programming Using OpenGL"
     * ISBN: 1-55860-659-9.
     */
    bool checkExtensionString(const char *name, const char *extensions)
    {
        const char *start;
        const char *c, *terminator;

        // Extension names should not have spaces.
        c = strchr(name, ' ');
        if (c || *name == '\0')
        {
            return false;
        }
        if (!extensions)
        {
            return false;
        }
        // It takes a bit of care to be fool-proof about parsing the
        // OpenGL extensions string. Don't be fooled by sub-strings, etc.
        start = extensions;
        for (;;)
        {
            c = strstr(start, name);
            if (!c)
                break;

            terminator = c + strlen(name);
            if (c == start || *(c - 1) == ' ')
            {
                if (*terminator == ' ' || *terminator == '\0')
                {
                    return true;
                }
            }
            start = terminator;
        }
        return false;
    }

    bool doQuery(const char *ext)
    {
        DE_ASSERT(ext);
        //DE_ASSERT(QOpenGLContext::currentContext() != nullptr);
        LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

// #if 0
// #ifdef WIN32
//         // Prefer the wgl-specific extensions.
//         if (wglGetExtensionsStringARB != nullptr &&
//            checkExtensionString(ext, (const GLubyte *)wglGetExtensionsStringARB(wglGetCurrentDC())))
//             return true;
// #endif
// #endif

//#ifdef DE_X11
//        // Check GLX specific extensions.
//        if (checkExtensionString(ext, glXQueryExtensionsString(QX11Info::display(),
//                                                               QX11Info::appScreen())))
//            return true;
//#endif

//        return checkExtensionString(ext, glGetString(GL_EXTENSIONS));
//#endif

        //return QOpenGLContext::currentContext()->hasExtension(ext);
        return SDL_GL_ExtensionSupported(ext);
    }

    bool query(const char *ext)
    {
        bool found = doQuery(ext);
        LOGDEV_GL_VERBOSE("%s: %b") << ext << found;
        return found;
    }

    void init()
    {
        LOG_AS("GLInfo");

        debug("[GLInfo] init");

        LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

        debug("[GLInfo] GL context active");
        
        if (inited) return;

#if defined (DE_OPENGL_ES)
        {
            /*
            initializeOpenGLFunctions();

            static OpenGLFeature const requiredFeatures[] =
            {
                Multitexture,
                Shaders,
                Buffers,
                Framebuffers,
                BlendEquationSeparate,
                BlendSubtract,
                NPOTTextures,
                NPOTTextureRepeat,
            };
            for (const auto &feature : requiredFeatures)
            {
                if (!openGLFeatures().testFlag(feature))
                {
                    qDebug() << "OpenGL feature reported as not available:" << feature;
                    //throw InitError("GLInfo::init", "Required OpenGL feature missing: " +
                    //                String("%1").arg(feature));
                }
            }
            */
        }
#elif defined (DE_OPENGL)
        {
            try
            {
                debug("[GLInfo] calling glbinding initialize");
#if 0 // glbinding 3.0
                glbinding::Binding::initialize(
                    reinterpret_cast<glbinding::ProcAddress (*)(const char *)>(
                        SDL_GL_GetProcAddress),
                    false);
#endif
                glbinding::Binding::initialize();
            }
            catch (const std::exception &x)
            {
                debug("[GLInfo] failed to init bindings");
                throw InitError("GLInfo::init",
                                "Failed to initialize OpenGL: " + std::string(x.what()));
            }
        }
#endif

        inited = true;

        debug("[GLInfo] querying extensions and caps");

        // Extensions.
        ext.EXT_texture_compression_s3tc        = query("GL_EXT_texture_compression_s3tc");
        ext.EXT_texture_filter_anisotropic      = query("GL_EXT_texture_filter_anisotropic");
        ext.NV_framebuffer_multisample_coverage = query("GL_NV_framebuffer_multisample_coverage");
        ext.NV_texture_barrier                  = query("GL_NV_texture_barrier");
        ext.KHR_debug                           = query("GL_KHR_debug");
        ext.OES_rgb8_rgba8                      = query("GL_OES_rgb8_rgba8");

#if defined (DE_ENABLE_OPENGL_DEBUG_LOGGER)
        {
            gl::glDebugMessageCallback(debugMessageCallback, nullptr);
            glEnable(gl::GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glEnable(gl::GL_DEBUG_OUTPUT);
            
            if (!ext.KHR_debug)
            {
                debug("[GLInfo] GL_KHR_debug is not available");
            }
            else
            {
                debug("[GLInfo] debug output enabled");
            }
        }
#endif

        // Limits.
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,        reinterpret_cast<GLint *>(&lim.maxTexSize));
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, reinterpret_cast<GLint *>(&lim.maxTexUnits)); // at least 16
        LIBGUI_ASSERT_GL_OK();
        
#if defined (DE_OPENGL)
        {
            glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE,       &lim.smoothLineWidth.start);
            LIBGUI_ASSERT_GL_OK();
            glGetFloatv(GL_SMOOTH_LINE_WIDTH_GRANULARITY, &lim.smoothLineWidthGranularity);
            LIBGUI_ASSERT_GL_OK();
        }
        LIBGUI_ASSERT_GL_OK();

        if (ext.EXT_texture_filter_anisotropic)
        {
            glGetIntegerv(gl33ext::GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &lim.maxTexFilterAniso);
        }
#endif

        // Set a custom maximum size?
        if (CommandLine_CheckWith("-maxtex", 1))
        {
            lim.maxTexSize = min(ceilPow2(String(CommandLine_Next()).toInt()),
                                 lim.maxTexSize);

            LOG_GL_NOTE("Using requested maximum texture size of %i x %i")
                << lim.maxTexSize << lim.maxTexSize;
        }
    }
};

GLInfo::GLInfo() : d(new Impl)
{}

void GLInfo::glInit()
{
    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

    info.d->init();
}

void GLInfo::glDeinit()
{
    #ifdef DE_ENABLE_OPENGL_DEBUG_LOGGER
    {
        glDisable(gl::GL_DEBUG_OUTPUT);
    }
    #endif
    info.d.reset();
}

//QOpenGLFunctions_Doomsday &GLInfo::api() // static
//{
//    DE_ASSERT(QOpenGLContext::currentContext() != nullptr);
//    DE_ASSERT(info.d->inited);
//    return *info.d;
//}

/*
QOpenGLExtension_ARB_draw_instanced *GLInfo::ARB_draw_instanced()
{
    DE_ASSERT(info.d->inited);
    return info.d->ARB_draw_instanced.get();
}

QOpenGLExtension_ARB_instanced_arrays *GLInfo::ARB_instanced_arrays()
{
    DE_ASSERT(info.d->inited);
    return info.d->ARB_instanced_arrays.get();
}

QOpenGLExtension_EXT_framebuffer_blit *GLInfo::EXT_framebuffer_blit()
{
    DE_ASSERT(info.d->inited);
    return info.d->EXT_framebuffer_blit.get();
}

QOpenGLExtension_EXT_framebuffer_multisample *GLInfo::EXT_framebuffer_multisample()
{
    DE_ASSERT(info.d->inited);
    return info.d->EXT_framebuffer_multisample.get();
}

QOpenGLExtension_EXT_framebuffer_object *GLInfo::EXT_framebuffer_object()
{
    DE_ASSERT(info.d->inited);
    return info.d->EXT_framebuffer_object.get();
}
*/

//#if defined (DE_OPENGL)
//QOpenGLExtension_NV_framebuffer_multisample_coverage *GLInfo::NV_framebuffer_multisample_coverage()
//{
//    DE_ASSERT(info.d->inited);
//    return info.d->NV_framebuffer_multisample_coverage.get();
//}
//#endif

void GLInfo::setSwapInterval(int interval)
{
    DE_ASSERT(info.d->inited);
    SDL_GL_SetSwapInterval(interval);
}

const GLInfo::Extensions &GLInfo::extensions()
{
    DE_ASSERT(info.d->inited);
    return info.d->ext;
}

const GLInfo::Limits &GLInfo::limits()
{
    DE_ASSERT(info.d->inited);
    return info.d->lim;
}

bool GLInfo::isFramebufferMultisamplingSupported()
{
    return true;
}

void GLInfo::checkError(const char *file, int line)
{
    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();
    GLenum error = GL_NO_ERROR;
    do
    {
        error = glGetError();
        if (error != GL_NO_ERROR)
        {
            LogBuffer_Flush();
            warning("%s:%i: OpenGL error: 0x%x (%s)", file, line, error, LIBGUI_GL_ERROR_STR(error));
            DE_ASSERT_FAIL("OpenGL operation failed");
        }
    }
    while (error != GL_NO_ERROR);
}

} // namespace de
