#ifndef __DRD3D_WINDOW_H__
#define __DRD3D_WINDOW_H__

#include "drD3D.h"

class Window
{
public:
	int		x, y;
	uint	width, height;
	int		bits;
	boolean	isWindow;	// Windowed or fullscreen?
	HWND	hwnd;

	Window(HWND handle, int w = 0, int h = 0, int bit = 0, boolean fullscreen = false);
	void Setup(void);
	void UseDesktopBits(void);
	void GetClientOrigin(int &x, int &y);
};

#endif