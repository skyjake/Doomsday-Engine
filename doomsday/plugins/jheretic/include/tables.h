/**\file
 * Copyright © 2006 Jaakko Keränen <skyjake@dengine.net>
 * Copyright © 2006 Daniel Swanson <danij@dengine.net>
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
 *
 * In addition, as a special exception, we, the authors of deng
 * give permission to link the code of our release of deng with
 * the libjhexen and/or the libjheretic libraries (or with modified
 * versions of it that use the same license as the libjhexen or
 * libjheretic libraries), and distribute the linked executables.
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than “libjhexen or libjheretic”. If
 * you modify this file, you may extend this exception to your
 * version of the file, but you are not obligated to do so. If you
 * do not wish to do so, delete this exception statement from your version.
 */

/*
 * Lookup tables.
 */

#ifndef __TABLES__
#define __TABLES__

#include "doomsday.h"

#define PI              3.141592657

//#include "m_fixed.h"

#define FINEANGLES      8192
#define FINEMASK        (FINEANGLES-1)

// 0x100000000 to 0x2000
#define ANGLETOFINESHIFT    19

// Effective size is 10240.
extern fixed_t  finesine[5 * FINEANGLES / 4];

// Re-use data, is just PI/2 pahse shift.
extern fixed_t *finecosine;

// Effective size is 4096.
extern fixed_t  finetangent[FINEANGLES / 2];

// Binary Angle Measument, BAM.
#define ANG45       0x20000000
#define ANG90       0x40000000
#define ANG180      0x80000000
#define ANG270      0xc0000000

#define SLOPERANGE      2048
#define SLOPEBITS       11
#define DBITS           (FRACBITS-SLOPEBITS)

// Effective size is 2049;
// The +1 size is to handle the case when x==y
//  without additional checking.
extern angle_t  tantoangle[SLOPERANGE + 1];

// Utility function,
//  called by R_PointToAngle.
int             SlopeDiv(unsigned num, unsigned den);

#endif
