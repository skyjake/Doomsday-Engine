/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2011 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/c_wrapper.h"
#include "de/LegacyCore"

LegacyCore* LegacyCore_New(int* argc, char** argv)
{
    de::LegacyCore* lc = new de::LegacyCore(*argc, argv);

    // Also start it right away.
    lc->start();

    return reinterpret_cast<LegacyCore*>(lc);
}

void LegacyCore_Delete(LegacyCore* lc)
{
    de::LegacyCore* self = reinterpret_cast<de::LegacyCore*>(lc);

    // It gets stopped automatically.
    delete self;
}
