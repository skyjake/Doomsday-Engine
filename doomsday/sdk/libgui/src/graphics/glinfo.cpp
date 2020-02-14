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

#include "de/GLInfo"
#include "de/GLWindow"
#include "de/graphics/opengl.h"

#include <cstring>
#include <de/String>
#include <de/Log>
#include <de/math.h>
#include <de/c_wrapper.h>

#ifdef DENG2_DEBUG
#  define DENG_ENABLE_OPENGL_DEBUG_LOGGER
#  include <QOpenGLDebugLogger>
#endif

#if defined (MACOSX)
#  include <OpenGL/OpenGL.h>
#endif

#if defined (DENG_X11)
#  include <QX11Info>
#  include <GL/glx.h>
#  include <GL/glxext.h>
#  undef None
#  undef Always
#  undef Status
#endif

namespace de {

static GLInfo info;

DENG2_PIMPL_NOREF(GLInfo), public QOpenGLFunctions_Doomsday
{
    bool inited = false;
    Extensions ext;
    Limits lim;

    //std::unique_ptr<QOpenGLExtension_ARB_draw_instanced>          ARB_draw_instanced;
    //std::unique_ptr<QOpenGLExtension_ARB_instanced_arrays>        ARB_instanced_arrays;
    //std::unique_ptr<QOpenGLExtension_EXT_framebuffer_blit>        EXT_framebuffer_blit;
    //std::unique_ptr<QOpenGLExtension_EXT_framebuffer_multisample> EXT_framebuffer_multisample;
    //std::unique_ptr<QOpenGLExtension_EXT_framebuffer_object>      EXT_framebuffer_object;
#if defined (DENG_OPENGL)
    std::unique_ptr<QOpenGLExtension_NV_framebuffer_multisample_coverage> NV_framebuffer_multisample_coverage;
    std::unique_ptr<QOpenGLExtension_NV_texture_barrier>                  NV_texture_barrier;
#endif

#ifdef WIN32
    BOOL (APIENTRY *wglSwapIntervalEXT)(int interval) = nullptr;
#endif

#ifdef DENG_X11
    PFNGLXSWAPINTERVALEXTPROC  glXSwapIntervalEXT  = nullptr;
    PFNGLXSWAPINTERVALSGIPROC  glXSwapIntervalSGI  = nullptr;
    PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA = nullptr;
#endif

#ifdef DENG_ENABLE_OPENGL_DEBUG_LOGGER
    QOpenGLDebugLogger *logger = nullptr;
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
    bool checkExtensionString(char const *name, char const *extensions)
    {
        GLubyte const *start;
        GLubyte const *c, *terminator;

        // Extension names should not have spaces.
        c = (GLubyte const *) strchr(name, ' ');
        if (c || *name == '\0')
            return false;

        if (!extensions)
            return false;

        // It takes a bit of care to be fool-proof about parsing the
        // OpenGL extensions string. Don't be fooled by sub-strings, etc.
        start = (GLubyte const *) extensions;
        for (;;)
        {
            c = (GLubyte const *) strstr((char const *) start, name);
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

    bool doQuery(char const *ext)
    {
        DENG2_ASSERT(ext);
        DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);

#if 0
#ifdef WIN32
        // Prefer the wgl-specific extensions.
        if (wglGetExtensionsStringARB != nullptr &&
           checkExtensionString(ext, (GLubyte const *)wglGetExtensionsStringARB(wglGetCurrentDC())))
            return true;
#endif
#endif

#ifdef DENG_X11
        // Check GLX specific extensions.
        if (checkExtensionString(ext, glXQueryExtensionsString(QX11Info::display(),
                                                               QX11Info::appScreen())))
            return true;
#endif

//        return checkExtensionString(ext, glGetString(GL_EXTENSIONS));
//#endif

        return QOpenGLContext::currentContext()->hasExtension(ext);
    }

    bool query(char const *ext)
    {
        bool found = doQuery(ext);
        LOGDEV_GL_VERBOSE("%s: %b") << ext << found;
        return found;
    }

    void init()
    {
        LOG_AS("GLInfo");

        if (inited) return;

        #if defined (DENG_OPENGL_ES)
        {
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
            for (auto const &feature : requiredFeatures)
            {
                if (!openGLFeatures().testFlag(feature))
                {
                    qDebug() << "OpenGL feature reported as not available:" << feature;
                    //throw InitError("GLInfo::init", "Required OpenGL feature missing: " +
                    //                String("%1").arg(feature));
                }
            }
        }
        #else
        {
            if (!initializeOpenGLFunctions())
            {
                throw InitError("GLInfo::init", "Failed to initialize OpenGL");
            }
        }
        #endif

        inited = true;

        // Extensions.
        //ext.ARB_draw_instanced             = query("GL_ARB_draw_instanced");
        //ext.ARB_instanced_arrays           = query("GL_ARB_instanced_arrays");
        //ext.ARB_texture_env_combine        = query("GL_ARB_texture_env_combine") || query("GL_EXT_texture_env_combine");
        //ext.ARB_texture_non_power_of_two   = query("GL_ARB_texture_non_power_of_two");

        //ext.EXT_blend_subtract             = query("GL_EXT_blend_subtract");
        //ext.EXT_framebuffer_blit           = query("GL_EXT_framebuffer_blit");
        //ext.EXT_framebuffer_multisample    = query("GL_EXT_framebuffer_multisample");
        //ext.EXT_framebuffer_object         = query("GL_EXT_framebuffer_object");
        //ext.EXT_packed_depth_stencil       = query("GL_EXT_packed_depth_stencil");
        ext.EXT_texture_compression_s3tc   = query("GL_EXT_texture_compression_s3tc");
        ext.EXT_texture_filter_anisotropic = query("GL_EXT_texture_filter_anisotropic");
        //ext.EXT_timer_query                = query("GL_EXT_timer_query");

        //ext.ATI_texture_env_combine3       = query("GL_ATI_texture_env_combine3");
        ext.NV_framebuffer_multisample_coverage
                                           = query("GL_NV_framebuffer_multisample_coverage");
        ext.NV_texture_barrier             = query("GL_NV_texture_barrier");
        //ext.NV_texture_env_combine4        = query("GL_NV_texture_env_combine4");
        //ext.SGIS_generate_mipmap           = query("GL_SGIS_generate_mipmap");

        ext.KHR_debug                      = query("GL_KHR_debug");

        #ifdef WIN32
        {
            //ext.Windows_ARB_multisample        = query("WGL_ARB_multisample");
            ext.Windows_EXT_swap_control       = query("WGL_EXT_swap_control");

            if (ext.Windows_EXT_swap_control)
            {
                wglSwapIntervalEXT = de::function_cast<decltype(wglSwapIntervalEXT)>
                        (QOpenGLContext::currentContext()->getProcAddress("wglSwapIntervalEXT"));
            }
        }
        #endif

        #ifdef DENG_X11
        {
            ext.X11_EXT_swap_control           = query("GLX_EXT_swap_control");
            ext.X11_SGI_swap_control           = query("GLX_SGI_swap_control");
            ext.X11_MESA_swap_control          = query("GLX_MESA_swap_control");

            if (ext.X11_EXT_swap_control)
            {
                glXSwapIntervalEXT = de::function_cast<decltype(glXSwapIntervalEXT)>
                        (glXGetProcAddress(reinterpret_cast<GLubyte const *>("glXSwapIntervalEXT")));
            }
            if (ext.X11_SGI_swap_control)
            {
                glXSwapIntervalSGI = de::function_cast<decltype(glXSwapIntervalSGI)>
                        (glXGetProcAddress(reinterpret_cast<GLubyte const *>("glXSwapIntervalSGI")));
            }
            if (ext.X11_MESA_swap_control)
            {
                glXSwapIntervalMESA = de::function_cast<decltype(glXSwapIntervalMESA)>
                        (glXGetProcAddress(reinterpret_cast<GLubyte const *>("glXSwapIntervalMESA")));
            }
        }
        #endif

        #ifdef DENG_ENABLE_OPENGL_DEBUG_LOGGER
        {
            logger = new QOpenGLDebugLogger(&GLWindow::main());
            if (!ext.KHR_debug)
            {
                qDebug() << "[GLInfo] GL_KHR_debug is not available";
            }
            else if (!logger->initialize())
            {
                qWarning() << "[GLInfo] Failed to initialize debug logger!";
            }
            else
            {
                QObject::connect(logger, &QOpenGLDebugLogger::messageLogged,
                                 [] (QOpenGLDebugMessage const &debugMessage)
                {
                    if (debugMessage.severity() == QOpenGLDebugMessage::NotificationSeverity)
                    {
                        // Too verbose.
                        return;
                    }

                    char const *mType = "--";
                    char const *mSeverity = "--";

                    switch (debugMessage.type())
                    {
                    case QOpenGLDebugMessage::ErrorType:
                        mType = "ERROR";
                        break;
                    case QOpenGLDebugMessage::DeprecatedBehaviorType:
                        mType = "Deprecated";
                        break;
                    case QOpenGLDebugMessage::UndefinedBehaviorType:
                        mType = "Undefined";
                        break;
                    case QOpenGLDebugMessage::PortabilityType:
                        mType = "Portability";
                        break;
                    case QOpenGLDebugMessage::PerformanceType:
                        mType = "Performance";
                        break;
                    case QOpenGLDebugMessage::OtherType:
                        mType = "Other";
                        break;
                    case QOpenGLDebugMessage::MarkerType:
                        mType = "Marker";
                        break;
                    case QOpenGLDebugMessage::GroupPushType:
                        mType = "Group Push";
                        break;
                    case QOpenGLDebugMessage::GroupPopType:
                        mType = "Group Pop";
                        break;
                    default:
                        break;
                    }

                    switch (debugMessage.severity())
                    {
                    case QOpenGLDebugMessage::HighSeverity:
                        mType = " HIGH ";
                        break;
                    case QOpenGLDebugMessage::MediumSeverity:
                        mType = "MEDIUM";
                        break;
                    case QOpenGLDebugMessage::LowSeverity:
                        mType = " low  ";
                        break;
                    case QOpenGLDebugMessage::NotificationSeverity:
                        mType = " note ";
                        break;
                    default:
                        break;
                    }

                    qDebug("[OpenGL] %04x %s (%s): %s",
                           debugMessage.id(),
                           mType,
                           mSeverity,
                           debugMessage.message().toLatin1().constData());
                });
                logger->startLogging(QOpenGLDebugLogger::SynchronousLogging);
            }
        }
        #endif

        /*
        if (ext.ARB_draw_instanced)
        {
            ARB_draw_instanced.reset(new QOpenGLExtension_ARB_draw_instanced);
            ARB_draw_instanced->initializeOpenGLFunctions();
        }
        if (ext.ARB_instanced_arrays)
        {
            ARB_instanced_arrays.reset(new QOpenGLExtension_ARB_instanced_arrays);
            ARB_instanced_arrays->initializeOpenGLFunctions();
        }
        if (ext.EXT_framebuffer_blit)
        {
            EXT_framebuffer_blit.reset(new QOpenGLExtension_EXT_framebuffer_blit);
            EXT_framebuffer_blit->initializeOpenGLFunctions();
        }
        if (ext.EXT_framebuffer_multisample)
        {
            EXT_framebuffer_multisample.reset(new QOpenGLExtension_EXT_framebuffer_multisample);
            EXT_framebuffer_multisample->initializeOpenGLFunctions();
        }
        if (ext.EXT_framebuffer_object)
        {
            EXT_framebuffer_object.reset(new QOpenGLExtension_EXT_framebuffer_object);
            EXT_framebuffer_object->initializeOpenGLFunctions();
        }*/

        #if defined (DENG_OPENGL)
        {
            if (ext.NV_framebuffer_multisample_coverage)
            {
                NV_framebuffer_multisample_coverage.reset(new QOpenGLExtension_NV_framebuffer_multisample_coverage);
                NV_framebuffer_multisample_coverage->initializeOpenGLFunctions();
            }
            if (ext.NV_texture_barrier)
            {
                NV_texture_barrier.reset(new QOpenGLExtension_NV_texture_barrier);
                NV_texture_barrier->initializeOpenGLFunctions();
            }
        }
        #endif

        // Limits.
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,              reinterpret_cast<GLint *>(&lim.maxTexSize));
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,       reinterpret_cast<GLint *>(&lim.maxTexUnits)); // at least 16
        #if defined (DENG_OPENGL)
        {
            glGetFloatv(GL_SMOOTH_LINE_WIDTH_RANGE,       &lim.smoothLineWidth.start);
            glGetFloatv(GL_SMOOTH_LINE_WIDTH_GRANULARITY, &lim.smoothLineWidthGranularity);
        }
        #endif

        LIBGUI_ASSERT_GL_OK();

        if (ext.EXT_texture_filter_anisotropic)
        {
            glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *) &lim.maxTexFilterAniso);
        }

        // Set a custom maximum size?
        if (CommandLine_CheckWith("-maxtex", 1))
        {
            lim.maxTexSize = min(ceilPow2(String(CommandLine_Next()).toInt()),
                                 lim.maxTexSize);

            LOG_GL_NOTE("Using requested maximum texture size of %i x %i") << lim.maxTexSize << lim.maxTexSize;
        }

        // Check default OpenGL format attributes.
        QOpenGLContext const *ctx = QOpenGLContext::currentContext();
        QSurfaceFormat form = ctx->format();

        LOGDEV_GL_MSG("Initial OpenGL format:");
        LOGDEV_GL_MSG(" - version: %i.%i") << form.majorVersion() << form.minorVersion();
        LOGDEV_GL_MSG(" - profile: %s") << (form.profile() == QSurfaceFormat::CompatibilityProfile? "Compatibility" : "Core");
        LOGDEV_GL_MSG(" - color: R%i G%i B%i A%i bits") << form.redBufferSize() << form.greenBufferSize() << form.blueBufferSize() << form.alphaBufferSize();
        LOGDEV_GL_MSG(" - depth: %i bits") << form.depthBufferSize();
        LOGDEV_GL_MSG(" - stencil: %i bits") << form.stencilBufferSize();
        LOGDEV_GL_MSG(" - samples: %i") << form.samples();
        LOGDEV_GL_MSG(" - swap behavior: %i") << form.swapBehavior();

        LIBGUI_ASSERT_GL_OK();
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
    #ifdef DENG_ENABLE_OPENGL_DEBUG_LOGGER
    {
        if (info.d->logger)
        {
            info.d->logger->stopLogging();
        }
    }
    #endif
    info.d.reset();
}

