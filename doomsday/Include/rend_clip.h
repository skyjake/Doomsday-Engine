//===========================================================================
// REND_CLIP.H
//===========================================================================
#ifndef __DOOMSDAY_RENDER_CLIP_H__
#define __DOOMSDAY_RENDER_CLIP_H__

#include "m_bams.h"

void		C_Init(void);
boolean		C_IsFull(void);
void		C_ClearRanges(void);
void		C_Ranger(void); // Debugging aid.
int			C_SafeAddRange(binangle_t startAngle, binangle_t endAngle);
boolean		C_IsPointVisible(float x, float y, float height);

// Add a segment relative to the current viewpoint.
void		C_AddViewRelSeg(float x1, float y1, float x2, float y2);

// Add an occlusion segment relative to the current viewpoint.
void		C_AddViewRelOcclusion(float *v1, float *v2, float height, boolean tophalf);

// Check a segment relative to the current viewpoint.
int			C_CheckViewRelSeg(float x1, float y1, float x2, float y2);

// Returns 1 if the specified angle is visible.
int			C_IsAngleVisible(binangle_t bang);

// Returns 1 if the subsector might be visible.
int			C_CheckSubsector(subsector_t *ssec);

#endif 