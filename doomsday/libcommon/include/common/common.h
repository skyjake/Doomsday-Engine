/**
 * @mainpage The Doomsday Engine Project: libcommon
 *
 * Copyright &copy; 2009 Jaakko Ker&auml;nen <jaakko.keranen@iki.fi>
 *
 * <p>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but <b>without any warranty</b>; without even the implied warranty of
 * <b>merchantability</b> or <b>fitness for a particular purpose</b>.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see http://www.gnu.org/licenses/
 */

/**
 * @file common.h
 *
 * Header file that contains common definitions and constants that will be used 
 * by libcommon.
 */ 

#ifndef LIBCOMMON_H
#define LIBCOMMON_H

#include <de/deng.h>

#ifdef WIN32
#   ifdef LIBCOMMON_EXPORTS
#       define LIBCOMMON_API __declspec(dllexport)
#   else
#       define LIBCOMMON_API __declspec(dllimport)
#   endif
#   define LIBCOMMON_EXPORT __declspec(dllexport)
#else
#   define LIBCOMMON_API
#   define LIBCOMMON_EXPORT
#endif

// Versioning.
#ifdef LIBCOMMON_RELEASE_LABEL
#   define LIBCOMMON_VERSION LIBDENG2_VER4( \
        LIBCOMMON_RELEASE_LABEL, \
        LIBCOMMON_MAJOR_VERSION, \
        LIBCOMMON_MINOR_VERSION, \
        LIBCOMMON_PATCHLEVEL)
#else
#   define LIBCOMMON_VERSION LIBDENG2_VER3( \
        LIBCOMMON_MAJOR_VERSION, \
        LIBCOMMON_MINOR_VERSION, \
        LIBCOMMON_PATCHLEVEL)
#endif

#endif /* LIBCOMMON_H */
