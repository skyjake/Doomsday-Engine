#ifndef __DOOMSDAY_REND_MAIN_H__
#define __DOOMSDAY_REND_MAIN_H__

#include "rend_list.h"
#include "r_things.h"

// Parts of a wall segment.
#define SEG_MIDDLE	0x1
#define SEG_TOP		0x2
#define SEG_BOTTOM	0x4

extern float	vx, vy, vz, vang, vpitch, fieldOfView, yfov;
extern boolean	smoothTexAnim;
extern float	viewsidex, viewsidey;	
extern int		missileBlend, litSprites;
extern boolean	whitefog;
extern int		r_ambient;

void	Rend_Init(void);
void	Rend_Reset(void);

void	Rend_RenderMap(void);
void	Rend_ModelViewMatrix(boolean use_angles);

float	Rend_PointDist2D(float c[2]);
float	Rend_PointDist3D(float c[3]);
float	Rend_SignedPointDist2D(float c[2]);
int		Rend_SegFacingDir(float v1[2], float v2[2]);
int		Rend_MidTexturePos(float *top, float *bottom, float *texoffy, 
						   float tcyoff, boolean lower_unpeg);

#endif