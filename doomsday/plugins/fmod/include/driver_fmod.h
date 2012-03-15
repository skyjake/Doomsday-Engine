/**
 * @file driver_fmod.h
 * FMOD Ex audio plugin. @ingroup dsfmod
 *
 * @authors Copyright © 2011-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/**
 * @defgroup dsfmod
 * FMOD Ex audio plugin.
 */

#ifndef __DSFMOD_DRIVER_H__
#define __DSFMOD_DRIVER_H__

#include <fmod.h>
#include <fmod.hpp>
#include <fmod_errors.h>
#include <stdio.h>
#include <cassert>
#include <iostream>

extern "C" {
    
int     DS_Init(void);
void    DS_Shutdown(void);
void    DS_Event(int type);
int     DS_Set(int prop, const void* ptr);

}

#ifdef DENG_DSFMOD_DEBUG
#  define DSFMOD_TRACE(args)  std::cerr << "[dsFMOD] " << args << std::endl;
#else
#  define DSFMOD_TRACE(args)
#endif

#define DSFMOD_ERRCHECK(result) \
    if(result != FMOD_OK) { \
        printf("[dsFMOD] Error at %s, line %i: (%d) %s\n", __FILE__, __LINE__, result, FMOD_ErrorString(result)); \
    }

extern FMOD::System* fmodSystem;

#include "fmod_sfx.h"
#include "fmod_music.h"
#include "fmod_cd.h"
#include "fmod_util.h"

#endif /* end of include guard: __DSFMOD_DRIVER_H__ */
