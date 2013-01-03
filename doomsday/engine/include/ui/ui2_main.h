/**
 * @file ui2_main.cpp UI Widgets
 * @ingroup ui
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2005-2012 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG_UI2_MAIN_H
#define LIBDENG_UI2_MAIN_H

#include "dd_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/// We'll use the base template directly as our object.
typedef struct fi_object_s {
    FIOBJECT_BASE_ELEMENTS()
} fi_object_t;

void UI_Init(void);
void UI_Shutdown(void);

void UI2_Ticker(timespan_t ticLength);
void UI2_Drawer(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_UI2_MAIN_H */
