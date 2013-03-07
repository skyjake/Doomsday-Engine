/** @file windowsystem.h  Window management subsystem.
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_WINDOWSYSTEM_H
#define CLIENT_WINDOWSYSTEM_H

#include <de/System>

/**
 * Window management subsystem.
 *
 * The window system processes events produced by the input drivers. In
 * practice, the events are passed to the widgets in the windows.
 *
 * @ingroup gui
 */
class WindowSystem : public de::System
{
public:
    WindowSystem();

    bool processEvent(de::Event const &);

    void timeChanged(de::Clock const &);

private:
    DENG2_PRIVATE(d)
};

#endif // CLIENT_WINDOWSYSTEM_H
