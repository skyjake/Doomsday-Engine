/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

/*
 * con_decl.h: Console Utility Macros
 */

#ifndef __DOOMSDAY_CONSOLE_DECL_H__
#define __DOOMSDAY_CONSOLE_DECL_H__

#include "con_main.h"

/*
 * Macros for creating new console commands.
 */
#define C_CMD(name, params, fn) \
    { ccmd_t _c = { name, params, CCmd##fn, 0 }; Con_AddCommand(&_c); }

#define C_CMD_FLAGS(name, params, fn, flags) \
    { ccmd_t _c = { name, params, CCmd##fn, flags }; Con_AddCommand(&_c); }

// A handy helper for declaring console commands.
#define D_CMD(x) int CCmd##x(byte src, int argc, char **argv)

/*
 * Macros for creating new console variables.
 */
#define C_VAR(name, ptr, type, flags, min, max, notifyChanged)            \
    { cvar_t _v = { name, flags, type, ptr, min, max, notifyChanged };    \
        Con_AddVariable(&_v); }

#define C_VAR_BYTE(name, ptr, flags, min, max)    \
    C_VAR(name, ptr, CVT_BYTE, flags, min, max, NULL)

#define C_VAR_INT(name, ptr, flags, min, max)     \
    C_VAR(name, ptr, CVT_INT, flags, min, max, NULL)

#define C_VAR_FLOAT(name, ptr, flags, min, max) \
    C_VAR(name, ptr, CVT_FLOAT, flags, min, max, NULL)

#define C_VAR_CHARPTR(name, ptr, flags, min, max) \
    C_VAR(name, ptr, CVT_CHARPTR, flags, min, max, NULL)

// Same as above but allow for a change notification callback func
#define C_VAR_BYTE2(name, ptr, flags, min, max, notifyChanged)    \
    C_VAR(name, ptr, CVT_BYTE, flags, min, max, notifyChanged)

#define C_VAR_INT2(name, ptr, flags, min, max, notifyChanged)     \
    C_VAR(name, ptr, CVT_INT, flags, min, max, notifyChanged)

#define C_VAR_FLOAT2(name, ptr, flags, min, max, notifyChanged) \
    C_VAR(name, ptr, CVT_FLOAT, flags, min, max, notifyChanged)

#define C_VAR_CHARPTR2(name, ptr, flags, min, max, notifyChanged) \
    C_VAR(name, ptr, CVT_CHARPTR, flags, min, max, notifyChanged)

#endif
