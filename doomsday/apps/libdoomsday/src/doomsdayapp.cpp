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

static DoomsdayApp *theDoomsdayApp = nullptr;

DENG2_PIMPL_NOREF(DoomsdayApp)
{
    BusyMode busyMode;

    ~Instance()
    {
        theDoomsdayApp = nullptr;
    }
};

DoomsdayApp::DoomsdayApp() : d(new Instance)
{
    DENG2_ASSERT(!theDoomsdayApp);
    theDoomsdayApp = this;
}

BusyMode &DoomsdayApp::busyMode()
{
    return d->busyMode;
}

DoomsdayApp &DoomsdayApp::app()
{
    DENG2_ASSERT(theDoomsdayApp);
    return *theDoomsdayApp;
}
