/**
 * @file gl_deferredapi.h
 * GL API deferring. @ingroup gl
 *
 * Redefines GL API functions so that they're replaced with ones that defer the
 * call when needed.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2012 Daniel Swanson <danij@dengine.net>
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

#include "sys_opengl.h"

#define glEnable(x)     Deferred_glEnable(x)
#define glDisable(x)    Deferred_glDisable(x)

void Deferred_glEnable(GLenum e);
void Deferred_glDisable(GLenum e);

#endif // LIBDENG_DEFERRED_GL_API_H
