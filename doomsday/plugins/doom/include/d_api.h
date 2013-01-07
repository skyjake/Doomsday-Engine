/**\file d_api.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2006-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
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
 * Doomsday API exchange - DOOM specific.
 */

#ifndef LIBJDOOM_API_H
#define LIBJDOOM_API_H

#ifndef __JDOOM__
#  error "Using jDoom headers without __JDOOM__"
#endif

#include "doomsday.h"

#ifdef __cplusplus
extern "C" {
#endif

extern game_export_t gx;

DENG_USING_API(Base);
DENG_USING_API(B);
DENG_USING_API(Busy);
DENG_USING_API(Client);
DENG_USING_API(Con);
DENG_USING_API(Def);
DENG_USING_API(F);
DENG_USING_API(FR);
DENG_USING_API(GL);
DENG_USING_API(Infine);
DENG_USING_API(InternalData);
DENG_USING_API(Map);
DENG_USING_API(Material);
DENG_USING_API(MaterialArchive);
DENG_USING_API(MPE);
DENG_USING_API(Player);
DENG_USING_API(Plug);
DENG_USING_API(R);
DENG_USING_API(Rend);
DENG_USING_API(S);
DENG_USING_API(Server);
DENG_USING_API(Svg);
DENG_USING_API(Thinker);
DENG_USING_API(Uri);
DENG_USING_API(W);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBJDOOM_API_H */
