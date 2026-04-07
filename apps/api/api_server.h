/** @file api_server.h Public API for the server.
 * @ingroup server
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DOOMSDAY_API_SERVER_H
#define DOOMSDAY_API_SERVER_H

#include "apis.h"

DE_API_TYPEDEF(Server)
{
    de_api_t api;

    /**
     * Determines whether the coordinates sent by a player are valid at the
     * moment.
     */
    dd_bool (*CanTrustClientPos)(int player);
}
DE_API_T(Server);

#ifndef DE_NO_API_MACROS_SERVER
#define Sv_CanTrustClientPos    _api_Server.CanTrustClientPos
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Server);
#endif

#endif // DOOMSDAY_API_SERVER_H
