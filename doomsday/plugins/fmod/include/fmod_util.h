/**
 * @file fmod_util.h
 * Utilities. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef __DSFMOD_UTIL_H__
#define __DSFMOD_UTIL_H__

#include <fmod.h>
#include <fmod.hpp>
#include <cstring>
#include <string.h>

#ifdef UNIX
#  define strnicmp strncasecmp
#endif

#ifdef WIN32
#  define strnicmp _strnicmp
#endif

class FMODVector : public FMOD_VECTOR {
public:
    FMODVector(float _x = 0, float _y = 0, float _z = 0) {
        x = _x;
        y = _y;
        z = _z;
    }
    void set(const float* values) {
        x = values[0];
        y = values[1];
        z = values[2];
    }
};

template <typename T> void zeroStruct(T& t) {
    std::memset(&t, 0, sizeof(T));
    t.cbsize = sizeof(T);
}

bool endsWith(const char* str, const char* ending);

#endif /* end of include guard: __DSFMOD_UTIL_H__ */
