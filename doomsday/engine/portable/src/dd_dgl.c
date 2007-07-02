/**\file
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2007 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2006 Jamie Jones <yagisan@dengine.net>
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

// HEADER FILES ------------------------------------------------------------

#ifdef UNIX
#include "sys_dylib.h"
typedef lt_dlhandle HINSTANCE;
#endif

#include "de_platform.h"
#include "de_base.h"
#include "de_misc.h"

// MACROS ------------------------------------------------------------------

// Default values.
#if defined WIN32
#  define DEFAULT_LIB_NAME "drOpenGL.dll"
#elif defined UNIX
#  ifdef MACOSX
#    define DEFAULT_LIB_NAME "drOpenGL.bundle"
#  else
#    define DEFAULT_LIB_NAME "libdropengl.so"
#  endif
#endif

// Optional function. Doesn't need to be exported.
#if defined WIN32
#   define Opt(fname) gl.fname = (void*) GetProcAddress(dglHandle, "DG_"#fname)
#elif defined UNIX
#   define Opt(fname) gl.fname = lt_dlsym(dglHandle, "DG_"#fname)
#endif

// Required function. If not exported, the rendering DLL can't be used.
#define Req(fname)  if(( Opt(fname) ) == NULL) return false

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

dgldriver_t __gl;               // Engine's internal function table.

#ifdef _DEBUG
dgldriver_t __gl2;
#endif

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HINSTANCE dglHandle;     // Instance handle to the rendering DLL.

#ifdef _DEBUG
uint glThreadId = 0; // ID of the thread who's allowed to use the GL.
#endif

// CODE --------------------------------------------------------------------

#define ASSERT_THREAD() assert(glThreadId == Sys_ThreadID())

#define DEF_VOID_VOID(F) void Routed_##F(void) { ASSERT_THREAD(); __gl2.F(); }
#define DEF_VOID_1(F, T1) void Routed_##F(T1 a) { ASSERT_THREAD(); __gl2.F(a); }
#define DEF_VOID_2(F, T1, T2) void Routed_##F(T1 a, T2 b) { ASSERT_THREAD(); __gl2.F(a, b); }
#define DEF_VOID_3(F, T1, T2, T3) void Routed_##F(T1 a, T2 b, T3 c) { ASSERT_THREAD(); __gl2.F(a, b, c); }
#define DEF_VOID_4(F, T1, T2, T3, T4) void Routed_##F(T1 a, T2 b, T3 c, T4 d) { ASSERT_THREAD(); __gl2.F(a, b, c, d); }
#define DEF_VOID_5(F, T1, T2, T3, T4, T5) void Routed_##F(T1 a, T2 b, T3 c, T4 d, T5 e) { ASSERT_THREAD(); __gl2.F(a, b, c, d, e); }
#define DEF_VOID_6(F, T1, T2, T3, T4, T5, T6) void Routed_##F(T1 a, T2 b, T3 c, T4 d, T5 e, T6 f) { ASSERT_THREAD(); __gl2.F(a, b, c, d, e, f); }
#define DEF_VOID(RT, F) RT Routed_##F(void) { ASSERT_THREAD(); return __gl2.F(); }
#define DEF_1(RT, F, T1) RT Routed_##F(T1 a) { ASSERT_THREAD(); return __gl2.F(a); }
#define DEF_2(RT, F, T1, T2) RT Routed_##F(T1 a, T2 b) { ASSERT_THREAD(); return __gl2.F(a, b); }
#define DEF_3(RT, F, T1, T2, T3) RT Routed_##F(T1 a, T2 b, T3 c) { ASSERT_THREAD(); return __gl2.F(a, b, c); }
#define DEF_4(RT, F, T1, T2, T3, T4) RT Routed_##F(T1 a, T2 b, T3 c, T4 d) { ASSERT_THREAD(); return __gl2.F(a, b, c, d); }
#define DEF_5(RT, F, T1, T2, T3, T4, T5) RT Routed_##F(T1 a, T2 b, T3 c, T4 d, T5 e) { ASSERT_THREAD(); return __gl2.F(a, b, c, d, e); }
#define DEF_6(RT, F, T1, T2, T3, T4, T5, T6) RT Routed_##F(T1 a, T2 b, T3 c, T4 d, T5 e, T6 f) { ASSERT_THREAD(); return __gl2.F(a, b, c, d, e, f); }

#ifdef _DEBUG
DEF_VOID_VOID(DestroyContext)
DEF_VOID_VOID(Show)
DEF_VOID_VOID(PushMatrix)
DEF_VOID_VOID(PopMatrix)
DEF_VOID_VOID(LoadIdentity)
DEF_VOID_VOID(UnlockArrays)
DEF_VOID_VOID(End)
DEF_VOID(DGLuint, NewTexture)
DEF_VOID_1(Clear, int)
DEF_VOID_1(Disable, int)
DEF_VOID_1(ZBias, int);
DEF_VOID_1(MatrixMode, int)
DEF_VOID_1(ArrayElement, int)
DEF_VOID_1(Begin, int)
DEF_VOID_1(Color4ubv, void*)
DEF_VOID_1(Vertex2fv, float*)
DEF_VOID_1(Vertex3fv, float*)
DEF_VOID_1(Color3ubv, void*)
DEF_VOID_1(Color3fv, float*)
DEF_VOID_1(Color4fv, float*)
DEF_VOID_1(TexCoord2fv, float*)
DEF_VOID_2(DeleteTextures, int, DGLuint*)
DEF_VOID_2(TexParameter, int, int)
DEF_VOID_2(Vertex2f, float, float)
DEF_VOID_2(MultiTexCoord2fv, int, float*)
DEF_VOID_2(Vertices2ftv, int, gl_ft2vertex_t*)
DEF_VOID_2(Vertices3ftv, int, gl_ft3vertex_t*)
DEF_VOID_2(Vertices3fctv, int, gl_fct3vertex_t*)
DEF_VOID_2(TexCoord2f, float, float)
DEF_VOID_2(Palette, int, void*)
DEF_VOID_2(Fog, int, float)
DEF_VOID_2(Fogv, int, void*)
DEF_VOID_3(EnableArrays, int, int, int)
DEF_VOID_3(DisableArrays, int, int, int)
DEF_VOID_3(Func, int, int, int)
DEF_VOID_3(GetTexParameterv, int, int, int*)
DEF_VOID_3(Translatef, float, float, float)
DEF_VOID_3(Scalef, float, float, float)
DEF_VOID_3(Color3ub, DGLubyte, DGLubyte, DGLubyte)
DEF_VOID_3(Color3f, float, float, float)
DEF_VOID_3(Vertex3f, float, float, float)
DEF_VOID_3(MultiTexCoord2f, int, float, float)
DEF_VOID_3(DrawElements, int, int, unsigned int*)
DEF_VOID_4(Viewport, int, int, int, int)
DEF_VOID_4(Scissor, int, int, int, int)
DEF_VOID_4(Color4ub, DGLubyte, DGLubyte, DGLubyte, DGLubyte)
DEF_VOID_4(Color4f, float, float, float, float)
DEF_VOID_4(Rotatef, float, float, float, float)
DEF_VOID_4(Perspective, float, float, float, float)
DEF_VOID_5(Arrays, void*, void*, int, void**, int)
DEF_VOID_6(Ortho, float, float, float, float, float, float)
DEF_1(int, GetInteger, int)
DEF_1(char*, GetString, int)
DEF_1(int, Enable, int)
DEF_1(int, Bind, DGLuint)
DEF_2(int, GetIntegerv, int, int*)
DEF_2(int, SetInteger, int, int)
DEF_2(int, SetFloatv, int, float*)
// DEF_3(int, ReadPixels, int*, int, void*)
DEF_3(int, Project, int, gl_fc3vertex_t*, gl_fc3vertex_t*)
DEF_3(int, ChangeVideoMode, int, int, int)
DEF_4(int, CreateContext, int, int, int, int)
DEF_5(int, TexImage, int, int, int, int, void*)
DEF_6(int, Grab, int, int, int, int, int, void*)

#endif

void DD_RouteAPI(void)
{
#ifdef _DEBUG
    glThreadId = Sys_ThreadID();

    memcpy(&__gl2, &__gl, sizeof(__gl));
    
#define ROUTE(n) __gl.n = Routed_##n;
    ROUTE(CreateContext);
    ROUTE(DestroyContext);
    ROUTE(ChangeVideoMode);
    ROUTE(Clear);
    ROUTE(Show);
    ROUTE(Viewport);
    ROUTE(Scissor);
    ROUTE(GetInteger);
    ROUTE(GetIntegerv);
    ROUTE(SetInteger);
    ROUTE(SetFloatv);
    ROUTE(GetString);
    ROUTE(Enable);
    ROUTE(Disable);
    ROUTE(EnableArrays);
    ROUTE(DisableArrays);
    ROUTE(Arrays);
    ROUTE(UnlockArrays);
    ROUTE(Func);
    ROUTE(NewTexture);
    ROUTE(DeleteTextures);
    ROUTE(TexImage);
    ROUTE(TexParameter);
    ROUTE(GetTexParameterv);
    ROUTE(Palette);
    ROUTE(Bind);
    ROUTE(MatrixMode);
    ROUTE(PushMatrix);
    ROUTE(PopMatrix);
    ROUTE(LoadIdentity);
    ROUTE(Translatef);
    ROUTE(Rotatef);
    ROUTE(Scalef);
    ROUTE(Ortho);
    ROUTE(Perspective);
    ROUTE(Color3ub);
    ROUTE(Color3ubv);
    ROUTE(Color4ub);
    ROUTE(Color4ubv);
    ROUTE(Color3f);
    ROUTE(Color3fv);
    ROUTE(Color4f);
    ROUTE(Color4fv);
    ROUTE(Begin);
    ROUTE(End);
    ROUTE(Vertex2f);
    ROUTE(Vertex2fv);
    ROUTE(Vertex3f);
    ROUTE(Vertex3fv);
    ROUTE(TexCoord2f);
    ROUTE(TexCoord2fv);
    ROUTE(MultiTexCoord2f);
    ROUTE(MultiTexCoord2fv);
    ROUTE(Vertices2ftv);
    ROUTE(Vertices3ftv);
    ROUTE(Vertices3fctv);
    ROUTE(Arrays);
    ROUTE(UnlockArrays);
    ROUTE(ArrayElement);
    ROUTE(DrawElements);
    ROUTE(Grab);
    ROUTE(Fog);
    ROUTE(Fogv);
    ROUTE(Project);
//    ROUTE(ReadPixels);
#undef ROUTE
#endif
}

/**
 * DD_InitDGLDriver
 *  Returns true if no required functions are missing.
 *  All exported DGL functions should have the DG_ prefix (Driver/Graphics).
 */
