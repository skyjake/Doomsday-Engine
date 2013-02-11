/** @file libgui.h  Common definitions for libgui.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBGUI_MAIN_H
#define LIBGUI_MAIN_H

/*
 * The LIBGUI_PUBLIC macro is used for declaring exported symbols. It must be
 * applied in all exported classes and functions. DEF files are not used for
 * exporting symbols out of libshell.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __LIBGUI__
// This is defined when compiling the library.
#    define LIBGUI_PUBLIC __declspec(dllexport)
#  else
#    define LIBGUI_PUBLIC __declspec(dllimport)
#  endif
#else
// No need to use any special declarators.
#  define LIBGUI_PUBLIC
#endif

namespace de {

} // namespace de

#endif // LIBGUI_MAIN_H
