//===========================================================================
// REND_MODEL.H
//===========================================================================
#ifndef __DOOMSDAY_RENDER_MODEL_H__
#define __DOOMSDAY_RENDER_MODEL_H__

#include "r_things.h"

extern int		modelLight;
extern int		frameInter;
extern int		mirrorHudModels;
extern int		modelShinyMultitex;
extern float	rend_model_lod;

void Rend_RenderModel(vissprite_t *spr);

#endif 