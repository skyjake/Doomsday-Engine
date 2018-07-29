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
#include "de/EventLoop"
#include "de/CoreEvent"

#include <de/GLBuffer>
#include <de/GLState>
#include <de/GLFramebuffer>
#include <de/Id>
#include <de/Log>
#include <de/c_wrapper.h>

#include <SDL_events.h>

namespace de {

static GLWindow *mainWindow = nullptr;

DE_PIMPL(GLWindow)
{
    SDL_Window *   window   = nullptr;
    SDL_GLContext glContext = nullptr;

    LoopCallback        mainCall;
    GLFramebuffer       backing;
    WindowEventHandler *handler       = nullptr; ///< Event handler.
    bool                initialized   = false;
    bool                readyPending  = false;
    bool                readyNotified = false;
    bool                paintPending  = false;
    Size                currentSize;
    double              pixelRatio = 0.0;

    uint  frameCount   = 0;
    float fps          = 0;

    std::unique_ptr<GLTimer> timer;
    Id totalFrameTimeQueryId;

    Impl(Public *i) : Base(i)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   5);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  5);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

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
            GuiLoop::get().setWindow(nullptr); // triggers GuiLoop iterations
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

        LIBGUI_ASSERT_GL_OK();

        debug("Window pixel size at notifyReady: %s", currentSize.asText().c_str());
        
        // Everybody can perform GL init now.
        DE_FOR_PUBLIC_AUDIENCE2(Init, i)   i->windowInit(self());
        DE_FOR_PUBLIC_AUDIENCE2(Resize, i) i->windowResized(self());

        readyNotified = true;

        self().doneCurrent();

        // Now we can paint.
        mainCall.enqueue([this] () { self().update(); });
    }

    void updateFrameRateStatistics()
    {
        static Time lastFpsTime;

        const Time nowTime = Clock::appTime();

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

        Size pendingSize = Size(pw, ph);

        // Only react if this is actually a resize.
        if (currentSize != pendingSize)
        {
            currentSize = pendingSize;

            if (readyNotified)
            {
                self().makeCurrent();
            }
            DE_FOR_PUBLIC_AUDIENCE2(Resize, i)
            {
                i->windowResized(self());
            }
            if (readyNotified)
            {
                self().doneCurrent();
            }
        }
    }

    void frameWasSwapped()
    {
        updateFrameRateStatistics();

        LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();
        DE_FOR_PUBLIC_AUDIENCE2(Swap, i) { i->windowSwapped(self()); }
    }

    /**
     * Get events from SDL and route them to the appropriate place for handling.
     */
    void handleEvents()
    {
        try
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                case SDL_QUIT:
                    DE_GUI_APP->quit(0);
                    break;

                case SDL_WINDOWEVENT:
                case SDL_MOUSEMOTION:
                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                case SDL_MOUSEWHEEL:
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                case SDL_TEXTINPUT:
                    handleSDLEvent(event);
                    break;
                }
            }
        }
        catch (const Error &er)
        {
            LOG_WARNING("Uncaught error during event processing: %s") << er.asText();
        }
    }

    void handleSDLEvent(const SDL_Event &event)
    {
        switch (event.type)
        {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTINPUT:
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            handler->handleSDLEvent(&event);
            break;

        case SDL_WINDOWEVENT:
            switch (event.window.event)
            {
            case SDL_WINDOWEVENT_EXPOSED:
                if (!initialized)
                {
                    self().initializeGL();
                    self().update();
                }
                break;

            case SDL_WINDOWEVENT_MOVED:
                DE_FOR_PUBLIC_AUDIENCE2(Move, i)
                {
                    i->windowMoved(self(), Vec2i(event.window.data1, event.window.data2));
                }
                break;

            case SDL_WINDOWEVENT_RESIZED: resizeEvent(event.window); break;

            case SDL_WINDOWEVENT_CLOSE: break;

            case SDL_WINDOWEVENT_FOCUS_GAINED:
            case SDL_WINDOWEVENT_FOCUS_LOST:
                handler->handleSDLEvent(&event);
                break;

            case SDL_WINDOWEVENT_MAXIMIZED:
            case SDL_WINDOWEVENT_MINIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_HIDDEN: break;

            case SDL_WINDOWEVENT_SHOWN:
                self().update();
                break;

            default: break;
            }
            break;

        default: break;
        }
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

//#if defined (DE_MOBILE)
//    setFocusPolicy(Qt::StrongFocus);
//#endif

    d->handler = new WindowEventHandler(this);
    d->handler->setKeyboardMode(WindowEventHandler::RawKeys);
    
    d->pixelRatio = devicePixelRatio();

    d->handler->audienceForMouseStateChange() += [this]() {
        const bool trap = d->handler->isMouseTrapped();
        SDL_SetWindowGrab(d->window, trap ? SDL_TRUE : SDL_FALSE);
        SDL_SetRelativeMouseMode(trap ? SDL_TRUE : SDL_FALSE);
    };

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

void GLWindow::setTitle(const String &title)
{
    SDL_SetWindowTitle(d->window, title);
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

void GLWindow::show()
{
    SDL_ShowWindow(d->window);
}

void GLWindow::showNormal()
{
    SDL_ShowWindow(d->window);
    SDL_RestoreWindow(d->window);
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

void GLWindow::raise()
{
    SDL_RaiseWindow(d->window);
}

void GLWindow::setGeometry(const Rectanglei &rect)
{
    SDL_SetWindowPosition(d->window, rect.left(),  rect.top());
    SDL_SetWindowSize    (d->window, rect.width(), rect.height());
    
    // Update the current size immediately.
    Vec2i pixels;
    SDL_GL_GetDrawableSize(d->window, &pixels.x, &pixels.y);
    if (d->currentSize != pixels.toVec2ui())
    {
        d->currentSize = pixels.toVec2ui();
        DE_FOR_AUDIENCE2(Resize, i) { i->windowResized(*this); }
    }
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

Vec2i GLWindow::mapToGlobal(const Vec2i &coordInsideWindow) const
{
    return pos() + coordInsideWindow;
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

void GLWindow::checkNativeEvents()
{
    d->handleEvents();
}

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

void GLWindow::update()
{
    if (!d->paintPending)
    {
        d->paintPending = true;
        EventLoop::post(new CoreEvent([this]() {
            d->paintPending = false;
            makeCurrent();
            paintGL();
            doneCurrent();
        }));
    }
}

void GLWindow::initializeGL()
{
    if (!d->initialized)
{
    LOG_AS("GLWindow");
    LOGDEV_GL_NOTE("Initializing OpenGL window");

        d->initialized = true;
    d->glInit();

        int w, h;
        SDL_GL_GetDrawableSize(d->window, &w, &h);
        debug("initializeGL: %i x %i", w, h);

        d->currentSize = Size(w, h);
    }
}

void GLWindow::paintGL()
{
    GLFramebuffer::setDefaultFramebuffer(0);

    // Repainting of the window should continue in an indefinite loop.
    // Before doing anything else, submit a new event to repaint the window.
    // If changing the current UI/frame/world time causes side effects
    // such as another event loop running busy mode, we'll still get uninterrupted
    // window content refresh.
    EventLoop::post(new CoreEvent([this]() {
        update();
        d->handleEvents(); // process new input/window events
    }));

    // Do not proceed with painting the window contents until after the application
    // has completed GL initialization. This is done via timer callback because we
    // don't want to perform a long-running operation during a paint event.
    if (!d->readyNotified)
    {
        if (!d->readyPending)
        {
            d->readyPending = true;
            d->mainCall.enqueue([this]() { d->notifyReady(); });
        }
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(d->window);
        d->frameWasSwapped();
        return;
    }

    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();

    GLBuffer::resetDrawCount();

    LIBGUI_ASSERT_GL_OK();

    // Make sure any changes to the state stack are in effect.
    GLState::current().target().glBind();

    // This will be the current time for the frame.
    {
        Time::updateCurrentHighPerformanceTime();
        Clock::get().setTime(Time::currentHighPerformanceTime());
        LIBGUI_ASSERT_GL_OK();
        // Clock observers may have deactivated the GL context.
        makeCurrent();
    }

    // Subclass-implemented drawing method.
    draw();
    
    LIBGUI_ASSERT_GL_OK();

    // Show the final frame contents.
    SDL_GL_SwapWindow(d->window);

    d->frameWasSwapped();
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
