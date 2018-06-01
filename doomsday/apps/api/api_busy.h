/** @file api_busy.h  Public API for additional Busy Mode features.
 * @ingroup base
 *
 * @authors Copyright &copy; 2007-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright &copy; 2007-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_BUSY_H
#define DOOMSDAY_API_BUSY_H

#include "api_base.h"

DE_API_TYPEDEF(Busy)
{
    de_api_t api;

    void (*FreezeGameForBusyMode)(void);
}
DE_API_T(Busy);

#ifndef DE_NO_API_MACROS_BUSY
#define BusyMode_FreezeGameForBusyMode _api_Busy.FreezeGameForBusyMode
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Busy);
#endif

#endif /* DOOMSDAY_API_BUSY_H */
