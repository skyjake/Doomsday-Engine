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

#include <QApplication>
#include <QMouseEvent>
#include <QShowEvent>
#include <QPaintEvent>
#include <QCursor>
#include <QTimer>
#include <QDebug>

#include "canvas.h"
#include "sys_opengl.h"

struct Canvas::Instance
{
    Canvas* self;
    bool initNotified;
    void (*initCallback)(Canvas&);
    void (*drawCallback)(Canvas&);
    bool mouseGrabbed;
    QPoint prevMousePos;

    Instance(Canvas* c)
        : self(c),
          initNotified(false), initCallback(0),
          drawCallback(0),
          mouseGrabbed(false)
    {}

    void grabMouse()
    {
        mouseGrabbed = true;

        // Start grabbing the mouse now.
        QCursor::setPos(self->mapToGlobal(self->rect().center()));
        self->grabMouse();
        qApp->setOverrideCursor(QCursor(Qt::BlankCursor));

        QTimer::singleShot(1, self, SLOT(checkMousePosition()));
    }

    void ungrabMouse()
    {
        mouseGrabbed = false;
        self->releaseMouse();
        qApp->restoreOverrideCursor();
    }
};

Canvas::Canvas(QWidget *parent) : QGLWidget(parent)
{
    d = new Instance(this);

    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true); // receive moves always

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

void Canvas::focusInEvent(QFocusEvent*)
{
    qDebug() << "Canvas: focus in";
}

void Canvas::focusOutEvent(QFocusEvent*)
{
    qDebug() << "Canvas: focus out";

    d->ungrabMouse();
}

void Canvas::mousePressEvent(QMouseEvent* ev)
{
    if(!d->mouseGrabbed)
    {
        ev->ignore();
        return;
    }

    ev->accept();

    qDebug() << "Canvas: mouse press at" << ev->pos();
}

void Canvas::checkMousePosition()
{
    if(d->mouseGrabbed)
    {
        QPoint curPos = mapFromGlobal(QCursor::pos());
        if(!d->prevMousePos.isNull())
        {
            QPoint delta = curPos - d->prevMousePos;
            if(!delta.isNull())
            {
                qDebug() << "Canvas: mouse delta" << delta;

                // Keep the cursor centered.
                QPoint mid = rect().center();
                QCursor::setPos(mapToGlobal(mid));
                d->prevMousePos = mid;
            }
        }
        else
        {
            d->prevMousePos = curPos;
        }
        QTimer::singleShot(1, this, SLOT(checkMousePosition()));
    }
    else
    {
        // Mouse was ungrabbed; reset the tracking.
        d->prevMousePos = QPoint();
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent* ev)
{
    ev->accept();

    if(!d->mouseGrabbed)
    {
        // Start grabbing after a click.
        d->grabMouse();
        return;
    }

    qDebug() << "Canvas: mouse release at" << ev->pos();
}
