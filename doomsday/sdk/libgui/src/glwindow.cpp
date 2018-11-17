/** @file glwindow.cpp  Top-level OpenGL window.
 * @ingroup base
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GLWindow"
#include "de/GuiApp"

#include <QElapsedTimer>
#include <QImage>
#include <QOpenGLTimerQuery>
#include <QScreen>
#include <QSurfaceFormat>
#include <QTimer>

#include <de/GLBuffer>
#include <de/GLState>
#include <de/GLFramebuffer>
#include <de/Log>
#include <de/c_wrapper.h>

namespace de {

static GLWindow *mainWindow = nullptr;

DENG2_PIMPL(GLWindow)
{
    LoopCallback        mainCall;
    GLFramebuffer       backing;                 // Represents QOpenGLWindow's framebuffer.
    WindowEventHandler *handler       = nullptr; ///< Event handler.
    bool                readyPending  = false;
    bool                readyNotified = false;
    Size                currentSize;
    Size                pendingSize;
    double              pixelRatio = 0.0;

    uint  frameCount = 0;
    float fps        = 0;

#if defined(DENG_HAVE_TIMER_QUERY)
    bool               timerQueryPending = false;
    QOpenGLTimerQuery *timerQuery        = nullptr;
    QElapsedTimer      gpuTimeRecordingStartedAt;
    QVector<TimeSpan>  recordedGpuTimes;
#endif

    Impl(Public *i) : Base(i) {}

    ~Impl()
    {
        self().makeCurrent();
        glDeinit();
        self().doneCurrent();

        if (thisPublic == mainWindow)
        {
            GuiLoop::get().setWindow(nullptr);
            mainWindow = nullptr;
        }
    }

    void glInit()
    {
        GLInfo::glInit();
        self().setState(Ready);
    }

    void glDeinit()
    {
        self().setState(NotReady);
        readyNotified = false;
        readyPending = false;
#if defined (DENG_HAVE_TIMER_QUERY)
        if (timerQuery)
        {
            if (timerQueryPending) timerQuery->waitForResult();
            delete timerQuery;
            timerQuery = nullptr;
            timerQueryPending = false;
        }
#endif
        GLInfo::glDeinit();
    }

    void notifyReady()
    {
        if (readyNotified) return;

        readyPending = false;

        self().makeCurrent();

        DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);

        // Print some information.
        QSurfaceFormat const fmt = self().format();

#if defined (DENG_OPENGL)
        LOG_GL_NOTE("OpenGL %i.%i supported%s")
                << fmt.majorVersion()
                << fmt.minorVersion()
                << (fmt.majorVersion() > 2?
                        (fmt.profile() == QSurfaceFormat::CompatibilityProfile? " (Compatibility)"
                                                                              : " (Core)") : "");
#else
        LOG_GL_NOTE("OpenGL ES %i.%i supported")
                << fmt.majorVersion() << fmt.minorVersion();
#endif

        // Everybody can perform GL init now.
        DENG2_FOR_PUBLIC_AUDIENCE2(Init, i) i->windowInit(self());

        readyNotified = true;

        self().doneCurrent();

        // Now we can paint.
        mainCall.enqueue([this] () { self().update(); });
    }

#if defined (DENG_HAVE_TIMER_QUERY)
    bool timerQueryReady() const
    {
        //if (!GLInfo::extensions().EXT_timer_query) return false;
        return timerQuery && !timerQueryPending;
    }

    void checkTimerQueryResult()
    {
        // Measure how long it takes to render a frame on average.
        if (//GLInfo::extensions().EXT_timer_query &&
            timerQueryPending &&
            timerQuery->isResultAvailable())
        {
            timerQueryPending = false;
            recordedGpuTimes.append(double(timerQuery->waitForResult()) / 1.0e9);

            if (!gpuTimeRecordingStartedAt.isValid())
            {
                gpuTimeRecordingStartedAt.start();
            }

            // There are minor time variations rendering the same frame, so average over
            // a second to find out a reasonable value.
            if (gpuTimeRecordingStartedAt.elapsed() > 1000)
            {
                TimeSpan average = 0;
                for (auto dt : recordedGpuTimes) average += dt;
                average = average / recordedGpuTimes.size();
                recordedGpuTimes.clear();

                //qDebug() << "[OpenGL average frame timed]" << average.asMicroSeconds() << "µs";
                //qDebug() << "[OpenGL draw count]" << GLBuffer::drawCount();

                gpuTimeRecordingStartedAt.restart();
            }
        }
    }
#endif

    void updateFrameRateStatistics()
    {
        static Time lastFpsTime;

        Time const nowTime = Clock::appTime();

        // Increment the (local) frame counter.
        frameCount++;

        // Count the frames every other second.
        TimeSpan elapsed = nowTime - lastFpsTime;
        if (elapsed > 2.5)
        {
            fps = float(frameCount / elapsed);
            lastFpsTime = nowTime;
            frameCount = 0;
        }
    }

    DENG2_PIMPL_AUDIENCE(Init)
    DENG2_PIMPL_AUDIENCE(Resize)
    DENG2_PIMPL_AUDIENCE(Swap)
};

DENG2_AUDIENCE_METHOD(GLWindow, Init)
DENG2_AUDIENCE_METHOD(GLWindow, Resize)
DENG2_AUDIENCE_METHOD(GLWindow, Swap)

GLWindow::GLWindow()
    : d(new Impl(this))
{
#if defined (MACOSX)
    setFlags(flags() | Qt::WindowFullscreenButtonHint);
#endif

#if defined (DENG_MOBILE)
    setFocusPolicy(Qt::StrongFocus);
#endif

    connect(this, SIGNAL(frameSwapped()), this, SLOT(frameWasSwapped()));

    // Create the drawing canvas for this window.
    d->handler = new WindowEventHandler(this);

    d->pixelRatio = devicePixelRatio();

    connect(this, &QWindow::screenChanged, [this](QScreen *scr) {
        qDebug() << "window screen changed:" << scr << scr->devicePixelRatio();
        if (!fequal(d->pixelRatio, scr->devicePixelRatio()))
        {
            d->pixelRatio = scr->devicePixelRatio();
            DENG2_FOR_AUDIENCE2(Resize, i)
            {
                i->windowResized(*this);
            }
        }
    });
}

#if defined (DENG_MOBILE)
void GLWindow::setTitle(QString const &title)
{
    setWindowTitle(title);
}
#endif

bool GLWindow::isGLReady() const
{
    return d->readyNotified;
}

bool GLWindow::isMaximized() const
{
#if defined (DENG_MOBILE)
    return false;
#else
    return visibility() == QWindow::Maximized;
#endif
}

bool GLWindow::isMinimized() const
{
#if defined (DENG_MOBILE)
    return false;
#else
    return visibility() == QWindow::Minimized;
#endif
}

bool GLWindow::isFullScreen() const
{
#if defined (DENG_MOBILE)
    return true;
#else
    return visibility() == QWindow::FullScreen;
#endif
}

bool GLWindow::isHidden() const
{
#if defined (DENG_MOBILE)
    return false;
#else
    return visibility() == QWindow::Hidden;
#endif
}

GLFramebuffer &GLWindow::framebuffer() const
{
    return d->backing;
}

float GLWindow::frameRate() const
{
    return d->fps;
}

uint GLWindow::frameCount() const
{
    return d->frameCount;
}

GLWindow::Size GLWindow::pointSize() const
{
    return Size(duint(de::max(0, QOpenGLWindow::width())),
                duint(de::max(0, QOpenGLWindow::height())));
}

GLWindow::Size GLWindow::pixelSize() const
{
    return d->currentSize;
}

double GLWindow::pixelRatio() const
{
    return d->pixelRatio;
}

int GLWindow::pointWidth() const
{
    return pointSize().x;
}

int GLWindow::pointHeight() const
{
    return pointSize().y;
}

int GLWindow::pixelWidth() const
{
    return pixelSize().x;
}

int GLWindow::pixelHeight() const
{
    return pixelSize().y;
}

WindowEventHandler &GLWindow::eventHandler() const
{
    DENG2_ASSERT(d->handler != 0);
    return *d->handler;
}

bool GLWindow::ownsEventHandler(WindowEventHandler *handler) const
{
    if (!handler) return false;
    return d->handler == handler;
}

void GLWindow::focusInEvent(QFocusEvent *ev)
{
    d->handler->focusInEvent(ev);
}

void GLWindow::focusOutEvent(QFocusEvent *ev)
{
    d->handler->focusOutEvent(ev);
}

void GLWindow::keyPressEvent(QKeyEvent *ev)
{
    d->handler->keyPressEvent(ev);
}

void GLWindow::keyReleaseEvent(QKeyEvent *ev)
{
    d->handler->keyReleaseEvent(ev);
}

void GLWindow::mousePressEvent(QMouseEvent *ev)
{
    d->handler->mousePressEvent(ev);
}

void GLWindow::mouseReleaseEvent(QMouseEvent *ev)
{
    d->handler->mouseReleaseEvent(ev);
}

void GLWindow::mouseDoubleClickEvent(QMouseEvent *ev)
{
    d->handler->mouseDoubleClickEvent(ev);
}

void GLWindow::mouseMoveEvent(QMouseEvent *ev)
{
    d->handler->mouseMoveEvent(ev);
}

void GLWindow::wheelEvent(QWheelEvent *ev)
{
    d->handler->wheelEvent(ev);
}

bool GLWindow::event(QEvent *ev)
{
/*#ifdef WIN32
    if (ev->type() == QEvent::ActivationChange)
    {
        //LOG_DEBUG("GLWindow: Forwarding QEvent::KeyRelease, Qt::Key_Alt");
        QKeyEvent keyEvent = QKeyEvent(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
        return QApplication::sendEvent(&canvas(), &keyEvent);
    }
#endif*/

    if (ev->type() == QEvent::Close)
    {
        windowAboutToClose();
    }
    return QOpenGLWindow::event(ev);
}

