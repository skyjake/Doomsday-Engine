/** @file libgamekit.cpp  GameKit.
 *
 * @authors Copyright (c) 2018 Jaakko Keränen <jaakko.keranen@iki.fi>
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

//#include "gamekit/libgamekit.h"
#include <de/extension.h>
#include <de/log.h>
#include <de/liblegacy.h>

DE_EXTENSION(doom);
DE_EXTENSION(doom64);
DE_EXTENSION(heretic);
DE_EXTENSION(hexen);

DE_ENTRYPOINT void GameKit_Init()
{
    LOG_AS("GameKit");
    LOG_MSG("Initializing...");
}
