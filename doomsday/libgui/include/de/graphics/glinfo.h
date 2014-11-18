/** @file glinfo.h  OpenGL information.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_GLINFO_H
#define LIBGUI_GLINFO_H

#include "../gui/libgui.h"

#include <de/libcore.h>

namespace de {

class LIBGUI_PUBLIC GLInfo
{
public:
    /// Extension availability bits.
    struct Extensions
    {
        duint32 ARB_draw_instanced : 1;
        //duint32 ARB_framebuffer_object : 1;
        duint32 ARB_instanced_arrays : 1;
        //duint32 ARB_texture_env_combine : 1;
        //duint32 ARB_texture_non_power_of_two : 1;

        //duint32 EXT_blend_subtract : 1;
        //duint32 EXT_framebuffer_blit : 1;
        //duint32 EXT_framebuffer_multisample : 1;
        //duint32 EXT_packed_depth_stencil : 1;
        duint32 EXT_texture_compression_s3tc : 1;
        duint32 EXT_texture_filter_anisotropic : 1;

        // Vendor-specific extensions:
        duint32 ATI_texture_env_combine3 : 1;
        duint32 NV_framebuffer_multisample_coverage : 1;
        duint32 NV_texture_env_combine4 : 1;
        duint32 SGIS_generate_mipmap : 1;

#ifdef WIN32
        duint32 Windows_ARB_multisample : 1;
        duint32 Windows_EXT_swap_control : 1;
#endif

#ifdef DENG_X11
        duint32 X11_EXT_swap_control : 1;
#endif
    };

    /// Implementation limits.
    struct Limits
    {
        int maxTexFilterAniso;
        int maxTexSize; ///< Texels.
        int maxTexUnits;
        int maxColorTexSamples;
        int maxDepthTexSamples;
        int maxSamples;
    };

    GLInfo();

    static Extensions const &extensions();
    static Limits const &limits();

    static bool isFramebufferMultisamplingSupported();

    /**
     * Initializes the static instance of GLInfo. Cannot be called before there
     * is a current OpenGL context. Canvas will call this after initialization.
     */
    static void glInit();

private:
    DENG2_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLINFO_H
