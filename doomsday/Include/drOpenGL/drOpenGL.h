//===========================================================================
// DGL OpenGL Rasterizer
//===========================================================================
#ifndef __DROPENGL_H__
#define __DROPENGL_H__

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#include <string.h>

#include "../doomsday.h"
#include "../dglib.h"

#define USE_MULTITEXTURE	1

#define DROGL_VERSION		210
#define DROGL_VERSION_TEXT	"2.1.0"
#define DROGL_VERSION_FULL	"DGL OpenGL Driver Version "DROGL_VERSION_TEXT" ("__DATE__")"

enum { VX, VY, VZ };
enum { CR, CG, CB, CA };

typedef struct
{
	unsigned char color[4];
} rgba_t;

typedef struct glvertex_s { 
	float pos[3];
	float color[4];
	float tex[2];
	float tex2[2];
	float tex3[2];
	float tex4[2];
} glvertex_t;


//-------------------------------------------------------------------------
// main.c
//
extern int			useFog, maxTexSize;
extern int			palExtAvailable, sharedPalExtAvailable;
extern boolean		texCoordPtrEnabled;
extern int			verbose;
extern int			useAnisotropic;
extern float		maxAniso;
extern int			maxTexUnits;

void DG_Clear(int bufferbits);


//-------------------------------------------------------------------------
// draw.c
//
extern int			polyCounter;

void CheckError(void);
void DG_Begin(int mode);
void DG_End(void);
void DG_Color3ub(DGLubyte r, DGLubyte g, DGLubyte b);
void DG_Color3ubv(void *data);
void DG_Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
void DG_Color4ubv(void *data);
void DG_Color3f(float r, float g, float b);
void DG_Color3fv(float *data);
void DG_Color4f(float r, float g, float b, float a);
void DG_Color4fv(float *data);
void DG_Vertex2f(float x, float y);
void DG_Vertex2fv(float *data);
void DG_Vertex3f(float x, float y, float z);
void DG_Vertex3fv(float *data);
void DG_TexCoord2f(float s, float t);
void DG_TexCoord2fv(float *data);
void DG_Vertices2ftv(int num, gl_ft2vertex_t *data);
void DG_Vertices3ftv(int num, gl_ft3vertex_t *data);
void DG_Vertices3fctv(int num, gl_fct3vertex_t *data);


//-------------------------------------------------------------------------
// texture.c
//
extern rgba_t		palette[256];
extern int			usePalTex, dumpTextures, useCompr;
extern float		grayMipmapFactor;

int Power2(int num);
int enablePalTexExt(int enable);
DGLuint DG_NewTexture(void);
int DG_TexImage(int format, int width, int height, int mipmap, void *data);
void DG_DeleteTextures(int num, DGLuint *names);
void DG_TexParameter(int pname, int param);
void DG_GetTexParameterv(int level, int pname, int *v);
void DG_Palette(int format, void *data);
int	DG_Bind(DGLuint texture);


//-------------------------------------------------------------------------
// ext.c
//
extern PFNGLCLIENTACTIVETEXTUREARBPROC	glClientActiveTextureARB;
extern PFNGLACTIVETEXTUREARBPROC	glActiveTextureARB;
extern PFNGLMULTITEXCOORD2FARBPROC	glMultiTexCoord2fARB;
extern PFNGLMULTITEXCOORD2FVARBPROC	glMultiTexCoord2fvARB;
extern PFNGLBLENDEQUATIONEXTPROC	glBlendEquationEXT;
extern PFNGLLOCKARRAYSEXTPROC		glLockArraysEXT;
extern PFNGLUNLOCKARRAYSEXTPROC		glUnlockArraysEXT;

extern int extMultiTex;
extern int extTexEnvComb;
extern int extNvTexEnvComb;
extern int extAtiTexEnvComb;
extern int extAniso;
extern int extGenMip;
extern int extBlendSub;
extern int extS3TC;

void initExtensions(void);

#endif