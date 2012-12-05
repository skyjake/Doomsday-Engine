/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/App>
#include <de/Log>

#include <QDebug>

using namespace de;

int main(int argc, char **argv)
{
    try
    {
        App app(argc, argv, App::GUIDisabled);
        app.initSubsystems(App::DisablePlugins);

        for(int i = 0; i < LogEntry::MAX_LOG_LEVELS; ++i)
        {
            LogEntry::Level level = LogEntry::Level(i);
            LogBuffer::appBuffer().enable(level);
            LOG_AT_LEVEL(level, "Enabled level ") << LogEntry::levelToText(level);

            for(int k = 0; k < LogEntry::MAX_LOG_LEVELS; ++k)
            {
                LogEntry::Level other = LogEntry::Level(k);
                LOG_AT_LEVEL(other, "- (currently enabled %8s) entry at level %8s: visible: %b")
                        << LogEntry::levelToText(level)
                        << LogEntry::levelToText(other)
                        << LogBuffer::appBuffer().isEnabled(other);
            }
        }
    }
    catch(Error const &err)
    {
        qWarning() << err.asText();
    }

    qDebug() << "Exiting main()...";
    return 0;        
}
