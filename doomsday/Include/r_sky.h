//===========================================================================
// R_SKY.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_SKY_H__
#define __DOOMSDAY_REFRESH_SKY_H__

#include "r_model.h"

typedef struct skymodel_s {
	ded_skymodel_t *def;
	modeldef_t *model;	
	int frame;
	int timer;
	int maxTimer;
	float yaw;
} skymodel_t;

extern skymodel_t	skyModels[NUM_SKY_MODELS];
extern boolean		skyModelsInited;
extern boolean		alwaysDrawSphere;

void R_SetupSkyModels(ded_mapinfo_t *info);
void R_PrecacheSky(void);
void R_SkyTicker(void);

#endif 