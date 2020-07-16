/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/commandline.h>
#include <de/log.h>
#include <de/textapp.h>

#include <iostream>

using namespace de;

int main(int argc, char **argv)
{
    init_Foundation();
    try
    {
        TextApp app(makeList(argc, argv));
        app.initSubsystems();

        CommandLine cmd;
#ifdef UNIX
        cmd << "/bin/ls";
        cmd << "-l";
#endif

        String output;
        if (cmd.executeAndWait(&output))
        {
            LOG_MSG("Output from %s:\n%s") << cmd.at(0) << output;
        }
        else
        {
            LOG_WARNING("Failed to execute!");
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