QOpenGLFunctions_Doomsday &GLInfo::api() // static
{
    DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);
    DENG2_ASSERT(info.d->inited);
    return *info.d;
}

/*
QOpenGLExtension_ARB_draw_instanced *GLInfo::ARB_draw_instanced()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->ARB_draw_instanced.get();
}

QOpenGLExtension_ARB_instanced_arrays *GLInfo::ARB_instanced_arrays()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->ARB_instanced_arrays.get();
}

QOpenGLExtension_EXT_framebuffer_blit *GLInfo::EXT_framebuffer_blit()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->EXT_framebuffer_blit.get();
}

QOpenGLExtension_EXT_framebuffer_multisample *GLInfo::EXT_framebuffer_multisample()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->EXT_framebuffer_multisample.get();
}

QOpenGLExtension_EXT_framebuffer_object *GLInfo::EXT_framebuffer_object()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->EXT_framebuffer_object.get();
}
*/

#if defined (DENG_OPENGL)
QOpenGLExtension_NV_framebuffer_multisample_coverage *GLInfo::NV_framebuffer_multisample_coverage()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->NV_framebuffer_multisample_coverage.get();
}

QOpenGLExtension_NV_texture_barrier *GLInfo::NV_texture_barrier()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->NV_texture_barrier.get();
}
#endif

