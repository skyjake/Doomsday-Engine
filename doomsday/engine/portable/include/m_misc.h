/**\file m_misc.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2012 Daniel Swanson <danij@dengine.net>
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
#include "m_vector.h"
#include "dfile.h"

#ifdef __cplusplus
extern "C" {
#endif

struct aaboxd_s;

#define ISSPACE(c)  ((c) == 0 || (c) == ' ' || (c) == '\t' || (c) == '\n' ||\
                     (c) == '\r')

// Memory.
void*           M_Malloc(size_t size);
void*           M_Calloc(size_t size);
void*           M_Realloc(void* ptr, size_t size);
void*           M_MemDup(const void* ptr, size_t size);
void            M_Free(void* ptr);

// File system utility routines.
size_t M_ReadFile(char const* path, char** buffer);
boolean M_WriteFile(char const* path, const char* source, size_t length);

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

// Bounding boxes.
void            M_ClearBox(fixed_t* box);
void            M_CopyBox(fixed_t dest[4], const fixed_t src[4]);
void            M_AddToBox(fixed_t* box, fixed_t x, fixed_t y);
float           M_BoundingBoxDiff(const float in[4], const float out[4]);
void            M_JoinBoxes(float box[4], const float other[4]);

// Text utilities.
char*           M_SkipWhite(char* str);
char*           M_FindWhite(char* str);
void            M_StripLeft(char* str);
void            M_StripRight(char* str, size_t len);
void            M_Strip(char* str, size_t len);
char*           M_SkipLine(char* str);
void            M_WriteCommented(FILE* file, const char* text);
void            M_WriteTextEsc(FILE* file, const char* text);
void            M_ReadLine(char* buffer, size_t len, DFile* file);

boolean         M_IsComment(const char* text);

/// @return  @c true if @a string can be interpreted as a valid integer.
boolean M_IsStringValidInt(const char* string);

/// @return  @c true if @a string can be interpreted as a valid byte.
boolean M_IsStringValidByte(const char* string);

/// @return  @c true if @a string can be interpreted as a valid floating-point value.
boolean M_IsStringValidFloat(const char* string);

char*           M_StrCat(char* buf, const char* str, size_t bufSize);
char*           M_StrnCat(char* buf, const char* str, size_t nChars, size_t bufSize);
char*           M_LimitedStrCat(char* buf, const char* str, size_t maxWidth,
                                char separator, size_t bufLength);
char*           M_StrCatQuoted(char* dest, const char* src, size_t len);
char*           M_StrTok(char** cursor, char* delimiters);
char*           M_TrimmedFloat(float val);

// Random numbers.
byte            RNG_RandByte(void);
float           RNG_RandFloat(void);
void            RNG_Reset(void);

/**
 * Math routines.
 */

/**
 * Gives an estimation of distance (not exact).
 */
double M_ApproxDistance(double dx, double dy);
float M_ApproxDistancef(float dx, float dy);

/**
 * Gives an estimation of 3D distance (not exact).
 */
double M_ApproxDistance3(double dx, double dy, double dz);
float M_ApproxDistance3f(float dx, float dy, float dz);

/**
 * To get a global angle from Cartesian coordinates, the coordinates are
 * flipped until they are in the first octant of the coordinate system, then
 * the y (<=x) is scaled and divided by x to get a tangent (slope) value
 * which is looked up in the tantoangle[] table.  The +1 size is to handle
 * the case when x==y without additional checking.
 *
 * @param x   X coordinate to test.
 * @param y   Y coordinate to test.
 *
 * @return  Angle between the test point and [0, 0].
 */
angle_t M_PointToAngle(double const point[2]);
angle_t M_PointXYToAngle(double x, double y);

/**
 * Translate a direction into an angle value (degrees).
 */
double M_DirectionToAngle(double const direction[2]);
double M_DirectionToAngleXY(double directionX, double directionY);

angle_t M_PointToAngle2(double const a[], double const b[]);
angle_t M_PointXYToAngle2(double aX, double aY, double bX, double bY);

double M_PointDistance(double const a[2], double const b[2]);
double M_PointXYDistance(double aX, double aY, double bX, double bY);

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

/**
 * Area of a triangle.
 */
double M_TriangleArea(double const v1[2], double const v2[2], double const v3[2]);

void M_RotateVector(float vec[3], float degYaw, float degPitch);

int M_CeilPow2(int num);
int M_FloorPow2(int num);
int M_RoundPow2(int num);
int M_WeightPow2(int num, float weight);
float M_CycleIntoRange(float value, float length);

/**
 * Using Euclid's Algorithm reduce the given numerator and denominator by
 * their greatest common integer divisor. @ingroup math
 * @param numerator  Input and output numerator.
 * @param denominator  Input and output denominator.
 * @return  Greatest common divisor.
 */
int M_RatioReduce(int* numerator, int* denominator);

slopetype_t M_SlopeType(double const direction[2]);
slopetype_t M_SlopeTypeXY(double directionX, double directionY);

int M_NumDigits(int num);

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

/**
 * Calculate CRC-32 for an arbitrary data buffer. @ingroup math
 */
uint M_CRC32(byte* data, uint length);

// Other utilities:

/**
 * Grabs the current contents of the frame buffer and outputs a Targa file.
 * Will create/overwrite as necessary.
 *
 * @param filePath      Local file path to write to.
 */
int M_ScreenShot(const char* filePath, int bits);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /// LIBDENG_M_MISC_H
