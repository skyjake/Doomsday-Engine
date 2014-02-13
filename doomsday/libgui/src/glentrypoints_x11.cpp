/** @file glentrypoints_x11.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/gui/glentrypoints.h"

#ifdef Q_WS_X11

#include "de/CanvasWindow"

#include <QX11Info>
#include <GL/glx.h>
#include <GL/glxext.h>

#define GET_PROC_EXT(name) *((void (**)())&name) = glXGetProcAddress((GLubyte const *)#name)

PFNGLXSWAPINTERVALEXTPROC   glXSwapIntervalEXT;

void getGLXEntryPoints()
{
    GET_PROC_EXT(glXSwapIntervalEXT);
}

char const *getGLXExtensionsString()
{
    return glXQueryExtensionsString(QX11Info::display(), QX11Info::appScreen());
}

void setXSwapInterval(int interval)
{
    if(glXSwapIntervalEXT)
    {
        DENG2_ASSERT(de::CanvasWindow::mainExists());
        glXSwapIntervalEXT(QX11Info::display(), de::CanvasWindow::main().canvas().winId(), interval);
    }
}

#endif // Q_WS_X11
