/** @file canvaswindow.cpp Canvas window implementation.
 * @ingroup base
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <QApplication>
#include <QGLFormat>
#include <QMoveEvent>
#include <de/App>
#include <de/Config>
#include <de/Record>
#include <de/NumberValue>
#include <de/Log>
#include <de/RootWidget>
#include <de/c_wrapper.h>

#include "de_platform.h"
#include "dd_loop.h"
#include "con_main.h"
#ifdef __CLIENT__
#  include "gl/gl_main.h"
#endif
#include "ui/canvaswindow.h"
#include "ui/legacywidget.h"
#include "ui/busywidget.h"
#include "clientapp.h"

#include <assert.h>
#include <QThread>

using namespace de;

static String const LEGACY_WIDGET_NAME = "legacy";

DENG2_PIMPL(CanvasWindow)
{
    Canvas* canvas; ///< Drawing surface for the contents of the window.
    Canvas* recreated;
    void (*moveFunc)(CanvasWindow&);
    bool (*closeFunc)(CanvasWindow&);
    bool mouseWasTrapped;
    float fps;

    Mode mode;

    /// Root of the nomal UI widgets of this window.
    RootWidget root;

    RootWidget busyRoot;

    Instance(Public *i)
        : Base(i),
          canvas(0),
          moveFunc(0),
          closeFunc(0),
          mouseWasTrapped(false),
          fps(0),
          mode(Normal)
    {
        LegacyWidget *legacy = new LegacyWidget(LEGACY_WIDGET_NAME);
        legacy->rule()
                .setLeftTop    (root.viewLeft(),  root.viewTop())
                .setRightBottom(root.viewRight(), root.viewBottom());
        root.add(legacy);

        // Initially the widget is disabled. It will be enabled when the window
        // is visible and ready to be drawn.
        legacy->disable();

        // For busy mode we have an entirely different widget tree.
        BusyWidget *busy = new BusyWidget;
        busy->rule()
                .setLeftTop    (busyRoot.viewLeft(),  busyRoot.viewTop())
                .setRightBottom(busyRoot.viewRight(), busyRoot.viewBottom());
        busyRoot.add(busy);
    }

    void setMode(Mode const &newMode)
    {
        LOG_VERBOSE("Switching to %s mode") << (newMode == Busy? "Busy" : "Normal");

        mode = newMode;
    }

    void updateFrameRateStatistics(void)
    {
        static uint lastFpsTime = 0;

        uint nowTime = Timer_RealMilliseconds();

        /*
        Con_Message("endFrame with %i ms (%i render)\n", nowTime - lastShowAt, nowTime - frameStartAt);
        lastShowAt = nowTime;
        */

        // Increment the (local) frame counter.
        rFrameCount++;

        // Count the frames every other second.
        if(nowTime - 2500 >= lastFpsTime)
        {
            static int lastFrameCount = 0;
            fps = (rFrameCount - lastFrameCount) / ((nowTime - lastFpsTime)/1000.0f);
            lastFpsTime = nowTime;
            lastFrameCount = rFrameCount;
        }
    }
};

CanvasWindow::CanvasWindow(QWidget *parent)
    : QMainWindow(parent), d(new Instance(this))
{
    // Create the drawing canvas for this window.
    setCentralWidget(d->canvas = new Canvas(this)); // window takes ownership

    // All input goes to the canvas.
    d->canvas->setFocus();
}

de::RootWidget &CanvasWindow::root()
{
    return d->mode == Busy? d->busyRoot : d->root;
}

void CanvasWindow::setMode(Mode const &mode)
{
    LOG_AS("CanvasWindow");
    d->setMode(mode);
}

float CanvasWindow::frameRate() const
{
    return d->fps;
}

void CanvasWindow::initCanvasAfterRecreation(Canvas &canvas)
{
    CanvasWindow *self = dynamic_cast<CanvasWindow *>(canvas.parentWidget());
    DENG2_ASSERT(self);

    LOG_DEBUG("About to replace Canvas %p with %p")
            << de::dintptr(self->d->canvas) << de::dintptr(self->d->recreated);

    // Take the existing callbacks of the old canvas.
    self->d->recreated->useCallbacksFrom(*self->d->canvas);

    // Switch the central widget. This will delete the old canvas automatically.
    self->setCentralWidget(self->d->recreated);
    self->d->canvas = self->d->recreated;
    self->d->recreated = 0;

    // Set up the basic GL state for the new canvas.
    self->d->canvas->makeCurrent();
#ifdef __CLIENT__
    GL_Init2DState();
#endif
    self->d->canvas->doneCurrent();
    self->d->canvas->updateGL();

    // Reacquire the focus.
    self->d->canvas->setFocus();
    if(self->d->mouseWasTrapped)
    {
        self->d->canvas->trapMouse();
    }

    LOG_DEBUG("Canvas replaced with %p") << de::dintptr(self->d->canvas);
}

