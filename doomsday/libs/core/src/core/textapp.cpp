/** @file textapp.h  Application with text-based/console interface.
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

#include "de/textapp.h"
#include "de/eventloop.h"
#include "de/log.h"
#include "de/garbage.h"
#include "de/nativepath.h"
#include "de/math.h"

namespace de {

DE_PIMPL(TextApp)
, DE_OBSERVES(Loop, Iteration)
{
    EventLoop eventLoop;
    Loop loop;

    Impl(Public *i) : Base(i)
    {
        loop.audienceForIteration() += this;

        // In text-based apps, we can limit the loop frequency.
        loop.setRate(35);
    }

    ~Impl() override
    {
        Garbage_Recycle();
    }

    void loopIteration() override
    {
        // Update the clock time. App listens to this clock and will inform
        // subsystems in the order they've been added in.
        Time::updateCurrentHighPerformanceTime();
        Clock::get().setTime(Time());
    }
};

TextApp::TextApp(const StringList &args)
    : App(args)
    , d(new Impl(this))
{}

void TextApp::setMetadata(const String &orgName,
                          const String &orgDomain,
                          const String &appName,
                          const String &appVersion)
{
    auto &amd = metadata(); // Application metadata.
    amd.set(ORG_NAME,    orgName);
    amd.set(ORG_DOMAIN,  orgDomain);
    amd.set(APP_NAME,    appName);
    amd.set(APP_VERSION, appVersion);
}

/*
bool TextApp::notify(QObject *receiver, QEvent *event)
{
    try
    {
        return QCoreApplication::notify(receiver, event);
    }
    catch (const std::exception &error)
    {
        handleUncaughtException(error.what());
    }
    catch (...)
    {
        handleUncaughtException("de::TextApp caught exception of unknown type.");
    }
    return false;
}
*/

int TextApp::exec(const std::function<void()> &startup)
{
    LOGDEV_NOTE("Starting TextApp event loop...");

    int code = d->eventLoop.exec([this, startup]() {
        d->loop.start();
        if (startup) startup();
    });

    LOGDEV_NOTE("TextApp event loop exited with code %i") << code;
    return code;
}

void TextApp::quit(int code)
{
    d->loop.stop();
    d->eventLoop.quit(code);
}

Loop &TextApp::loop()
{
    return d->loop;
}

NativePath TextApp::appDataPath() const
{
    return NativePath::homePath() / unixHomeFolderName();
}

} // namespace de

