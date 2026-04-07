/** @file main.cpp Application startup and shutdown.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/libcore.h>
#include <de/counted.h>
#include "shellapp.h"

#if defined (DE_CYGWIN) || defined (DE_MSYS)
#  include <the_Foundation/path.h>
#endif

int main(int argc, char *argv[])
{
    init_Foundation();
#if defined (DE_CYGWIN) || defined (DE_MSYS)
    {
        // Curses needs to have terminfo.
        de::NativePath exePath(de::String::take(unixToWindows_Path(argv[0])));
        setenv("TERMINFO", exePath.fileNamePath() / "..\\share\\terminfo", 1);
    }
#endif
    int result;
    {
        ShellApp a(argc, argv);
        a.initSubsystems();
        result = a.exec();
    }
#if defined (DE_DEBUG)
    DE_ASSERT(de::Counted::totalCount == 0);
#endif
    deinit_Foundation();
    return result;
}
