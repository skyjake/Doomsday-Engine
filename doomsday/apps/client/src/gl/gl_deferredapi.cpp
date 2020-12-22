/** @file gl_deferredapi.cpp GL API deferring.
 * @ingroup gl
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#define DE_DISABLE_DEFERRED_GL_API // using regular GL API calls

#include "de_platform.h"

#include <de/legacy/concurrency.h>
#include <de/glinfo.h>
#include <de/guiapp.h>
#include "gl/gl_defer.h"

static dd_bool __inline mustDefer(void)
{
    return !de::GuiApp::inRenderThread();
}

static void GL_CALL deng_glEnable(GLenum e)
{
    glEnable(e);
}

static void GL_CALL deng_glDisable(GLenum e)
{
    glDisable(e);
}

static void GL_CALL deng_glDeleteTextures(GLsizei num, const GLuint *names)
{
    glDeleteTextures(num, names);
}

#define GL_CALL1(form, func, x) \
    if(mustDefer()) GL_Defer_##form(func, x); else func(x);
#define GL_CALL2(form, func, x, y) \
    if(mustDefer()) GL_Defer_##form(func, x, y); else func(x, y);

DE_EXTERN_C void Deferred_glEnable(GLenum e)
{
    GL_CALL1(e, deng_glEnable, e);
}

DE_EXTERN_C void Deferred_glDisable(GLenum e)
{
    GL_CALL1(e, deng_glDisable, e);
}

DE_EXTERN_C void Deferred_glDeleteTextures(GLsizei num, const GLuint *names)
{
    GL_CALL2(uintArray, deng_glDeleteTextures, num, names);
}

