/**
 * @file dehread.h
 * DeHackEd patch reader plugin for Doomsday Engine. @ingroup dehread
 *
 * @author Copyright &copy; 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @author Copyright &copy; 2006-2012 Daniel Swanson <danij@dengine.net>
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
 * @defgroup dehread
 * DeHackEd patch reader plugin.
 */

#ifndef LIBDEHREAD_DEHREAD_H
#define LIBDEHREAD_DEHREAD_H

/**
 * @attention @todo This plugin requires access to the internal definition arrays.
 * This dependency should be removed entirely, by making this plugin modify the
 * definitions via a public API provided by the engine.
 */
#include "../../../engine/include/def_data.h"
struct font_s;

#define __INTERNAL_MAP_DATA_ACCESS__
#include <doomsday.h>

#include <de/libdeng2.h>
#include <de/types.h>

extern "C" {

void    DP_Initialize(void);

}

// Internal:
extern ded_t* ded; // @todo Remove me.

const int NUMSPRITES = 138;
const int NUMSTATES  = 968;
extern ded_sprid_t  origSpriteNames[NUMSPRITES];
extern ded_funcid_t origActionNames[NUMSTATES];

DENG_USING_API(Def);
DENG_USING_API(Plug);
DENG_USING_API(Uri);
DENG_USING_API(W);

#endif /// LIBDEHREAD_DEHREAD_H
