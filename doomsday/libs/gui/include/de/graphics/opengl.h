/** @file opengl.h  Headers for OpenGL.
 *
 * @ingroup gl
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

#ifndef LIBGUI_GRAPHICS_OPENGL_H
#define LIBGUI_GRAPHICS_OPENGL_H

#include <SDL_video.h>

/*
 * OpenGL API selection
 */
#if (DE_OPENGL == 330)
#  include <glbinding/gl33core/gl.h>
#  include <glbinding/gl33ext/enum.h>
using namespace gl33core;
//#  include <QOpenGLFunctions_3_3_Core>
//#  include <QOpenGLExtensions>
//#  define QOpenGLFunctions_Doomsday QOpenGLFunctions_3_3_Core
//#  ifndef GL_VERSION_3_3
//#    error "OpenGL 3.3 (or newer) headers not found"
//#  endif
#  define DE_HAVE_TIMER_QUERY

#elif (DE_OPENGL_ES == 30)
#  include <QOpenGLExtraFunctions>
#  include <QOpenGLExtensions>
#  define QOpenGLFunctions_Doomsday QOpenGLExtraFunctions

#elif (DE_OPENGL_ES == 20)
#  include <QOpenGLFunctions>
#  include <QOpenGLExtensions>
#  define QOpenGLFunctions_Doomsday QOpenGLFunctions
#endif

// Defined in GLES2.
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#  define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#endif

#if !defined (DE_OPENGL_ES) || DE_OPENGL_ES > 20
#  define DE_HAVE_VAOS
#  define DE_HAVE_INSTANCES
#  define DE_HAVE_BLIT_FRAMEBUFFER
#endif

#endif // LIBGUI_GRAPHICS_OPENGL_H
