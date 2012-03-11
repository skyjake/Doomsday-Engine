/**
 * @file canvas.cpp
 * OpenGL drawing surface implementation. @ingroup gl
 *
 * @authors Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QShowEvent>
#include <QPaintEvent>
#include <QTimer>
#include <QDebug>

#include "canvas.h"
#include "sys_opengl.h"

struct Canvas::Instance
{
    bool initNotified;
    void (*initCallback)(Canvas&);
    void (*drawCallback)(Canvas&);

    Instance() : initNotified(false), initCallback(0), drawCallback(0)
    {}
};

Canvas::Canvas(QWidget *parent) : QGLWidget(parent)
{
    d = new Instance;

    // We will be doing buffer swaps manually (for timing purposes).
    setAutoBufferSwap(false);
}

Canvas::~Canvas()
{
    delete d;
}

void Canvas::setInitCallback(void (*canvasInitializeFunc)(Canvas&))
{
    d->initCallback = canvasInitializeFunc;
}

void Canvas::setDrawCallback(void (*canvasDrawFunc)(Canvas&))
{
    d->drawCallback = canvasDrawFunc;
}

void Canvas::forcePaint()
{
    if(isVisible())
    {
        QPaintEvent ev(QRect(0, 0, width(), height()));
        paintEvent(&ev);
    }
}

void Canvas::initializeGL()
{
    Sys_GLConfigureDefaultState();
}

void Canvas::resizeGL(int w, int h)
{
    Q_UNUSED(w);
    Q_UNUSED(h);
}

void Canvas::showEvent(QShowEvent* ev)
{
    QGLWidget::showEvent(ev);

    // The first time the window is shown, run the initialization callback. On
    // some platforms, OpenGL is not fully ready to be used before the window
    // actually appears on screen.
    if(isVisible() && !d->initNotified)
    {
        QTimer::singleShot(1, this, SLOT(notifyInit()));
    }
}

void Canvas::notifyInit()
{
    if(!d->initNotified && d->initCallback)
    {
        d->initNotified = true;
        d->initCallback(*this);
    }
}

void Canvas::paintGL()
{
    if(d->drawCallback)
    {
        d->drawCallback(*this);
    }
    else
    {
        qDebug() << "Canvas: drawing with default";
        // If we don't know what else to draw, just draw a black screen.
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        swapBuffers();
    }
}
