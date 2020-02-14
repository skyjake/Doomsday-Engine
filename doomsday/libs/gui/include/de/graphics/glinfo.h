/** @file glinfo.h  OpenGL information.
 *
 * @authors Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/libcore.h>
#include <de/GuiApp> // checking for render thread
#include <de/Range>
#include "../libgui.h"
#include "de/graphics/opengl.h"

#ifndef NDEBUG
#  define LIBGUI_ASSERT_GL_OK()     de::GLInfo::checkError(__FILE__, __LINE__)
#else
#  define LIBGUI_ASSERT_GL_OK()
#endif

#define LIBGUI_ASSERT_GL_CONTEXT_ACTIVE()  DE_ASSERT(SDL_GL_GetCurrentContext() != nullptr)

namespace de {

/**
 * OpenGL hardware and driver capabilities.
 *
 * @ingroup gl
 */
class LIBGUI_PUBLIC GLInfo
{
public:
    DE_ERROR(InitError);

    /// Extension availability bits.
    struct Extensions
    {
        duint32 EXT_texture_compression_s3tc : 1;
        duint32 EXT_texture_filter_anisotropic : 1;

        // Vendor-specific extensions:
        duint32 NV_framebuffer_multisample_coverage : 1;
        duint32 NV_texture_barrier : 1;

        duint32 KHR_debug : 1;

// #ifdef WIN32
//         //duint32 Windows_ARB_multisample : 1;
//         duint32 Windows_EXT_swap_control : 1;
// #endif

// #ifdef DE_X11
//         duint32 X11_EXT_swap_control : 1;
//         duint32 X11_SGI_swap_control : 1;
//         duint32 X11_MESA_swap_control : 1;
// #endif
    };

    /// Implementation limits.
    struct Limits
    {
        int maxTexFilterAniso = 0;
        int maxTexSize = 0; ///< Texels.
        int maxTexUnits = 0;
        Rangef smoothLineWidth;
        float smoothLineWidthGranularity = 0.f;
    };

    GLInfo();

    static const Extensions &extensions();
    static const Limits &limits();

    static bool isFramebufferMultisamplingSupported();

    /**
     * Initializes the static instance of GLInfo. Cannot be called before there
     * is a current OpenGL context. Canvas will call this after initialization.
     */
    static void glInit();

    static void glDeinit();

    static void setSwapInterval(int interval);

    static void checkError(const char *file, int line);

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBGUI_GLINFO_H
