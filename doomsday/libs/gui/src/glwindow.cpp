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
#include "de/GLTimer"
#include "de/ElapsedTimer"

//#include <QElapsedTimer>
//#include <QImage>
//#include <QOpenGLTimerQuery>
#include <QScreen>
//#include <QSurfaceFormat>
//#include <QTimer>

#include <de/GLBuffer>
#include <de/GLState>
#include <de/GLFramebuffer>
#include <de/Id>
#include <de/Log>
#include <de/c_wrapper.h>

#include <SDL2/SDL_events.h>

namespace de {

static GLWindow *mainWindow = nullptr;

DE_PIMPL(GLWindow)
{
    SDL_Window *   window   = nullptr;
    SDL_GLContext glContext = nullptr;

    LoopCallback        mainCall;
    GLFramebuffer       backing;                 // Represents QOpenGLWindow's framebuffer.
    WindowEventHandler *handler       = nullptr; ///< Event handler.
    bool                readyPending  = false;
    bool                readyNotified = false;
    Size                currentSize;
    double              pixelRatio = 0.0;

    uint  frameCount = 0;
    float fps        = 0;

    std::unique_ptr<GLTimer> timer;
    Id totalFrameTimeQueryId;

    Impl(Public *i) : Base(i)
    {
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE,     8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,   8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,    8);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,   16);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        window = SDL_CreateWindow("GLWindow",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  640,
                                  400,
                                  SDL_WINDOW_OPENGL |
                                  SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_ALLOW_HIGHDPI);
        glContext = SDL_GL_CreateContext(window);
    }

    ~Impl()
    {
        self().makeCurrent();
        {
            // Perform cleanup of GL objects.
        glDeinit();
        }
        self().doneCurrent();

        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);

        if (thisPublic == mainWindow)
        {
            GuiLoop::get().setWindow(nullptr);
            mainWindow = nullptr;
        }
    }

    void glInit()
    {
        GLInfo::glInit();
        timer.reset(new GLTimer);
        self().setState(Ready);
    }

    void glDeinit()
    {
        self().setState(NotReady);
        readyNotified = false;
        readyPending = false;
//#if defined (DE_HAVE_TIMER_QUERY)
//        if (timerQuery)
//        {
//            if (timerQueryPending) timerQuery->waitForResult();
//            delete timerQuery;
//            timerQuery = nullptr;
//            timerQueryPending = false;
//        }
//#endif
        timer.reset();
        GLInfo::glDeinit();
    }

    void notifyReady()
    {
        if (readyNotified) return;

        readyPending = false;

        self().makeCurrent();

        DE_ASSERT(SDL_GL_GetCurrentContext() != nullptr);

#if 0
        // Print some information.
        QSurfaceFormat const fmt = self().format();

#if defined (DE_OPENGL)
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
#endif

        // Everybody can perform GL init now.
        DE_FOR_PUBLIC_AUDIENCE2(Init, i) i->windowInit(self());

        readyNotified = true;

        self().doneCurrent();

        // Now we can paint.
        mainCall.enqueue([this] () { self().update(); });
    }

#if 0
#if defined (DE_HAVE_TIMER_QUERY)
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
#endif // 0

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
            fps         = float(frameCount / elapsed);
            lastFpsTime = nowTime;
            frameCount  = 0;
        }
    }

    Flags winFlags() const
    {
        return SDL_GetWindowFlags(window);
    }

    void resizeEvent(const SDL_WindowEvent &ev)
    {
        const int w = ev.data1;
        const int h = ev.data2;

        /// @todo This is likely points, not pixels.
        debug("[GLWindow] SDL window resize event to %dx%d", w, h);

        int pw, ph;
        SDL_GL_GetDrawableSize(window, &pw, &ph);
        debug("[GLWindow] Drawable size is %dx%d pixels", pw, ph);

        pendingSize = Size(pw, ph);

        // Only react if this is actually a resize.
        if (currentSize != pendingSize)
        {
            currentSize = pendingSize;

            if (readyNotified)
            {
                self().makeCurrent();
            }

            DE_FOR_PUBLIC_AUDIENCE2(Resize, i) { i->windowResized(self()); }
            
            if (readyNotified)
            {
                self().doneCurrent();
            }
        }
    }

    void frameWasSwapped()
    {
        self().makeCurrent();
        DE_FOR_PUBLIC_AUDIENCE2(Swap, i) i->windowSwapped(self());
        updateFrameRateStatistics();
        self().doneCurrent();
    }

    DE_PIMPL_AUDIENCES(Init, Resize, PixelRatio, Swap, Move, Visibility)
};

