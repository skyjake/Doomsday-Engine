//===========================================================================
// REND_SHADOW.H
//===========================================================================
#ifndef __DOOMSDAY_RENDER_SHADOW_H__
#define __DOOMSDAY_RENDER_SHADOW_H__

extern int		useShadows, shadowMaxRad, shadowMaxDist;
extern float	shadowFactor;

//void	Rend_SubsectorShadows(subsector_t *sub);
void	Rend_RenderShadows(void);

#endif 