//===========================================================================
// REND_LIST.H
//===========================================================================
#ifndef __DOOMSDAY_REND_LIST_H__
#define __DOOMSDAY_REND_LIST_H__

#include "r_data.h"

// Multiplicative blending for dynamic lights?
#define IS_MUL	(!dlBlend && !useFog)

// PrepareFlat directions.
#define RLPF_NORMAL		0
#define RLPF_REVERSE	1

extern int		renderTextures;
extern int		useMultiTexLights;
extern int		useMultiTexDetails;

extern float	rend_light_wall_angle;
extern float	detailFactor, detailScale;

void RL_Init();
void RL_ClearLists();
void RL_DeleteLists();
void RL_AddPoly(rendpoly_t *poly);
void RL_PrepareFlat(planeinfo_t *plane, rendpoly_t *poly, subsector_t *subsector);
void RL_VertexColors(rendpoly_t *poly, int lightlevel, byte *rgb);
void RL_RenderAllLists();

void RL_SelectTexUnits(int count);
void RL_Bind(DGLuint texture);
void RL_BindTo(int unit, DGLuint texture);
void RL_FloatRGB(byte *rgb, float *dest);

#endif