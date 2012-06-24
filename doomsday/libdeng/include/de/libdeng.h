/**
 * @file libdeng.h
 * Common definitions for libdeng.
 *
 * @authors Copyright © 2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG_H
#define LIBDENG_H

#ifdef __cplusplus
#  define DENG_USE_QT
#endif

#if defined(__x86_64__) || defined(__x86_64) || defined(_LP64)
#  define __64BIT__
#endif

#ifdef DENG2_USE_QT
#  include <QtCore/qglobal.h>
#endif

#include <assert.h>

/*
 * The DENG_PUBLIC macro is used for declaring exported symbols. It must be
 * applied in all exported classes and functions. DEF files are not used for
 * exporting symbols out of libdeng2.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __DENG__
// This is defined when compiling the library.
#    define DENG_PUBLIC __declspec(dllexport)
#  else
#    define DENG_PUBLIC __declspec(dllimport)
#  endif
#else
// No need to use any special declarators.
#  define DENG_PUBLIC
#endif

#ifndef NDEBUG
#  define DENG_DEBUG
#  ifdef DENG_USE_QT
#    define DENG_ASSERT(x) Q_ASSERT(x)
#  else
#    define DENG_ASSERT(x) assert(x)
#  endif
#  define DENG_DEBUG_ONLY(x) x
#else
#  define DENG_NO_DEBUG
#  define DENG_ASSERT(x)
#  define DENG_DEBUG_ONLY(x)
#endif

#endif // LIBDENG_H
