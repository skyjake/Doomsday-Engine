/** @file doomsdayapp.cpp  Common application-level state and components.
 *
 * @authors Copyright (c) 2015 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "doomsday/doomsdayapp.h"
#include "doomsday/filesys/sys_direc.h"
#include "doomsday/filesys/fs_util.h"
#include "doomsday/paths.h"

#include <de/App>
#include <de/strutil.h>

#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

using namespace de;

static DoomsdayApp *theDoomsdayApp = nullptr;

DENG2_PIMPL_NOREF(DoomsdayApp)
{
    Plugins plugins;
    BusyMode busyMode;

    /// @c true = We are using a custom user dir specified on the command line.
    bool usingUserDir = false;

#ifdef UNIX
# ifndef MACOSX
    /// @c true = We are using the user dir defined in the HOME environment.
    bool usingHomeDir = false;
# endif
#endif

#ifdef WIN32
    HINSTANCE hInstance = NULL;
#endif

    ~Instance()
    {
        theDoomsdayApp = nullptr;
    }

#ifdef UNIX
    void determineGlobalPaths()
    {
        // By default, make sure the working path is the home folder.
        App::setCurrentWorkPath(App::app().nativeHomePath());

# ifndef MACOSX
        if(getenv("HOME"))
        {
            filename_t homePath;
            directory_t* temp;
            dd_snprintf(homePath, FILENAME_T_MAXLEN, "%s/%s/runtime/", getenv("HOME"),
                        DENG2_APP->unixHomeFolderName().toLatin1().constData());
            temp = Dir_New(homePath);
            Dir_mkpath(Dir_Path(temp));
            usingHomeDir = Dir_SetCurrent(Dir_Path(temp));
            if(usingHomeDir)
            {
                DD_SetRuntimePath(Dir_Path(temp));
            }
            Dir_Delete(temp);
        }
# endif

        // The -userdir option sets the working directory.
        if(CommandLine_CheckWith("-userdir", 1))
        {
            filename_t runtimePath;
            directory_t* temp;

            strncpy(runtimePath, CommandLine_NextAsPath(), FILENAME_T_MAXLEN);
            Dir_CleanPath(runtimePath, FILENAME_T_MAXLEN);
            // Ensure the path is closed with a directory separator.
            F_AppendMissingSlashCString(runtimePath, FILENAME_T_MAXLEN);

            temp = Dir_New(runtimePath);
            usingUserDir = Dir_SetCurrent(Dir_Path(temp));
            if(usingUserDir)
            {
                DD_SetRuntimePath(Dir_Path(temp));
# ifndef MACOSX
                usingHomeDir = false;
# endif
            }
            Dir_Delete(temp);
        }

# ifndef MACOSX
        if(!usingHomeDir && !usingUserDir)
# else
        if(!usingUserDir)
# endif
        {
            // The current working directory is the runtime dir.
            directory_t* temp = Dir_NewFromCWD();
            DD_SetRuntimePath(Dir_Path(temp));
            Dir_Delete(temp);
        }

        // libcore has determined the native base path, so let FS1 know about it.
        DD_SetBasePath(DENG2_APP->nativeBasePath().toUtf8());
    }
#endif // UNIX
};

DoomsdayApp::DoomsdayApp() : d(new Instance)
{
    DENG2_ASSERT(!theDoomsdayApp);
    theDoomsdayApp = this;
}

void DoomsdayApp::determineGlobalPaths()
{
    d->determineGlobalPaths();
}

DoomsdayApp &DoomsdayApp::app()
{
    DENG2_ASSERT(theDoomsdayApp);
    return *theDoomsdayApp;
}

Plugins &DoomsdayApp::plugins()
{
    return DoomsdayApp::app().d->plugins;
}

BusyMode &DoomsdayApp::busyMode()
{
    return DoomsdayApp::app().d->busyMode;
}

bool DoomsdayApp::isUsingUserDir() const
{
    return d->usingUserDir;
}
