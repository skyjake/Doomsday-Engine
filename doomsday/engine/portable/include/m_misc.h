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
#include "dfile.h"

#define ISSPACE(c)  ((c) == 0 || (c) == ' ' || (c) == '\t' || (c) == '\n' ||\
                     (c) == '\r')

// Memory.
void*           M_Malloc(size_t size);
void*           M_Calloc(size_t size);
void*           M_Realloc(void* ptr, size_t size);
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

// Math routines.
float           M_ApproxDistancef(float dx, float dy);
float           M_ApproxDistance3(const float delta[3]);
float           M_ApproxDistance3f(float dx, float dy, float dz);
float           M_PointLineDistance(const float* a, const float* b, const float* c);
float           M_PointUnitLineDistance(const float* a, const float* b, const float* c);
float           M_Normalize(float* a);
float           M_Distance(const float* a, const float* b);
void            M_Scale(float* dest, const float* a, float scale);
float           M_DotProduct(const float* a, const float* b);
void            M_CrossProduct(const float* a, const float* b, float* out);
void            M_PointCrossProduct(const float* v1, const float* v2,
                                    const float* v3, float* out);
float           M_TriangleArea(const float* v1, const float* v2,
                               const float* v3);
void            M_RotateVector(float vec[3], float degYaw, float degPitch);
float           M_ProjectPointOnLine(const float* point, const float* linepoint,
                                     const float* delta, float gap,
                                     float* result);
void            M_ProjectViewRelativeLine2D(const float center[2],
                                            boolean alignToViewPlane,
                                            float width, float offset,
                                            float start[2], float end[2]);
double          M_ParallelDist(double lineDX, double lineDY, double linePara,
                               double lineLength, double x, double y);
double          M_PerpDist(double lineDX, double lineDY, double linePerp,
                           double lineLength, double x, double y);
int             M_CeilPow2(int num);
int             M_FloorPow2(int num);
int             M_RoundPow2(int num);
int             M_WeightPow2(int num, float weight);
float           M_CycleIntoRange(float value, float length);

/**
 * Using Euclid's Algorithm reduce the given numerator and denominator by
 * their greatest common integer divisor. @ingroup math
 * @param numerator  Input and output numerator.
 * @param denominator  Input and output denominator.
 * @return  Greatest common divisor.
 */
int M_RatioReduce(int* numerator, int* denominator);

double          M_SlopeToAngle(double dx, double dy);
double          M_Length(double x, double y);
int             M_NumDigits(int num);

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

#endif /* LIBDENG_M_MISC_H */
