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