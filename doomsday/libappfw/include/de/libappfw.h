/** @file libappfw.h  Common definitions for libappfw.
 *
 * @authors Copyright © 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBAPPFW_MAIN_H
#define LIBAPPFW_MAIN_H

/*
 * The LIBAPPFW_PUBLIC macro is used for declaring exported symbols. It must be
 * applied in all exported classes and functions. DEF files are not used for
 * exporting symbols out of libappfw.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __LIBAPPFW__
// This is defined when compiling the library.
#    define LIBAPPFW_PUBLIC __declspec(dllexport)
#  else
#    define LIBAPPFW_PUBLIC __declspec(dllimport)
#  endif
#else
// No need to use any special declarators.
#  define LIBAPPFW_PUBLIC
#endif

namespace de {

} // namespace de

#endif // LIBAPPFW_MAIN_H