int DD_InitDGLDriver(void)
{    
    Req(Init);
    Req(Shutdown);
    Req(ChangeVideoMode);
    Req(CreateContext);
    Req(DestroyContext);

    // Viewport.
    Req(Clear);
    Req(Show);
    Req(Viewport);
    Req(Scissor);

    // State.
    Req(GetInteger);
    Req(GetIntegerv);
    Req(SetInteger);
    Req(SetFloatv);
    Req(GetString);
    Req(Enable);
    Req(Disable);
    Req(EnableArrays);
    Req(DisableArrays);
    Req(Arrays);
    Req(UnlockArrays);
    Req(Func);

    // Textures.
    Req(NewTexture);
    Req(DeleteTextures);
    Req(TexImage);
    Req(TexParameter);
    Req(GetTexParameterv);
    Req(Palette);
    Req(Bind);

    // Matrix operations.
    Req(MatrixMode);
    Req(PushMatrix);
    Req(PopMatrix);
    Req(LoadIdentity);
    Req(Translatef);
    Req(Rotatef);
    Req(Scalef);
    Req(Ortho);
    Req(Perspective);

    // Colors.
    Req(Color3ub);
    Req(Color3ubv);
    Req(Color4ub);
    Req(Color4ubv);
    Req(Color3f);
    Req(Color3fv);
    Req(Color4f);
    Req(Color4fv);

    // Drawing.
    Req(Begin);
    Req(End);
    Req(Vertex2f);
    Req(Vertex2fv);
    Req(Vertex3f);
    Req(Vertex3fv);
    Req(TexCoord2f);
    Req(TexCoord2fv);
    Req(MultiTexCoord2f);
    Req(MultiTexCoord2fv);
    Req(Vertices2ftv);
    Req(Vertices3ftv);
    Req(Vertices3fctv);
    Req(Arrays);
    Req(UnlockArrays);
    Req(ArrayElement);
    Req(DrawElements);

    // Miscellaneous.
    Req(Grab);
    Req(Fog);
    Req(Fogv);
    Req(Project);
//    Req(ReadPixels);

    DD_RouteAPI();
    
    // All was OK.
    return true;
}