DE_AUDIENCE_METHODS(GLWindow, Init, Resize, PixelRatio, Swap, Move, Visibility)

GLWindow::GLWindow()
    : d(new Impl(this))
{
//#if defined (MACOSX)
//    setFlags(flags() | Qt::WindowFullscreenButtonHint);
//#endif

#if defined (DE_MOBILE)
    setFocusPolicy(Qt::StrongFocus);
#endif

//    connect(this, SIGNAL(frameSwapped()), this, SLOT(frameWasSwapped()));

    d->handler = new WindowEventHandler(this);

    d->pixelRatio = devicePixelRatio();

    connect(this, &QWindow::screenChanged, [this](QScreen *scr) {
        //qDebug() << "window screen changed:" << scr << scr->devicePixelRatio();
        if (!fequal(d->pixelRatio, scr->devicePixelRatio()))
        {
            d->pixelRatio = scr->devicePixelRatio();
            d->submitResize(pointSize() * d->pixelRatio);
            DENG2_FOR_AUDIENCE2(PixelRatio, i)
            {
                i->windowPixelRatioChanged(*this);
            }
        }
    });
}

void GLWindow::setMinimumSize(const Size &minSize)
{
    SDL_SetWindowMinimumSize(d->window, minSize.x, minSize.y);
}

void GLWindow::makeCurrent()
{
    SDL_GL_MakeCurrent(d->window, d->glContext);
}

void GLWindow::doneCurrent()
{
    SDL_GL_MakeCurrent(d->window, nullptr);
}

void GLWindow::update()
{
    /// @todo Schedule a redraw event.

}

void GLWindow::showNormal()
{
    SDL_ShowWindow(d->window);
}

void GLWindow::showMaximized()
{
    SDL_ShowWindow(d->window);
    SDL_MaximizeWindow(d->window);
}

