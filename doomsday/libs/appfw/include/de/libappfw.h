/** @file libappfw.h  Common definitions for libappfw.
 *
 * @authors Copyright © 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include <de/libcore.h>

/** @namespace de::ui User interface. */

/** @defgroup appfw Application Framework
 * Graphical UI framework based on a tree of widgets. Input events get passed
 * down the tree until handled. Uses Rule objects to calculate positioning of
 * widgets in the view. @ingroup gui */

/** @defgroup guiWidgets GUI Widgets
 * GUI widgets based on the Core library's @ref widgets.
 * @ingroup appfw */

/** @defgroup dialogs Dialogs
 * Widgets for modal or nonmodal dialogs. @ingroup appfw */

/** @defgroup uidata Data Model
 * Data model and general purpose items for representing data in widgets.
 * @ingroup appfw */

/** @defgroup vr Virtual Reality
 * @ingroup gui */

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
#  define LIBAPPFW_PUBLIC   DE_PUBLIC
#endif

#endif // LIBAPPFW_MAIN_H