void CanvasWindow::recreateCanvas()
{
    // Update the GL format for subsequently created Canvases.
    if(!setDefaultGLFormat())
    {
        // No need to recreate.
        LOG_DEBUG("Canvas not recreated because the format was not changed.");
        return;
    }

    // We'll re-trap the mouse after the new canvas is ready.
    d->mouseWasTrapped = canvas().isMouseTrapped();
    canvas().trapMouse(false);

    // Create the replacement Canvas. Once it's created and visible, we'll
    // finish the switch-over.
    d->recreated = new Canvas(this, d->canvas);
    d->recreated->setInitFunc(initCanvasAfterRecreation);

    d->recreated->setGeometry(d->canvas->geometry());
    d->recreated->show();

    LOG_DEBUG("Canvas recreated, old one still exists.");
}

Canvas& CanvasWindow::canvas()
{
    DENG2_ASSERT(d->canvas != 0);
    return *d->canvas;
}

bool CanvasWindow::ownsCanvas(Canvas *c) const
{
    if(!c) return false;
    return (d->canvas == c || d->recreated == c);
}

void CanvasWindow::setMoveFunc(void (*func)(CanvasWindow &))
{
    d->moveFunc = func;
}

void CanvasWindow::setCloseFunc(bool (*func)(CanvasWindow &))
{
    d->closeFunc = func;
}

bool CanvasWindow::event(QEvent *ev)
{
    if(ev->type() == QEvent::ActivationChange)
    {
        //LOG_DEBUG("CanvasWindow: Forwarding QEvent::KeyRelease, Qt::Key_Alt");
        QKeyEvent keyEvent = QKeyEvent(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        return QApplication::sendEvent(&canvas(), &keyEvent);
    }
    return QMainWindow::event(ev);
}

void CanvasWindow::closeEvent(QCloseEvent *ev)
{
    if(d->closeFunc)
    {
        if(!d->closeFunc(*this))
        {
            ev->ignore();
            return;
        }
    }

    QMainWindow::closeEvent(ev);
}

void CanvasWindow::moveEvent(QMoveEvent *ev)
{
    QMainWindow::moveEvent(ev);

    if(d->moveFunc)
    {
        d->moveFunc(*this);
    }
}

void CanvasWindow::resizeEvent(QResizeEvent *ev)
{
    QMainWindow::resizeEvent(ev);

    LOG_AS("CanvasWindow");

    de::Vector2i size(width(), height());
    LOG_DEBUG("Resized ") << size.asText();

    Window_UpdateAfterResize(Window_Main()); /// @todo remove this

    d->root.setViewSize(size);
    d->busyRoot.setViewSize(size);
}

void CanvasWindow::hideEvent(QHideEvent *ev)
{
    LOG_AS("CanvasWindow");

    QMainWindow::hideEvent(ev);

    LOG_DEBUG("Hide event (hidden:%b)") << isHidden();
}

void CanvasWindow::canvasReady(Canvas &/*canvas*/)
{
    // Now that the Canvas is ready for drawing we can enable
    // the LegacyWidget.
    d->root.find(LEGACY_WIDGET_NAME)->enable();

    LOG_DEBUG("LegacyWidget enabled");
}

void CanvasWindow::paintCanvas(Canvas &/*canvas*/)
{
    // All of this occurs during the Canvas paintGL event.

    ClientApp::app().preFrame(); /// @todo what about multiwindow?

    LIBDENG_ASSERT_IN_MAIN_THREAD();
    LIBDENG_ASSERT_GL_CONTEXT_ACTIVE();

    root().draw();

    // Finish GL drawing and swap it on to the screen. Blocks until buffers
    // swapped.
    GL_DoUpdate();

    d->updateFrameRateStatistics();

    ClientApp::app().postFrame(); /// @todo what about multiwindow?
}

bool CanvasWindow::setDefaultGLFormat() // static
{
    LOG_AS("DefaultGLFormat");

    // Configure the GL settings for all subsequently created canvases.
    QGLFormat fmt;
    fmt.setDepthBufferSize(16);
    fmt.setStencilBufferSize(8);
    fmt.setDoubleBuffer(true);

    if(CommandLine_Exists("-novsync") || !Con_GetByte("vid-vsync"))
    {
        fmt.setSwapInterval(0); // vsync off
        LOG_DEBUG("vsync off");
    }
    else
    {
        fmt.setSwapInterval(1);
        LOG_DEBUG("vsync on");
    }

    // The value of the "vid-fsaa" variable is written to this settings
    // key when the value of the variable changes.
    bool configured = de::App::config().getb("window.fsaa");

    if(CommandLine_Exists("-nofsaa") || !configured)
    {
        fmt.setSampleBuffers(false);
        LOG_DEBUG("multisampling off");
    }
    else
    {
        fmt.setSampleBuffers(true); // multisampling on (default: highest available)
        //fmt.setSamples(4);
        LOG_DEBUG("multisampling on (max)");
    }

    if(fmt != QGLFormat::defaultFormat())
    {
        LOG_DEBUG("Applying new format...");
        QGLFormat::setDefaultFormat(fmt);
        return true;
    }
    else
    {
        LOG_DEBUG("New format is the same as before.");
        return false;
    }
}
