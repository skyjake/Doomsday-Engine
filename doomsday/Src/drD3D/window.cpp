
//**************************************************************************
//**
//** WINDOW.CPP
//**
//** Target:		DGL Driver for Direct3D 8.1
//** Description:	Configuration of the actual Windows window
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "drD3D.h"

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// FUNCTION PROTOTYPES -----------------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

//===========================================================================
// Window Constructor
//===========================================================================
Window::Window(HWND handle, int w, int h, int bt, boolean fullscreen)
{
	x = y = 0;
	width = w;
	bits = bt;
	height = h;
	isWindow = !fullscreen;
	hwnd = handle;

	DP("Window:");
	DP("  x=%i, y=%i, w=%i, h=%i, bits=%i", x, y, width, height, bits);
	DP("  isWnd=%i, hwnd=%p", isWindow, hwnd);
}

//===========================================================================
// UseDesktopBits
//===========================================================================
void Window::UseDesktopBits(void)
{
	HWND hDesktop = GetDesktopWindow();
	HDC desktopHDC = GetDC(hDesktop);
	bits = GetDeviceCaps(desktopHDC, PLANES) * GetDeviceCaps(desktopHDC, 
		BITSPIXEL);
	ReleaseDC(hDesktop, desktopHDC);

	DP("DesktopBits:");
	DP("  bits=%i", bits);
}

//===========================================================================
// Setup
//	window, height, etc. must be set before calling this.
//===========================================================================
void Window::Setup(void)
{
	uint realWidth = GetSystemMetrics(SM_CXSCREEN);
	uint realHeight = GetSystemMetrics(SM_CYSCREEN);
	int xOff, yOff;
	RECT rect;

	// Update our bitdepth if it's not yet known.
	if(!bits) UseDesktopBits();

	DP("Window setup:");

	if(isWindow)
	{
		if(width > realWidth) width = realWidth;
		if(height > realHeight) height = realHeight;
		
		// Center by default.
		xOff = (realWidth - width) / 2;
		yOff = (realHeight - height) / 2;
		
		// Check for modifier options.
		if(ArgCheck("-nocenter")) xOff = yOff = 0;
		if(ArgCheckWith("-xpos", 1)) xOff = atoi(ArgNext());
		if(ArgCheckWith("-ypos", 1)) yOff = atoi(ArgNext());

		LONG style = GetWindowLong(hwnd, GWL_STYLE)
			| WS_VISIBLE | WS_CAPTION 
			| WS_CLIPCHILDREN | WS_CLIPSIBLINGS
			| WS_SYSMENU | WS_MINIMIZEBOX;
		SetWindowLong(hwnd, GWL_STYLE, style);

		// Adjust the size of the window so the client area is the 
		// right size.
		rect.left = rect.top = 0;
		rect.right = width;
		rect.bottom = height;
		AdjustWindowRect(&rect, style, FALSE);

		// But we also want the window's top left corner to be at 
		// (xOff, yOff).
		SetWindowPos(hwnd, HWND_TOP, xOff, yOff, 
			rect.right - rect.left, 
			rect.bottom - rect.top, 0);

		DP("  Windowed mode: x=%i, y=%i, w=%i, h=%i",
			xOff, yOff, rect.right - rect.left, rect.bottom - rect.top);
	}
	else
	{
		// In fullscreen, the setup is much simpler.
		SetWindowLong(hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP 
			| WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
		SetWindowPos(hwnd, HWND_TOP, 0, 0, width, height, 0);

		DP("  Fullscreen mode: w=%i, h=%i", width, height);
	}
}

//===========================================================================
// GetClientOrigin
//	Returns the actual (x,y) coordinates of the game window's client area.
//===========================================================================
void Window::GetClientOrigin(int &x, int &y)
{
	POINT orig = { 0, 0 };
	
	ClientToScreen(hwnd, &orig);
	x = orig.x;
	y = orig.y;
}