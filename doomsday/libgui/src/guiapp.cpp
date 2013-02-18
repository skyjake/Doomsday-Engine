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
#include <de/math.h>

#include <QTimer>
#include <QDesktopServices>

namespace de {

DENG2_PIMPL(GuiApp)
{
    TimeDelta refreshInterval;
    bool loopRunning;

    Instance(Public *i)
        : Base(*i),
          refreshInterval(0),
          loopRunning(false)
    {}
};

GuiApp::GuiApp(int &argc, char **argv)
    : QApplication(argc, argv),
      App(applicationFilePath(), arguments()),
      d(new Instance(this))
{}

GuiApp::~GuiApp()
{
    delete d;
}

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
    d->loopRunning = true;
    /// @todo LegacyCore currently updates clock time for us.
    //refresh();
    return QApplication::exec();
}

void GuiApp::stopLoop(int code)
{
    d->loopRunning = false;
    return QApplication::exit(code);
}

void GuiApp::refresh()
{
    if(!d->loopRunning) return;

    // Update the clock time. App listens to this clock and will inform
    // subsystems in the order they've been added in.
    Clock::appClock().setTime(Time());

    // Schedule the next refresh.
    QTimer::singleShot(de::max(duint64(1), d->refreshInterval.asMilliSeconds()), this, SLOT(refresh()));
}

NativePath GuiApp::appDataPath() const
{
    return QDesktopServices::storageLocation(QDesktopServices::DataLocation);
}

} // namespace de
