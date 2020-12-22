/** @file main_server.cpp  Server application entrypoint.
 * @ingroup base
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "serverapp.h"
#include <de/escapeparser.h>

using namespace de;

DE_EXTERN_C void GameKit_Init();

/**
 * Server application entry point.
 */
int main(int argc, char** argv)
{
    int exitCode;
    init_Foundation();
    GameKit_Init();
    ServerApp serverApp(makeList(argc, argv));
    try
    {
        serverApp.initialize();
        exitCode = serverApp.exec();
    }
    catch (const Error &er)
    {
        EscapeParser esc;
        esc.parse(er.asText());
        warning("App init failed: %s", esc.plainText().c_str());
        exitCode = -1;
    }
    deinit_Foundation();
    return exitCode;
}

