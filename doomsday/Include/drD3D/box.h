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
 * box.h: Generic Box Class
 */

#ifndef __DRD3D_BOX_H__
#define __DRD3D_BOX_H__

class Box 
{
public:
	int		x, y, width, height;

	Box(int X = 0, int Y = 0, int W = 0, int H = 0) { Set(X, Y, W, H); }
	void Set(int X, int Y, int W, int H);
};

#endif