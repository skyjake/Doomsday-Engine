/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/textapp.h>
#include <de/log.h>
#include <de/logfilter.h>

using namespace de;

int main(int argc, char **argv)
{
    init_Foundation();
    try
    {
        TextApp app(makeList(argc, argv));
        app.initSubsystems(App::DisablePersistentData);

        for (int j = 0; j < 2; ++j)
        {
            bool devMode = j > 0;
            app.logFilter().setAllowDev(devMode);

            for (int i = LogEntry::LowestLogLevel; i <= LogEntry::HighestLogLevel; ++i)
            {
                LogEntry::Level level = LogEntry::Level(i);
                app.logFilter().setMinLevel(level);
                LOG_AT_LEVEL(level, "Enabled level %s with dev:%b")
                        << LogEntry::levelToText(level) << devMode;

                for (int k = LogEntry::LowestLogLevel; k <= LogEntry::HighestLogLevel; ++k)
                {
                    for (int d = 0; d < 2; ++d)
                    {
                        duint32 other = k | (d? LogEntry::Dev : 0);
                        LOG_AT_LEVEL(other,
                                     "- (currently enabled %8s) entry at level %8s (context %3s): visible: %b")
                                << LogEntry::levelToText(level)
                                << LogEntry::levelToText(other)
                                << LogEntry::contextToText(other)
                                << LogBuffer::get().isEnabled(LogEntry::Generic | other);
                    }
                }
            }
        }
    }
    catch (const Error &err)
    {
        err.warnPlainText();
    }
    deinit_Foundation();
    debug("Exiting main()...");
    return 0;
}