/*
 * DD_InitDGL
 *  Load the rendering DLL and setup the driver struct. The rendering DLL
 *  could be changed at runtime (but such an operation is currently never
 *  done). Returns true if successful.
 */
int DD_InitDGL(void)
{
    char   *libName = DEFAULT_LIB_NAME;

    if(ArgCheck("-dedicated"))
        return true;

    // Load the DLL.
#ifdef WIN32
    dglHandle = LoadLibrary((LPCTSTR) libName);
#endif
#ifdef UNIX
    dglHandle = lt_dlopenext(libName);
#endif
    if(!dglHandle)
    {
#ifdef WIN32
        DD_ErrorBox(true, "DD_InitDGL: Loading of %s failed (error %i).\n",
                    libName, GetLastError());
#endif
#ifdef UNIX
        DD_ErrorBox(true, "DD_InitDGL: Loading of %s failed.\n  %s.\n",
                    libName, lt_dlerror());
#endif
        return false;
    }

    // Prepare the driver struct.
    if(!DD_InitDGLDriver())
    {
        DD_ErrorBox(true, "DD_InitDGL: Rendering library %s "
                    "is incompatible.\n", libName);
        return false;
    }

    // Check the version of the DLL.
    if(gl.GetInteger(DGL_VERSION) < DGL_VERSION_NUM)
    {
        DD_ErrorBox(true,
                    "DD_InitDGL: Version %i renderer found. "
                    "Version %i is required.\n", gl.GetInteger(DGL_VERSION),
                    DGL_VERSION_NUM);
        return false;
    }

    // Looking good, try to init.
    if(!gl.Init())
        return false;

    return true;
}

/*
 * DD_ShutdownDGL
 *  Free the rendering DLL. You should shut it down first.
 */
void DD_ShutdownDGL(void)
{
#ifdef WIN32
    FreeLibrary(dglHandle);
#endif
#ifdef UNIX
    lt_dlclose(dglHandle);
#endif
    dglHandle = NULL;
}

/*
 * DD_GetDGLProcAddress
 *  Used by other modules (the Game) to get the addresses of the
 *  DGL routines.
 */
void *DD_GetDGLProcAddress(const char *name)
{
#ifdef WIN32
    return GetProcAddress(dglHandle, name);
#endif
#ifdef UNIX
    return (void *) lt_dlsym(dglHandle, name);
#endif
}
