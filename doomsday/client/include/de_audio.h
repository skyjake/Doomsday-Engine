/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * de_audio.h: Audio Subsystem
 */

#ifndef __DOOMSDAY_AUDIO__
#define __DOOMSDAY_AUDIO__

#ifdef __CLIENT__
#  include "audio/audiodriver.h"
#  include "audio/audiodriver_music.h"
#  include "audio/s_sfx.h"
#  include "audio/s_mus.h"
#endif

#include "audio/s_main.h"
#include "audio/s_cache.h"
#include "audio/s_environ.h"
#include "audio/s_wav.h"
#include "audio/s_logic.h"

#endif
