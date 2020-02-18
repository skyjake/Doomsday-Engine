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
#include "de/Image"

#include <de/Garbage>
#include <de/GLBuffer>
#include <de/GLState>
#include <de/GLFramebuffer>
#include <de/Id>
#include <de/Log>
#include <de/WindowSystem>
#include <de/c_wrapper.h>

#include <SDL_events.h>

namespace de {

static GLWindow *currentWindow = nullptr;

DE_PIMPL(GLWindow)
{
    String        id;
    SDL_Window *  window    = nullptr;
    SDL_GLContext glContext = nullptr;
    GLStateStack  glStack;

    Dispatch            mainCall;
    GLFramebuffer       backing;
    WindowEventHandler *handler       = nullptr; ///< Event handler.
    bool                initialized   = false;
    bool                readyPending  = false;
    bool                readyNotified = false;
    bool                paintPending  = false;
    bool                isClosing     = false;
    Size                currentSize;
    double              pixelRatio = 0.0;
    int                 displayIndex;

    uint  frameCount = 0;
    float fps        = 0;

    std::unique_ptr<GLTimer> timer;
    Id                       totalFrameTimeQueryId;

    Impl(Public * i, const String &id)
        : Base(i)
        , id(id)
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#if defined(DE_DEBUG)
        debug("[GLWindow] enabling debug context");
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   5);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  5);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);

        if (auto *mainWin = WindowSystem::get().mainPtr())
        {
            SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, SDL_TRUE);
            mainWin->glActivate();
        }

        window = SDL_CreateWindow("GLWindow",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  640,
                                  400,
                                  SDL_WINDOW_OPENGL |
                                  SDL_WINDOW_RESIZABLE |
                                  SDL_WINDOW_ALLOW_HIGHDPI);
        glContext = SDL_GL_CreateContext(window);
        displayIndex = SDL_GetWindowDisplayIndex(window);
        debug("[GLWindow] created context %p", glContext);

        // Initialize the state stack.
        glStack.at(0)->setTarget(backing);
    }

    ~Impl()
    {
        // Cancel all pending events concerning this window.
        EventLoop::cancel([this](Event *ev) {
            return ev->as<CoreEvent>().context() == this;
        });

        self().makeCurrent();
        {
            // Perform cleanup of GL objects.
            glDeinit();
        }
        self().doneCurrent();

        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);

        WindowSystem::get().removeWindow(self());
    }

    void updatePixelRatio()
    {
        int points, pixels;
        SDL_GetWindowSize(window, &points, nullptr);
        SDL_GL_GetDrawableSize(window, &pixels, nullptr);
        const double ratio = double(pixels) / double(points);
        if (!fequal(ratio, pixelRatio))
        {
            pixelRatio = ratio;
            debug("[GLWindow] pixel ratio changed: %f", pixelRatio);
            DE_NOTIFY_PUBLIC(PixelRatio, i)
            {
                i->windowPixelRatioChanged(self());
            }
        }
    }

    void glInit()
    {
        debug("[GLWindow] glInit");
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

        // All windows going away?
        if (WindowSystem::get().count() == 1)
        {
            GLInfo::glDeinit();
        }
    }

    void notifyReady()
    {
        if (readyNotified) return;

        readyPending = false;

        self().makeCurrent();

        DE_ASSERT(SDL_GL_GetCurrentContext() != nullptr);
        LIBGUI_ASSERT_GL_OK();

        debug("[GLWindow] pixel size at notifyReady: %s", currentSize.asText().c_str());

        // Everybody can perform GL init now.
        DE_NOTIFY_PUBLIC(Init, i)   i->windowInit(self());
        DE_NOTIFY_PUBLIC(Resize, i) i->windowResized(self());

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

        checkResize();
    }

    void checkResize()
    {
        if (!readyNotified) return; // We'll do this later.

        int pw, ph;
        SDL_GL_GetDrawableSize(window, &pw, &ph);

        const Size pendingSize = Size(pw, ph);

        // Only react if this is actually a resize.
        if (currentSize != pendingSize)
        {
            debug("[GLWindow] Drawable size is %dx%d pixels", pw, ph);
            currentSize = pendingSize;

            self().makeCurrent();
            DE_NOTIFY_PUBLIC(Resize, i)
            {
                i->windowResized(self());
            }
            self().doneCurrent();
        }

        updatePixelRatio();
    }

    void frameWasSwapped()
    {
        updateFrameRateStatistics();

        LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();        
        DE_NOTIFY_PUBLIC(Swap, i) { i->windowSwapped(self()); }
    }

    void checkWhichDisplay()
    {
        const int disp = SDL_GetWindowDisplayIndex(window);
        if (disp != displayIndex)
        {
            displayIndex = disp;
            debug("[GLWindow] display index changed: %d", displayIndex);
            DE_NOTIFY_PUBLIC(Display, i) { i->windowDisplayChanged(self()); }
            updatePixelRatio();
        }
    }

    DE_PIMPL_AUDIENCES(Init, Resize, Display, PixelRatio, Swap, Move, Visibility)
};

