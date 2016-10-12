/** @file glinfo.cpp  OpenGL information.
 *
 * @authors Copyright (c) 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include "de/graphics/opengl.h"

#include <cstring>
#include <de/String>
#include <de/Log>
#include <de/math.h>
#include <de/c_wrapper.h>

namespace de {

static GLInfo info;

DENG2_PIMPL_NOREF(GLInfo), public QOpenGLFunctions_Doomsday
{
    bool inited = false;
    Extensions ext;
    Limits lim;

    std::unique_ptr<QOpenGLExtension_ARB_draw_instanced>          ARB_draw_instanced;
    std::unique_ptr<QOpenGLExtension_ARB_instanced_arrays>        ARB_instanced_arrays;
    std::unique_ptr<QOpenGLExtension_EXT_framebuffer_blit>        EXT_framebuffer_blit;
    std::unique_ptr<QOpenGLExtension_EXT_framebuffer_multisample> EXT_framebuffer_multisample;
    std::unique_ptr<QOpenGLExtension_EXT_framebuffer_object>      EXT_framebuffer_object;
    std::unique_ptr<QOpenGLExtension_NV_framebuffer_multisample_coverage> NV_framebuffer_multisample_coverage;

#ifdef WIN32
    BOOL (APIENTRY *wglSwapIntervalEXT)(int interval) = nullptr;
#endif

    Impl()
    {
        zap(ext);
        zap(lim);
    }

#if 0
    /**
     * This routine is based on the method used by David Blythe and Tom
     * McReynolds in the book "Advanced Graphics Programming Using OpenGL"
     * ISBN: 1-55860-659-9.
     */
    bool checkExtensionString(char const *name, GLubyte const *extensions)
    {
        GLubyte const *start;
        GLubyte *c, *terminator;

        // Extension names should not have spaces.
        c = (GLubyte *) strchr(name, ' ');
        if (c || *name == '\0')
            return false;

        if (!extensions)
            return false;

        // It takes a bit of care to be fool-proof about parsing the
        // OpenGL extensions string. Don't be fooled by sub-strings, etc.
        start = extensions;
        for (;;)
        {
            c = (GLubyte*) strstr((char const *) start, name);
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
#endif

    bool doQuery(char const *ext)
    {
        DENG2_ASSERT(ext);
#if 0
#ifdef WIN32
        // Prefer the wgl-specific extensions.
        if (wglGetExtensionsStringARB != nullptr &&
           checkExtensionString(ext, (GLubyte const *)wglGetExtensionsStringARB(wglGetCurrentDC())))
            return true;
#endif

#ifdef DENG_X11
        // Check GLX specific extensions.
        if (checkExtensionString(ext, (GLubyte const *) getGLXExtensionsString()))
            return true;
#endif

        return checkExtensionString(ext, glGetString(GL_EXTENSIONS));
#endif
        DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);

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

        if (!initializeOpenGLFunctions())
        {
            throw InitError("GLInfo::init", "Failed to initialize OpenGL");
        }

        // Extensions.
        ext.ARB_draw_instanced             = query("GL_ARB_draw_instanced");
        ext.ARB_instanced_arrays           = query("GL_ARB_instanced_arrays");
        ext.ARB_texture_env_combine        = query("GL_ARB_texture_env_combine") || query("GL_EXT_texture_env_combine");
        ext.ARB_texture_non_power_of_two   = query("GL_ARB_texture_non_power_of_two");

        ext.EXT_blend_subtract             = query("GL_EXT_blend_subtract");
        ext.EXT_framebuffer_blit           = query("GL_EXT_framebuffer_blit");
        ext.EXT_framebuffer_multisample    = query("GL_EXT_framebuffer_multisample");
        ext.EXT_framebuffer_object         = query("GL_EXT_framebuffer_object");
        ext.EXT_packed_depth_stencil       = query("GL_EXT_packed_depth_stencil");
        ext.EXT_texture_compression_s3tc   = query("GL_EXT_texture_compression_s3tc");
        ext.EXT_texture_filter_anisotropic = query("GL_EXT_texture_filter_anisotropic");
        ext.EXT_timer_query                = query("GL_EXT_timer_query");

        ext.ATI_texture_env_combine3       = query("GL_ATI_texture_env_combine3");
        ext.NV_framebuffer_multisample_coverage
                                           = query("GL_NV_framebuffer_multisample_coverage");
        ext.NV_texture_env_combine4        = query("GL_NV_texture_env_combine4");
        ext.SGIS_generate_mipmap           = query("GL_SGIS_generate_mipmap");

#ifdef WIN32
        ext.Windows_ARB_multisample        = query("WGL_ARB_multisample");
        ext.Windows_EXT_swap_control       = query("WGL_EXT_swap_control");

        if (ext.Windows_EXT_swap_control)
        {
            wglSwapIntervalEXT = de::function_cast<decltype(wglSwapIntervalEXT)>
                (QOpenGLContext::currentContext()->getProcAddress("wglSwapIntervalEXT"));
        }
#endif

#ifdef DENG_X11
        ext.X11_EXT_swap_control           = query("GLX_EXT_swap_control");
#endif

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
        }
        if (ext.NV_framebuffer_multisample_coverage)
        {
            NV_framebuffer_multisample_coverage.reset(new QOpenGLExtension_NV_framebuffer_multisample_coverage);
            NV_framebuffer_multisample_coverage->initializeOpenGLFunctions();
        }

        // Limits.
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,  (GLint *) &lim.maxTexSize);
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, (GLint *) &lim.maxTexUnits);

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

        inited = true;
    }
};

GLInfo::GLInfo() : d(new Impl)
{}

void GLInfo::glInit()
{
    info.d->init();
}

void GLInfo::glDeinit()
{
    info.d.reset();
}

QOpenGLFunctions_Doomsday &GLInfo::api() // static
{
    DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);
    DENG2_ASSERT(info.d->inited);
    return *info.d;
}

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

QOpenGLExtension_NV_framebuffer_multisample_coverage *GLInfo::NV_framebuffer_multisample_coverage()
{
    DENG2_ASSERT(info.d->inited);
    return info.d->NV_framebuffer_multisample_coverage.get();
}

void GLInfo::setSwapInterval(int interval)
{
    DENG2_ASSERT(info.d->inited);

#if defined (WIN32)
    if (extensions().Windows_EXT_swap_control)
    {
        info.d->wglSwapIntervalEXT(interval);
    }
#endif

#if defined (MACOSX)
    {
        CGLContextObj context = CGLGetCurrentContext();
        DENG2_ASSERT(context != nullptr);
        if (context)
        {
            GLint params[1] = { interval };
            CGLSetParameter(context, kCGLCPSwapInterval, params);
        }
    }
#endif

#if defined (Q_WS_X11)
    {
        //setXSwapInterval(on? 1 : 0);
    }
#endif
}

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

/*bool GLInfo::isFramebufferMultisamplingSupported()
{
    return extensions().EXT_framebuffer_multisample &&
           extensions().EXT_framebuffer_blit;
}*/

} // namespace de
