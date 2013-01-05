/** @file api_internaldata.h Public API for accessing the engine's internal data.
 * @ingroup base
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DOOMSDAY_API_INTERNALDATA_H
#define DOOMSDAY_API_INTERNALDATA_H

#include "apis.h"
#include "dd_share.h"

/**
 * Data exported out of the Doomsday engine.
 *
 * @deprecated Avoid this API.
 */
DENG_API_TYPEDEF(InternalData)
{
    de_api_t api;

    // Data arrays.
    mobjinfo_t **   mobjInfo;
    state_t **      states;
    sprname_t **    sprNames;
    ddtext_t **     text;

    // General information.
    int *           validCount;
}
DENG_API_T(InternalData);

#ifdef __DOOMSDAY__
DENG_USING_API(InternalData);
#endif

#endif // DOOMSDAY_API_INTERNALDATA_H
