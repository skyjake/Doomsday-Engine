/**\file gl_defer.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/**
 * Deferred GL Tasks.
 */

#ifndef LIBDENG_GL_DEFERRED_H
#define LIBDENG_GL_DEFERRED_H

typedef enum {
    DEFERREDTASK_TYPES_FIRST = 0,
    DTT_UPLOAD_TEXTURECONTENT = DEFERREDTASK_TYPES_FIRST,
    DEFERREDTASK_TYPES_COUNT
} deferredtask_type_t;

#define VALID_DEFERREDTASK_TYPE(t)      ((t) >= DEFERREDTASK_TYPES_FIRST || (t) < DEFERREDTASK_TYPES_COUNT)

/// Initialize this module.
void GL_InitDeferredTask(void);

/// Shutdown this module.
void GL_ShutdownDeferredTask(void);

/// @return  Number of waiting tasks else @c 0
int GL_GetDeferredTaskCount(void);

/**
 * @param timeOutMilliSeconds  Zero for no timeout.
 */
void GL_RunDeferredTasks(uint timeOutMilliSeconds);

/**
 * @param type  Type of task to add.
 * @param data  Caller-supplied additional data ptr, linked with the task.
 */
void GL_EnqueueDeferredTask(deferredtask_type_t type, void* data);

DGLuint GL_GetReservedTextureName(void);
void GL_ReserveNames(void);
void GL_ReleaseReservedNames(void);

#endif /* LIBDENG_GL_DEFERRED_H */
