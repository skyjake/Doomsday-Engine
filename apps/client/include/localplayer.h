/** @file localplayer.h
 *
 * @authors Copyright (c) 2015-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef CLIENT_LOCALPLAYER_H
#define CLIENT_LOCALPLAYER_H

#include <de/libcore.h>

/**
 * Represents a local player.
 * 
 * Usually there is only one of these, except in a split-screen setup where multiple
 * players are controlling players locally at the same time.
 */
class LocalPlayer
{
public:
    LocalPlayer();
    
private:
    DE_PRIVATE(d)
};

#endif // CLIENT_LOCALPLAYER_H
