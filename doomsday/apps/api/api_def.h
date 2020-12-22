/** @file api_def.h Public API for definitions.
 * @ingroup defs
 *
 * @authors Copyright &copy; 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_DEF_H
#define DOOMSDAY_DEF_H

#include "api_base.h"

/// @addtogroup defs
///@{

DE_API_TYPEDEF(Def) // v2
{
    de_api_t api;

    int (*_Get)(int type, const char *id, void *out);
    int (*_Set)(int type, int index, int value, const void *ptr);
    int (*EvalFlags)(const char *flags);
}
DE_API_T(Def);

#ifndef DE_NO_API_MACROS_DEFINITIONS
#define Def_Get         _api_Def._Get
#define Def_Set         _api_Def._Set
#define Def_EvalFlags   _api_Def.EvalFlags
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Def);
#endif

///@}

#endif // DOOMSDAY_DEF_H
