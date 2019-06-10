/*
 * The Doomsday Engine Project
 *
 * Copyright (c) 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "testwindow.h"

#include <de/GuiApp>
#include <de/LogBuffer>
#include <SDL_main.h>

using namespace de;

DE_EXTERN_C int main(int argc, char **argv)
{
    init_Foundation();
    try
    {
        GuiApp app(makeList(argc, argv));
        app.setMetadata("Deng Team", "dengine.net", "GLSandbox", Version().fullNumber());
        app.addInitPackage("net.dengine.test.glsandbox");
        app.initSubsystems();

        TestWindow win;
        win.show();

        return app.exec();
    }
    catch (Error const &err)
    {
        err.warnPlainText();
    }
    debug("Exiting main()...");
    return 0;
}
