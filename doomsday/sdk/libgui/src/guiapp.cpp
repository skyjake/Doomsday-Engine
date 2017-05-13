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

#include <QSurfaceFormat>
#include <QThread>

#ifdef DENG2_QT_5_0_OR_NEWER
#  include <QStandardPaths>
#else
#  include <QDesktopServices>
#endif

namespace de {

DENG2_PIMPL(GuiApp)
{
    GuiLoop loop;
    QThread *renderThread = nullptr;

    Impl(Public *i) : Base(i)
    {
        loop.audienceForIteration() += self();
    }
};

void GuiApp::setDefaultOpenGLFormat() // static
{
    QSurfaceFormat fmt;
#if defined (DENG_OPENGL_ES)
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setVersion(DENG_OPENGL_ES / 10, DENG_OPENGL_ES % 10);
#else
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setVersion(3, 3);
#endif
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
#if defined (DENG2_DEBUG)
    fmt.setOption(QSurfaceFormat::DebugContext, true);
#endif
    QSurfaceFormat::setDefaultFormat(fmt);
}

GuiApp::GuiApp(int &argc, char **argv)
    : LIBGUI_GUIAPP_BASECLASS(argc, argv)
    , App(applicationFilePath(), arguments())
    , d(new Impl(this))
{
    static ImageFile::Interpreter intrpImageFile;
    fileSystem().addInterpreter(intrpImageFile);

    // Core packages for GUI functionality.
    addInitPackage("net.dengine.stdlib.gui");
}

void GuiApp::setMetadata(String const &orgName, String const &orgDomain,
                         String const &appName, String const &appVersion)
{
    setName(appName);

    // Qt metadata.
    setOrganizationName  (orgName);
    setOrganizationDomain(orgDomain);
    setApplicationName   (appName);
    setApplicationVersion(appVersion);
}

bool GuiApp::notify(QObject *receiver, QEvent *event)
{
    try
    {
        return LIBGUI_GUIAPP_BASECLASS::notify(receiver, event);
    }
    catch (std::exception const &error)
    {
        handleUncaughtException(error.what());
    }
    catch (...)
    {
        handleUncaughtException("de::GuiApp caught exception of unknown type.");
    }
    return false;
}

void GuiApp::notifyDisplayModeChanged()
{
    emit displayModeChanged();
}

int GuiApp::execLoop()
{
    LOGDEV_NOTE("Starting GuiApp event loop...");

    d->loop.start();
    int code = LIBGUI_GUIAPP_BASECLASS::exec();

    LOGDEV_NOTE("GuiApp event loop exited with code %i") << code;
    return code;
}

void GuiApp::stopLoop(int code)
{
    LOGDEV_MSG("Stopping GuiApp event loop");

    d->loop.stop();
    return LIBGUI_GUIAPP_BASECLASS::exit(code);
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
    return DENG2_GUI_APP->d->renderThread == QThread::currentThread();
}

void GuiApp::setRenderThread(QThread *thread)
{
    DENG2_GUI_APP->d->renderThread = thread;
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
#ifdef DENG2_QT_5_0_OR_NEWER
    return QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#else
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
#endif
}

} // namespace de
