/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_VERSION_H
#define LIBDENG2_VERSION_H

#include <string>

/**
 * @file version.h
 *
 * Header file for version numbering.
 */ 

// SVN revision, file created during build. Defines LIBDENG2_REVISION.
#include "revision.h"

#define LIBDENG2_TO_STRING(s) #s

#define LIBDENG2_VER5(Label, Major, Minor, Patch, Rev) \
    Label" "LIBDENG2_TO_STRING(Major)"."LIBDENG2_TO_STRING(Minor)"."\
    LIBDENG2_TO_STRING(Patch)" ("LIBDENG2_TO_STRING(Rev)")"

#define LIBDENG2_VER4(Major, Minor, Patch, Rev) \
    LIBDENG2_TO_STRING(Major)"."LIBDENG2_TO_STRING(Minor)"."\
    LIBDENG2_TO_STRING(Patch)" ("LIBDENG2_TO_STRING(Rev)")"

#ifdef LIBDENG2_RELEASE_LABEL
#   define LIBDENG2_VERSION LIBDENG2_VER5( \
        LIBDENG2_RELEASE_LABEL, \
        LIBDENG2_MAJOR_VERSION, \
        LIBDENG2_MINOR_VERSION, \
        LIBDENG2_PATCHLEVEL, \
        LIBDENG2_REVISION)
#else
#   define LIBDENG2_VERSION LIBDENG2_VER4( \
        LIBDENG2_MAJOR_VERSION, \
        LIBDENG2_MINOR_VERSION, \
        LIBDENG2_PATCHLEVEL, \
        LIBDENG2_REVISION)
#endif

namespace de
{
    struct Version 
    {
        duint major;
        duint minor;
        duint patchlevel;
        duint revision;
        std::string label;
    };
}

#endif /* LIBDENG2_VERSION_H */
