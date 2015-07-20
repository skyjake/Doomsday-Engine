/** @file doomsdayapp.h  Common application-level state and components.
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

#ifndef LIBDOOMSDAY_DOOMSDAYAPP_H
#define LIBDOOMSDAY_DOOMSDAYAPP_H

#include "plugins.h"
#include "busymode.h"
#include "gameexport.h"

/**
 * Common application-level state and components.
 *
 * Both the server and client applications have an instance of DoomsdayApp
 * to manage the common state and subsystems.
 */
class LIBDOOMSDAY_PUBLIC DoomsdayApp
{
public:
    DoomsdayApp();

    void determineGlobalPaths();

    bool isUsingUserDir() const;

public:
    static DoomsdayApp &app();
    static Plugins &plugins();
    static BusyMode &busyMode();

private:
    DENG2_PRIVATE(d)
};

#endif // LIBDOOMSDAY_DOOMSDAYAPP_H
