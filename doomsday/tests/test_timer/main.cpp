/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <de/CommandLine>
#include <de/Timer>
#include <de/Log>
#include <de/TextApp>

#include <iostream>

using namespace de;

int main(int argc, char **argv)
{
    try
    {
        TextApp app(makeList(argc, argv));
        app.initSubsystems(App::DisablePlugins);

        Timer quittingTime;
        debug("Timer %p created", &quittingTime);
        quittingTime.setInterval(3);
        quittingTime.setSingleShot(true);
        quittingTime.audienceForTrigger() += [&](){ app.stopLoop(12345); };
        quittingTime.start();

        Timer test;
        test.audienceForTrigger() += [](){ debug("Testing!"); };
        test.start(1);

        int code = app.execLoop();
        debug("Event loop returned %i", code);
    }
    catch (Error const &err)
    {
        err.warnPlainText();
    }
    debug("Exiting main()...");
    return 0;
}
