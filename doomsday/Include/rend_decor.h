//===========================================================================
// REND_DECOR.H
//===========================================================================
#ifndef __DOOMSDAY_RENDER_DECOR_H__
#define __DOOMSDAY_RENDER_DECOR_H__

extern boolean	useDecorations;
extern float	decorWallMaxDist; // No decorations are visible beyond this.
extern float	decorPlaneMaxDist;
extern float	decorWallFactor;
extern float	decorPlaneFactor;

void	Rend_InitDecorationsForFrame(void);
void	Rend_ProjectDecorations(void);

#endif 