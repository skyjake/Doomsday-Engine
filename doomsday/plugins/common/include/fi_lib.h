/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2010 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBCOMMON_INFINE_LIB
#define LIBCOMMON_INFINE_LIB

#include "dd_share.h"

typedef struct finale_conditions_s {
    // Bits:
    uint        secret : 1;
    uint        leavehub : 1;
} finale_extradata_t;

#define FIRCF_LEAVEHUB          0x1

void            FI_RegisterHooks(void);

int             FI_GetGameState(void);
void            FI_DemoEnds(void);

#endif /* LIBCOMMON_INFINE_LIB */
