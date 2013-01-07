/**
 * @file gl_deferredapi.h
 * GL API deferring. @ingroup gl
 *
 * Redefines GL API functions so that they're replaced with ones that defer the
 * call when needed.
 *
 * @note  Only the GL API functions declared in this file are safe to call from
 * outside the main thread!
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_DEFERRED_GL_API_H
#define LIBDENG_DEFERRED_GL_API_H

#ifdef __CLIENT__

/**
 * @def LIBDENG_DISABLE_DEFERRED_GL_API
 * Disables the automatic rerouting of GL API calls to the deferring queue.
 * Put this in the beginning of a source file, before all #includes.
 */
#ifndef LIBDENG_DISABLE_DEFERRED_GL_API

#include "sys_opengl.h" // ensure native OpenGL has been included

#ifdef __cplusplus
extern "C" {
#endif

#define glEnable(x)             Deferred_glEnable(x)
#define glDisable(x)            Deferred_glDisable(x)
#define glDeleteTextures(x, y)  Deferred_glDeleteTextures(x, y)
#define glFogi(x, y)            Deferred_glFogi(x, y)
#define glFogf(x, y)            Deferred_glFogf(x, y)
#define glFogfv(x, y)           Deferred_glFogfv(x, y)

void Deferred_glEnable(GLenum e);
void Deferred_glDisable(GLenum e);
void Deferred_glDeleteTextures(GLsizei num, const GLuint* names);
void Deferred_glFogi(GLenum p, GLint v);
void Deferred_glFogf(GLenum p, GLfloat v);
void Deferred_glFogfv(GLenum p, const GLfloat* v);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __CLIENT__

#endif // LIBDENG_DISABLE_DEFERRED_GL_API
#endif // LIBDENG_DEFERRED_GL_API_H
