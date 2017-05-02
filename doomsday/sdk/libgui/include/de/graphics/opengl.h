/** @file opengl.h  Headers for OpenGL 2.1.
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

#ifndef LIBGUI_SYSTEM_OPENGL_H
#define LIBGUI_SYSTEM_OPENGL_H

#ifdef glDeleteTextures
#  error "glDeleteTextures defined as a macro! (would be undefined by Qt)"
#endif

/*
 * OpenGL API selection
 */
#if (DENG_OPENGL == 330)
#  include <QOpenGLFunctions_3_3_Core>
#  define QOpenGLFunctions_Doomsday QOpenGLFunctions_3_3_Core
#  ifndef GL_VERSION_3_3
#    error "OpenGL 3.3 (or newer) headers not found"
#  endif

#elif (DENG_OPENGL_ES == 30)
#  include <QOpenGLExtraFunctions>
#  define QOpenGLFunctions_Doomsday QOpenGLExtraFunctions

#elif (DENG_OPENGL_ES == 20)
#  include <QOpenGLFunctions>
#  define QOpenGLFunctions_Doomsday QOpenGLFunctions
#endif

#include <QOpenGLExtensions>

// Defined in GLES2.
#ifndef GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS
#  define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS 0x8CD9
#endif

#endif // LIBGUI_SYSTEM_OPENGL_H
