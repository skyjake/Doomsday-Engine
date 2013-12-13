/** @file glinfo.cpp  OpenGL information.
 *
 * @authors Copyright (c) 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2007-2013 Daniel Swanson <danij@dengine.net>
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

#include "de/GLInfo"
#include "de/gui/opengl.h"

#include <cstring>
#include <de/String>
#include <de/Log>
#include <de/math.h>
#include <de/c_wrapper.h>

namespace de {

static GLInfo info;

DENG2_PIMPL_NOREF(GLInfo)
{
    bool inited;
    Extensions ext;
    Limits lim;

    Instance()
        : inited(false)
    {
        zap(ext);
        zap(lim);
    }

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
        if(c || *name == '\0')
            return false;

        if(!extensions)
            return false;

        // It takes a bit of care to be fool-proof about parsing the
        // OpenGL extensions string. Don't be fooled by sub-strings, etc.
        start = extensions;
        for(;;)
        {
            c = (GLubyte*) strstr((char const *) start, name);
            if(!c)
                break;

            terminator = c + strlen(name);
            if(c == start || *(c - 1) == ' ')
            {
                if(*terminator == ' ' || *terminator == '\0')
                {
                    return true;
                }
            }
            start = terminator;
        }

        return false;
    }

    bool query(char const *ext)
    {
        DENG2_ASSERT(ext);

#ifdef WIN32
        // Prefer the wgl-specific extensions.
        if(checkExtensionString(ext, (GLubyte const *) wglGetExtensionsStringARB(wglGetCurrentDC())))
            return true;
#endif

        return checkExtensionString(ext, glGetString(GL_EXTENSIONS));
    }

    void init()
    {
        if(inited) return;

        // Extensions.
        ext.ARB_texture_env_combine        = query("GL_ARB_texture_env_combine") || query("GL_EXT_texture_env_combine");
        ext.ARB_texture_non_power_of_two   = query("GL_ARB_texture_non_power_of_two");

        ext.EXT_blend_subtract             = query("GL_EXT_blend_subtract");
        ext.EXT_framebuffer_blit           = query("GL_EXT_framebuffer_blit");
        ext.EXT_framebuffer_multisample    = query("GL_EXT_framebuffer_multisample");
        ext.EXT_texture_compression_s3tc   = query("GL_EXT_texture_compression_s3tc");
        ext.EXT_texture_filter_anisotropic = query("GL_EXT_texture_filter_anisotropic");

        ext.ATI_texture_env_combine3       = query("GL_ATI_texture_env_combine3");
        ext.NV_texture_env_combine4        = query("GL_NV_texture_env_combine4");
        ext.SGIS_generate_mipmap           = query("GL_SGIS_generate_mipmap");

#ifdef WIN32
        ext.Windows_ARB_multisample        = query("WGL_ARB_multisample");
        ext.Windows_EXT_swap_control       = query("WGL_EXT_swap_control");
#endif

        // Limits.
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,  (GLint *) &lim.maxTexSize);
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, (GLint *) &lim.maxTexUnits);

        if(ext.EXT_texture_filter_anisotropic)
        {
            glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, (GLint *) &lim.maxTexFilterAniso);
        }

        // Set a custom maximum size?
        if(CommandLine_CheckWith("-maxtex", 1))
        {
            lim.maxTexSize = min(ceilPow2(String(CommandLine_Next()).toInt()),
                                 lim.maxTexSize);

            LOG_INFO("Using requested maximum texture size of %i x %i") << lim.maxTexSize << lim.maxTexSize;
        }

        inited = true;
    }
};

GLInfo::GLInfo() : d(new Instance)
{}

void GLInfo::glInit()
{
    info.d->init();
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

bool GLInfo::supportsFramebufferMultisampling()
{
    return extensions().EXT_framebuffer_multisample &&
           extensions().EXT_framebuffer_blit;
}

} // namespace de
