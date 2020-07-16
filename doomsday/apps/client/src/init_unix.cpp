/** @file dd_uinit.cpp  Engine Initialization (Unix).
 *
 * @authors Copyright © 2004-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006-2007 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <doomsday/doomsdayapp.h>
#include <doomsday/filesys/fs_util.h>
#include <de/c_wrapper.h>
#include <de/app.h>

#include "de_base.h"
#include "init_unix.h"

#ifdef __CLIENT__
#  include "gl/sys_opengl.h"
#endif

#ifdef __CLIENT__
static int initDGL(void)
{
    return (int) Sys_GLPreInit();
}
#endif

dd_bool DD_Unix_Init(void)
{
    bool failed = true;

    // Determine our basedir and other global paths.
    DoomsdayApp::app().determineGlobalPaths();

    if (!DD_EarlyInit())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error during early init.", 0);
    }
#ifdef __CLIENT__
    else if (!initDGL())
    {
        Sys_MessageBox(MBT_ERROR, DOOMSDAY_NICENAME, "Error initializing DGL.", 0);
    }
#endif
    else
    {
        // Everything okay so far.
        failed = false;
    }

    return !failed;
}

/**
 * Shuts down the engine.
 */
void DD_Shutdown(void)
{
    // Shutdown all subsystems.
    DD_ShutdownAll();

//    DoomsdayApp::plugins().unloadAll();
//    Library_Shutdown();

//#ifdef __CLIENT__
//    DisplayMode_Shutdown();
//#endif
}
