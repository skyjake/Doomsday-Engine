/* DE1: $Id$
 * Copyright (C) 2003 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * d3dinit.cpp: Initialization and Shutdown of the Direct3D Interfaces
 */

// HEADER FILES ------------------------------------------------------------

#include "drD3D.h"

// MACROS ------------------------------------------------------------------

#define SUPPORT(x)	(x? "OK" : "not supported")

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

HRESULT					hr;
UINT					adapter;
IDirect3D8				*d3d = NULL;
IDirect3DDevice8		*dev = NULL;
D3DDISPLAYMODE			displayMode;
D3DPRESENT_PARAMETERS	presentParms;
D3DCAPS8				caps;

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// DXError
//===========================================================================
void DXError(const char *funcName)
{
	char buf[300];

	D3DXGetErrorString(hr, buf, 300);
	Con_Message("Direct3D: Call to %s failed:\n  %s\n", funcName, buf);
}

//===========================================================================
// GetMode
//	Returns a supported display mode that matches the current window 
//	configuration. Used only when running fullscreen.
//===========================================================================
boolean GetMode(D3DDISPLAYMODE *match, int wantedRefresh)
{
	D3DDISPLAYMODE mode, fallback;
	int i, num = d3d->GetAdapterModeCount(adapter);
	int targetBits = (!wantedColorDepth? window->bits : wantedColorDepth);
	int found = false;

	memset(match, 0, sizeof(*match));
	memset(&fallback, 0, sizeof(fallback));

	DP("GetMode:");
	DP("Requesting: %i x %i x %i", window->width, window->height, targetBits);

	if(verbose)
	{
		Con_Printf("Direct3D: Requesting %i x %i x %i.\n", 
			window->width, window->height, targetBits);
	}
	for(i = 0; i < num; i++)
	{
		if(FAILED(d3d->EnumAdapterModes(adapter, i, &mode)))
			continue;
		// Is this perhaps the mode we're looking for?
		// (Nice indentation.)
		if(mode.Width == window->width
			&& mode.Height == window->height
			&& (targetBits == 16 
					&& (mode.Format == D3DFMT_X1R5G5B5 
						|| mode.Format == D3DFMT_R5G6B5)
				|| targetBits == 32 
					&& (mode.Format == D3DFMT_X8R8G8B8 
						|| mode.Format == D3DFMT_A8R8G8B8)))
		{
			// This is at least a match.
			memcpy(&fallback, &mode, sizeof(mode));

			// If the refresh rate is closer, use it.
			if(abs(wantedRefresh - mode.RefreshRate)
				<= abs(wantedRefresh - match->RefreshRate))
			{
				// This might be the one!
				found = true;

				memcpy(match, &mode, sizeof(mode));
			}
		}
	}

	if(!found)
	{
		// Let's try the fallback.
		memcpy(match, &fallback, sizeof(fallback));
		found = true;
	}

	return found;
}

//===========================================================================
// PrintAdapterInfo
//===========================================================================
void PrintAdapterInfo(void)
{
	D3DADAPTER_IDENTIFIER8 id;

	if(FAILED(d3d->GetAdapterIdentifier(adapter, D3DENUM_NO_WHQL_LEVEL, 
		&id))) return;
	
	Con_Message("  Driver: %s\n", id.Driver);
	Con_Message("  Description: %s\n", id.Description);
}

//===========================================================================
// IsDepthFormatOk
//	Copied from the DX8 documentation. Thank you Microsoft.
//===========================================================================
BOOL IsDepthFormatOk( D3DFORMAT DepthFormat, D3DFORMAT AdapterFormat, 
                      D3DFORMAT BackBufferFormat ) 
{
    // Verify that the depth format exists.
    HRESULT hr = d3d->CheckDeviceFormat(D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL, AdapterFormat, D3DUSAGE_DEPTHSTENCIL,
		D3DRTYPE_SURFACE, DepthFormat);

    if( FAILED( hr ) ) return FALSE;

    // Verify that the depth format is compatible.
    hr = d3d->CheckDepthStencilMatch(D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL, AdapterFormat, BackBufferFormat, DepthFormat);
    return SUCCEEDED( hr );
}