bool GLWindow::grabToFile(NativePath const &path) const
{
    return grabImage().save(path.toString());
}

QImage GLWindow::grabImage(QSize const &outputSize) const
{
    return grabImage(QRect(QPoint(0, 0), QSize(pixelWidth(), pixelHeight())), outputSize);
}

QImage GLWindow::grabImage(QRect const &area, QSize const &outputSize) const
{
    // We will be grabbing the visible, latest complete frame.
    //LIBGUI_GL.glReadBuffer(GL_FRONT);
    QImage grabbed = const_cast<GLWindow *>(this)->grabFramebuffer(); // no alpha
    if (area.size() != grabbed.size())
    {
        // Just take a portion of the full image.
        grabbed = grabbed.copy(area);
    }
    //LIBGUI_GL.glReadBuffer(GL_BACK);
    if (outputSize.isValid())
    {
        grabbed = grabbed.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    return grabbed;
}

void GLWindow::glActivate()
{
    makeCurrent();
}

void GLWindow::glDone()
{
    doneCurrent();
}

void *GLWindow::nativeHandle() const
{
    return reinterpret_cast<void *>(winId());
}

void GLWindow::initializeGL()
{
    LOG_AS("GLWindow");
    LOGDEV_GL_NOTE("Initializing OpenGL window");

    d->glInit();
}

void GLWindow::paintGL()
{
    GLFramebuffer::setDefaultFramebuffer(defaultFramebufferObject());

    // Do not proceed with painting until after the application has completed
    // GL initialization. This is done via timer callback because we don't
    // want to perform a long-running operation during a paint event.

    if (!d->readyNotified)
    {
        if (!d->readyPending)
        {
            d->readyPending = true;
            d->mainCall.enqueue([this]() { d->notifyReady(); });
        }
        LIBGUI_GL.glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    DENG2_ASSERT(QOpenGLContext::currentContext() != nullptr);

    //if (GLInfo::extensions().EXT_timer_query)
#if defined (DENG_HAVE_TIMER_QUERY)
    {
        d->checkTimerQueryResult();

        if (!d->timerQuery)
        {
            d->timerQuery = new QOpenGLTimerQuery();
            if (!d->timerQuery->create())
            {
                LOG_GL_ERROR("Failed to create timer query object");
            }
        }

        if (d->timerQueryReady())
        {
            d->timerQuery->begin();
        }
    }
#endif

    GLBuffer::resetDrawCount();

    LIBGUI_ASSERT_GL_OK();

    // Make sure any changes to the state stack are in effect.
    GLState::current().target().glBind();

    draw();

    LIBGUI_ASSERT_GL_OK();

#if defined (DENG_HAVE_TIMER_QUERY)
    if (d->timerQueryReady())
    {
        d->timerQuery->end();
        d->timerQueryPending = true;
    }
#endif
}

void GLWindow::windowAboutToClose()
{}

void GLWindow::resizeEvent(QResizeEvent *ev)
{
    d->pendingSize = Size(ev->size().width(), ev->size().height()) * devicePixelRatio();

    qDebug() << "resize event:" << d->pendingSize.asText();
    qDebug() << "pixel ratio:" << devicePixelRatio();

    // Only react if this is actually a resize.
    if (d->currentSize != d->pendingSize)
    {
        d->currentSize = d->pendingSize;

        if (d->readyNotified)
        {
            makeCurrent();
        }

        DENG2_FOR_AUDIENCE2(Resize, i) i->windowResized(*this);

        if (d->readyNotified)
        {
            doneCurrent();
        }
    }
}

void GLWindow::frameWasSwapped()
{
    makeCurrent();
    DENG2_FOR_AUDIENCE2(Swap, i) i->windowSwapped(*this);
    d->updateFrameRateStatistics();
    doneCurrent();
}

bool GLWindow::mainExists() // static
{
    return mainWindow != 0;
}

GLWindow &GLWindow::main() // static
{
    DENG2_ASSERT(mainWindow != 0);
    return *mainWindow;
}

void GLWindow::glActiveMain()
{
    if (mainExists()) main().glActivate();
}

void GLWindow::setMain(GLWindow *window) // static
{
    mainWindow = window;
    GuiLoop::get().setWindow(window);
}

} // namespace de
