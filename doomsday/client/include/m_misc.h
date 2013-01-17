/**\file m_misc.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
 * Miscellanous Routines.
 */

#ifndef LIBDENG_M_MISC_H
#define LIBDENG_M_MISC_H

#include <stdio.h>

#include "dd_types.h"
#include <de/vector1.h>
#include "filehandle.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aaboxd_s;

/**
 * Reads x bits from the source stream and writes them to out.
 *
 * \warning Output buffer must be large enough to hold at least @a numBits!
 *
 * @param numBits  Number of bits to be read.
 * @param src  Current position in the source stream.
 * @param cb  Current byte. Used for tracking the current byte being read.
 * @param out  Read bits are ouput here.
 */
void M_ReadBits(uint numBits, const uint8_t** src, uint8_t* cb, uint8_t* out);

// Text utilities.
void M_WriteCommented(FILE* file, const char* text);
void M_WriteTextEsc(FILE* file, const char* text);
void M_ReadLine(char* buffer, size_t len, FileHandle* file);

/**
 * Check the spatial relationship between the given box and a partitioning line.
 *
 * @param box            Box being tested.
 * @param linePoint      Point on the line.
 * @param lineDirection  Direction of the line (slope).
 *
 * @return  @c <0= bbox is wholly on the left side.
 *          @c  0= line intersects bbox.
 *          @c >0= bbox wholly on the right side.
 */
int M_BoxOnLineSide(const struct aaboxd_s* box, double const linePoint[2], double const lineDirection[2]);

int M_BoxOnLineSide_FixedPrecision(const fixed_t box[], const fixed_t linePoint[], const fixed_t lineDirection[]);

/**
 * Check the spatial relationship between the given box and a partitioning line.
 *
 * An alternative version of M_BoxOnLineSide() which allows specifying many of the
 * intermediate values used in the calculation a priori for performance reasons.
 *
 * @param box            Box being tested.
 * @param linePoint      Point on the line.
 * @param lineDirection  Direction of the line (slope).
 * @param linePerp       Perpendicular distance of the line.
 * @param lineLength     Length of the line.
 * @param epsilon        Points within this distance will be considered equal.
 *
 * @return  @c <0= bbox is wholly on the left side.
 *          @c  0= line intersects bbox.
 *          @c >0= bbox wholly on the right side.
 */
int M_BoxOnLineSide2(const struct aaboxd_s* box, double const linePoint[2],
    double const lineDirection[2], double linePerp, double lineLength, double epsilon);

typedef struct trigger_s {
    timespan_t duration;
    timespan_t accum;
} trigger_t;

/**
 * Advances time and return true if the trigger is triggered.
 *
 * @param trigger      Time trigger.
 * @param advanceTime  Amount of time to advance the trigger.
 *
 * @return              @c true, if the trigger has accumulated enough time
 *                      to fill the trigger's time threshold.
 */
boolean M_RunTrigger(trigger_t* trigger, timespan_t advanceTime);

/**
 * Checks if the trigger will trigger after @a advanceTime seconds.
 * The trigger itself is not modified in any way.
 *
 * @param trigger      Time trigger.
 * @param advanceTime  Amount of time to advance the trigger.
 *
 * @return @c true, if the trigger will accumulate enough time after @a advanceTime
 *         to fill the trigger's time threshold.
 */
boolean M_CheckTrigger(const trigger_t* trigger, timespan_t advanceTime);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_M_MISC_H
