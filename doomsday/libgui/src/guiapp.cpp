/** @file guiapp.cpp  Application with GUI support.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "de/GuiApp"
#include <de/Log>

#include <QTimer>
#include <QDesktopServices>

namespace de {

DENG2_PIMPL(GuiApp)
{
    Loop loop;

    Instance(Public *i) : Base(i)
    {
        loop.audienceForIteration += self;
    }
};

GuiApp::GuiApp(int &argc, char **argv)
    : QApplication(argc, argv),
      App(applicationFilePath(), arguments()),
      d(new Instance(this))
{}

bool GuiApp::notify(QObject *receiver, QEvent *event)
{
    try
    {
        return QApplication::notify(receiver, event);
    }
    catch(std::exception const &error)
    {
        handleUncaughtException(error.what());
    }
    catch(...)
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
    LOG_MSG("Starting GuiApp event loop...");

    d->loop.start();
    int code = QApplication::exec();

    LOG_MSG("GuiApp event loop exited with code %i") << code;
    return code;
}

void GuiApp::stopLoop(int code)
{
    LOG_MSG("Stopping GuiApp event loop");

    d->loop.stop();
    return QApplication::exit(code);
}

Loop &GuiApp::loop()
{
    return d->loop;
}

void GuiApp::loopIteration()
{
    //LOG_DEBUG("GuiApp loopIteration @ ") << Time().asText();

    // Update the clock time. de::App listens to this clock and will inform
    // subsystems in the order they've been added.
    Clock::appClock().setTime(Time());
}

NativePath GuiApp::appDataPath() const
{
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
}

} // namespace de
