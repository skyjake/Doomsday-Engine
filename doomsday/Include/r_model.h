//===========================================================================
// R_MODEL.H
//===========================================================================
#ifndef __DOOMSDAY_REFRESH_MODEL_H__
#define __DOOMSDAY_REFRESH_MODEL_H__

#include "gl_model.h"

#define MAX_FRAME_MODELS		DED_MAX_SUB_MODELS

// Model frame flags.
#define MFF_FULLBRIGHT			0x0001
#define MFF_SHADOW1				0x0002
#define MFF_SHADOW2				0x0004
#define MFF_BRIGHTSHADOW		0x0008
#define MFF_MOVEMENT_PITCH		0x0010	// Pitch aligned to movement.
#define MFF_SPIN				0x0020	// Spin around (for bonus items).
#define MFF_SKINTRANS			0x0040	// Color translation -> skins.
#define MFF_AUTOSCALE			0x0080	// Scale to match sprite height.
#define MFF_MOVEMENT_YAW		0x0100
#define MFF_DONT_INTERPOLATE	0x0200	// Don't interpolate from the frame.
#define MFF_BRIGHTSHADOW2		0x0400
#define MFF_ALIGN_YAW			0x0800
#define MFF_ALIGN_PITCH			0x1000
#define MFF_DARKSHADOW			0x2000
#define MFF_IDSKIN				0x4000	// Mobj id -> skin in skin range
#define MFF_DISABLE_Z_WRITE		0x8000
#define MFF_NO_DISTANCE_CHECK	0x00010000
#define MFF_SELSKIN				0x00020000
#define MFF_PARTICLE_SUB1		0x00040000 // Sub1 center is particle origin.
#define MFF_NO_PARTICLES		0x00080000 // No particles for this object.
#define MFF_SHINY_SPECULAR		0x00100000 // Shiny skin rendered as additive.
#define MFF_SHINY_LIT			0x00200000 // Shiny skin is not fullbright.
#define MFF_IDFRAME				0x00400000 // Mobj id -> frame in frame range
#define MFF_IDANGLE				0x00800000 // Mobj id -> static angle offset
#define MFF_DIM					0x01000000 // Never fullbright.
#define MFF_SUBTRACT			0x02000000 // Subtract blending.
#define MFF_REVERSE_SUBTRACT	0x04000000 // Reverse subtract blending.

typedef struct
{
	short			model;
	short			frame;
	char			framerange;
	int				flags;
	short			skin;
	char			skinrange;
	float			offset[3];
	byte			alpha;
	short			shinyskin;	// Skinname ID (index)
} submodeldef_t;

typedef struct modeldef_s
{
	int				order;

	state_t			*state;		// Pointer to the states list (in dd_defns.c).
	int				flags;
	unsigned int	group;
	int				select;
	short			skintics;
	float			intermark;	// [0,1) When is this frame in effect?
	float			interrange[2];
	float			offset[3];
	float			resize, scale[3];
	float			ptcoffset[3];
	float			visualradius;
	ded_model_t		*def;

	// Points to next inter-frame, or NULL.
	struct modeldef_s *internext;
	
	// Points to next selector, or NULL (only for "base" modeldefs).
	struct modeldef_s *selectnext;

	// Submodels.
	submodeldef_t	sub[MAX_FRAME_MODELS];	// 8
} modeldef_t;

extern modeldef_t	*models;
extern int			nummodels;
extern int			useModels;
extern float		rModelAspectMod;

void R_InitModels(void);
void R_ShutdownModels(void);
void R_ClearModelPath(void);
void R_AddModelPath(char *addPath, boolean append);
float R_CheckModelFor(mobj_t *mo, modeldef_t **mdef, modeldef_t **nextmdef);
void R_SetSpriteReplacement(int sprite, char *modelname);
int R_FindModelFile(const char *filename, char *outfn);
byte *R_LoadSkin(model_t *mdl, int skin, int *width, int *height, int *pxsize);
void R_PrecacheSkinsForMobj(mobj_t *mo);

#endif 