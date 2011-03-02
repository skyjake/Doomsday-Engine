/**\file m_misc.h
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2005-2011 Daniel Swanson <danij@dengine.net>
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

#include "dd_types.h"
#include "sys_file.h"

#define ISSPACE(c)  ((c) == 0 || (c) == ' ' || (c) == '\t' || (c) == '\n' ||\
                     (c) == '\r')

// Memory.
void*           M_Malloc(size_t size);
void*           M_Calloc(size_t size);
void*           M_Realloc(void* ptr, size_t size);
void            M_Free(void* ptr);

// File system utility routines.
size_t M_ReadFile(char const* name, byte** buffer);
size_t M_ReadFileCLib(char const* name, byte** buffer);
boolean M_WriteFile(char const* name, void* source, size_t length);

void M_ExtractFileBase(char* dest, const char* path, size_t len);
void M_ExtractFileBase2(char* dest, const char* path, size_t len, int ignore);
const char* M_FindFileExtension(const char* path);

/**
 * The new extension must not include a dot.
 */
void M_ReplaceFileExt(char* path, const char* newext, size_t len);

boolean M_CheckPath(const char* path);
int M_FileExists(const char* file);

void M_TranslatePath(char* translated, const char* path, size_t len);
void M_PrependBasePath(char* newpath, const char* path, size_t len);
void M_RemoveBasePath(char* newPath, const char* absPath, size_t len);

/**
 * @return  A 'prettier' copy of the original path.
 */
const char* M_PrettyPath(const char* path);

/**
 * Removes references to the current (.) and parent (..) directories.
 * The given path should be an absolute path.
 */
void M_ResolvePath(char* path);

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
char*           M_SkipLine(char* str);
void            M_WriteCommented(FILE* file, const char* text);
void            M_WriteTextEsc(FILE* file, const char* text);
void            M_ReadLine(char* buffer, size_t len, DFILE* file);

boolean         M_IsComment(const char* text);
boolean         M_IsStringValidInt(const char* str);
boolean         M_IsStringValidByte(const char* str);
boolean         M_IsStringValidFloat(const char* str);
char*           M_LimitedStrCat(char* buf, const char* str, size_t maxWidth,
                                char separator, size_t bufLength);
char*           M_StrCatQuoted(char* dest, const char* src, size_t len);
char*           M_StrTok(char** cursor, char* delimiters);
char* M_TrimmedFloat(float val);

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
 * their greatest common integer divisor.
 * @param numerator  Input and output numerator.
 * @param denominator  Input and output denominator.
 * @return  Greatest common divisor.
 */
int M_RatioReduce(int* numerator, int* denominator);

double          M_SlopeToAngle(double dx, double dy);
double          M_Length(double x, double y);
int             M_NumDigits(int num);
uint            M_CRC32(byte* data, uint length);

// Time utilities.
boolean         M_RunTrigger(trigger_t* trigger, timespan_t advanceTime);
boolean         M_CheckTrigger(const trigger_t* trigger, timespan_t advanceTime);

// Other utilities:

/**
 * Grabs the current contents of the frame buffer and outputs a Targa file.
 * Will create/overwrite as necessary.
 *
 * @param filePath      Local file path to write to.
 */
int M_ScreenShot(const char* filePath, int bits);

#endif /* LIBDENG_M_MISC_H */
