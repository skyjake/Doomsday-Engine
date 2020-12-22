/** @file api_thinker.h Thinkers.
 * @ingroup thinker
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DE_THINKER_H
#define DE_THINKER_H

#include "api_base.h"
#include <de/legacy/reader.h>
#include <de/legacy/writer.h>
#include <doomsday/world/thinker.h>

#ifdef __DOOMSDAY__
namespace world {
class Map;
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup thinker Thinkers
 * @ingroup playsim
 */
///@{

DE_API_TYPEDEF(Thinker)
{
    de_api_t api;

    void (*Init)(void);
    void (*Run)(void);
    void (*Add)(thinker_t *th);
    void (*Remove)(thinker_t *th);

    int (*Iterate)(thinkfunc_t func, int (*callback) (thinker_t *, void *), void *context);
}
DE_API_T(Thinker);

#ifndef DE_NO_API_MACROS_THINKER
#define Thinker_Init        _api_Thinker.Init
#define Thinker_Run         _api_Thinker.Run
#define Thinker_Add         _api_Thinker.Add
#define Thinker_Remove      _api_Thinker.Remove
#define Thinker_Iterate     _api_Thinker.Iterate
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Thinker);
#endif

///@}

#ifdef __cplusplus
} // extern "C"
#endif

#endif // DE_THINKER_H
