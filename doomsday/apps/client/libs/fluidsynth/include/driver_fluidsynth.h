/**
 * @file driver_fluidsynth.h
 * FluidSynth music plugin. @ingroup dsfluidsynth
 *
 * @authors Copyright © 2011-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * @defgroup dsfluidsynth
 * FluidSynth music plugin.
 */

#ifndef __DSFLUIDSYNTH_DRIVER_H__
#define __DSFLUIDSYNTH_DRIVER_H__

#include <stdio.h>
#include <cassert>
#include <iostream>
#include <fluidsynth.h>
#include <de/log.h>
#include "api_console.h"
#include "api_audiod_sfx.h"

//DE_ENTRYPOINT   int     DS_Init(void);
//DE_ENTRYPOINT   void    DS_Shutdown(void);
//DE_ENTRYPOINT   void    DS_Event(int type);
//DE_ENTRYPOINT   int     DS_Set(int prop, const void* ptr);

fluid_synth_t *                 DMFluid_Synth();
fluid_audio_driver_t *          DMFluid_Driver();
audiointerface_sfx_generic_t *  DMFluid_Sfx();

#define MAX_SYNTH_GAIN      0.4f

#define DSFLUIDSYNTH_TRACE(args)  LOGDEV_AUDIO_XVERBOSE("[FluidSynth] ", args)

#include "fluidsynth_music.h"

DE_USING_API(Con);

#endif /* end of include guard: __DSFLUIDSYNTH_DRIVER_H__ */