DE_AUDIENCE_METHODS(GLWindow, Init, Resize, Display, PixelRatio, Swap, Move, Visibility)

GLWindow::GLWindow(const String &id)
    : d(new Impl(this, id))
{
    d->handler = new WindowEventHandler(this);
    d->handler->setKeyboardMode(WindowEventHandler::RawKeys);
    
    d->handler->audienceForMouseStateChange() += [this]() {
        const bool trap = d->handler->isMouseTrapped();
        SDL_SetWindowGrab(d->window, trap ? SDL_TRUE : SDL_FALSE);
        SDL_SetRelativeMouseMode(trap ? SDL_TRUE : SDL_FALSE);
    };
}

const String &GLWindow::id() const
{
    return d->id;
}

void GLWindow::setTitle(const String &title)
{
    SDL_SetWindowTitle(d->window, title);
}

void GLWindow::setIcon(const Image &image)
{
    const Image rgba = image.convertToFormat(Image::RGBA_8888);
    SDL_Surface *icon = SDL_CreateRGBSurfaceWithFormatFrom(const_cast<dbyte *>(image.bits()),
                                                           image.width(), image.height(),
                                                           32, image.stride(),
                                                           SDL_PIXELFORMAT_ABGR8888);
    SDL_SetWindowIcon(d->window, icon);
    SDL_FreeSurface(icon);
}
    
void GLWindow::setMinimumSize(const Size &minSize)
{
    SDL_SetWindowMinimumSize(d->window, minSize.x, minSize.y);
}

void GLWindow::makeCurrent()
{
    DE_ASSERT_IN_MAIN_THREAD();
    DE_ASSERT(d->glContext);
    currentWindow = this;
    SDL_GL_MakeCurrent(d->window, d->glContext);
    GLStateStack::activate(d->glStack);
    LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();
}

void GLWindow::doneCurrent()
{
    DE_ASSERT_IN_MAIN_THREAD();
    currentWindow = nullptr;
    SDL_GL_MakeCurrent(d->window, nullptr);
}

void GLWindow::show()
{
    SDL_ShowWindow(d->window);
}

void GLWindow::showNormal()
{
    SDL_ShowWindow(d->window);
    SDL_SetWindowFullscreen(d->window, 0);
    SDL_RestoreWindow(d->window);
}

void GLWindow::showMaximized()
{
    SDL_ShowWindow(d->window);
    SDL_SetWindowFullscreen(d->window, 0);
    SDL_MaximizeWindow(d->window);
}

void GLWindow::showFullScreen()
{
    SDL_ShowWindow(d->window);
    const bool isDesktop = (fullscreenDisplayMode() == desktopDisplayMode());
    SDL_SetWindowFullscreen(d->window,
                            isDesktop ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN);
}

void GLWindow::hide()
{
    SDL_HideWindow(d->window);
}

void GLWindow::raise()
{
    SDL_RaiseWindow(d->window);
}

void GLWindow::close()
{
    // Prevent further updates, instead destroying the window on the next update.
    d->isClosing = true;
    update();
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
        glActivate();
        DE_NOTIFY(Resize, i) { i->windowResized(*this); }
    }
}

bool GLWindow::isGLReady() const
{
    return d->readyNotified;
}

