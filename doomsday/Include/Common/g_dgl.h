//===========================================================================
// G_DGL.H
//	The game's interface to DGL. Only includes the functions the game can
//	safely access.	
//===========================================================================
#ifndef __GAME_DGL_DRIVER_H__
#define __GAME_DGL_DRIVER_H__

#include "../dglib.h"

typedef struct gamedgl_s
{
	// Viewport.
	void	(*Clear)(int bufferbits);
	void	(*Show)(void);
	void	(*Viewport)(int x, int y, int width, int height);
	void	(*Scissor)(int x, int y, int width, int height);

	// State.
	int		(*GetInteger)(int name);
	int		(*GetIntegerv)(int name, int *v);
	int		(*SetInteger)(int name, int value);
	char*	(*GetString)(int name);
	int		(*Enable)(int cap);
	void	(*Disable)(int cap);
	void	(*Func)(int func, int param1, int param2);

	// Textures.
	DGLuint	(*NewTexture)(void);
	void	(*DeleteTextures)(int num, DGLuint *names);
	int		(*TexImage)(int format, int width, int height, int mipmap, void *data);
	void	(*TexParameter)(int pname, int param);
	void	(*GetTexParameterv)(int level, int pname, int *v);
	void	(*Palette)(int format, void *data);	
	int		(*Bind)(DGLuint texture);

	// Matrix operations.
	void	(*MatrixMode)(int mode);
	void	(*PushMatrix)(void);
	void	(*PopMatrix)(void);
	void	(*LoadIdentity)(void);
	void	(*Translatef)(float x, float y, float z);
	void	(*Rotatef)(float angle, float x, float y, float z);
	void	(*Scalef)(float x, float y, float z);
	void	(*Ortho)(float left, float top, float right, float bottom, float znear, float zfar);
	void	(*Perspective)(float fovy, float aspect, float zNear, float zFar);

	// Colors.
	void	(*Color3ub)(DGLubyte r, DGLubyte g, DGLubyte b);
	void	(*Color3ubv)(void *data);
	void	(*Color4ub)(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a);
	void	(*Color4ubv)(void *data);
	void	(*Color3f)(float r, float g, float b);
	void	(*Color3fv)(float *data);
	void	(*Color4f)(float r, float g, float b, float a);
	void	(*Color4fv)(float *data);

	// Drawing.
	void	(*Begin)(int mode);
	void	(*End)(void);
	void	(*Vertex2f)(float x, float y);
	void	(*Vertex2fv)(float *data);
	void	(*Vertex3f)(float x, float y, float z);
	void	(*Vertex3fv)(float *data);
	void	(*TexCoord2f)(float s, float t);
	void	(*TexCoord2fv)(float *data);
	void	(*Vertices2ftv)(int num, gl_ft2vertex_t *data);
	void	(*Vertices3ftv)(int num, gl_ft3vertex_t *data);
	void	(*Vertices3fctv)(int num, gl_fct3vertex_t *data);

	// Miscellaneous.
	int		(*Grab)(int x, int y, int width, int height, int format, void *buffer);
	void	(*Fog)(int pname, float param);
	void	(*Fogv)(int pname, void *data);
	int		(*Project)(int num, gl_fc3vertex_t *inVertices, gl_fc3vertex_t *outVertices);
	int		(*ReadPixels)(int *inData, int format, void *pixels); 
}
gamedgl_t;

extern gamedgl_t gl;

void	G_InitDGL(void);

#endif 
