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
#include <de/FileSystem>
#include <de/Log>
#include <de/NativePath>
#include <de/Thread>
#include <de/EventLoop>

#include <SDL2/SDL.h>
#include <SDL2/SDL_video.h>

//#ifdef DE_QT_5_0_OR_NEWER
//#  include <QStandardPaths>
//#else
//#  include <QDesktopServices>
//#endif

namespace de {

DE_PIMPL(GuiApp)
{
    EventLoop eventLoop;
    GuiLoop   loop;
    Thread *  renderThread;

    Impl(Public *i) : Base(i)
    {
        loop.setRate(120);
        loop.audienceForIteration() += self();

        // The default render thread is the main thread.
        renderThread = Thread::currentThread();
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
    : //LIBGUI_GUIAPP_BASECLASS(argc, argv)
      App(args)
    , d(new Impl(this))
{
    if (!SDL_InitSubSystem(SDL_INIT_EVENTS |
                           SDL_INIT_VIDEO |
                           SDL_INIT_JOYSTICK |
                           SDL_INIT_GAMECONTROLLER))
    {
        throw Error("GuiApp::GuiApp", "Failed to initialize SDL");
    }

    static ImageFile::Interpreter intrpImageFile;
    fileSystem().addInterpreter(intrpImageFile);

    // Core packages for GUI functionality.
    addInitPackage("net.dengine.stdlib.gui");
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

int GuiApp::execLoop()
{
    LOGDEV_NOTE("Starting GuiApp event loop...");

    int code = d->eventLoop.exec([this](){
        d->loop.start();
    });

    LOGDEV_NOTE("GuiApp event loop exited with code %i") << code;
    return code;
}

void GuiApp::stopLoop(int code)
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
