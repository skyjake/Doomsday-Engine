/** @file gl_defer.h  Deferred GL tasks.
 *
 * @ingroup gl
 *
 * GL is only available from the main thread. When accessed from other threads,
 * the operations need to be deferred for processing later in the main thread.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_CLIENT_GL_DEFERRED_H
#define DENG_CLIENT_GL_DEFERRED_H

#include "api_gl.h"
#include "sys_opengl.h"

namespace de
{
    namespace gl
    {
        enum UploadMethod  {
            Immediate, ///< Upload the data immediately.
            Deferred   ///< Defer the data upload until convenient.
        };
    }
}

struct texturecontent_s;

/**
 * Initializes the deferred tasks module.
 */
void GL_InitDeferredTask();

/**
 * Shuts down the deferred tasks.
 */
void GL_ShutdownDeferredTask();

/**
 * Clears the currently queued GL tasks. They will not be executed.
 */
void GL_PurgeDeferredTasks();

/**
 * @return  Number of GL tasks waiting to be carried out.
 */
int GL_DeferredTaskCount();

/**
 * Processes deferred GL tasks. This must be called from the main thread.
 *
 * @param timeOutMilliSeconds  Processing will continue until this timeout expires.
 *                             Use zero for no timeout.
 */
void GL_ProcessDeferredTasks(uint timeOutMilliSeconds);

DGLuint GL_GetReservedTextureName();

void GL_ReserveNames();

void GL_ReleaseReservedNames();

/**
 * Returns the chosen method for uploading the given texture @a content.
 */
de::gl::UploadMethod GL_ChooseUploadMethod(struct texturecontent_s const *content);

/**
 * Adds a new deferred texture upload task to the queue.
 *
 * @param content  Texture content to upload. Caller can free its copy of the content;
 *                 a copy is made for the deferred task.
 */
void GL_DeferTextureUpload(struct texturecontent_s const *content);

void GL_DeferSetVSync(dd_bool enableVSync);

#ifdef __cplusplus
extern "C" {
#endif

// Deferring functions for various function signatures.
#define LIBDENG_GL_DEFER1(form, x)          void GL_Defer_##form(void (GL_CALL *ptr)(x), x)
#define LIBDENG_GL_DEFER2(form, x, y)       void GL_Defer_##form(void (GL_CALL* ptr)(x, y), x, y)
#define LIBDENG_GL_DEFER3(form, x, y, z)    void GL_Defer_##form(void (GL_CALL* ptr)(x, y, z), x, y, z)
#define LIBDENG_GL_DEFER4(form, x, y, z, w) void GL_Defer_##form(void (GL_CALL* ptr)(x, y, z, w), x, y, z, w)

LIBDENG_GL_DEFER1(e,         GLenum e);
LIBDENG_GL_DEFER2(i,         GLenum e, GLint i);
LIBDENG_GL_DEFER2(f,         GLenum e, GLfloat f);
LIBDENG_GL_DEFER2(fv4,       GLenum e, const GLfloat* floatArrayFourValues);
LIBDENG_GL_DEFER2(uintArray, GLsizei count, const GLuint* values);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DENG_CLIENT_GL_DEFERRED_H
