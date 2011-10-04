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

#ifndef LEGACYNETWORK_H
#define LEGACYNETWORK_H

namespace de {

/**
 * Network communications for the legacy engine implementation.
 * Implements simple socket networking for streaming blocks of data.
 *
 * There is a C wrapper for LegacyNetwork, @see c_wrapper.h
 */
class LegacyNetwork
{
public:
    LegacyNetwork();
    ~LegacyNetwork();

private:
    struct Instance;
    Instance* d;
};

}

#endif // LEGACYNETWORK_H
