// dr???.dll
// The Doomsday graphics library driver for ???

#include "../dd_gl.h"

// The APIs.
gl_import_t gim;
gl_export_t gex;


int Init(int width, int height, int bpp, int fullscreen)
{
	return DGL_OK;
}


void Shutdown(void)
{
}


int	ChangeMode(int width, int height, int bpp, int fullscreen)
{
	return DGL_OK;
}


void Viewport(int x, int y, int width, int height)
{
}

void Scissor(int x, int y, int width, int height)
{
}


int	GetInteger(int value)
{
	return DGL_FALSE;
}


int	SetInteger(int name, int value)
{
	return DGL_FALSE;
}


char* GetString(int value)
{
	return NULL;
}
	

void Enable(int cap)
{
}

void Disable(int cap)
{
}


DGLuint	LoadTexture(int format, int width, int height, int mipmap, void *data)
{
	return 0;
}

	
void TexParam(int pname, int param)
{
}


void Palette(int format, void *data)
{
}


int	Bind(DGLuint texture)
{
	return 0;
}

	
void MatrixMode(int mode)
{
}


void PushMatrix(void)
{
}


void PopMatrix(void)
{
}


void LoadIdentity(void)
{
}


void Translatef(float x, float y, float z)
{
}


void Rotatef(float angle, float x, float y, float z)
{
}


void Scalef(float x, float y, float z)
{
}


void Ortho(float left, float top, float right, float bottom, float near, float far)
{
}


void Perspective(float fovy, float aspect, float zNear, float zFar)
{
}


void Color3ub(DGLubyte r, DGLubyte g, DGLubyte b)
{
}


void Color3ubv(void *data)
{
}


void Color4ub(DGLubyte r, DGLubyte g, DGLubyte b, DGLubyte a)
{
}


void Color4ubv(void *data)
{
}


void Color3f(float r, float g, float b)
{
}


void Color3fv(float *data)
{
}


void Color4f(float r, float g, float b, float a)
{
}


void Color4fv(float *data)
{
}


int	BeginScene(void)
{
	return DGL_OK;
}


int	EndScene(void)
{
	return DGL_OK;
}


void Begin(int mode)
{
}


void End(void)
{
}


void Vertex2f(float x, float y)
{
}


void Vertex2fv(float *data)
{
}


void Vertex3f(float x, float y, float z)
{
}


void Vertex3fv(float *data)
{
}


void TexCoord2f(float s, float t)
{
}


void TexCoord2fv(float *data)
{
}
	

void RenderList(int format, void *data)
{
}


int Grab(int x, int y, int width, int height, int format, void *buffer)
{
	return DGL_OK;
}


void Fog(int pname, int param)
{
}


void Fogv(int pname, void *data)
{
}


// The API exchange.
gl_export_t* GetGLAPI(gl_import_t *api)
{
	// Get the imports, with them we can print stuff and
	// generate error messages.
	memcpy(&gim, api, api->apiSize);
			
	memset(&gex, 0, sizeof(gex));
	
	// Fill in the exports. Nothing must be left null!
	gex.version = DGL_VERSION;	// The version this driver was compiled with.

	gex.Init = Init;
	gex.Shutdown = Shutdown;
	gex.ChangeMode = ChangeMode;

	gex.Viewport = Viewport;
	gex.Scissor = Scissor;

	gex.GetInteger = GetInteger;
	gex.SetInteger = SetInteger;
	gex.GetString = GetString;
	gex.Enable = Enable;
	gex.Disable = Disable;

	gex.LoadTexture = LoadTexture;
	gex.TexParam = TexParam;
	gex.Palette = Palette;
	gex.Bind = Bind;

	gex.MatrixMode = MatrixMode;
	gex.PushMatrix = PushMatrix;
	gex.PopMatrix = PopMatrix;
	gex.LoadIdentity = LoadIdentity;
	gex.Translatef = Translatef;
	gex.Rotatef = Rotatef;
	gex.Scalef = Scalef;
	gex.Ortho = Ortho;
	gex.Perspective = Perspective;

	gex.Color3ub = Color3ub;
	gex.Color3ubv = Color3ubv;
	gex.Color4ub = Color4ub;
	gex.Color4ubv = Color4ubv;
	gex.Color3f = Color3f;
	gex.Color3fv = Color3fv;
	gex.Color4f = Color4f;
	gex.Color4fv = Color4fv;

	gex.BeginScene = BeginScene;
	gex.EndScene = EndScene;
	gex.Begin = Begin;
	gex.End = End;
	gex.Vertex2f = Vertex2f;
	gex.Vertex2fv = Vertex2fv;
	gex.Vertex3f = Vertex3f;
	gex.Vertex3fv = Vertex3fv;
	gex.TexCoord2f = TexCoord2f;
	gex.TexCoord2fv = TexCoord2fv;
	
	gex.RenderList = RenderList;
	gex.Grab = Grab;
	gex.Fog = Fog;
	gex.Fogv = Fogv;

	return &gx;
}
