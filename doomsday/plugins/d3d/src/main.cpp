/**\file
 *\section Copyright and License Summary
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2006 Jaakko Keränen <skyjake@dengine.net>
 *\author Copyright © 2006 Daniel Swanson <danij@dengine.net>
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

/*
 * main.cpp: DGL Driver for Direct3D 9
 */

// HEADER FILES ------------------------------------------------------------

#include <cstdarg>
#include "drd3d.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

HWND        hwnd = NULL;
HINSTANCE   hinst;  // Instance handle to the DLL.
Window      *window;
boolean     verbose, diagnose = false;

// Limits and capabilities.
int         maxTexSize, maxTextures, maxStages, maxAniso;
boolean     useBadAlpha;

// Availability flags.
boolean     availPalTex, availMulAdd;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch(fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        hinst = hinstDLL;
        break;
    }
    return TRUE;
}

/**
 * Diagnose printf.
 */
void DP(const char *format, ...)
{
    if(!diagnose)
        return;

    va_list args;
    char buf[2048];
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Con_Message("%s\n", buf);
}

/**
 * @param mode          Either DGL_MODE_WINDOW or DGL_MODE_FULLSCREEN.
 * @param bpp           If <code>0</code>, use the current display color depth.
 *
 * @return              <code>DGL_OK</code> if successful.
 */
int DG_Init(int width, int height, int bpp, int mode)
{
    Con_Message("DG_Init: Direct3D 9.\n");
    verbose = ArgExists("-verbose");
    diagnose = ArgExists("-diag");
    useBadAlpha = ArgExists("-badtexalpha");
    d3d = NULL;
    dev = NULL;

    // The window handle must be provided by now.
    if(!hwnd) Con_Error("DG_Init: No window handle specified!\n");

    // Position and resize the window appropriately.
    window = new Window(hwnd, width, height, bpp, mode == DGL_MODE_FULLSCREEN);
    window->Setup();

    if(!InitDirect3D())
    {
        Con_Error("DG_Init: Failed to initialize Direct3D.\n");
        return DGL_ERROR;
    }
    //InitVertexBuffers();
    InitDraw();
    InitMatrices();
    InitState();
    InitTextures();

    // No errors encountered.
    return DGL_OK;
}

/**
 * This is called during display mode changes and at final shutdown.
 */
void DG_Shutdown(void)
{
    Con_Message("DG_Shutdown: Shutting down Direct3D...\n");

    ShutdownTextures();
    ShutdownMatrices();
    //ShutdownVertexBuffers();
    ShutdownDirect3D();
    delete window;
    window = NULL;
}

void DG_Clear(int bufferbits)
{
    dev->Clear(0, NULL,
        (bufferbits & DGL_COLOR_BUFFER_BIT? D3DCLEAR_TARGET : 0) |
        (bufferbits & DGL_DEPTH_BUFFER_BIT? D3DCLEAR_ZBUFFER : 0),
        0, 1, 0);
}

void DG_Show(void)
{
    dev->Present(NULL, NULL, NULL, NULL);
}

int DG_Grab(int x, int y, int width, int height, int format, void *buffer)
{
    if(format != DGL_RGB)
        return DGL_UNSUPPORTED;

    D3DDISPLAYMODE dispMode;
    IDirect3DSurface9 *copyFront;
    D3DLOCKED_RECT lockRect;
    int         i, k, winX, winY;
    byte       *out, *in;

    dev->GetDisplayMode(0, &dispMode);

    // Create the surface that will hold a copy of the *entire* front buffer.
    // In windowed mode we must figure out where exactly the game window is
    // ourselves...
    if(FAILED(hr = dev->CreateOffscreenPlainSurface(
        dispMode.Width, dispMode.Height,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &copyFront, NULL)))
    {
        DXError("CreateImageSurface");
        return DGL_ERROR;
    }

    // Make a copy of the front buffer.
    if(FAILED(hr = dev->GetFrontBufferData(0, copyFront)))
    {
        DXError("GetFrontBuffer");
        return DGL_ERROR;
    }

    // We need to copy it into the caller's buffer.
    if(FAILED(hr = copyFront->LockRect(&lockRect, 0, D3DLOCK_READONLY)))
    {
        DXError("LockRect");
        return DGL_ERROR;
    }

    // Make the coords relative to the game window.
    window->GetClientOrigin(winX, winY);
    x += winX;
    y += winY;

    for(k = y + height - 1, out = (byte*) buffer; k >= y; k--)
    {
        in = (byte*) lockRect.pBits + k*lockRect.Pitch + 4*x;
        for(i = 0; i < width; i++, in += 4)
        {
            *out++ = in[2];
            *out++ = in[1];
            *out++ = in[0];
        }
    }

    // Release the copy of the front buffer.
    copyFront->UnlockRect();
    copyFront->Release();

    return DGL_OK;
}

/**
 * NOTE: DEPRECATED
 */
int DG_ReadPixels(int *inData, int format, void *pixels)
{
    return DGL_UNSUPPORTED;
}

/**
 * NOTE: DEPRECATED
 *
 * The caller must provide both the in and out buffers.
 * @return          The number of vertices returned in the out buffer. The out
 *                  buffer will not include clipped vertices.
 */
int DG_Project(int num, gl_fc3vertex_t *inVertices,
               gl_fc3vertex_t *outVertices)
{
    return DGL_UNSUPPORTED;
}
