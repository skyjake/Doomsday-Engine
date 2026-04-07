/** @file importdeh.h  DeHackEd patch reader plugin for Doomsday Engine.
 *
 * @ingroup dehread
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDEHREAD_DEHREAD_H
#define LIBDEHREAD_DEHREAD_H

#include <doomsday/defs/ded.h>

struct font_s;

#include <doomsday.h>

#include <de/libcore.h>
#include <de/legacy/types.h>

// Internal:
extern ded_t *ded; // @todo Remove me.

const int NUMSPRITES = 138;
const int NUMSTATES  = 968;
extern ded_sprid_t  origSpriteNames[NUMSPRITES];
extern de::String origActionNames[NUMSTATES];

DE_USING_API(Base);
DE_USING_API(Con);
DE_USING_API(Def);
DE_USING_API(F);

#endif // LIBDEHREAD_DEHREAD_H
