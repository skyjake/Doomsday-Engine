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
 * window.h: Rendering Window
 */

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