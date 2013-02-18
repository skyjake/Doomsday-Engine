/** @file textapp.h  Application with text-based/console interface.
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

#include "de/TextApp"
#include <de/Log>
#include <de/math.h>
#include <QDir>
#include <QTimer>

namespace de {

DENG2_PIMPL(TextApp)
{
    TimeDelta refreshInterval;
    bool loopRunning;

    Instance(Public *i)
        : Base(i),
          refreshInterval(0),
          loopRunning(false)
    {}
};

TextApp::TextApp(int &argc, char **argv)
    : QCoreApplication(argc, argv),
      App(applicationFilePath(), arguments()),
      d(new Instance(this))
{}

TextApp::~TextApp()
{
    delete d;
}

bool TextApp::notify(QObject *receiver, QEvent *event)
{
    try
    {
        return QCoreApplication::notify(receiver, event);
    }
    catch(std::exception const &error)
    {
        handleUncaughtException(error.what());
    }
    catch(...)
    {
        handleUncaughtException("de::TextApp caught exception of unknown type.");
    }
    return false;
}

int TextApp::execLoop()
{
    d->loopRunning = true;
    /// @todo LegacyCore currently updates clock time for us.
    //refresh();
    return QCoreApplication::exec();
}

void TextApp::stopLoop(int code)
{
    d->loopRunning = false;
    return QCoreApplication::exit(code);
}

NativePath TextApp::appDataPath() const
{
    return NativePath(QDir::homePath()) / ".doomsday";
}

void TextApp::refresh()
{
    if(!d->loopRunning) return;

    // Update the clock time. App listens to this clock and will inform
    // subsystems in the order they've been added in.
    Clock::appClock().setTime(Time());

    // Schedule the next refresh.
    QTimer::singleShot(de::max(duint64(1), d->refreshInterval.asMilliSeconds()), this, SLOT(refresh()));
}

} // namespace de