bool GLWindow::isMaximized() const
{
    return d->winFlags().testFlag(SDL_WINDOW_MAXIMIZED);
}

bool GLWindow::isMinimized() const
{
    return d->winFlags().testFlag(SDL_WINDOW_MINIMIZED);
}

bool GLWindow::isVisible() const
{
    return d->winFlags().testFlag(SDL_WINDOW_SHOWN);
}

bool GLWindow::isFullScreen() const
{
    return (d->winFlags() & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0;
}

bool GLWindow::isHidden() const
{
    return d->winFlags().testFlag(SDL_WINDOW_HIDDEN);
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

int GLWindow::displayIndex() const
{
    DE_ASSERT(d->displayIndex == SDL_GetWindowDisplayIndex(d->window));
    return d->displayIndex;
}

void GLWindow::setFullscreenDisplayMode(const DisplayMode &mode)
{
    SDL_DisplayMode wanted{};
    wanted.w            = mode.resolution.x;
    wanted.h            = mode.resolution.y;
    wanted.format       = (mode.bitDepth == 16 ? SDL_PIXELFORMAT_RGB565 : SDL_PIXELFORMAT_RGB888);
    wanted.refresh_rate = mode.refreshRate;

    if (mode.isDefault())
    {
        SDL_GetDesktopDisplayMode(d->displayIndex, &wanted);
    }

    SDL_DisplayMode disp;
    if (SDL_GetClosestDisplayMode(d->displayIndex, &wanted, &disp))
    {
        SDL_SetWindowDisplayMode(d->window, &disp);
    }
}

static GLWindow::DisplayMode fromSdl(const SDL_DisplayMode &disp)
{
    return {{disp.w, disp.h}, SDL_BITSPERPIXEL(disp.format), disp.refresh_rate};
}

GLWindow::DisplayMode GLWindow::fullscreenDisplayMode() const
{
    SDL_DisplayMode disp;
    SDL_GetWindowDisplayMode(d->window, &disp);
    return fromSdl(disp);
}

GLWindow::DisplayMode GLWindow::desktopDisplayMode() const
{
    SDL_DisplayMode disp;
    SDL_GetDesktopDisplayMode(d->displayIndex, &disp);
    return fromSdl(disp);
}

bool GLWindow::isNotDesktopDisplayMode() const
{
    return fullscreenDisplayMode() != desktopDisplayMode();
}

List<GLWindow::DisplayMode> GLWindow::displayModes(int displayIndex)
{
    List<DisplayMode> modes;
    for (int i = 0; i < SDL_GetNumDisplayModes(displayIndex); ++i)
    {
        SDL_DisplayMode disp;
        SDL_GetDisplayMode(displayIndex, i, &disp);
        modes << fromSdl(disp);
    }
    return modes;
}

duint GLWindow::pointWidth() const
{
    return pointSize().x;
}

duint GLWindow::pointHeight() const
{
    return pointSize().y;
}

duint GLWindow::pixelWidth() const
{
    return pixelSize().x;
}

duint GLWindow::pixelHeight() const
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

void GLWindow::handleWindowEvent(const void *ptr)
{
    const SDL_Event &event = *reinterpret_cast<const SDL_Event *>(ptr);

    glActivate();
    switch (event.window.event)
    {
        case SDL_WINDOWEVENT_EXPOSED:
            debug("[GLWindow] %p window expose event", this);
            if (!d->initialized)
            {
                initializeGL();
                update();
            }
            d->updatePixelRatio();
            d->checkWhichDisplay();
            break;

        case SDL_WINDOWEVENT_MOVED:
            d->checkWhichDisplay();
            DE_NOTIFY(Move, i)
            {
                i->windowMoved(*this, Vec2i(event.window.data1, event.window.data2));
            }
            break;

        case SDL_WINDOWEVENT_RESIZED:
            d->resizeEvent(event.window);
            d->checkWhichDisplay();
            break;

        case SDL_WINDOWEVENT_CLOSE:
            windowAboutToClose();
            break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_FOCUS_LOST: d->handler->handleSDLEvent(&event); break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_MINIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
        case SDL_WINDOWEVENT_HIDDEN: break;

        case SDL_WINDOWEVENT_SHOWN:
            update();
            break;

        default: break;
    }
}

void GLWindow::grabToFile(const NativePath &path) const
{
    grabImage().save(path);
}

Image GLWindow::grabImage(const Size &outputSize) const
{
    return grabImage(Rectanglei::fromSize(pixelSize()), outputSize);
}

Image GLWindow::grabImage(const Rectanglei &area, const Size &outputSize) const
{
    // We will be grabbing the visible, latest complete frame.
    Image grabbed = framebuffer().toImage();
    if (area.size() != grabbed.size())
    {
        // Just take a portion of the full image.
        grabbed = grabbed.subImage(area);
    }
    if (outputSize != Size())
    {
        grabbed.resize(outputSize);
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

void GLWindow::update()
{
    if (!d->paintPending)
    {
        d->paintPending = true;
        EventLoop::post(new CoreEvent(this, [this]() {
            if (d->isClosing)
            {
                delete this;
                return;
            }
            d->paintPending = false;
            makeCurrent();
            LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();
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
    DE_ASSERT(SDL_GL_GetCurrentContext() == d->glContext);

    if (d->isClosing) return;

    GLFramebuffer::setDefaultFramebuffer(0);

    // Repainting of the window should continue in an indefinite loop.
    // Before doing anything else, submit a new event to repaint the window.
    // If changing the current UI/frame/world time causes side effects
    // such as another event loop running busy mode, we'll still get uninterrupted
    // window content refresh.
    EventLoop::post(new CoreEvent(this, [this]() {
        update();
        WindowSystem::get().pollAndDispatchEvents(); // process new input/window events
    }));

    // Do not proceed with painting the window contents until after the application
    // has completed GL initialization. This is done via timer callback because we
    // don't want to perform a long-running operation during a paint event.
    if (!d->readyNotified)
    {
        debug("[GLWindow] paintGL notifying ready");
        if (!d->readyPending)
        {
            d->readyPending = true;
            d->mainCall.enqueue([this]() { d->notifyReady(); });
        }
        LIBGUI_ASSERT_GL_CONTEXT_ACTIVE();
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(d->window);
        d->frameWasSwapped();
        return;
    }

    DE_ASSERT(SDL_GL_GetCurrentContext() == d->glContext);

    d->checkResize();
    makeCurrent();
    LIBGUI_ASSERT_GL_OK();

    GLBuffer::resetDrawCount();

    // Make sure any changes to the state stack are in effect.
    GLState::current().target().glBind();

    LIBGUI_ASSERT_GL_OK();
    
    // This will be the current time for the frame.
    {
        Time::updateCurrentHighPerformanceTime();
        Clock::get().setTime(Time::currentHighPerformanceTime());
        // Clock observers may have deactivated the GL context.
        makeCurrent();
        LIBGUI_ASSERT_GL_OK();
    }

    // Subclass-implemented drawing method.
    DE_ASSERT(SDL_GL_GetCurrentContext() == d->glContext);
    draw();

    LIBGUI_ASSERT_GL_OK();

    // Show the final frame contents.
    SDL_GL_SwapWindow(d->window);

    d->frameWasSwapped();
}

void GLWindow::windowAboutToClose()
{
    // Derived classes should take this as a request to close the window now.
}

bool GLWindow::mainExists() // static
{
    return WindowSystem::mainExists();
}

GLWindow &GLWindow::getMain() // static
{
    //    DE_ASSERT(mainWindow != nullptr);
    //    return *mainWindow;
    return WindowSystem::getMain();
}

void GLWindow::glActivateMain()
{
    if (mainExists()) getMain().glActivate();
}

GLWindow &GLWindow::current() // static
{
    // if (currentWindow == nullptr) 
    // {
    //     debug("no window: %p", SDL_GL_GetCurrentContext());
    // }
    DE_ASSERT(currentWindow != nullptr);
    return *currentWindow;
}

void GLWindow::setMain(GLWindow *window) // static
{
//    mainWindow = window;
//    GuiLoop::get().setWindow(window);
    WindowSystem::get().setMainWindow(window->id());
}

void *GLWindow::sdlWindow() const
{
    return d->window;
}

} // namespace de
