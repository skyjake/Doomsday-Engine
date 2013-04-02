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

namespace de {

DENG2_PIMPL(TextApp)
{
    Loop loop;

    Instance(Public *i) : Base(i)
    {
        loop.audienceForIteration += self;

        // In text-based apps, we can limit the loop frequency.
        loop.setRate(35);
    }
};

TextApp::TextApp(int &argc, char **argv)
    : QCoreApplication(argc, argv),
      App(applicationFilePath(), arguments()),
      d(new Instance(this))
{}

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
    LOG_MSG("Starting TextApp event loop...");

    d->loop.start();
    int code = QCoreApplication::exec();

    LOG_MSG("TextApp event loop exited with code %i") << code;
    return code;
}

void TextApp::stopLoop(int code)
{
    d->loop.stop();
    return QCoreApplication::exit(code);
}

Loop &TextApp::loop()
{
    return d->loop;
}

NativePath TextApp::appDataPath() const
{
    return NativePath(QDir::homePath()) / ".doomsday";
}

void TextApp::loopIteration()
{
    // Update the clock time. App listens to this clock and will inform
    // subsystems in the order they've been added in.
    Clock::appClock().setTime(Time());
}

} // namespace de

