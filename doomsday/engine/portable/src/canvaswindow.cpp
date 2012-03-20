/**
 * @file canvaswindow.cpp
 * Canvas window implementation. @ingroup base
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

#include "canvaswindow.h"
#include "con_main.h"
#include <assert.h>

#include <QGLFormat>
#include <QMoveEvent>
#include <QDebug>

struct CanvasWindow::Instance
{
    Canvas* canvas;
    void (*moveFunc)(CanvasWindow&);

    Instance() : canvas(0), moveFunc(0) {}
};

CanvasWindow::CanvasWindow(QWidget *parent)
    : QMainWindow(parent)
{
    d = new Instance;

    // Create the drawing canvas for this window.
    setCentralWidget(d->canvas = new Canvas); // takes ownership

    // All input goes to the canvas.
    d->canvas->setFocus();
}

CanvasWindow::~CanvasWindow()
{
    delete d;
}

Canvas& CanvasWindow::canvas()
{
    assert(d->canvas != 0);
    return *d->canvas;
}

void CanvasWindow::setMoveFunc(void (*func)(CanvasWindow&))
{
    d->moveFunc = func;
}

void CanvasWindow::closeEvent(QCloseEvent* ev)
{
    /// @todo autosave and quit?
    Con_Execute(CMDS_DDAY, "quit", true, false);
    ev->ignore();
}

void CanvasWindow::moveEvent(QMoveEvent *ev)
{
    QMainWindow::moveEvent(ev);

    if(d->moveFunc)
    {
        d->moveFunc(*this);
    }
}

void CanvasWindow::hideEvent(QHideEvent* ev)
{
    QMainWindow::hideEvent(ev);

    qDebug() << "CanvasWindow: hide event" << isHidden();
}

void CanvasWindow::setDefaultGLFormat() // static
{
    // Configure the GL settings for all subsequently created canvases.
    QGLFormat fmt;
    fmt.setDepthBufferSize(16);
    fmt.setStencilBufferSize(8);
    fmt.setDoubleBuffer(true);
    fmt.setSwapInterval(1);     // vsync on
    fmt.setSampleBuffers(true); // multisampling on (default: highest available)
    QGLFormat::setDefaultFormat(fmt);
}
