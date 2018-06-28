/** @file guiapp.cpp  Application with GUI support.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/GuiApp"
#include "de/graphics/opengl.h"
#include "de/ImageFile"
#include "de/GLWindow"

#include <de/CommandLine>
#include <de/Config>
#include <de/DisplayMode>
#include <de/EventLoop>
#include <de/FileSystem>
#include <de/Log>
#include <de/NativePath>
#include <de/ScriptSystem>
#include <de/Thread>

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_events.h>

namespace de {

DE_PIMPL(GuiApp)
{
    EventLoop eventLoop;
    GuiLoop   loop;
    Thread *  renderThread;
    double    dpiFactor = 1.0;

    Impl(Public *i) : Base(i)
    {
        loop.setRate(120);
        loop.audienceForIteration() += self();

        // The default render thread is the main thread.
        renderThread = Thread::currentThread();
    }

    ~Impl()
    {
        DisplayMode_Shutdown();
        SDL_Quit();
    }

    void determineDevicePixelRatio()
    {
#if defined (WIN32)
        // Use the Direct2D API to find out the desktop DPI factor.
        ID2D1Factory *d2dFactory = nullptr;
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dFactory);
        if (SUCCEEDED(hr))
        {
            FLOAT dpiX = 96;
            FLOAT dpiY = 96;
            d2dFactory->GetDesktopDpi(&dpiX, &dpiY);
            dpiFactor = dpiX / 96.0;
            d2dFactory->Release();
            d2dFactory = nullptr;
        }
#else
        // Use a hidden SDL window to determine pixel ratio.
        SDL_Window *temp =
            SDL_CreateWindow("",
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             100,
                             100,
                             SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI);
        int points, pixels;
        SDL_GetWindowSize(temp, &points, nullptr);
        SDL_GL_GetDrawableSize(temp, &pixels, nullptr);
        SDL_DestroyWindow(temp);

        dpiFactor = double(pixels) / double(points);
#endif
    }

    void matchLoopRateToDisplayMode()
    {
        SDL_DisplayMode mode;
        if (SDL_GetCurrentDisplayMode(0, &mode) == 0)
        {
            LOG_GL_MSG("Current display mode refresh rate: %d Hz") << mode.refresh_rate;
            self().loop().setRate(mode.refresh_rate ? mode.refresh_rate : 60.0);
        }
    }

    /**
     * Get events from SDL and route them to the appropriate place for handling.
     */
    void postEvents()
    {
        GLWindow *window = GLWindow::mainExists() ? &GLWindow::main() : nullptr;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                eventLoop.quit(0);
                break;

            case SDL_WINDOWEVENT:
            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            case SDL_TEXTINPUT:
                if (window) window->handleSDLEvent(&event);
                break;
            }
        }
    }

    DE_PIMPL_AUDIENCE(DisplayModeChange)
};
DE_AUDIENCE_METHOD(GuiApp, DisplayModeChange)

//void GuiApp::setDefaultOpenGLFormat() // static
//{
//    QSurfaceFormat fmt;
//#if defined (DE_OPENGL_ES)
//    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
//    fmt.setVersion(DE_OPENGL_ES / 10, DE_OPENGL_ES % 10);
//#else
//    fmt.setRenderableType(QSurfaceFormat::OpenGL);
//    fmt.setProfile(QSurfaceFormat::CoreProfile);
//    fmt.setVersion(3, 3);
//#endif
//    fmt.setDepthBufferSize(24);
//    fmt.setStencilBufferSize(8);
//    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
//#if defined (DE_DEBUG)
//    fmt.setOption(QSurfaceFormat::DebugContext, true);
//#endif
//    QSurfaceFormat::setDefaultFormat(fmt);
//}