void GLInfo::setSwapInterval(int interval)
{
    DENG2_ASSERT(info.d->inited);

    #if defined (WIN32)
    {
        if (extensions().Windows_EXT_swap_control)
        {
            info.d->wglSwapIntervalEXT(interval);
        }
    }
    #elif defined (MACOSX)
    {
        CGLContextObj context = CGLGetCurrentContext();
        DENG2_ASSERT(context != nullptr);
        if (context)
        {
            GLint params[1] = { interval };
            CGLSetParameter(context, kCGLCPSwapInterval, params);
        }
    }
    #elif defined (DENG_X11)
    {
        if (extensions().X11_SGI_swap_control)
        {
            info.d->glXSwapIntervalSGI(interval);
        }
        else if (extensions().X11_MESA_swap_control)
        {
            info.d->glXSwapIntervalMESA(uint(interval));
        }
        else if (extensions().X11_EXT_swap_control)
        {
            info.d->glXSwapIntervalEXT(QX11Info::display(),
                                       GLWindow::main().winId(),
                                       interval);
        }
    }
    #endif
}

#if 0
void GLInfo::setLineWidth(float lineWidth)
{
    #if defined (DENG_OPENGL)
    {
        LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();
        DENG2_ASSERT(info.d->inited);
        info.d->glLineWidth(info.d->lim.smoothLineWidth.clamp(lineWidth));
        LIBGUI_ASSERT_GL_OK();
    }
    #else
    {
        DENG2_UNUSED(lineWidth);
    }
    #endif
}
#endif

GLInfo::Extensions const &GLInfo::extensions()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->ext;
}

GLInfo::Limits const &GLInfo::limits()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->lim;
}

bool GLInfo::isFramebufferMultisamplingSupported()
{
    return true;
}

void GLInfo::checkError(char const *file, int line)
{
    GLuint error = GL_NO_ERROR;
    do
    {
        error = LIBGUI_GL.glGetError();
        if (error != GL_NO_ERROR)
        {
            LogBuffer_Flush();
            qWarning("%s:%i: OpenGL error: 0x%x (%s)", file, line, error,
                     LIBGUI_GL_ERROR_STR(error));
            LIBGUI_ASSERT_GL(0!="OpenGL operation failed");
        }
    }
    while (error != GL_NO_ERROR);
}

} // namespace de
