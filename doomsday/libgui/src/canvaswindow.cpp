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

#include "de/CanvasWindow"

#include <QApplication>
#include <QGLFormat>
#include <QMoveEvent>
#include <QThread>

#include <de/App>
#include <de/Config>
#include <de/Record>
#include <de/NumberValue>
#include <de/Log>
#include <de/RootWidget>
#include <de/c_wrapper.h>

namespace de {

DENG2_PIMPL(CanvasWindow)
{
    Canvas* canvas; ///< Drawing surface for the contents of the window.
    Canvas* recreated;
    bool ready;
    bool mouseWasTrapped;
    unsigned int frameCount;
    float fps;

    Instance(Public *i)
        : Base(i),
          canvas(0),
          recreated(0),
          ready(false),
          mouseWasTrapped(false),
          frameCount(0),
          fps(0)
    {}

    void updateFrameRateStatistics(void)
    {
        static Time lastFpsTime;

        Time const nowTime = Clock::appTime();

        // Increment the (local) frame counter.
        frameCount++;

        // Count the frames every other second.
        TimeDelta elapsed = nowTime - lastFpsTime;
        if(elapsed > 2.5)
        {
            fps = frameCount / elapsed;
            lastFpsTime = nowTime;
            frameCount = 0;
        }
    }

    void finishCanvasRecreation()
    {
        LOG_DEBUG("About to replace Canvas %p with %p")
                << de::dintptr(canvas) << de::dintptr(recreated);

        // Copy the audiences of the old canvas.
        recreated->copyAudiencesFrom(*canvas);

        // Switch the central widget. This will delete the old canvas automatically.
        self.setCentralWidget(recreated);
        canvas = recreated;
        recreated = 0;

        // Set up the basic GL state for the new canvas.
        canvas->makeCurrent();

        DENG2_FOR_EACH_OBSERVER(Canvas::GLInitAudience, i, canvas->audienceForGLInit)
        {
            i->canvasGLInit(*canvas);

            /*#ifdef __CLIENT__
                GL_Init2DState();
            #endif*/
        }

        canvas->doneCurrent();
        canvas->updateGL();

        // Reacquire the focus.
        canvas->setFocus();
        if(mouseWasTrapped)
        {
            canvas->trapMouse();
        }

        LOG_DEBUG("Canvas replaced with %p") << de::dintptr(canvas);
    }
};

CanvasWindow::CanvasWindow()
    : QMainWindow(0), d(new Instance(this))
{
    // Create the drawing canvas for this window.
    setCentralWidget(d->canvas = new Canvas(this)); // takes ownership

    d->canvas->audienceForGLReady += this;
    d->canvas->audienceForGLDraw += this;

    // All input goes to the canvas.
    d->canvas->setFocus();
}

bool CanvasWindow::isReady() const
{
    return d->ready;
}

float CanvasWindow::frameRate() const
{
    return d->fps;
}

void CanvasWindow::recreateCanvas()
{
    /*
    // Update the GL format for subsequently created Canvases.
    if(!setDefaultGLFormat())
    {
        // No need to recreate.
        LOG_DEBUG("Canvas not recreated because the format was not changed.");
        return false;
    }
    */

    d->ready = false;

    // We'll re-trap the mouse after the new canvas is ready.
    d->mouseWasTrapped = canvas().isMouseTrapped();
    canvas().trapMouse(false);
    canvas().setParent(0);

    // Create the replacement Canvas. Once it's created and visible, we'll
    // finish the switch-over.
    d->recreated = new Canvas(this, d->canvas);
    d->recreated->audienceForGLReady += this;

    d->recreated->setGeometry(d->canvas->geometry());
    d->recreated->show();

    LOG_DEBUG("Canvas recreated, old one still exists.");
}

Canvas &CanvasWindow::canvas()
{
    DENG2_ASSERT(d->canvas != 0);
    return *d->canvas;
}

Canvas const &CanvasWindow::canvas() const
{
    DENG2_ASSERT(d->canvas != 0);
    return *d->canvas;
}

bool CanvasWindow::ownsCanvas(Canvas *c) const
{
    if(!c) return false;
    return (d->canvas == c || d->recreated == c);
}

#ifdef WIN32
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
#endif

/*
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
*/

/*
void CanvasWindow::resizeEvent(QResizeEvent *ev)
{
    QMainWindow::resizeEvent(ev);

    LOG_AS("CanvasWindow");

    Vector2i size(width(), height());
    LOG_DEBUG("Resized ") << size.asText();

    Window::main().updateAfterResize(); /// @todo remove this

    d->root.setViewSize(size);
    d->busyRoot.setViewSize(size);
}
*/

void CanvasWindow::hideEvent(QHideEvent *ev)
{
    LOG_AS("CanvasWindow");

    QMainWindow::hideEvent(ev);

    LOG_DEBUG("Hide event (hidden:%b)") << isHidden();
}

void CanvasWindow::canvasGLReady(Canvas &canvas)
{
    d->ready = true;

    if(d->recreated == &canvas)
    {
        d->finishCanvasRecreation();
    }
}

void CanvasWindow::canvasGLDraw(Canvas &/*canvas*/)
{
    // All of this occurs during the Canvas paintGL event.
/* TODO: move to ClientWindow
    ClientApp::app().preFrame(); /// @todo what about multiwindow?

    DENG_ASSERT_IN_MAIN_THREAD();
    DENG_ASSERT_GL_CONTEXT_ACTIVE();

    root().draw();

    // Finish GL drawing and swap it on to the screen. Blocks until buffers
    // swapped.
    GL_DoUpdate();
  //  ClientApp::app().postFrame(); /// @todo what about multiwindow?
*/

    d->updateFrameRateStatistics();
}

#if 0
/*
 * TODO: move to ClientWindow
 */
bool ClientWindow::updateDefaultGLFormat() // static
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
#endif

duint CanvasWindow::grabAsTexture(GrabMode mode) const
{
    return d->canvas->grabAsTexture(
                mode == GrabHalfSized? QSize(width()/2, height()/2) : QSize());
}

bool CanvasWindow::grabToFile(NativePath const &path) const
{
    return d->canvas->grabImage().save(path.toString());
}

void CanvasWindow::swapBuffers() const
{
    // Force a swapbuffers right now.
    d->canvas->swapBuffers();
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

} // namespace de
