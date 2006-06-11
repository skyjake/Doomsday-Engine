/* DE1: $Id$
 * Copyright (C) 2006 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not: http://www.opensource.org/
 */

// HEADER FILES ------------------------------------------------------------

#ifdef UNIX
#  ifdef MACOSX
#    include "sys_dylib.h"
#  else
#    include <ltdl.h>
#  endif
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
#    define DEFAULT_LIB_NAME "libdropengl"
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

// PRIVATE DATA DEFINITIONS ------------------------------------------------

static HINSTANCE dglHandle;     // Instance handle to the rendering DLL.

// CODE --------------------------------------------------------------------

/*
 * DD_InitDGLDriver
 *  Returns true if no required functions are missing.
 *  All exported DGL functions should have the DG_ prefix (Driver/Graphics).
 */
int DD_InitDGLDriver(void)
{
    Req(Init);
    Req(Shutdown);

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
    Req(ReadPixels);

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

    // See if a specific renderer DLL is specified.
    if(ArgCheckWith("-gl", 1))
        libName = ArgNext();

    // Load the DLL.
#ifdef WIN32
    dglHandle = LoadLibrary(libName);
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
void   *DD_GetDGLProcAddress(const char *name)
{
#ifdef WIN32
    return GetProcAddress(dglHandle, name);
#endif
#ifdef UNIX
    return (void *) lt_dlsym(dglHandle, name);
#endif
}
