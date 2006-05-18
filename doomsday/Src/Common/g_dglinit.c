/* DE1: $Id$
 * Copyright (C) 2004 Jaakko Keränen <jaakko.keranen@iki.fi>
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

/*
 * Retrieves the function addresses we need from the rendering DLL.
 */

// HEADER FILES ------------------------------------------------------------

#include "doomsday.h"
#include "g_dgl.h"

// MACROS ------------------------------------------------------------------

#define Imp(fname)  gl.fname = DD_GetDGLProcAddress("DG_"#fname)

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

gamedgl_t gl;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

/*
 * Init the game's interface to the DGL driver.
 * Since the engine has already loaded the DGL DLL successfully, we can
 * assume no required functions are missing.
 */
void G_InitDGL(void)
{
    // Viewport.
    Imp(Clear);
    Imp(Show);
    Imp(Viewport);
    Imp(Scissor);

    // State.
    Imp(GetInteger);
    Imp(GetIntegerv);
    Imp(SetInteger);
    Imp(GetString);
    Imp(Enable);
    Imp(Disable);
    Imp(Func);

    // Textures.
    Imp(NewTexture);
    Imp(DeleteTextures);
    Imp(TexImage);
    Imp(TexParameter);
    Imp(GetTexParameterv);
    Imp(Palette);
    Imp(Bind);

    // Matrix operations.
    Imp(MatrixMode);
    Imp(PushMatrix);
    Imp(PopMatrix);
    Imp(LoadIdentity);
    Imp(Translatef);
    Imp(Rotatef);
    Imp(Scalef);
    Imp(Ortho);
    Imp(Perspective);

    // Colors.
    Imp(Color3ub);
    Imp(Color3ubv);
    Imp(Color4ub);
    Imp(Color4ubv);
    Imp(Color3f);
    Imp(Color3fv);
    Imp(Color4f);
    Imp(Color4fv);

    // Drawing.
    Imp(Begin);
    Imp(End);
    Imp(Vertex2f);
    Imp(Vertex2fv);
    Imp(Vertex3f);
    Imp(Vertex3fv);
    Imp(TexCoord2f);
    Imp(TexCoord2fv);
    Imp(Vertices2ftv);
    Imp(Vertices3ftv);
    Imp(Vertices3fctv);

    // Miscellaneous.
    Imp(Grab);
    Imp(Fog);
    Imp(Fogv);
    Imp(Project);
    Imp(ReadPixels);
}
