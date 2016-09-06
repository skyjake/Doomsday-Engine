/** @file canvaswindow.cpp Canvas window implementation.
 * @ingroup base
 *
 * @authors Copyright © 2012-2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de/CanvasWindow"
#include "de/GuiApp"

#include <QApplication>
#include <QMoveEvent>
#include <QSurfaceFormat>
#include <QThread>
#include <QTimer>

#include <de/Config>
#include <de/Record>
#include <de/NumberValue>
#include <de/Log>
#include <de/RootWidget>
#include <de/GLState>
#include <de/c_wrapper.h>

namespace de {

static CanvasWindow *mainWindow = 0;

DENG2_PIMPL(CanvasWindow)
, DENG2_OBSERVES(Canvas, GLInit)
{
    Canvas *canvas; ///< Drawing surface for the contents of the window.
    Canvas::FocusChangeAudience canvasFocusAudience; ///< Stored here during recreation.
    bool mouseWasTrapped;
    unsigned int frameCount;
    float fps;

    Impl(Public *i)
        : Base(i)
        , canvas(0)
        , mouseWasTrapped(false)
        , frameCount(0)
        , fps(0)
    {}

    ~Impl()
    {
        if (thisPublic == mainWindow)
        {
            GuiLoop::get().setWindow(nullptr);
            mainWindow = nullptr;
        }
    }

    void canvasGLInit(Canvas &)
    {
        self.setState(Ready);
    }

    void updateFrameRateStatistics(void)
    {
        static Time lastFpsTime;

        Time const nowTime = Clock::appTime();

        // Increment the (local) frame counter.
        frameCount++;

        // Count the frames every other second.
        TimeDelta elapsed = nowTime - lastFpsTime;
        if (elapsed > 2.5)
        {
            fps = frameCount / elapsed;
            lastFpsTime = nowTime;
            frameCount = 0;
        }
    }
};

CanvasWindow::CanvasWindow()
    : QMainWindow()
    , d(new Impl(this))
{
    // Create the drawing canvas for this window.
    setCentralWidget(d->canvas = new Canvas(this)); // takes ownership

    d->canvas->audienceForGLInit() += d;

    // All input goes to the canvas.
    d->canvas->setFocus();
}

float CanvasWindow::frameRate() const
{
    return d->fps;
}

Canvas &CanvasWindow::canvas() const
{
    DENG2_ASSERT(d->canvas != 0);
    return *d->canvas;
}

bool CanvasWindow::ownsCanvas(Canvas *c) const
{
    if (!c) return false;
    return (d->canvas == c/* || d->recreated == c*/);
}

#ifdef WIN32
bool CanvasWindow::event(QEvent *ev)
{
    if (ev->type() == QEvent::ActivationChange)
    {
        //LOG_DEBUG("CanvasWindow: Forwarding QEvent::KeyRelease, Qt::Key_Alt");
        QKeyEvent keyEvent = QKeyEvent(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        return QApplication::sendEvent(&canvas(), &keyEvent);
    }
    return QMainWindow::event(ev);
}
#endif

void CanvasWindow::hideEvent(QHideEvent *ev)
{
    LOG_AS("CanvasWindow");

    QMainWindow::hideEvent(ev);

    LOG_GL_VERBOSE("Hide event (hidden:%b)") << isHidden();
}

void CanvasWindow::draw()
{
    d->updateFrameRateStatistics();
}

bool CanvasWindow::grabToFile(NativePath const &path) const
{
    return d->canvas->grabImage().save(path.toString());
}

void CanvasWindow::glActivate()
{
    canvas().makeCurrent();
}

void CanvasWindow::glDone()
{
    canvas().doneCurrent();
}

void *CanvasWindow::nativeHandle() const
{
    return reinterpret_cast<void *>(winId());
}

bool CanvasWindow::mainExists()
{
    return mainWindow != 0;
}

CanvasWindow &CanvasWindow::main()
{
    DENG2_ASSERT(mainWindow != 0);
    return *mainWindow;
}

void CanvasWindow::setMain(CanvasWindow *window)
{
    mainWindow = window;
    GuiLoop::get().setWindow(window);
}

} // namespace de