//===========================================================================
// InitDirect3D
//===========================================================================
int	InitDirect3D(void)
{
	D3DDISPLAYMODE targetMode;
	D3DPRESENT_PARAMETERS *pp;
	int wantedRefresh;

	DP("InitDirect3D:");

	d3d = Direct3DCreate8(D3D_SDK_VERSION);
	if(!d3d) return DGL_ERROR;

	DP("  d3d=%p", d3d);

	// Read configuration from drD3D.ini (or display config dialog).
	ReadConfig();
	adapter = wantedAdapter;

	DP("  Using adapter %i", adapter);

	// Get the current display mode.
	if(FAILED(hr = d3d->GetAdapterDisplayMode(adapter, &displayMode)))
	{
		DXError("GetAdapterDisplayMode");
		return DGL_ERROR;
	}
	DP("Current display mode:");
	DP("  w=%i, h=%i, rfsh=%i, fmt=%i", 
		displayMode.Width, displayMode.Height, 
		displayMode.RefreshRate, displayMode.Format);

	// By default we'll use the current refresh rate.
	wantedRefresh = displayMode.RefreshRate;

	// Override refresh rate?
	if(ArgCheckWith("-refresh", 1))
		wantedRefresh = strtol(ArgNext(), 0, 0);

	// Let's see what this adapter can do for us.
	if(FAILED(hr = d3d->GetDeviceCaps(adapter, D3DDEVTYPE_HAL, &caps)))
	{
		DXError("GetDeviceCaps");
		return DGL_ERROR;
	}

#define DC(cn)	DP(#cn" = 0x%x", caps.cn)
#define DCi(cn)	DP(#cn" = %i", caps.cn)
#define DCf(cn)	DP(#cn" = %g", caps.cn)

	DP("Device caps:");
	DCi(DeviceType);
	DCi(AdapterOrdinal);
	DC(Caps);
    DC(Caps2);
    DC(Caps3);
    DC(PresentationIntervals);
	DC(CursorCaps);
	DC(DevCaps);
	DC(PrimitiveMiscCaps);
    DC(RasterCaps);
    DC(ZCmpCaps);
    DC(SrcBlendCaps);
    DC(DestBlendCaps);
    DC(AlphaCmpCaps);
    DC(ShadeCaps);
    DC(TextureCaps);
    DC(TextureFilterCaps);
    DC(CubeTextureFilterCaps);
    DC(VolumeTextureFilterCaps);
    DC(TextureAddressCaps);
    DC(VolumeTextureAddressCaps);
	DC(LineCaps);
	DCi(MaxTextureWidth);
	DCi(MaxTextureHeight);
    DCi(MaxVolumeExtent);
	DCi(MaxTextureRepeat);
    DCi(MaxTextureAspectRatio);
    DCi(MaxAnisotropy);
    DCf(MaxVertexW);
	DCf(GuardBandLeft);
    DCf(GuardBandTop);
    DCf(GuardBandRight);
    DCf(GuardBandBottom);
    DCf(ExtentsAdjust);
    DC(StencilCaps);
	DC(FVFCaps);
    DC(TextureOpCaps);
    DCi(MaxTextureBlendStages);
    DCi(MaxSimultaneousTextures);
    DC(VertexProcessingCaps);
    DC(MaxActiveLights);
    DC(MaxUserClipPlanes);
    DC(MaxVertexBlendMatrices);
    DC(MaxVertexBlendMatrixIndex);
	DCf(MaxPointSize);
	DCi(MaxPrimitiveCount);
    DCi(MaxVertexIndex);
    DCi(MaxStreams);
    DCi(MaxStreamStride);
	DC(VertexShaderVersion);
    DCi(MaxVertexShaderConst);
	DC(PixelShaderVersion);
    DCf(MaxPixelShaderValue);

	// Maximums.
	maxTextures = MIN_OF(caps.MaxSimultaneousTextures, MAX_TEX_UNITS);
	maxStages = caps.MaxTextureBlendStages;
	maxTexSize = MIN_OF(caps.MaxTextureWidth, caps.MaxTextureHeight);
	maxAniso = caps.MaxAnisotropy;
	availMulAdd = (caps.TextureOpCaps & D3DTEXOPCAPS_MULTIPLYADD) != 0;
	/*
	if(verbose)
	{*/
	Con_Message("Direct3D information:\n");
	PrintAdapterInfo();
	Con_Message("  Texture units: %i\n", maxTextures);
	Con_Message("  Texture blending stages: %i\n", maxStages);
	Con_Message("  Modulate2X: %s\n", 
		SUPPORT(caps.TextureOpCaps & D3DTEXOPCAPS_MODULATE2X));
	Con_Message("  MultiplyAdd: %s\n", SUPPORT(availMulAdd));
	Con_Message("  BlendFactorAlpha: %s\n", 
		SUPPORT(caps.TextureOpCaps & D3DTEXOPCAPS_BLENDFACTORALPHA));
	Con_Message("  Maximum texture size: %i x %i\n",
		caps.MaxTextureWidth, caps.MaxTextureHeight);
	if(caps.MaxTextureAspectRatio)
		Con_Message("  Maximum texture aspect ratio: 1:%i\n",
			caps.MaxTextureAspectRatio);
	Con_Message("  Maximum anisotropy: %i\n", maxAniso);
/*	}*/
	
	// Configure the presentation parameters.
	pp = &presentParms;
	memset(pp, 0, sizeof(*pp));
	pp->hDeviceWindow = window->hwnd;
	pp->Windowed = window->isWindow? TRUE : FALSE;
	pp->EnableAutoDepthStencil = TRUE;
	pp->AutoDepthStencilFormat = (wantedZDepth == 32? D3DFMT_D32 : D3DFMT_D16);

	DP("Presentation:");
	DP("  hwnd=%p", pp->hDeviceWindow);
	DP("  windowed=%i", pp->Windowed);
	DP("  EnabAutoDS=%i", pp->EnableAutoDepthStencil);
	DP("  AutoDSFmt=%i", pp->AutoDepthStencilFormat);
	
	if(window->isWindow)
	{
		// Running in a window.
		pp->BackBufferFormat = displayMode.Format;
		pp->SwapEffect = D3DSWAPEFFECT_DISCARD; // ?

		DP("  Going for windowed mode");
		DP("  BackBufFmt=%i", displayMode.Format);
		DP("  swpef=discard");
	}
	else
	{
		DP("  Going for fullscreen mode");

		// Running fullscreen.
		// Does the adapter support a display mode that suits our needs?
		if(!GetMode(&targetMode, wantedRefresh))
		{
			Con_Message("Direct3D: Display adapter does not support "
				"the requested mode.\n");
			return DGL_ERROR;
		}
		pp->BackBufferWidth = targetMode.Width;
		pp->BackBufferHeight = targetMode.Height;
		pp->BackBufferFormat = targetMode.Format;
		pp->FullScreen_RefreshRateInHz = targetMode.RefreshRate;
		pp->SwapEffect = D3DSWAPEFFECT_DISCARD;

		// Enable triple buffering?
		if(ArgExists("-triple"))
		{
			Con_Message("Direct3D: Triple buffering enabled.\n");
			pp->BackBufferCount = 2;
			pp->SwapEffect = D3DSWAPEFFECT_FLIP;
			pp->FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;
		}

		DP("  bbw=%i, bbh=%i bbfmt=%i", targetMode.Width,
			targetMode.Height, targetMode.Format);
	}

	DP("Verifying depth format:");
	if(!IsDepthFormatOk(pp->AutoDepthStencilFormat, pp->BackBufferFormat, 
		pp->BackBufferFormat))
	{
		DP("  current dsformat %i is not suitable", pp->AutoDepthStencilFormat);
		// Try the other one.
		pp->AutoDepthStencilFormat = (pp->AutoDepthStencilFormat == 
			D3DFMT_D32? D3DFMT_D16 : D3DFMT_D32);
		DP("  trying %i", pp->AutoDepthStencilFormat);
		if(!IsDepthFormatOk(pp->AutoDepthStencilFormat, pp->BackBufferFormat,
			pp->BackBufferFormat))
		{
			DP("  dsformat %i is not suitable, either; crash and burn imminent",
				pp->AutoDepthStencilFormat);
		}
	}

	DP("Creating device:");
	DP("  ad=%i, hal, hwnd=%p, softvp", adapter, window->hwnd);

	// Create the D3D device.
	if(FAILED(hr = d3d->CreateDevice(adapter, D3DDEVTYPE_HAL,
		window->hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, pp, &dev)))
	{
		DXError("CreateDevice");
		return DGL_ERROR;
	}
	dev->SetVertexShader(DRVTX_FORMAT);

	// Clear the screen with a mid-gray color.
	dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 
		D3DCOLOR_XRGB(128, 128, 128), 1, 0);

	// Everything has been initialized.
	return DGL_OK;
}

//===========================================================================
// ShutdownDirect3D
//===========================================================================
void ShutdownDirect3D(void)
{
	if(dev) dev->Release();
	dev = NULL;
	
	if(d3d) d3d->Release();
	d3d = NULL;
}