GuiApp::GuiApp(const StringList &args)
    : App(args)
    , d(new Impl(this))
{
    if (SDL_InitSubSystem(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_JOYSTICK |
                          SDL_INIT_GAMECONTROLLER) != 0)
    {
        throw Error("GuiApp::GuiApp", stringf("Failed to initialize SDL: %s", SDL_GetError()));
    }
    if (SDL_GetNumVideoDisplays() == 0)
    {
        throw Error("GuiApp::GuiApp", "No video displays available");
    }

    d->matchLoopRateToDisplayMode();
    d->determineDevicePixelRatio();

    static ImageFile::Interpreter intrpImageFile;
    fileSystem().addInterpreter(intrpImageFile);

    // Core packages for GUI functionality.
    addInitPackage("net.dengine.stdlib.gui");
}

void GuiApp::initSubsystems(SubsystemInitFlags subsystemInitFlags)
{
    App::initSubsystems(subsystemInitFlags);

    // The "-dpi" option overrides the detected DPI factor.
    if (auto dpi = commandLine().check("-dpi", 1))
    {
        d->dpiFactor = dpi.params.at(0).toDouble();
    }

    // Apply the overall UI scale factor.
    d->dpiFactor *= config().getf("ui.scaleFactor", 1.f);

    DisplayMode_Init();
    scriptSystem().nativeModule("DisplayMode").set("DPI_FACTOR", d->dpiFactor);
}

double GuiApp::dpiFactor() const
{
    return d->dpiFactor;
}

void GuiApp::setMetadata(String const &orgName, String const &orgDomain,
                         String const &appName, String const &appVersion)
{
    Record &amd = metadata();

    amd.set(APP_NAME,    appName);
    amd.set(APP_VERSION, appVersion);
    amd.set(ORG_NAME,    orgName);
    amd.set(ORG_DOMAIN,  orgDomain);
}

void GuiApp::notifyDisplayModeChanged()
{
    DE_FOR_AUDIENCE2(DisplayModeChange, i)
    {
        i->displayModeChanged();
    }
}

//bool GuiApp::notify(QObject *receiver, QEvent *event)
//{
//    try
//    {
//        return LIBGUI_GUIAPP_BASECLASS::notify(receiver, event);
//    }
//    catch (std::exception const &error)
//    {
//        handleUncaughtException(error.what());
//    }
//    catch (...)
//    {
//        handleUncaughtException("de::GuiApp caught exception of unknown type.");
//    }
//    return false;
//}

//void GuiApp::notifyDisplayModeChanged()
//{
//    emit displayModeChanged();
//}

int GuiApp::exec(const std::function<void ()> &startup)
{
    LOGDEV_NOTE("Starting GuiApp event loop...");

    int code = d->eventLoop.exec([this, startup](){
        d->loop.start();
        if (startup) startup();
    });

    LOGDEV_NOTE("GuiApp event loop exited with code %i") << code;
    return code;
}

void GuiApp::quit(int code)
{
    LOGDEV_MSG("Stopping GuiApp event loop");

    d->loop.stop();
    d->eventLoop.quit(code);
}

GuiLoop &GuiApp::loop()
{
    return d->loop;
}

bool GuiApp::inRenderThread()
{
    if (!App::appExists())
    {
        return false;
    }
    return DE_GUI_APP->d->renderThread == Thread::currentThread();
}

void GuiApp::setRenderThread(Thread *thread)
{
    DE_GUI_APP->d->renderThread = thread;
}

void GuiApp::loopIteration()
{
    d->postEvents();

    // Update the clock time. de::App listens to this clock and will inform
    // subsystems in the order they've been added.
    Time::updateCurrentHighPerformanceTime();
    Clock::get().setTime(Time::currentHighPerformanceTime());
}

NativePath GuiApp::appDataPath() const
{
    const auto &amd = metadata();
#if defined (WIN32)
    return NativePath::homePath() / "AppData/Local" / amd.gets(ORG_NAME) / amd.gets(APP_NAME);
#elif defined (MACOSX)
    return NativePath::homePath() / "Library/Application Support" / amd.gets(APP_NAME);
#else
    return NativePath::homePath() / amd.gets(UNIX_HOME);
#endif
}

} // namespace de
