/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2007-2013 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

/**
 * sys_audio.h: OS Specific Audio Drivers
 *
 * Not included in de_system.h because this is only needed by the
 * Sfx/Mus modules.
 */

#ifndef __DOOMSDAY_SYSTEM_AUDIO_H__
#define __DOOMSDAY_SYSTEM_AUDIO_H__

#include "api_audiod.h"
#include "api_audiod_sfx.h"
#include "api_audiod_mus.h"
#include "sys_audiod_dummy.h"

#ifndef DENG_DISABLE_SDLMIXER
#  include "sys_audiod_sdlmixer.h"
#endif

#endif
