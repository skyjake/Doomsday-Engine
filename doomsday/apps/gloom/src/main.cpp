/** @file main.cpp
 *
 * @authors Copyright (c) 2014-2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "gloomapp.h"
#include "mainwindow.h"
#include <de/escapeparser.h>
#include <SDL_messagebox.h>

using namespace de;

int main(int argc, char **argv)
{
    int exitCode = 0;
    init_Foundation();
    GloomApp app(makeList(argc, argv));
    try
    {
        app.initialize();
        exitCode = app.exec();
    }
    catch (const Error &er)
    {
        EscapeParser esc;
        esc.parse(er.asText());
        warning("App init failed: %s", esc.plainText().c_str());
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "Gloom", "App init failed:\n" + esc.plainText(), nullptr);
        exitCode = -1;
    }
#if defined (DE_DEBUG)
    // Check that all reference-counted objects have been deleted.
    DE_ASSERT(Counted::totalCount == 0);
#endif
    deinit_Foundation();
    return exitCode;
}
