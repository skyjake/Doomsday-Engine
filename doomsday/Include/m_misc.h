//===========================================================================
// M_MISC.H
//===========================================================================
#ifndef __DOOMSDAY_MISCELLAN_H__
#define __DOOMSDAY_MISCELLAN_H__

#include "sys_file.h"

#define MAX_READ	500
#define ISSPACE(c)	((c) == 0 || (c) == ' ' || (c) == '\t' || (c) == '\n' || (c) == '\r')

extern int	read_count;
extern int	rndindex;

// Memory.
void *		M_Malloc(size_t size);
void *		M_Calloc(size_t size);
void *		M_Realloc(void *ptr, size_t size);
void		M_Free(void *ptr);

// File system routines.
boolean		M_CheckFileID(const char *path);
int			M_ReadFile(char const *name, byte **buffer);
int			M_ReadFileCLib(char const *name, byte **buffer);
boolean		M_WriteFile(char const *name, void *source, int length);
void		M_ExtractFileBase(const char *path, char *dest);
void		M_GetFileExt(const char *path, char *ext);
void		M_ReplaceFileExt(char *path, char *newext);
boolean		M_CheckPath(char *path);
int			M_FileExists(const char *file);
void		M_TranslatePath(const char *path, char *translated);
void		M_PrependBasePath(const char *path, char *newpath);
void		M_RemoveBasePath(const char *absPath, char *newPath);
void		M_ReadLine(char *buffer, int len, DFILE *file);

// Bounding boxes.
void		M_ClearBox (fixed_t *box);
void		M_AddToBox (fixed_t *box, fixed_t x, fixed_t y);
float		M_BoundingBoxDiff(float in[4], float out[4]);

// Text utilities.
char *		M_SkipWhite(char *str);
char *		M_FindWhite(char *str);
char *		M_SkipLine(char *str);
void		M_WriteCommented(FILE *file, char *text);
void		M_WriteTextEsc(FILE *file, char *text);
boolean		M_IsComment(char *text);
char *		M_LimitedStrCat(const char *str, unsigned int maxWidth, char separator, char *buf, unsigned int bufLength);

// Random numbers.
byte		M_Random(void);
float		M_FRandom(void);
void		M_ClearRandom(void);

// Math routines.
float		M_ApproxDistancef(float dx, float dy);
float		M_ApproxDistance3f(float dx, float dy, float dz);
float		M_PointLineDistance(float *a, float *b, float *c);
float		M_PointUnitLineDistance(float *a, float *b, float *c);
float		M_Normalize(float *a);
float		M_DotProduct(float *a, float *b);
void		M_CrossProduct(float *a, float *b, float *out);
void		M_PointCrossProduct(float *v1, float *v2, float *v3, float *out);
void		M_RotateVector(float vec[3], float degYaw, float degPitch);
void		M_ProjectPointOnLinef(fixed_t *point, fixed_t *linepoint, fixed_t *delta, float gap, float *result);
float		M_CycleIntoRange(float value, float length);

// Other utilities.
int			M_ScreenShot(char *filename, int bits);

#endif 