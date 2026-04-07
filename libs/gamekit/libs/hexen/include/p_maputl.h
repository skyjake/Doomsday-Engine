/** @file p_maputl.h Movement/collision map utility functions.
 *
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBHEXEN_P_MAPUTL_H
#define LIBHEXEN_P_MAPUTL_H

#ifndef __JHEXEN__
#  error "Using jHexen headers without __JHEXEN__"
#endif

#include "p_mobj.h"

//void P_ApplyTorque(mobj_t *mo);

/**
 * Searches around for targetable monsters/players near @a mobj.
 *
 * @return  The targeted mobj if found; otherwise @c 0.
 */
mobj_t *P_RoughMonsterSearch(mobj_t *mobj, int distance);

#endif // LIBHEXEN_P_MAPUTL_H