void GLWindow::showFullScreen()
{
    SDL_ShowWindow(d->window);
    SDL_SetWindowFullscreen(d->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void GLWindow::hide()
{
    SDL_HideWindow(d->window);
}

void GLWindow::setGeometry(const Rectanglei &rect)
{
    SDL_SetWindowPosition(d->window, rect.left(),  rect.top());
    SDL_SetWindowSize    (d->window, rect.width(), rect.height());
}

#if defined (DE_MOBILE)
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
#if defined (DE_MOBILE)
    return false;
#else
    return d->winFlags().testFlag(SDL_WINDOW_MAXIMIZED);
#endif
}

bool GLWindow::isMinimized() const
{
#if defined (DE_MOBILE)
    return false;
#else
    return d->winFlags().testFlag(SDL_WINDOW_MINIMIZED);
#endif
}

bool GLWindow::isVisible() const
{
    return d->winFlags().testFlag(SDL_WINDOW_SHOWN);
}

bool GLWindow::isFullScreen() const
{
#if defined (DE_MOBILE)
    return true;
#else
    return (d->winFlags() & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
#endif
}

bool GLWindow::isHidden() const
{
#if defined (DE_MOBILE)
    return false;
#else
    return d->winFlags().testFlag(SDL_WINDOW_HIDDEN);
#endif
}

GLFramebuffer &GLWindow::framebuffer() const
{
    return d->backing;
}

GLTimer &GLWindow::timer() const
{
    return *d->timer;
}

float GLWindow::frameRate() const
{
    return d->fps;
}

uint GLWindow::frameCount() const
{
    return d->frameCount;
}

Vec2i GLWindow::pos() const
{
    Vec2i p;
    SDL_GetWindowPosition(d->window, &p.x, &p.y);
    return p;
}

GLWindow::Size GLWindow::pointSize() const
{
    int w, h;
    SDL_GetWindowSize(d->window, &w, &h);
    return Size(w, h);
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
    DE_ASSERT(d->handler != nullptr);
    return *d->handler;
}

bool GLWindow::ownsEventHandler(WindowEventHandler *handler) const
{
    if (!handler) return false;
    return d->handler == handler;
}

#if 0
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
#endif

void GLWindow::grabToFile(NativePath const &path) const
{
    grabImage().save(path);
}

Image GLWindow::grabImage(Size const &outputSize) const
{
    return grabImage(Rectanglei::fromSize(pixelSize()), outputSize);
}

Image GLWindow::grabImage(Rectanglei const &area, Size const &outputSize) const
{
    Image grabbed;
    DE_ASSERT_FAIL("[GLWindow] grabImage not implemented");
#if 0
    // We will be grabbing the visible, latest complete frame.
    //glReadBuffer(GL_FRONT);
    Image grabbed = const_cast<GLWindow *>(this)->grabFramebuffer(); // no alpha
    if (area.size() != grabbed.size())
    {
        // Just take a portion of the full image.
        grabbed = grabbed.copy(area);
    }
    //glReadBuffer(GL_BACK);
    if (outputSize.isValid())
    {
        grabbed = grabbed.scaled(outputSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
#endif
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

void GLWindow::handleSDLEvent(const void *ptr)
{
    DE_ASSERT(ptr);
    const SDL_Event *event = reinterpret_cast<const SDL_Event *>(ptr);
    switch (event->type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        d->handler->handleSDLEvent(ptr);
        break;

    case SDL_WINDOWEVENT:
        switch (event->window.event)
        {
        case SDL_WINDOWEVENT_EXPOSED: break;

        case SDL_WINDOWEVENT_MOVED:
            DE_FOR_AUDIENCE2(Move, i)
            {
                i->windowMoved(*this, Vec2i(event->window.data1, event->window.data2));
            }
            break;

        case SDL_WINDOWEVENT_RESIZED: d->resizeEvent(event->window); break;

        case SDL_WINDOWEVENT_CLOSE: break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_FOCUS_LOST: break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_MINIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
        case SDL_WINDOWEVENT_SHOWN:
        case SDL_WINDOWEVENT_HIDDEN: break;

        default: break;
        }
    break;

    default: break;
    }
}

void GLWindow::initializeGL()
{
    LOG_AS("GLWindow");
    LOGDEV_GL_NOTE("Initializing OpenGL window");

    d->glInit();
}

void GLWindow::paintGL()
{
    GLFramebuffer::setDefaultFramebuffer(0); //defaultFramebufferObject());

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
        glClear(GL_COLOR_BUFFER_BIT);
        return;
    }

    //DE_ASSERT(QOpenGLContext::currentContext() != nullptr);
    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

    //qDebug() << "Frame time:" << d->timer->elapsedTime(d->totalFrameTimeQueryId);

//#if defined (DE_HAVE_TIMER_QUERY)
//    {
//        d->checkTimerQueryResult();

//        if (!d->timerQuery)
//        {
//            d->timerQuery = new QOpenGLTimerQuery();
//            if (!d->timerQuery->create())
//            {
//                LOG_GL_ERROR("Failed to create timer query object");
//            }
//        }

//        if (d->timerQueryReady())
//        {
//            d->timerQuery->begin();
//        }
//    }
//#endif

    //d->timer->beginTimer(d->totalFrameTimeQueryId);

    GLBuffer::resetDrawCount();

    LIBGUI_ASSERT_GL_OK();

    // Make sure any changes to the state stack are in effect.
    GLState::current().target().glBind();

    draw();

    LIBGUI_ASSERT_GL_OK();

    //d->timer->endTimer(d->totalFrameTimeQueryId);

//#if defined (DE_HAVE_TIMER_QUERY)
//    if (d->timerQueryReady())
//    {
//        d->timerQuery->end();
//        d->timerQueryPending = true;
//    }
//#endif
}

void GLWindow::windowAboutToClose()
{}

bool GLWindow::mainExists() // static
{
    return mainWindow != nullptr;
}

GLWindow &GLWindow::main() // static
{
    DE_ASSERT(mainWindow != nullptr);
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
