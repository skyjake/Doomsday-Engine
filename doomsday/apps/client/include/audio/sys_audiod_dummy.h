/** @file
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
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

/**
 * sys_audiod_dummy.h: Dummy Music Driver.
 *
 * Used in dedicated server mode.
 */

#ifndef __DOOMSDAY_SYSTEM_AUDIO_DUMMY_H__
#define __DOOMSDAY_SYSTEM_AUDIO_DUMMY_H__

#include <de/liblegacy.h>
#include "api_audiod.h"
#include "api_audiod_sfx.h"
#include "api_audiod_mus.h"

DE_EXTERN_C audiodriver_t          audiod_dummy;
DE_EXTERN_C audiointerface_sfx_t   audiod_dummy_sfx;
DE_EXTERN_C audiointerface_music_t audiod_dummy_music;
DE_EXTERN_C audiointerface_cd_t    audiod_dummy_cd;

#endif
